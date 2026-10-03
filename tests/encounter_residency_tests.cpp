#include "encore/encounter_residency.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace encore;std::string error;EncounterResidency cache;
 SceneEncounterDependencies scene{9,{{"single","data/a.encbattle","data/a.encround"},{"second-dependency","data/ab.encbattle","data/ab.encround"}}};
 assert(cache.declare(scene,{1000,1000},{100,100},error));assert(!cache.scene_ready());
 assert(cache.reserve(0,{300,300},error));assert(!cache.ready({9,0}));assert(!cache.publish({8,0},{250,250},error));assert(cache.publish({9,0},{250,250},error));
 assert(cache.ready({9,0})&&!cache.scene_ready());assert(!cache.reserve(1,{651,651},error));assert(cache.reserve(1,{650,650},error));assert(cache.publish({9,1},{500,500},error));assert(cache.scene_ready());
 assert(cache.ready({9,0})&&cache.ready({9,1}));cache.retire();assert(!cache.ready({9,0})&&!cache.scene_ready());
 scene.scene_epoch=10;scene.encounters[1].battle_pack.clear();assert(!cache.declare(scene,{1000,1000},{},error));scene.encounters[1].battle_pack="../bad.encbattle";assert(!cache.declare(scene,{1000,1000},{},error));
 scene.encounters[1].battle_pack="data/a.encbattle";assert(!cache.declare(scene,{1000,1000},{},error));scene.encounters[1].battle_pack="data/ab.encbattle";assert(cache.declare(scene,{1000,1000},{},error));assert(cache.reserve(0,{100,100},error));assert(!cache.publish({10,0},{101,100},error));assert(!cache.ready({10,0}));
 std::cout<<"PASS encounter working-set declarations, dependency unions, cold readiness, retained repeats, peak budgets, epoch invalidation, missing/duplicate packs, and stale publication\n";
}
