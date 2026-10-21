#pragma once
#include "encore/poison_hazards.hpp"
#include "encore/world_damage.hpp"
namespace encore::upstream {
// Selected Player contact delivery: combine the checked original poison/step
// callback with actual world-damage/status execution. All content and numerical
// policies come from the three independently loaded existing resources.
// Inputs are borrowed only for this call. This is not a physics sampler or
// geyser-instance registry. The caller must supply its validated body identity.
// Party damage is outside the selected Player profile and rejects explicitly;
// Other bodies retain the original no-op semantics. Unknown/stale/incompatible
// owners and nonempty output batches reject before any mutation.
bool deliver_poison_player_damage(const PoisonHazardsData&,PoisonContactKind,
 uint32_t area,bool entered,FieldStepBody,bool player_paused,uint64_t epoch,
 uint64_t instance,WorldDamageActor&,SessionCharacter&,FieldRunLoop&,
 FieldStepShadowState*,std::vector<WorldDamageEvent>&,std::string&);
}
