#pragma once
#include "encore/coin_scene.hpp"

namespace coin_scene_reference {
struct World {
 encore::upstream::SessionSnapshot session;
 encore::upstream::SourceRandom random;
 encore::upstream::CoinServices coins;
 encore::upstream::BreakableServices breaks;
 std::map<uint32_t,uint64_t>handles;
 World(const encore::upstream::CoinSceneData&d,const encore::upstream::CoinsData&c,const encore::upstream::BreakablesData&b,uint64_t seed):random(seed){
  using namespace encore::upstream;
  session.cash=43;session.storage.resize(3);coins.epoch=breaks.epoch=71;coins.session=&session;coins.random=&random;coins.hud_ready=true;coins.sounds={c.info().drop_audio,c.info().collect_audio};coins.next_owner=100;breaks.scene=d.scene();breaks.sounds.insert(b.policy().sound);uint64_t ticket=10000,parent=51;
  for(const auto&v:d.bindings()){handles[v.id]=ticket;coins.factories.insert(ticket++);breaks.factories.insert(v.path);for(const auto*name:{&v.sink,&v.reparent}){coins.parents.emplace(*name,CoinParent{71,parent++,{},{1,1},0});breaks.parents.insert(*name);}}
  for(const auto&o:b.owners()){breaks.parents.insert(o.parent);breaks.parents.insert(o.drop_parent);if(!o.new_parent.empty())breaks.parents.insert(o.new_parent);}
 }
 World(const World&)=delete;World&operator=(const World&)=delete;
};
}
