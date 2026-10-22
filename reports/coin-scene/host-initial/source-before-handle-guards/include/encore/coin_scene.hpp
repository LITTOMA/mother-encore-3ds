#pragma once
#include "encore/coins.hpp"
#include "encore/breakables.hpp"

namespace encore::upstream {
// Stable source identities and initial source-tree placement. Runtime node
// handles and later reparent/process order belong to the prepared scene owner.
struct CoinFactoryBinding {
 uint32_t id=0,owner=0,order=0,owner_order=0,default_count=0;
 std::string path,parent,source,owner_path,sink,reparent;
 Vec2 local{},position{};
};
class CoinSceneData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}uint64_t generation()const{return generation_;}
 uint32_t scene()const{return scene_;}
 const std::string&source()const{return source_;}const std::string&sink()const{return sink_;}
 const std::vector<CoinFactoryBinding>&bindings()const{return bindings_;}
 const CoinFactoryBinding*owner(uint32_t)const;
 bool compatible(const BreakablesData&,const CoinsData&,std::string&)const;
private:
 bool valid_=false;uint64_t generation_=0;uint32_t scene_=0;
 std::string source_,sink_;std::vector<CoinFactoryBinding>bindings_;
};
// Owns concrete prepared factories and delivers only the checked Coin request
// family. Other known Breakable requests stay in their original queue order.
// The full scene still dispatches physical/idle callbacks in actual tree order.
class CoinSceneFactories {
public:
 bool prepare(std::shared_ptr<const CoinSceneData>,std::shared_ptr<const CoinsData>,const BreakablesData&,uint64_t,const std::map<uint32_t,uint64_t>&handles,const BreakableServices&,CoinServices&,std::string&);
 bool consume(uint64_t,const BreakablesData&,BreakableServices&,CoinServices&,std::string&);
 bool leave(uint64_t,const BreakablesData&,const BreakableServices&,CoinServices&,std::string&);
 bool valid()const{return valid_;}
 CoinFactory*factory(uint32_t owner){auto v=factories_.find(owner);return v==factories_.end()?nullptr:&v->second;}
 const std::map<uint32_t,CoinFactory>&factories()const{return factories_;}
private:
 bool context(uint64_t,const BreakablesData&,const BreakableServices&,const CoinServices&,std::string&)const;
 std::shared_ptr<const CoinSceneData>data_;std::shared_ptr<const CoinsData>coins_;
 SessionSnapshot*session_=nullptr;SourceRandom*random_=nullptr;
 uint64_t epoch_=0,generation_=0,coin_generation_=0;bool valid_=false;
 std::map<uint32_t,CoinFactory>factories_;std::map<std::string,uint64_t>parents_;
 struct Delivery {bool delivered=false;uint64_t frame=0;};
 std::map<uint32_t,Delivery>delivered_;
};
}
