#pragma once
#include "encore/continue_menu_data.hpp"
#include "encore/save_menu.hpp"
namespace encore::upstream {
enum class ContinuePhase {Closed,Title,Activating,Slots,Actions,LoadFade,LoadPending,WorldReveal};
enum class ContinueBoundary {NewGame,Settings,Copy,Delete,Options};
enum class ContinueEventKind {SoundRequested,TitleMusicRequested,MusicFadeRequested,LoadFadeRequested,LoadRequested,ExitRequested,BoundaryRequested};
struct ContinueEvent {ContinueEventKind kind=ContinueEventKind::SoundRequested;uint32_t slot=0;std::string path;double seconds=0;ContinueBoundary boundary=ContinueBoundary::NewGame;};
struct ContinueFadePose {bool visible=false,circle=false,focused=false;float cut=1,alpha=0;};
struct ContinuePose {ContinuePhase phase=ContinuePhase::Closed;uint32_t title_option=0,action=0,arrow_frame=0;SaveMenuPose slots;bool world_visible=false,arrow_visible=false;std::array<float,2>action_x{};ContinueFadePose fade;};
class ContinueMenu {
public:
 bool open(const ContinueMenuData&,const SaveMenuData&,const std::vector<SaveSlotMetadata>&,uint32_t remembered_slot,std::string&);
 // navigation_pulse is set by the 3DS press/repeat adapter; distinct taps
 // must not be discarded by the original held-input cursor timer.
 bool step(double dt,const SaveMenuInput&,std::string&,bool navigation_pulse=false);
 // A successful application is acknowledged only after fresh-scene restoration.
 bool acknowledge_load(bool success,std::string&);
 bool poll_event(ContinueEvent&);void close();
 bool is_open()const{return phase_!=ContinuePhase::Closed;}ContinuePhase phase()const{return phase_;}
 const ContinueMenuData*data()const{return data_;}const SaveMenu&slot_model()const{return slots_;}
 uint32_t selected_slot()const{return slots_.selected_slot();}ContinuePose pose()const;
private:
 enum class Transit {None,ToSlotsIn,ToSlotsYield,ToSlotsOut,ToTitleIn,ToTitleYield,ToTitleOut,LoadIn,RevealYield,RevealOut};
 struct Tween {float from=0,to=0;double time=0,duration=0;float value()const;void start(float,double);void tick(double);};
 void sound(ContinueSound);void title_music();void begin(Transit);void drain_slots();
 const ContinueMenuData*data_=nullptr;const SaveMenuData*save_data_=nullptr;SaveMenu slots_;std::vector<SaveSlotMetadata>metadata_;std::deque<ContinueEvent>events_;
 ContinuePhase phase_=ContinuePhase::Closed;Transit transit_=Transit::None;uint32_t remembered_=0,title_=0,action_=0;double transit_time_=0,cursor_time_=0,action_repeat_=0;std::vector<double>arrow_times_;std::vector<bool>arrow_running_;std::array<Tween,2>action_x_{};
};
}
