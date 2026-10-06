// Manual-only source cursor/target bridge cases. Not registered or executed.
#include "encore/dialogue_choices.hpp"
#include "podunk_programme_host.hpp"
#include <cassert>
#include <limits>
using namespace encore::upstream;
int main(int argc, char **argv) {
  assert(argc == 2);
  DialogueChoicesData data;
  std::string error;
  assert(data.load_file(argv[1], error));
  uint32_t group = 0;
  while (group < data.groups().size() &&
         data.groups()[group].options.size() < 2)
    ++group;
  assert(group < data.groups().size());
  const auto &source = data.groups()[group];
  DialogueChoices choices;
  assert(!choices.source_cursor_selection(0, error));
  assert(choices.prepare(data, group, source.program_identity,
                         source.program_command_count, error));
  assert(!choices.source_cursor_selection(1, error));
  assert(choices.phase() == DialogueChoicesPhase::WaitingText);
  assert(choices.text_completed(error));
  const auto previous = choices.pose();
  assert(choices.source_cursor_selection(1, error));
  const auto current = choices.pose();
  assert(current.visible && current.selected == 1);
  // Only the target index changed; the real Cursor owns its own native
  // animated pose. This bridge must never replay movement or its sound.
  assert(current.arrow_x == previous.arrow_x &&
         current.arrow_y == previous.arrow_y &&
         current.arrow_frame == previous.arrow_frame);
  DialogueChoicesEvent event;
  assert(!choices.poll_event(event));
  assert(!choices.source_cursor_selection(-1, error));
  assert(!choices.source_cursor_selection(std::numeric_limits<int32_t>::max(),
                                          error));
  assert(
      !choices.source_cursor_selection(int32_t(source.options.size()), error));
  assert(choices.pose().selected == 1 && !choices.poll_event(event));
  assert(choices.source_cursor_selection(1, error));
  assert(!choices.poll_event(event));
  assert(choices.step(0, {0, 0, true, false}, error));
  assert(choices.poll_event(event));
  assert(event.kind == DialogueChoicesEventKind::Selected && !event.cancelled &&
         event.target_pc == source.options[1].target_pc &&
         event.clear_dialogue && event.sound_after_target);
  assert(!choices.poll_event(event));
  assert(!choices.source_cursor_selection(0, error));
  assert(choices.pose().selected == 1);
  choices.close();
  assert(choices.prepare(data, group, source.program_identity,
                         source.program_command_count, error));
  assert(choices.text_completed(error));
  assert(choices.source_cursor_selection(1, error));
  assert(choices.step(0, {0, 0, true, true}, error));
  assert(choices.poll_event(event));
  assert(event.cancelled && event.target_pc == source.cancel_target_pc &&
         event.sound_after_target);
  assert(!choices.poll_event(event));
  // Legacy House/Phone consumers retain their existing directional path.
  choices.close();
  assert(choices.prepare(data, group, source.program_identity,
                         source.program_command_count, error));
  assert(choices.text_completed(error));
  assert(choices.step(0, {1, 0, false, false}, error));
  assert(choices.pose().selected == 1 && choices.poll_event(event));
  assert(event.kind == DialogueChoicesEventKind::SoundRequested &&
         event.sound == DialogueChoiceSound::Move);
  encore::ctr::PodunkProgrammeHost not_prepared;
  assert(!not_prepared.source_cursor_input(1, 1, 0, true, false, error));
}
