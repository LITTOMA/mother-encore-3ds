#include "encore/dialogue.hpp"
#include <cmath>

namespace encore::upstream {
bool DialoguePlayer::active() const {
    return status_==DialogueStatus::AwaitActor || status_==DialogueStatus::AwaitIdle ||
           status_==DialogueStatus::AwaitTimer || status_==DialogueStatus::AwaitDialogue ||
           status_==DialogueStatus::AwaitChoices || status_==DialogueStatus::AwaitSubmenu || status_==DialogueStatus::Running;
}
bool DialoguePlayer::fail(const char* message) {
    status_=DialogueStatus::Error; timer_active_=false; error_=message; return false;
}
bool DialoguePlayer::start(const RoomView& content,uint32_t program_index,DialogueSink& sink,uint32_t generation) {
    if(status_!=DialogueStatus::Idle || generation==0 || !content.valid() || program_index>=content.program_count()) return false;
    content_=content;program_index_=program_index;
    generation_=generation; status_=DialogueStatus::Running; return resume(sink);
}
bool DialoguePlayer::actor_ready(DialogueActor actor, uint32_t generation, DialogueSink& sink) {
    // An old scene's queued actor_ready must never resume a new scene's task.
    if(generation!=generation_ || status_!=DialogueStatus::AwaitActor || actor!=expected_actor_) return false;
    expected_actor_=kRoomNoActor; status_=DialogueStatus::Running; return resume(sink);
}
bool DialoguePlayer::idle_begin(DialogueSink& sink) {
    if(!active()) return status_!=DialogueStatus::Error;
    if(idle_open_) return fail("Dialogue idle phase begun twice");
    idle_open_=true; ++idle_count_;
    if(status_==DialogueStatus::AwaitIdle && idle_count_>=resume_idle_) {
        status_=DialogueStatus::Running; return resume(sink);
    }
    return true;
}
bool DialoguePlayer::idle_process(double delta, DialogueSink& sink) {
    if(!active()) return status_!=DialogueStatus::Error;
    if(!idle_open_) return fail("Dialogue WaitTimer processed before idle_frame");
    idle_open_=false;
    // This port's bounded update contract admits up to a 250ms idle step.
    // Larger deltas can race a yielded _handle_phrase against its next phrase
    // in upstream; that unreviewed overlapping-coroutine case is rejected.
    if(!std::isfinite(delta) || delta<=0 || delta>0.25) return fail("Unsupported dialogue idle delta");
    if(timer_active_) {
        timer_-=delta;
        // Godot 3.6.2 Timer uses strict negativity, not <=, and discards
        // overshoot for one_shot timers. A restarted timer is not decremented
        // again during its timeout callback.
        if(timer_<0) {
            timer_active_=false; timer_=0;
            if(status_==DialogueStatus::AwaitDialogue&&text_minimum_wait_)return true;
            if(status_!=DialogueStatus::AwaitTimer) return fail("Dialogue timer raced a suspended phrase");
            status_=DialogueStatus::Running; return resume(sink);
        }
    }
    return true;
}
bool DialoguePlayer::dialogue_finished(DialogueSink& sink,bool automatic) {
    if(!input_allowed()||(automatic&&!text_auto_advance_))return false;
    status_=DialogueStatus::Running;return resume(sink);
}
bool DialoguePlayer::choices_selected(uint32_t target_pc,uint32_t generation,DialogueSink& sink) {
    if(generation!=generation_ || status_!=DialogueStatus::AwaitChoices ||
       target_pc>=content_.program(program_index_).command_count)return false;
    pc_=target_pc;status_=DialogueStatus::Running;return resume(sink);
}
bool DialoguePlayer::submenu_closed(uint32_t generation,DialogueSink& sink) {
    if(generation!=generation_ || status_!=DialogueStatus::AwaitSubmenu)return false;
    status_=DialogueStatus::Running;return resume(sink);
}
void DialoguePlayer::cancel() {
    if(active()) { status_=DialogueStatus::Cancelled; timer_active_=false; expected_actor_=kRoomNoActor; }
}
bool DialoguePlayer::resume(DialogueSink& sink) {
    constexpr size_t budget=32;
    const auto program=content_.program(program_index_);
    for(size_t n=0;n<budget && pc_<program.command_count;++n) {
        const auto raw=content_.command(program.first_command+uint32_t(pc_++));
        DialogueAction action;
        action.kind=static_cast<DialogueActionKind>(raw.opcode);action.actor=raw.actor_index;
        action.phrase=raw.phrase;action.target_index=raw.target_index;action.auxiliary_index=raw.auxiliary_index;
        action.flags=raw.flags;action.vector=raw.vector;action.value=raw.value;action.duration=raw.duration;
        phrase_=action.phrase;
        if(action.kind==DialogueActionKind::Jump) {
            if(action.target_index>=program.command_count)return fail("Dialogue jump outside program");
            pc_=action.target_index;continue;
        }
        if(action.kind==DialogueActionKind::BranchFlag || action.kind==DialogueActionKind::BranchLeader) {
            if(action.auxiliary_index>=program.command_count)return fail("Dialogue branch outside program");
            if(action.kind==DialogueActionKind::BranchFlag) {
                if(action.target_index>=content_.flag_count() || (action.value!=0&&action.value!=1))
                    return fail("Invalid dialogue flag condition");
            } else if(action.target_index>=content_.string_count() || content_.string(action.target_index).empty()) {
                return fail("Invalid dialogue leader condition");
            }
            bool matched=false;
            if(!sink.branch_condition(action,matched))return fail("World rejected dialogue branch condition");
            if(matched)pc_=action.auxiliary_index;
            continue;
        }
        if(action.kind==DialogueActionKind::AwaitChoices) {
            // The presenter consumes source WAIT tags and shows choices after
            // printing. Ordinary text/idle/actor callbacks cannot pass this gate.
            status_=DialogueStatus::AwaitChoices;
            if(!sink.apply(action))return fail("World rejected dialogue choices");
            return true;
        }
        if(action.kind==DialogueActionKind::AwaitSubmenu) {
            status_=DialogueStatus::AwaitSubmenu;return true;
        }
        if(action.kind==DialogueActionKind::YieldIdle) {
            status_=DialogueStatus::AwaitIdle; resume_idle_=idle_count_+1; return true;
        }
        if(action.kind==DialogueActionKind::AwaitDialogue) {
            text_minimum_wait_=(action.flags&1)!=0;text_auto_advance_=(action.flags&2)!=0;text_input_enabled_=(action.flags&4)==0;
            if(text_minimum_wait_&&!timer_active_)return fail("Text minimum delay lacks timer");
            status_=DialogueStatus::AwaitDialogue;return true;
        }
        if(action.kind==DialogueActionKind::AwaitTimer) {
            if(!timer_active_) return fail("Dialogue phrase has no wait timer");
            status_=DialogueStatus::AwaitTimer; return true;
        }
        if(action.kind==DialogueActionKind::BindActor) {
            // Set the wait before issuing the deferred creation request.
            status_=DialogueStatus::AwaitActor; expected_actor_=action.actor;
            if(!sink.apply(action)) return fail("World rejected dialogue actor binding");
            return true;
        }
        if(action.kind==DialogueActionKind::StartWait) {
            if(timer_active_ || !std::isfinite(action.duration) || action.duration<=0)
                return fail("Invalid generated dialogue timer");
            timer_=action.duration; timer_active_=true;
        }
        if(action.kind==DialogueActionKind::QueueBattle) queued_battle_=true;
        if(action.kind==DialogueActionKind::RequestBattle && !queued_battle_)
            return fail("Dialogue battle requested without queue");
        if(!sink.apply(action)) return fail("World rejected dialogue action");
        if(action.kind==DialogueActionKind::DialogueDone) {
            // Branched programs may have several source endings. A queued
            // battle retains the existing final wait/RequestBattle continuation.
            if(!queued_battle_ && (pc_==program.command_count || action.duration>0)) {
                status_=DialogueStatus::Completed;timer_active_=false;return true;
            }
            if(pc_==program.command_count)return fail("World continuation ended with an unstarted battle");
        }
        if(action.kind==DialogueActionKind::RequestBattle) {
            status_=DialogueStatus::BattleRequested; return true;
        }
    }
    return fail("Dialogue instruction limit or missing terminator");
}
const char* dialogue_action_name(DialogueActionKind kind) {
    static constexpr const char* names[]={"BeginCutscene","BindActor","ActorPersistent","StartWait","MusicFadeOut",
        "SetTalker","CallObjectDeferred","OverworldBattleMusic","PlaySound","MoveActor","TurnActor","ShakeActor",
        "JumpActor","AnimateActor","EmoteActor","ShakeCamera","ChangeCamera","MoveCamera","QueueBattle",
        "StopInteraction","RestoreActor","ReleaseBattleActor","CutsceneEnded","DialogueDone","RequestBattle","YieldIdle","AwaitTimer","SetActorDirection","TeleportActor","MoveActorPath","ReturnCamera","SetFlag","ShowDialogue","AwaitDialogue","PlayMusicImmediate","HideDialogue","Jump","BranchFlag","BranchLeader","AwaitChoices","OpenSave","AwaitSubmenu","StopActorLoop","OpenStorage"};
    const auto i=static_cast<size_t>(kind); return i<sizeof(names)/sizeof(names[0]) ? names[i] : "Unknown";
}
}
