#pragma once
#include "podunk_dialogue_root_script.hpp"

namespace encore::ctr {
// Borrowed bridge for the concrete source _add_dialog_options and selected-
// option Label hide. It owns no input, printing, clock, Cursor, RNG or objects.
class PodunkDialogueOptionsAdapter {
public:
  using CurrentTree = std::function<upstream::FieldNodeTreeRuntime *(
      upstream::FieldObjectId)>;
  bool initialize(const PodunkDialogueRootData &,
                  const upstream::FieldNodeRecipeData &, PodunkDialogueHost &,
                  PodunkDialogueRootOwner &, PodunkDialogueProgrammePort &,
                  upstream::DialogueChoices &, CurrentTree, std::string &);
  // Same closed VM/printer/choices proof as the Root owner; transfer happens
  // before the destination creates any real DialogueBox, never during input.
  bool rebind_programme(const PodunkDialogueProgrammePort &,
                        const PodunkDialogueProgrammeBinding &,
                        PodunkDialogueProgrammePort &, std::string &);
  // Bind these methods to the two corresponding RootEndpoints. The root owner
  // retains the actual Arrow/on/index/Grid/down-arrow source ordering.
  bool prepare(upstream::FieldObjectId, const upstream::DialogueChoices &,
               const upstream::LocaleSelection *, std::string &);
  bool hide(upstream::FieldObjectId, std::string &);

private:
  const PodunkDialogueRootData *data_ = nullptr;
  const upstream::FieldNodeRecipeData *recipe_ = nullptr;
  PodunkDialogueHost *dialogue_ = nullptr;
  PodunkDialogueRootOwner *owner_ = nullptr;
  PodunkDialogueProgrammePort *programme_ = nullptr;
  upstream::DialogueChoices *choices_ = nullptr;
  CurrentTree tree_;
  upstream::FieldDialogueUiRuntime *checked(upstream::FieldObjectId,
                                          std::string &);
};
} // namespace encore::ctr
