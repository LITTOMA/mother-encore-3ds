#include "encore/coin_scene.hpp"
#include <cmath>

namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool identity(const CoinParent&p,uint64_t epoch){return p.epoch==epoch&&p.token&&p.position.x==0&&p.position.y==0&&p.scale.x==1&&p.scale.y==1&&p.rotation==0;}
}
bool CoinSceneFactories::context(uint64_t epoch,const BreakablesData&b,const BreakableServices&bs,const CoinServices&s,std::string&e)const{
 if(!valid_||!data_||!coins_||data_->generation()!=generation_||coins_->generation()!=coin_generation_||epoch!=epoch_||bs.epoch!=epoch||s.epoch!=epoch||bs.scene!=data_->scene()||s.session!=session_||s.random!=random_||!data_->compatible(b,*coins_,e)||factories_.size()!=data_->bindings().size()||delivered_.size()!=factories_.size()||tickets_.size()!=factories_.size()||bs.requests.size()>4096)return fail(e,"Coin scene factory source/epoch/services");
 for(const auto&request:bs.requests)if(uint32_t(request.kind)<1||uint32_t(request.kind)>6)return fail(e,"Coin scene unknown Breakable request kind");
 for(const auto&parent:parents_){const auto v=s.parents.find(parent.first);if(v==s.parents.end()||v->second.token!=parent.second||!identity(v->second,epoch)||!bs.parents.count(parent.first))return fail(e,"Coin scene factory parent handle/transform changed");}
 for(const auto&v:data_->bindings()){const auto f=factories_.find(v.owner);const auto ticket=tickets_.find(v.owner);if(f==factories_.end()||ticket==tickets_.end()||f->second.instance_!=ticket->second||f->second.data_.get()!=coins_.get()||!bs.factories.count(v.path)||!f->second.context(epoch,s,e)||f->second.position().x!=v.position.x||f->second.position().y!=v.position.y)return fail(e,"Coin scene concrete factory unavailable/rebound/moved");}
 return true;
}
bool CoinSceneFactories::prepare(std::shared_ptr<const CoinSceneData>d,std::shared_ptr<const CoinsData>coins,const BreakablesData&b,uint64_t epoch,const std::map<uint32_t,uint64_t>&handles,const BreakableServices&bs,CoinServices&s,std::string&e){
 if(valid_||!d||!coins||!d->valid()||!coins->valid()||!epoch||handles.size()!=d->bindings().size()||!d->compatible(b,*coins,e))return fail(e,"Coin scene factory prepare/source/handles");
 CoinSceneFactories v;v.data_=std::move(d);v.coins_=std::move(coins);v.epoch_=epoch;v.generation_=v.data_->generation();v.coin_generation_=v.coins_->generation();v.session_=s.session;v.random_=s.random;std::set<uint64_t>tickets;
 for(const auto&binding:v.data_->bindings()){
  const auto handle=handles.find(binding.id);if(handle==handles.end()||!handle->second||!tickets.insert(handle->second).second||!s.factories.count(handle->second)||!bs.factories.count(binding.path))return fail(e,"Coin scene concrete prepared handle missing/aliased");
  for(const auto*name:{&binding.sink,&binding.reparent}){const auto p=s.parents.find(*name);if(p==s.parents.end()||!identity(p->second,epoch))return fail(e,"Coin scene prepared parent transform");v.parents_[*name]=p->second.token;}
  CoinFactory factory;if(!factory.prepare(v.coins_,epoch,handle->second,binding.position,s,e))return false;v.factories_.emplace(binding.owner,std::move(factory));v.delivered_.emplace(binding.owner,Delivery{});v.tickets_.emplace(binding.owner,handle->second);
 }
 v.valid_=true;if(!v.context(epoch,b,bs,s,e))return false;*this=std::move(v);e.clear();return true;
}
bool CoinSceneFactories::consume(uint64_t epoch,const BreakablesData&b,BreakableServices&bs,CoinServices&s,std::string&e){
 if(!context(epoch,b,bs,s,e))return false;
 std::vector<BreakableRequest>remaining;remaining.reserve(bs.requests.size());auto delivered=delivered_;std::map<uint32_t,uint32_t>counts;
 for(const auto&request:bs.requests){
  const auto kind=uint32_t(request.kind);if(kind<1||kind>6)return fail(e,"Coin scene unknown Breakable request kind");if(request.kind!=BreakableRequestKind::Coins){remaining.push_back(request);continue;}
  const auto*binding=data_->owner(request.owner);const auto*owner=b.owner(request.owner);auto last=delivered.find(request.owner);const auto factory=factories_.find(request.owner);
  if(!binding||!owner||factory==factories_.end()||last==delivered.end()||request.epoch!=epoch||request.frame>s.frame||request.identity||!owner->coins||request.value[0]!=double(owner->coins)||request.value[1]!=0||request.value[2]!=0||request.value[3]!=0||(last->second.delivered&&request.frame<=last->second.frame))return fail(e,"Coin scene unmatched/repeated burst request");
  const auto&flags=owner->object_flag?bs.object_flags:bs.story_flags;const auto flag=flags.find(owner->flag);if(flag==flags.end()||!flag->second)return fail(e,"Coin scene original burst flag was not committed");
  // Preflight on a copy changes no children, services, timers or random stream.
  auto probe=factory->second;if(!probe.spawn(epoch,owner->coins,s,e))return false;counts[owner->id]=owner->coins;last->second={true,request.frame};
 }
 // No allocation, callbacks or fallible validation after commit begins. Keep
 // each actual factory and child address stable; spawn changes only these fields.
 for(const auto&count:counts){auto&factory=factories_.find(count.first)->second;factory.remaining_=count.second;factory.spawning_=true;}
 delivered_=std::move(delivered);bs.requests.swap(remaining);e.clear();return true;
}
bool CoinSceneFactories::leave(uint64_t epoch,const BreakablesData&b,const BreakableServices&bs,CoinServices&s,std::string&e){
 if(!context(epoch,b,bs,s,e))return false;
 for(const auto&request:bs.requests)if(request.kind==BreakableRequestKind::Coins&&factories_.count(request.owner))return fail(e,"Coin scene undelivered burst request");
 for(const auto&request:s.requests)for(const auto&factory:factories_)if(factory.second.instances_.count(request.instance))return fail(e,"Coin scene undelivered child command");
 for(auto&factory:factories_){for(const auto&child:factory.second.instances_)s.owners.erase(child.first);s.factories.erase(factory.second.instance_);factory.second.instances_.clear();factory.second.valid_=false;}
 factories_.clear();valid_=false;e.clear();return true;
}
}
