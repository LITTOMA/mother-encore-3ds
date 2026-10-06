// Manual-only missing/stale source ownership cases. No factory or fake Ready.
#include "podunk_dialogue_options_adapter.hpp"
#include <cstdlib>
#include <iostream>
using namespace encore::ctr;
using namespace encore::upstream;
namespace {
void check(bool value, const std::string &message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
} // namespace
int main() {
  std::string error;
  PodunkDialogueOptionsAdapter adapter;
  DialogueChoices choices;
  check(!adapter.prepare(1, choices, nullptr, error) && !error.empty(),
        "Another/uninitialized source choice owner accepted");
  check(!adapter.hide(1, error) && !error.empty(),
        "Missing actual dialogue/Options Tree owner accepted");
  PodunkDialogueRootData data;
  FieldNodeRecipeData recipe;
  PodunkDialogueHost dialogue;
  PodunkDialogueRootOwner owner;
  PodunkProgrammeHost programme;
  check(!adapter.initialize(data, recipe, dialogue, owner, programme, choices,
                            {}, error),
        "Missing recipe/source/registry callback admitted Options");
  FieldDialogueUiRuntime ui;
  check(!ui.prepare_choice_labels(1, choices, nullptr, error),
        "Unprepared native Label tree accepted choice mutation");
  check(!ui.hide_choice_labels(1, error),
        "Missing native Labels silently hidden");
  check(ui.option_labels(1).empty(), "Missing tree manufactured option text");
  return 0;
}
