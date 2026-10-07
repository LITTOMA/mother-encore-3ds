#pragma once
#include "house_ui_continuation.hpp"
#include "podunk_dialogue_root_script.hpp"
#include "podunk_player_host.hpp"

namespace encore::ctr {
struct PodunkDialogueCoroutineState {
  std::vector<upstream::FieldObjectId> receivers,history;
  size_t callback_depth=0;
  upstream::FieldObjectId callback_receiver=0;
};
// Actual source business owners outside DialogueBox's 47-node subtree. The
// adapter never infers a closed Key/Cash/BlackBars object from absent widgets.
class PodunkDialogueBusiness {
public:
  virtual ~PodunkDialogueBusiness() = default;
  virtual const upstream::FieldGlobalRegistry *registry() const = 0;
  virtual bool observe(upstream::FieldObjectId,
                       upstream::FieldDialogueObservation &, std::string &) const = 0;
  virtual bool admit(const upstream::FieldDialogueStep &,
                     const upstream::FieldProgrammeContext &, std::string &) const = 0;
  virtual bool manager(const upstream::FieldDialogueStep &,
                       upstream::FieldObjectId, std::string &) = 0;
  virtual bool global(const upstream::FieldDialogueStep &,
                      upstream::FieldObjectId, upstream::FieldObjectId,
                      std::string &) = 0;
  virtual bool close_sound(std::string &) = 0;
  virtual bool restore_telepathy(std::string &) = 0;
  virtual bool return_camera(bool, double, std::string &) = 0;
};
struct PodunkDialogueLifecycleInput {
  const upstream::HouseUiContinuationData *ui_data = nullptr;
  const upstream::FieldDialogueLifecycleData *life = nullptr;
  const upstream::FieldNodeRecipeData *recipe = nullptr;
  const upstream::FieldProgrammeData *programmes = nullptr;
  const upstream::PlayerMotionData *motion = nullptr;
  upstream::FieldGlobalRegistry *registry = nullptr;
  upstream::FieldObjectSignals *signals = nullptr;
  HouseUiContinuation *ui = nullptr;
  PodunkPlayerHost *player = nullptr;
  PodunkDialogueHost *dialogue = nullptr;
  PodunkDialogueRootOwner *root_script = nullptr;
  PodunkProgrammeHost *programme = nullptr;
  PodunkDialogueBusiness *business = nullptr;
};
// Concrete limited coroutine owner: source open_dialogue_box ready/done and
// _close_dialog_box animation_finished. No new frame/RNG or script interpreter.
class PodunkDialogueLifecyclePorts {
public:
  PodunkDialogueLifecyclePorts() = default;
  ~PodunkDialogueLifecyclePorts();
  PodunkDialogueLifecyclePorts(const PodunkDialogueLifecyclePorts &) = delete;
  PodunkDialogueLifecyclePorts &operator=(const PodunkDialogueLifecyclePorts &) = delete;
  bool prepare(PodunkDialogueLifecycleInput, std::string &);
  upstream::FieldDialogueLifecycleHost host();
  bool observe(upstream::FieldObjectId, upstream::FieldDialogueObservation &,
               std::string &) const;
  bool disconnect(upstream::FieldObjectId, uint32_t, std::string &);
  bool shutdown(std::string &);
  bool source_frame_closed(std::string &)const;
  bool source_coroutines(PodunkDialogueCoroutineState &,std::string &)const;
private:
  class Wait;
  friend class Wait;
  enum class Kind { Ready, Done, Animation };
  bool player(bool &, std::string &) const;
  bool wait(upstream::FieldObjectId, uint32_t, std::string_view, Kind,
            std::function<bool()>, std::function<bool(int64_t)>, std::string &);
  bool resume(upstream::FieldObjectId, const upstream::FieldDeferredMessage &,
              std::string &);
  bool checked_step(const upstream::FieldDialogueStep &, std::string &) const;
  PodunkDialogueLifecycleInput in_{};
  std::map<upstream::FieldObjectId, std::shared_ptr<Wait>> waiting_;
  std::set<upstream::FieldObjectId> wait_history_;
  uint32_t callback_depth_=0;
  upstream::FieldObjectId callback_receiver_=0;
};
} // namespace encore::ctr
