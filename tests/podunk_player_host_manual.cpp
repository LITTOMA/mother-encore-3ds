#include "platform/ctr/podunk_player_host.hpp"
#include <type_traits>

// Explicitly manual. No harness, fixture source, automatic runner or GPU/DSP
// initialization is introduced by these negative owning-boundary cases.
static_assert(!std::is_copy_constructible_v<encore::ctr::PodunkPlayerHost>);
bool podunk_player_host_manual_unowned_boundaries() {
  encore::ctr::PodunkPlayerHost owner;
  std::string error;
  encore::upstream::FieldDeferredMessage message;
  encore::upstream::PlayerInputEvent event;
  if (owner.assembled() || owner.ready_complete() || owner.owns(1) ||
      owner.source_candidate(1))
    return false;
  if (owner.begin_frame(1, 0, 0, false, false, error) || error.empty())
    return false;
  if (owner.input(event, error) || error.empty())
    return false;
  if (owner.deferred(message, error) || error.empty())
    return false;
  return true;
}
