#pragma once
#include "encore/movement.hpp"
#include "encore/room_data.hpp"
#include <cstddef>
#include <cstdint>

namespace encore::upstream {
using DialogueActor = uint16_t;
enum class DialogueActionKind : uint8_t {
    BeginCutscene, BindActor, ActorPersistent, StartWait, MusicFadeOut,
    SetTalker, CallObjectDeferred, OverworldBattleMusic, PlaySound,
    MoveActor, TurnActor, ShakeActor, JumpActor, AnimateActor, EmoteActor,
    ShakeCamera, ChangeCamera, MoveCamera, QueueBattle,
    StopInteraction, RestoreActor, ReleaseBattleActor, CutsceneEnded, DialogueDone, RequestBattle,
    YieldIdle, AwaitTimer, SetActorDirection, TeleportActor, MoveActorPath, ReturnCamera, SetFlag, ShowDialogue, AwaitDialogue, PlayMusicImmediate, HideDialogue,
    // Program-relative branches and explicit menu suspension. AwaitChoices is
    // passed to the sink to prepare the menu; only a selected callback resumes.
    Jump, BranchFlag, BranchLeader, AwaitChoices, OpenSave, AwaitSubmenu, StopActorLoop, OpenStorage,
    // Rules8/cap9: stable identities live in separately checked source resources.
    GrantKeyItem, LearnSkill, AnimateSpecialActor
};
struct DialogueAction {
    DialogueActionKind kind=DialogueActionKind::BeginCutscene;
    DialogueActor actor=kRoomNoActor;
    uint32_t phrase=0;
    Vec2 vector{};
    double value=0, duration=0;
    uint32_t target_index=kRoomNoIndex,auxiliary_index=kRoomNoIndex,flags=0;
};
class DialogueSink {
public:
    virtual ~DialogueSink()=default;
    // False stops the program immediately; already accepted actions are not
    // rolled back. A sink must explicitly handle every action it accepts.
    virtual bool apply(const DialogueAction& action)=0;
    // BranchFlag compares a RoomFlag against action.value; BranchLeader checks
    // the identity in the Room string table. Return false for an unsupported or
    // failed query, and true with matched=false for an ordinary false condition.
    // Existing sinks reject new conditional programs by default.
    virtual bool branch_condition(const DialogueAction&,bool& matched) { matched=false;return false; }
};
enum class DialogueStatus : uint8_t { Idle, AwaitActor, AwaitIdle, AwaitTimer, AwaitDialogue, Running, BattleRequested, Cancelled, Error, Completed, AwaitChoices, AwaitSubmenu };

// Executes the checked instruction table selected by the scene pack. This is not
// M0's fixture VM or a general GDScript/YAML interpreter. Actor motion is
// nonblocking and owned by the world, never advanced inside this scheduler.
// Immutable checked source table, owned by the caller for the scheduler lifetime.
// Both RoomData and independent FieldProgrammeData retain source provenance;
// a mutable or unchecked command vector is not a valid implementation.
class DialogueProgrammeSource {
public:
    virtual ~DialogueProgrammeSource()=default;
    virtual bool valid()const=0;
    virtual uint32_t program_count()const=0;
    virtual RoomProgram program(uint32_t)const=0;
    virtual RoomCommand command(uint32_t)const=0;
    virtual uint32_t flag_count()const=0;
    virtual uint32_t string_count()const=0;
    virtual std::string_view string(uint32_t)const=0;
};
class DialoguePlayer {
public:
    bool start(const RoomView& content,uint32_t program_index,DialogueSink& sink,uint32_t generation=1);
    bool start(const DialogueProgrammeSource& content,uint32_t program_index,DialogueSink& sink,uint32_t generation=1);
    bool actor_ready(DialogueActor actor, uint32_t generation, DialogueSink& sink);
    // SceneTree emits idle_frame before internal idle processing. Call once
    // before actor idle callbacks, then idle_process at WaitTimer's phase.
    bool idle_begin(DialogueSink& sink);
    // delta is the native real_t idle delta promoted to double (1.0f/60 at 60Hz).
    bool idle_process(double delta, DialogueSink& sink);
    bool dialogue_finished(DialogueSink& sink,bool automatic=false);
    // Text completion belongs to the presenter, and does not resume a choice
    // phrase. The checked menu result supplies a program-relative target PC.
    // Invalid/stale callbacks leave the current suspension unchanged.
    bool choices_selected(uint32_t target_pc,uint32_t generation,DialogueSink& sink);
    bool submenu_closed(uint32_t generation,DialogueSink& sink);
    void cancel();
    DialogueStatus status() const { return status_; }
    uint32_t phrase() const { return phrase_; }
    uint32_t generation() const { return generation_; }
    uint32_t next_command_index()const{return uint32_t(pc_); }
    double wait_remaining() const { return timer_active_ ? timer_ : 0; }
    bool active() const;
    bool has_next_phrase()const{if(!source_valid()||program_index_>=source_program_count())return false;const auto p=source_program(program_index_);return pc_<p.command_count&&source_command(p.first_command+uint32_t(pc_)).phrase!=phrase_;}
    bool input_allowed()const{return status_==DialogueStatus::AwaitDialogue&&text_input_enabled_&&(!text_minimum_wait_||!timer_active_); }
    bool auto_advance_ready()const{return text_auto_advance_&&input_allowed();}
    const char* error() const { return error_; }
private:
    bool resume(DialogueSink& sink);
    bool fail(const char* message);
    DialogueStatus status_=DialogueStatus::Idle;
    DialogueActor expected_actor_=kRoomNoActor;
    bool source_valid()const{return independent_?independent_->valid():content_.valid();}
    uint32_t source_program_count()const{return independent_?independent_->program_count():content_.program_count();}
    RoomProgram source_program(uint32_t i)const{return independent_?independent_->program(i):content_.program(i);}
    RoomCommand source_command(uint32_t i)const{return independent_?independent_->command(i):content_.command(i);}
    uint32_t source_flag_count()const{return independent_?independent_->flag_count():content_.flag_count();}
    uint32_t source_string_count()const{return independent_?independent_->string_count():content_.string_count();}
    std::string_view source_string(uint32_t i)const{return independent_?independent_->string(i):content_.string(i);}
    const DialogueProgrammeSource*independent_=nullptr;
    RoomView content_;
    uint32_t program_index_=kRoomNoIndex;
    size_t pc_=0;
    uint32_t generation_=0;
    uint64_t idle_count_=0, resume_idle_=0;
    uint32_t phrase_=0;
    bool timer_active_=false, idle_open_=false, queued_battle_=false,text_minimum_wait_=false,text_auto_advance_=false,text_input_enabled_=true;
    double timer_=0;
    const char* error_="";
};
const char* dialogue_action_name(DialogueActionKind kind);
}
