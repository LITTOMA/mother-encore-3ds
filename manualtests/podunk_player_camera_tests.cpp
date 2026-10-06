#include "../tests/manual_require.hpp"
#include "platform/ctr/podunk_player_camera.hpp"
// Only for the explicitly requested manual suite. The caller supplies the
// actual initialized production owner and a different live Registry object.
void podunk_player_camera_negative_manual(
    encore::ctr::PodunkPlayerCamera &owner,
    encore::upstream::FieldObjectId other) {
  using namespace encore::upstream;
  std::string error;
  MANUAL_REQUIRE(owner.object() && other && other != owner.object());
  MANUAL_REQUIRE(
      !owner.phase(other, FieldTreePhase::ReadyNative, false, error));
  MANUAL_REQUIRE(!error.empty());
  error.clear();
  MANUAL_REQUIRE(!owner.phase(owner.object(), FieldTreePhase::PhysicsInternal,
                              false, error));
  MANUAL_REQUIRE(!error.empty());
  error.clear();
  MANUAL_REQUIRE(!owner.begin_frame(0, false, error));
  MANUAL_REQUIRE(!error.empty());
  error.clear();
  MANUAL_REQUIRE(!owner.script_process(owner.object(), FieldTreePhase::Physics,
                                       0, false, error));
  MANUAL_REQUIRE(!error.empty());
}
