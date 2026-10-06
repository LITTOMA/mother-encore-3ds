#include "podunk_dialogue_options_adapter.hpp"

namespace encore::ctr {
using namespace encore::upstream;
namespace {
bool reject(std::string &error, const char *message) {
  error = message;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
} // namespace
bool PodunkDialogueOptionsAdapter::initialize(
    const PodunkDialogueRootData &data, const FieldNodeRecipeData &recipe,
    PodunkDialogueHost &dialogue, PodunkDialogueRootOwner &owner,
    PodunkProgrammeHost &programme, DialogueChoices &choices, CurrentTree tree,
    std::string &error) {
  const auto *options = recipe.record(data.options_source());
  if (data_ || !data.valid() || !recipe.valid() || !tree ||
      !same(data.identity(), recipe.identity()) ||
      data.recipe_sha() != recipe.ir_sha256() || !options ||
      options->native_class != "GridContainer")
    return reject(error, "Dialogue option bridge checked source owners differ");
  data_ = &data;
  recipe_ = &recipe;
  dialogue_ = &dialogue;
  owner_ = &owner;
  programme_ = &programme;
  choices_ = &choices;
  tree_ = std::move(tree);
  error.clear();
  return true;
}
FieldDialogueUiRuntime *PodunkDialogueOptionsAdapter::checked(
    FieldObjectId root, std::string &error) {
  const auto fail = [&](const char *message) -> FieldDialogueUiRuntime * {
    error = message;
    return nullptr;
  };
  if (!data_ || !root || !programme_->active() || !choices_->active())
    return fail("Dialogue option bridge has no active source choice owner");
  const auto &programme = programme_->programme();
  if (programme.context().dialogue_object != root ||
      programme.scheduler().status() != DialogueStatus::AwaitChoices ||
      !programme.scheduler().generation() ||
      !dialogue_->admit_ready(root, programme.scheduler().generation(), error))
    return fail("Dialogue option bridge actual programme/Ready lease differs");
  auto *tree = tree_(root);
  auto *ui = dialogue_->ui(root);
  const auto *source = tree ? tree->descriptor(root) : nullptr;
  const auto *state = tree ? tree->state(root) : nullptr;
  PodunkDialogueScriptState script;
  if (!tree || !ui || !ui->data() || !source || !state || !state->alive ||
      !state->inside || !state->ready_notified ||
      source->script_sha != data_->script_sha() ||
      !same(ui->data()->identity(), recipe_->identity()) ||
      ui->data()->recipe_ir_sha() != data_->recipe_sha() ||
      !owner_->state(root, script, error) || script.object != root ||
      !script.constructed || !script.entered || !script.ready ||
      !same(script.identity, recipe_->identity()))
    return fail("Dialogue option bridge actual script/Tree/UI owner differs");
  const auto *options = ui->data()->role(FieldDialogueUiRole::Options);
  FieldObjectId grid = 0;
  const auto *recipe_options = recipe_->record(data_->options_source());
  if (!options || !recipe_options || options->id != data_->options_source() ||
      !tree->get_node(root, recipe_options->path, grid, error))
    return fail("Dialogue option bridge actual source Grid absent");
  const auto *control = ui->control(grid);
  const auto *grid_state = tree->state(grid);
  if (!control || !control->entered || !control->ready ||
      control->source != data_->options_source() || !grid_state ||
      grid_state->source != data_->options_source() ||
      !grid_state->ready_notified)
    return fail("Dialogue option bridge actual Grid native owner differs");
  error.clear();
  return ui;
}
bool PodunkDialogueOptionsAdapter::prepare(
    FieldObjectId root, const DialogueChoices &choices,
    const LocaleSelection *locale, std::string &error) {
  if (&choices != choices_)
    return reject(error, "Dialogue option bridge received another choice owner");
  auto *ui = checked(root, error);
  return ui && ui->prepare_choice_labels(root, choices, locale, error);
}
bool PodunkDialogueOptionsAdapter::hide(FieldObjectId root,
                                        std::string &error) {
  auto *ui = checked(root, error);
  return ui && ui->hide_choice_labels(root, error);
}
} // namespace encore::ctr
