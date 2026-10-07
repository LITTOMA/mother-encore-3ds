#pragma once
#include "house_ui_continuation.hpp"
#include "encore/house_runtime.hpp"
#include "podunk_mick_session.hpp"
#include "podunk_dialogue_programme_port.hpp"

namespace encore::ctr {
class HouseReturnNpcRuntime;
class HouseReturnDialogueNativeOwner;
class PodunkPlayerHost;
// Borrow the already committed House and the SAME native DialogueBox owners.
// RoomView is the original checked programme source; no FieldProgrammeData or
// FieldNpcRuntime is manufactured to impersonate a House programme/talker.
struct HouseReturnDialogueContext {
  upstream::FreshHouseState *house = nullptr;
  upstream::RoomView room{};
  upstream::HouseView text{};
  upstream::FieldObjectId house_root = 0, canvas = 0, dialogue = 0, talker = 0;
  uint32_t programme = upstream::kRoomNoIndex;
  uint32_t original_npc = upstream::kRoomNoIndex;
  uint32_t generation = 0;
};
// The concrete native owner must inspect its real script/factory/wait maps and
// callback receivers. This is a receipt, never a substitute for implementing
// source callbacks. Outstanding legitimate Ready/animation waits are retained.
struct HouseReturnDialogueReceipt {
  const upstream::FieldGlobalRegistry *registry = nullptr;
  const upstream::FieldNodeTreeRuntime *lifecycle_tree = nullptr;
  const upstream::FreshHouseState *house = nullptr;
  const upstream::OpeningWorld *world = nullptr;
  const upstream::HouseRuntime *runtime = nullptr;
  const upstream::HousePresentation *printer = nullptr;
  const PodunkMickSession *session = nullptr;
  const PodunkDialogueRootScript *script = nullptr;
  const upstream::SourceRandom *random = nullptr;
  const std::vector<uint32_t> *uid_ledger = nullptr;
  const upstream::DialogueChoices *choices = nullptr;
  // ALL actual native factories/instances and waiter receivers, not just the
  // current root. The closed receipt must also inspect ownership history.
  std::vector<upstream::FieldObjectId> dialogue_objects, wait_receivers,
      message_targets;
  upstream::FieldObjectId dialogue = 0, canvas = 0, talker = 0;
  uint32_t programme = upstream::kRoomNoIndex, generation = 0;
  // Read OpeningWorld's actual private generation_ counter through its narrow
  // source getter, NOT story_generation() (the VM can still be unstarted).
  uint32_t world_generation = 0;
  // Exact target observed inside the existing options/input source callback;
  // do not synthesize this from a requested arbitrary PC.
  uint32_t selected_choice_pc = upstream::kRoomNoIndex;
  size_t callback_depth = 0, pending_callbacks = 0;
  int32_t cursor_index = -1;
  size_t pending_choice_events = 0;
  upstream::FieldDialogueLifecyclePhase lifecycle_phase =
      upstream::FieldDialogueLifecyclePhase::Error;
  bool inspected = false, source_closed = false, podunk_retired = false;
  bool choice_callback = false, choice_event_consumed = false;
  bool input_callback = false;
  bool ready_waiting = false;
};
class HouseReturnDialogueNativePort {
public:
  virtual ~HouseReturnDialogueNativePort() = default;
  virtual bool observe(const HouseReturnDialogueContext &,
                       HouseReturnDialogueReceipt &, std::string &) const = 0;
  virtual bool admit_npc_before_open(const HouseReturnNpcRuntime&,
      const upstream::HouseSourceNpcProgramme&,uint32_t,std::string&e)const{
    e="House native owner has no actual NPC BeforeOpen consumer";return false;
  }
  // Read-only admission of EVERY immutable Room command, including branches
  // and waits not passed to OpeningWorld::apply. Reject missing source owners.
  virtual bool admit_command(const HouseReturnDialogueContext &, uint32_t pc,
                            const upstream::RoomCommand &, std::string &) const = 0;
  // Exact uiManager.open_dialogue_box prefix: nullable talker, real factory,
  // source UI store/stack mutation/deferred add and real Ready waiter. No VM
  // start here. context.generation is the checked next World source generation
  // used by the existing Ready waiter. This must not resume synchronously.
  virtual bool open(const HouseReturnDialogueContext &,
                    upstream::FieldObjectId &, std::string &) = 0;
  // Called from the existing World start boundary after its actual generation
  // assignment, before DialoguePlayer::start. Rebind existing RootOwner's
  // programme port to this Room context and run actual start_from_id fields.
  virtual bool source_started(const HouseReturnDialogueContext &,
                              std::string &) = 0;
  // Source/native UI effects only. World keeps its existing flags, movement,
  // camera, inventory, timers and programme VM; do not execute those twice.
  virtual bool apply_native(const HouseReturnDialogueContext &, uint32_t pc,
                            const upstream::DialogueAction &, std::string &) = 0;
  // Same source Cursor/InputSound callback: consume actual choices events and
  // clear old text before returning the real Selected target. Retain its sound
  // receipt until choice_target_committed; unknown events/waiters reject.
  virtual bool select_choices(const HouseReturnDialogueContext &, int32_t index,
                              bool confirm, bool cancel, uint32_t &selected_pc,
                              std::string &) = 0;
  // Source post-target InputSound, using that retained real Selected event.
  virtual bool choice_target_committed(const HouseReturnDialogueContext &,
                                       std::string &) = 0;
};
struct HouseReturnDialogueInput {
  upstream::FieldGlobalRegistry *registry = nullptr;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree;
  upstream::FreshHouseState *house = nullptr;
  upstream::HouseView text{};
  const upstream::HouseReentryData *source = nullptr;
  const upstream::FieldNodeTreeData *source_tree = nullptr;
  const upstream::FieldDoorData *doors = nullptr;
  const upstream::FieldDialogueLifecycleData *lifecycle = nullptr;
  const upstream::FieldNodeRecipeData *recipe = nullptr;
  PodunkMickSession *session = nullptr;
  HouseUiContinuation *ui = nullptr;
  PodunkDialogueRootScript *script = nullptr;
  upstream::SourceRandom *random = nullptr;
  std::vector<uint32_t> *uid_ledger = nullptr;
  const upstream::DialogueChoicesData *choice_data = nullptr;
  upstream::DialogueChoices *choices = nullptr;
  HouseReturnDialogueNativePort *native = nullptr;
};
// Fixed-address driver: retained callbacks borrow this owner. There is no new
// scheduler, printer tick, timer list, synthetic NPC, cold Ready or LOAD path.
class HouseReturnDialogue final : public upstream::OpeningHouseProgrammeOwner,
                                  public upstream::HouseProgrammeOwner,
                                  public PodunkDialogueProgrammePort {
public:
  HouseReturnDialogue() = default;
  HouseReturnDialogue(const HouseReturnDialogue &) = delete;
  HouseReturnDialogue &operator=(const HouseReturnDialogue &) = delete;
  HouseReturnDialogue(HouseReturnDialogue &&) = delete;
  HouseReturnDialogue &operator=(HouseReturnDialogue &&) = delete;
  bool bind(HouseReturnDialogueInput, std::string &);
  // CurrentScene is already the real constructed House; the retired source
  // tree still owns the same detached, previously Ready Player. No request,
  // inventory frame or closed transfer frame is admitted in this phase.
  bool bind_staged(HouseReturnDialogueInput, HouseReturnDialogueNativeOwner&,
      std::shared_ptr<upstream::FieldNodeTreeRuntime> retired_tree,
      upstream::FieldObjectId retired_root, PodunkPlayerHost& retained_player,
      std::string &);
  bool finish_ready(std::string &);
  bool staged_npc_context(const HouseReturnNpcRuntime&,
      upstream::FieldObjectId, std::string&) const;
  bool staged_player(const HouseReturnNpcRuntime&, upstream::FieldObjectId npc,
      upstream::FieldObjectId player, const upstream::FieldNodeTreeRuntime&,
      std::string&) const;
  // Exact-owner unbind after actual native closure. Keep this fixed owner and
  // all borrowed endpoints alive if pending callbacks/waits reject unbinding.
  bool unbind(std::string &);
  const upstream::OpeningWorld *world() const override;
  bool source_frame_closed(const upstream::OpeningWorld &,
                           std::string &) const override;
  bool programme_binding(PodunkDialogueProgrammeBinding &, std::string &) const override;
  bool dialogue_context(PodunkDialogueProgrammeContext &, std::string &) const override;
  bool dialogue_status(upstream::DialogueStatus &, std::string &) const override;
  bool ready_waiting(bool &, std::string &) const override;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const override;
  bool text_completed(std::string &) override;
  bool source_cursor_input(uint64_t dialogue, uint32_t generation, int32_t index,
                           bool confirm, bool cancel, std::string &) override;
  bool advance(bool confirm, bool cancel, std::string &) override;
  // Replace only the actual HouseRuntime programme-start call sites. Keep its
  // real interaction/seen/phase mutations and mark the Ready wait explicitly.
  bool request(uint32_t programme, uint32_t original_npc, std::string &) override;
  // Scoped only to the concrete NPC's already executed original source prefix.
  // Core repeated snapshots do not turn this into a closed transfer frame.
  bool request_npc_programme(const HouseReturnNpcRuntime&,
                            const upstream::HouseSourceNpcProgramme&,std::string&);
  bool observe_house_programme(upstream::HouseProgrammeState &,
                               std::string &) const override;
  bool native_ready(upstream::FieldObjectId, std::string &);
  // World hooks; do not invoke from a trace replay or copied command cursor.
  bool source_started(upstream::OpeningWorld &, uint32_t generation,
                      std::string &) override;
  bool before_action(upstream::OpeningWorld &, const upstream::DialogueAction &,
                     std::string &) override;
  // Original source input/options callbacks resume the same existing VM.
  bool advance(upstream::FieldObjectId, uint32_t generation, bool automatic,
               std::string &);
  bool choose(upstream::FieldObjectId, uint32_t generation, uint32_t pc,
              std::string &);
  bool submenu_closed(upstream::FieldObjectId, uint32_t generation,
                      std::string &);
  // Resolve original ShowDialogue stable ID against the same House resource;
  // native RootOwner presents these segments through its existing printer.
  bool source_text(uint32_t dialogue_id, upstream::HouseDialogue &,
                   std::string &) const;
  // At a CLOSED actual source callback boundary after native Done/animation,
  // observe removal and all pending ownership. Never clears those callbacks.
  bool native_closed(upstream::FieldObjectId, uint32_t generation,
                     std::string &);
  bool waiting_ready() const { return phase_ == Phase::WaitingReady; }
  bool staged() const { return phase_ == Phase::AssignedStaged; }
  bool owns_programme() const { return phase_ != Phase::Unbound && phase_ != Phase::AssignedStaged && phase_ != Phase::Closed; }
  bool failed() const { return phase_ == Phase::Failed; }
  bool native_open_live()const{return phase_==Phase::Opening&&!world_call_&&!action_call_;}
  bool native_start_live()const{return phase_==Phase::Starting&&world_call_&&!action_call_;}
  bool native_action_live()const{return phase_==Phase::Running&&action_call_;}
  const HouseReturnDialogueContext &context() const { return context_; }
  // Read actual owners during inventory opcodes, including a live native box.
  // callback_depth is observed, not required to be zero by this getter.
  bool inventory_frame(HouseReturnDialogueReceipt &, std::string &) const;
private:
  struct NpcRequestScope;
  enum class Phase { Unbound, AssignedStaged, Closed, Opening, WaitingReady, Starting, Running, Failed };
  HouseReturnDialogueInput input_{};
  HouseReturnDialogueContext context_{};
  Phase phase_ = Phase::Unbound;
  bool world_call_ = false, action_call_ = false;
  const NpcRequestScope*npc_request_=nullptr;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> retired_tree_;
  upstream::FieldObjectId retired_root_=0, retained_player_=0;
  uint32_t staged_generation_=0;
  PodunkPlayerHost *retained_player_host_=nullptr;
  bool staged_binding_call_=false;
  HouseReturnDialogueNativeOwner *staged_native_=nullptr;
  bool actual(bool require_ready, std::string &) const;
  bool receipt(bool closed, HouseReturnDialogueReceipt &, std::string &,
               const NpcRequestScope*request_scope=nullptr) const;
  bool checked_npc_request(const NpcRequestScope&,std::string&)const;
  bool current(upstream::FieldObjectId, uint32_t, std::string &) const;
  bool fail(std::string &, const char *);
  bool world_result(bool, std::string &);
};
} // namespace encore::ctr
