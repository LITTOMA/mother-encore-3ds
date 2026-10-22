#include "coin_reference_support.hpp"
#include "coin_scene_reference_support.hpp"
#include <iomanip>
using namespace coin_reference;
namespace {
struct ExpectedBirth {uint32_t factory=0;uint64_t physics=0,idle=0;Vec2 position{},direction{};uint64_t speed=0,air=0;uint32_t sparkle=0;};
struct Live {uint64_t id=0;CoinActor*actor=nullptr;};
}
int main(int argc,char**argv){
 CHECK(argc==5);auto data=std::make_shared<CoinSceneData>();auto coin_data=std::make_shared<CoinsData>();BreakablesData break_data;CHECK(data->load_file(argv[1],error)&&coin_data->load_file(argv[2],error)&&break_data.load_file(argv[3],error));std::ifstream in(argv[4]);CHECK(read<std::string>(in)=="ENCCOIN_SCENE02");const auto cases=read<unsigned>(in),binding_count=read<unsigned>(in);CHECK(cases==3&&binding_count==data->bindings().size());
 for(unsigned ci=0;ci<cases;++ci){
  const auto mode=read<std::string>(in);CHECK(mode=="forward"||mode=="reverse"||mode=="flagged");const auto seed=read<uint64_t>(in);const auto rows=read<unsigned>(in),birth_count=read<unsigned>(in);const auto tail=read<uint32_t>(in);const auto cash=read<int64_t>(in);const auto hud=read<unsigned>(in),collect=read<unsigned>(in);CHECK(rows>0&&rows<=10000&&birth_count<=4096&&hud==0&&collect==0);std::vector<ExpectedBirth>births;
  for(unsigned n=0;n<birth_count;++n){CHECK(read<unsigned>(in)==n);ExpectedBirth b;b.factory=read<uint32_t>(in);b.physics=read<uint64_t>(in);b.idle=read<uint64_t>(in);const auto x=read<uint32_t>(in),y=read<uint32_t>(in);b.position={real(x),real(y)};b.speed=read<uint64_t>(in);const auto dx=read<uint32_t>(in),dy=read<uint32_t>(in);b.direction={real(dx),real(dy)};b.air=read<uint64_t>(in);b.sparkle=read<uint32_t>(in);births.push_back(b);}
  coin_scene_reference::World w(*data,*coin_data,break_data,seed);const auto*storage=w.session.storage.data();if(mode=="flagged"){const auto*first=break_data.owner(data->bindings().front().owner);CHECK(first&&first->object_flag);w.breaks.object_flags[first->flag]=true;}
  CoinSceneFactories scene;CHECK(scene.prepare(data,coin_data,break_data,71,w.handles,w.breaks,w.coins,error));std::map<uint32_t,BreakableActor>objects;
  for(const auto&binding:data->bindings()){BreakableActor actor;CHECK(actor.prepare(break_data,binding.owner,71,w.breaks,error));objects.emplace(binding.owner,std::move(actor));}
  CircleMotionSolver solver;CHECK(solver.configure(coin_data->policy().body_radius,{},coin_data->policy().margin,{}));std::vector<Live>live;
  for(unsigned row=0;row<rows;++row){
   const auto frame=read<int>(in);const auto physics=read<uint64_t>(in),idle=read<uint64_t>(in);const auto expected_born=read<unsigned>(in),has_live=read<unsigned>(in);label=mode+':'+std::to_string(frame);
   if(frame==0){auto bindings=data->bindings();if(mode=="reverse")std::reverse(bindings.begin(),bindings.end());for(const auto&binding:bindings)CHECK(objects.at(binding.owner).hit(71,break_data.owner(binding.owner)->position,w.breaks,error));}
   if(frame>0){
    // The signal observation precedes the next internal idle step. Existing
    // Coin children advance once, then the original callbacks/physics run.
    w.coins.frame=uint64_t(frame-1);for(auto&v:live){CHECK(v.actor->advance_idle(71,coin_data->policy().delta,w.coins,error));CHECK(v.actor->flush_deferred(71,w.coins,error));}
    for(const auto&binding:data->bindings())CHECK(objects.at(binding.owner).advance_idle(71,coin_data->policy().delta,w.breaks,error));w.coins.frame=uint64_t(frame);w.coins.physics_step=uint64_t(frame);CHECK(scene.consume(71,break_data,w.breaks,w.coins,error));const auto existing=live.size();
    for(const auto&binding:data->bindings()){
     auto*factory=scene.factory(binding.owner);CHECK(factory);const auto next=w.coins.next_owner;CHECK(factory->advance_physics(71,w.coins,error));
     if(w.coins.next_owner!=next){CHECK(w.coins.next_owner==next+1&&live.size()<births.size());auto*actor=factory->actor(next);CHECK(actor);const auto&expected=births[live.size()];const auto&s=actor->snapshot();CHECK(expected.factory==binding.id&&expected.physics==w.coins.physics_step&&expected.idle==w.coins.frame);CHECK(s.position.x==expected.position.x&&s.position.y==expected.position.y&&s.direction.x==expected.direction.x&&s.direction.y==expected.direction.y);CHECK(bits(s.speed)==expected.speed&&bits(s.air_time)==expected.air&&s.sparkle==expected.sparkle);live.push_back({next,actor});}
    }
    for(size_t n=0;n<existing;++n){CHECK(live[n].actor->advance_physics(71,solver,w.coins,error));CHECK(live[n].actor->flush_deferred(71,w.coins,error));}
   }
   CHECK(physics==uint64_t(std::max(frame,0))&&idle==physics&&live.size()==expected_born);
   for(const auto&binding:data->bindings()){
    CHECK(read<uint32_t>(in)==binding.id);const auto timer=read<uint64_t>(in);const auto remaining=read<uint32_t>(in),spawning=read<unsigned>(in);const auto x=read<uint32_t>(in),y=read<uint32_t>(in);std::string parent;in>>std::quoted(parent);CHECK(bool(in));const auto flag=read<unsigned>(in),disabled=read<unsigned>(in);const auto*f=scene.factory(binding.owner);const auto&object=objects.at(binding.owner);CHECK(f&&bits(f->timer())==timer&&f->remaining()==remaining&&f->spawning()==bool(spawning));CHECK(f->position().x==real(x)&&f->position().y==real(y)&&object.parent()==parent&&object.body_disabled()==bool(disabled));const auto*owner=break_data.owner(binding.owner);const auto&flags=owner->object_flag?w.breaks.object_flags:w.breaks.story_flags;const auto at=flags.find(owner->flag);CHECK(bool(at!=flags.end()&&at->second)==bool(flag));
   }
   if(has_live){CHECK(read<unsigned>(in)==live.size());for(size_t n=0;n<live.size();++n){label=mode+':'+std::to_string(frame)+":coin"+std::to_string(n);CHECK(read<unsigned>(in)==n);state(in,*live[n].actor,0);const auto drops=read<unsigned>(in);std::vector<uint64_t>actual;for(const auto&r:w.coins.requests)if(r.instance==live[n].id&&r.kind==CoinRequestKind::DropAudio)actual.push_back(r.frame);CHECK(actual.size()==drops);for(const auto clock:actual)CHECK(clock==read<uint64_t>(in));}}
   CHECK(w.session.cash==cash&&w.session.storage.data()==storage);
  }
  CHECK(live.size()==births.size()&&w.random.randi()==tail);for(const auto&r:w.coins.requests)CHECK(r.kind==CoinRequestKind::DropAudio);w.coins.requests.clear();CHECK(scene.leave(71,break_data,w.breaks,w.coins,error));CHECK(w.coins.owners.empty());
 }
 in>>std::ws;CHECK(in.peek()==std::char_traits<char>::eof());std::cout<<checks<<" original49-factory composition source checks passed\n";return 0;
}
