#pragma once
#include "encore/save_menu_data.hpp"
#include <deque>
namespace encore::upstream {
// Caller supplies metadata only after full session decoding and domain validation.
// Empty is a missing file; a corrupt/incompatible existing file is never empty.
struct SaveSlotMetadata {
 bool occupied=false;std::string lead_name,scene_label,menu_flavor;
 uint32_t highest_level=0;double playtime_seconds=0;std::vector<std::string>party;
};
struct SaveMenuInput {int vertical=0,horizontal=0;bool confirm=false,cancel=false;};
enum class SaveMenuPhase {Closed,Activating,Slots,OverwriteYield,Overwrite,Writing};
enum class SaveMenuEventKind {SoundRequested,SaveRequested,Closed};
struct SaveMenuEvent {SaveMenuEventKind kind=SaveMenuEventKind::SoundRequested;uint32_t slot=0;SaveMenuSound sound=SaveMenuSound::Move;bool any_saved=false;};
struct SaveMenuPose {SaveMenuPhase phase=SaveMenuPhase::Closed;uint32_t selected_slot=0,overwrite_choice=0,arrow_frame=0;bool any_saved=false,cursor_visible=false;float cards_y=0,cursor_y=0,arrow_choice=0;SaveMenuRect cursor_margins;};
class SaveMenu {
public:
 bool open(const SaveMenuData&,const std::vector<SaveSlotMetadata>&,uint32_t initial_slot,std::string&);
 // dt must be finite, nonnegative, and bounded by caller. One supplied direction
 // step per call; the platform owns press/repeat adaptation.
 bool step(double dt,const SaveMenuInput&,std::string&);
 bool acknowledge_save(bool success,const SaveSlotMetadata* refreshed_slot,std::string&);
 bool poll_event(SaveMenuEvent&);void close();
 bool is_open()const{return phase_!=SaveMenuPhase::Closed;}SaveMenuPhase phase()const{return phase_;}
 SaveMenuPose pose()const;const std::vector<SaveSlotMetadata>&slots()const{return slots_;}
 const SaveMenuData*data()const{return data_;}uint32_t selected_slot()const{return selected_;}bool any_saved()const{return saved_;}
private:
 struct Tween {float from=0,to=0;double elapsed=0,duration=0;float value()const;void start(float,double);void tick(double);};
 bool validate_slot(const SaveSlotMetadata&,std::string&)const;void sound(SaveMenuSound);void request_save();void activate();
 const SaveMenuData*data_=nullptr;std::vector<SaveSlotMetadata>slots_;std::deque<SaveMenuEvent>events_;
 SaveMenuPhase phase_=SaveMenuPhase::Closed;uint32_t selected_=0,choice_=0;bool cursor_top_=true,saved_=false;
 double elapsed_=0,cursor_time_=0,arrow_time_=0;float target_cards_y_=0;
 Tween cards_,cursor_,arrow_choice_;
};
}
