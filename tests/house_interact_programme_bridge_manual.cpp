// Explicit manual harness only. Call from an actual Interact source invocation
// with its live receipt; no synthetic Enter/Ready or fixture game is installed.
#include "../platform/ctr/house_return_interact_programme.hpp"
#include <cassert>
using namespace encore::ctr;
using namespace encore::upstream;
void house_interact_programme_bridge_manual(HouseReturnInteractProgrammeBridge&owner,
    const HouseReturnInteractProgramme&actual){
 std::string error;
 assert(owner.admit(actual,error));
 const auto before=actual.house->world.source_generation();
 auto bad=actual;bad.source=nullptr;assert(!owner.admit(bad,error));
 bad=actual;bad.runtime=nullptr;assert(!owner.admit(bad,error));
 bad=actual;bad.object=0;assert(!owner.admit(bad,error));
 bad=actual;bad.thoughts=true;assert(!owner.admit(bad,error));
 bad=actual;bad.programme=kRoomNoIndex;assert(!owner.admit(bad,error));
 bad=actual;bad.dialogue="__missing_source_dialogue__";assert(!owner.admit(bad,error));
 bad=actual;bad.registry=nullptr;assert(!owner.admit(bad,error));
 HouseSourceInteractProgramme absent,observed;
 assert(!owner.observe(absent,observed,error));
 assert(actual.house->world.source_generation()==before);
 assert(actual.house->house.phase()==HousePhase::Idle);
}
