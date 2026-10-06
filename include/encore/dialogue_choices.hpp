#pragma once
#include "encore/dialogue_choices_data.hpp"
#include <deque>

namespace encore::upstream {
struct DialogueChoicesInput {int horizontal=0,vertical=0;bool confirm=false,cancel=false;};
enum class DialogueChoicesPhase:uint8_t {Closed,WaitingText,Active,Resolved};
enum class DialogueChoicesEventKind:uint8_t {SoundRequested,Selected};
struct DialogueChoicesEvent {
 DialogueChoicesEventKind kind=DialogueChoicesEventKind::SoundRequested;
 DialogueChoiceSound sound=DialogueChoiceSound::Move;uint32_t target_pc=0;
 bool cancelled=false,clear_dialogue=false;
 // Selected carries the source InputSound, to play after clearing old text
 // and dispatching the selected phrase (including the hidden Record phrase).
 bool sound_after_target=false;
};
struct DialogueChoicesPose {bool visible=false;uint32_t selected=0,arrow_frame=0;float arrow_x=0,arrow_y=0;};
class DialogueChoices {
public:
 // The scheduler prepares at AwaitChoices, then calls text_completed only on
 // the presenter's real completion callback. No synthesized accept is needed.
 bool prepare(const DialogueChoicesData&,uint32_t group,std::string_view program_identity,uint32_t command_count,std::string&);
 bool text_completed(std::string&);
 // A checked original Cursor owns movement/sound/tween/time in a composed
 // scene. Copy only its current visible option index for source target lookup.
 // This does not advance the legacy arrow, enqueue an event or consume input.
 bool source_cursor_selection(int32_t index,std::string&);
 // Directions are already press/repeat-adapted by the caller. This class owns
 // no filesystem, save operation, platform input polling or MenuRepeat.
 bool step(double dt,const DialogueChoicesInput&,std::string&);
 bool poll_event(DialogueChoicesEvent&);void close();
 DialogueChoicesPhase phase()const{return phase_;}bool active()const{return phase_==DialogueChoicesPhase::Active;}
 DialogueChoicesPose pose()const;const DialogueChoicesData*data()const{return data_;}
 const DialogueChoiceGroup*group()const{return data_?&data_->groups()[group_]:nullptr;}
private:
 struct Tween {float from=0,to=0;double elapsed=0,duration=0;float value()const;void start(float,double);void tick(double);};
 const DialogueChoicesData*data_=nullptr;uint32_t group_=0,selected_=0;
 DialogueChoicesPhase phase_=DialogueChoicesPhase::Closed;double arrow_time_=0;
 Tween arrow_x_,arrow_y_;std::deque<DialogueChoicesEvent>events_;
};
}
