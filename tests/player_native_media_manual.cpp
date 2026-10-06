#include "../platform/ctr/podunk_player_native_media.hpp"
#include <cassert>
// Explicit manual negative boundary cases. Never registered with automatic CI.
// Run only through a deliberately created device/manual driver.
void player_native_media_missing_actual_owners() {
  encore::ctr::PodunkPlayerNativeMedia media;
  std::string e;
  encore::ctr::PodunkPlayerAnimatedState sprite;
  encore::ctr::PodunkPlayerAudioState audio;
  assert(!media.animated_state(1, sprite, e));
  assert(!media.audio_state(1, audio, e));
  assert(!media.set_frame(1, 0, e));
  assert(!media.audio_stream(1, 0, e));
  assert(!media.phase(1, encore::upstream::FieldTreePhase::ReadyNative, 0,
                      false, true, e));
}
