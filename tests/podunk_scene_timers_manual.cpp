#include "platform/ctr/podunk_scene_timers.hpp"
// Manual source cases; never registered or automatically executed.
int podunk_scene_timers_manual_cases() {
  encore::ctr::PodunkSceneTimers timers;
  std::string e;
  encore::upstream::FieldNodeBinding b;
  encore::upstream::FieldDeferredMessage m;
  if (timers.bind(1, b, e))
    return 1;
  if (timers.start(1, 1.f, e))
    return 2;
  if (timers.stop(1, e))
    return 3;
  if (timers.deferred(m, e))
    return 4;
  if (timers.finish_factory(e))
    return 5;
  if (timers.phase(1, encore::upstream::FieldTreePhase::ReadyNative, 0, false,
                   e))
    return 6;
  return timers.state(1) || timers.owns(1) ? 7 : 0;
}
