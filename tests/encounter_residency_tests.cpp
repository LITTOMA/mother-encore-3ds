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
 // Manual regression: cold round/actor atlases exceed the former warm-only
 // LINEAR grant. The corrected ceiling still rejects platform shortage and
 // reports the exhausted component without publishing a partial candidate.
 SceneEncounterDependencies cold{11,{{"cold","data/cold.encbattle","data/cold.encround"}}};
 assert(cache.declare(cold,{28*1024*1024,6*1024*1024},{},error));
 assert(!cache.reserve(0,{16542182,11273472},error));assert(error.find("LINEAR")!=std::string::npos);
 assert(cache.declare(cold,{28*1024*1024,12*1024*1024},{},error));
 assert(cache.reserve(0,{16542182,11273472},error));assert(cache.publish({11,0},{8000000,10000000},error));
 assert(cache.scene_ready());
 assert(cache.declare(cold,{28*1024*1024,10*1024*1024},{},error));
 assert(!cache.reserve(0,{16542182,11273472},error));assert(!cache.scene_ready());
 assert(cache.declare(cold,{1000,1000},{},error));assert(!cache.reserve(0,{1001,1},error));assert(error.find("CPU")!=std::string::npos);
 std::cout<<"PASS encounter working-set declarations, dependency unions, cold readiness, retained repeats, peak budgets, epoch invalidation, missing/duplicate packs, and stale publication\n";
}
