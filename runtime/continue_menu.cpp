#include "encore/continue_menu.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
float ContinueMenu::Tween::value()const{if(duration<=0||time>=duration)return to;double x=1-time/duration;return from+(to-from)*float(1-x*x*x*x);}
void ContinueMenu::Tween::start(float x,double seconds){from=value();to=x;time=0;duration=seconds;}
void ContinueMenu::Tween::tick(double dt){time=std::min(duration,time+dt);}
void ContinueMenu::sound(ContinueSound s){events_.push_back({ContinueEventKind::SoundRequested,slots_.selected_slot(),data_->sound(s)});}
void ContinueMenu::title_music(){events_.push_back({ContinueEventKind::TitleMusicRequested,0,data_->title_music()});}
void ContinueMenu::begin(Transit t){transit_=t;transit_time_=0;}
void ContinueMenu::drain_slots(){SaveMenuEvent e;while(slots_.poll_event(e))if(e.kind==SaveMenuEventKind::SoundRequested)sound(ContinueSound::Move);}
bool ContinueMenu::open(const ContinueMenuData&d,const SaveMenuData&sd,const std::vector<SaveSlotMetadata>&metadata,uint32_t remembered,std::string&e){
 if(is_open()){e="Continue menu already open";return false;}if(!d.valid()||!sd.valid()){e="Continue requires checked title and save menu data";return false;}
 for(uint32_t i=0;i<4;++i)if(!sd.supports_text(d.text(i),true)){e="Continue action text outside checked font domain";return false;}
 SaveMenu candidate;const auto initial=std::clamp<uint32_t>(remembered,1,sd.slot_count());if(!candidate.open(sd,metadata,initial,e))return false;
 data_=&d;save_data_=&sd;metadata_=metadata;remembered_=remembered;slots_=std::move(candidate);slots_.close();SaveMenuEvent ignored;while(slots_.poll_event(ignored)){}
 events_.clear();title_=remembered?1:0;action_=0;phase_=ContinuePhase::Title;transit_=Transit::None;transit_time_=cursor_time_=action_repeat_=0;arrow_times_.clear();arrow_running_.clear();action_x_={};title_music();e.clear();return true;
}
bool ContinueMenu::step(double dt,const SaveMenuInput&i,std::string&e,bool navigation_pulse){
 if(!std::isfinite(dt)||dt<0||dt>1||i.vertical<-1||i.vertical>1||i.horizontal<-1||i.horizontal>1){e="Invalid Continue frame/input bounds";return false;}e.clear();if(!is_open())return true;if(events_.size()>64){e="Continue events must be drained";return false;}
 cursor_time_+=dt;action_repeat_=std::max(0.0,action_repeat_-dt);for(auto&t:action_x_)t.tick(dt);if(slots_.is_open())for(size_t a=0;a<arrow_times_.size();++a)if(arrow_running_[a])arrow_times_[a]+=dt;
 if(phase_==ContinuePhase::LoadPending)return true;
 if(transit_!=Transit::None){
  transit_time_+=dt;
  if(transit_==Transit::ToSlotsYield){if(!slots_.step(dt,{},e))return false;drain_slots();if(slots_.phase()==SaveMenuPhase::Slots)phase_=ContinuePhase::Slots;begin(Transit::ToSlotsOut);return true;}
  if(transit_==Transit::ToTitleYield){begin(Transit::ToTitleOut);return true;}
  if(transit_==Transit::RevealYield){begin(Transit::RevealOut);return true;}
  if(transit_==Transit::ToSlotsIn){if(transit_time_+1e-12>=data_->animation(ContinueAnimation::CircleIn).length/data_->door_in_speed()){
   if(!slots_.open(*save_data_,metadata_,std::clamp<uint32_t>(remembered_,1,save_data_->slot_count()),e))return false;
   phase_=ContinuePhase::Activating;cursor_time_=0;arrow_times_.assign(metadata_.size(),0);arrow_running_.assign(metadata_.size(),true);begin(Transit::ToSlotsYield);}return true;}
  if(transit_==Transit::ToTitleIn){if(transit_time_+1e-12>=data_->animation(ContinueAnimation::FadeIn).length/data_->door_in_speed()){slots_.close();drain_slots();title_=remembered_?1:0;phase_=ContinuePhase::Title;title_music();begin(Transit::ToTitleYield);}return true;}
  if(transit_==Transit::LoadIn){if(transit_time_+1e-12>=data_->animation(ContinueAnimation::FadeIn).length){phase_=ContinuePhase::LoadPending;transit_=Transit::None;events_.push_back({ContinueEventKind::LoadRequested,slots_.selected_slot(),{},0,ContinueBoundary::NewGame});}return true;}
  if(transit_==Transit::RevealOut){if(transit_time_+1e-12>=data_->animation(ContinueAnimation::CircleOut).length){phase_=ContinuePhase::Closed;transit_=Transit::None;}return true;}
  if(transit_==Transit::ToTitleOut){if(transit_time_+1e-12>=data_->animation(ContinueAnimation::FadeOut).length/data_->door_out_speed())transit_=Transit::None;return true;}
  if(transit_==Transit::ToSlotsOut&&transit_time_+1e-12>=data_->animation(ContinueAnimation::FadeOut).length/data_->door_out_speed())transit_=Transit::None;
 }
 if(phase_==ContinuePhase::Title){
  if(i.vertical){title_=uint32_t((int(title_)+i.vertical+4)%4);sound(ContinueSound::Move);}
  if(i.confirm){sound(ContinueSound::Accept);if(title_==1){events_.push_back({ContinueEventKind::MusicFadeRequested,0,{},data_->door_music_fade_seconds()});begin(Transit::ToSlotsIn);}
   else if(title_==3){events_.push_back({ContinueEventKind::ExitRequested,0,{},0,ContinueBoundary::NewGame});phase_=ContinuePhase::Closed;}
   else events_.push_back({ContinueEventKind::BoundaryRequested,0,{},0,title_==0?ContinueBoundary::NewGame:ContinueBoundary::Settings});}return true;
 }
 if(phase_==ContinuePhase::Activating){if(!slots_.step(dt,{},e))return false;drain_slots();if(slots_.phase()==SaveMenuPhase::Slots)phase_=ContinuePhase::Slots;return true;}
 if(phase_==ContinuePhase::Slots){
  if(i.cancel){begin(Transit::ToTitleIn);return true;}
  if(i.confirm){if(!slots_.slots()[slots_.selected_slot()-1].occupied){sound(ContinueSound::Restricted);return true;}sound(ContinueSound::Accept);phase_=ContinuePhase::Actions;action_=0;arrow_running_[slots_.selected_slot()-1]=true;for(size_t v=0;v<action_x_.size();++v){float x=data_->viewports()[v].actions[0].x;action_x_[v]={x,x,0,0};}return true;}
  if(!slots_.step(dt,{i.vertical,0,false,false},e))return false;
  drain_slots();return true;
 }
 // Keep list scroll animations advancing while its action cursor is active.
 if(phase_==ContinuePhase::Actions){if(!slots_.step(dt,{},e))return false;drain_slots();
  if(i.cancel){sound(ContinueSound::Back);arrow_running_[slots_.selected_slot()-1]=false;phase_=ContinuePhase::Slots;cursor_time_=0;return true;}
  if(i.confirm){sound(ContinueSound::Accept);if(action_==0){arrow_running_[slots_.selected_slot()-1]=false;phase_=ContinuePhase::LoadFade;begin(Transit::LoadIn);events_.push_back({ContinueEventKind::LoadFadeRequested,slots_.selected_slot(),{},data_->music_fade_seconds()});}else events_.push_back({ContinueEventKind::BoundaryRequested,slots_.selected_slot(),{},0,action_==1?ContinueBoundary::Copy:action_==2?ContinueBoundary::Delete:ContinueBoundary::Options});return true;}
  if(i.horizontal&&(navigation_pulse||action_repeat_<=0)){action_repeat_=data_->action_repeat_seconds();action_=uint32_t((int(action_)+i.horizontal+4)%4);for(size_t v=0;v<action_x_.size();++v)action_x_[v].start(data_->viewports()[v].actions[action_].x,save_data_->arrow_move_seconds());sound(ContinueSound::Move);}return true;
 }
 return true;
}
bool ContinueMenu::acknowledge_load(bool success,std::string&e){if(phase_!=ContinuePhase::LoadPending){e="No pending Continue load to acknowledge";return false;}if(success){remembered_=slots_.selected_slot();phase_=ContinuePhase::WorldReveal;begin(Transit::RevealYield);}else{phase_=ContinuePhase::Actions;arrow_running_[slots_.selected_slot()-1]=true;transit_=Transit::None;}e.clear();return true;}
bool ContinueMenu::poll_event(ContinueEvent&e){if(events_.empty())return false;e=std::move(events_.front());events_.pop_front();return true;}
void ContinueMenu::close(){phase_=ContinuePhase::Closed;transit_=Transit::None;slots_.close();events_.clear();}
ContinuePose ContinueMenu::pose()const{
 ContinuePose p;p.phase=phase_;p.title_option=title_;p.action=action_;p.slots=slots_.pose();p.world_visible=phase_==ContinuePhase::WorldReveal;p.arrow_visible=phase_==ContinuePhase::Actions;if(!data_)return p;
 for(size_t v=0;v<action_x_.size();++v)p.action_x[v]=action_x_[v].value();
 if(phase_==ContinuePhase::Actions||phase_==ContinuePhase::LoadFade||phase_==ContinuePhase::LoadPending)p.slots.cursor_margins=data_->layout(ContinueLayout::SelectedCursor);
 if(save_data_&&(phase_==ContinuePhase::Slots||phase_==ContinuePhase::Activating)){double t=std::fmod(cursor_time_,save_data_->cursor_seconds());for(const auto&k:save_data_->cursor_keys())if(k.time<=t+1e-12)p.slots.cursor_margins=k.margins;}
 if(save_data_){double t=std::fmod(arrow_times_.empty()?0:arrow_times_[slots_.selected_slot()-1],save_data_->arrow_seconds());for(const auto&k:save_data_->arrow_keys())if(k.time<=t+1e-12)p.arrow_frame=k.frame;}
 if(phase_==ContinuePhase::LoadPending||transit_==Transit::ToSlotsYield||transit_==Transit::ToTitleYield||transit_==Transit::RevealYield){p.fade={true,false,false,0,1};return p;}
 ContinueAnimation anim=ContinueAnimation::FadeIn;double speed=1;
 switch(transit_){case Transit::None:return p;case Transit::ToSlotsIn:anim=ContinueAnimation::CircleIn;speed=data_->door_in_speed();p.fade.circle=true;break;case Transit::ToTitleIn:speed=data_->door_in_speed();break;case Transit::ToSlotsOut:case Transit::ToTitleOut:anim=ContinueAnimation::FadeOut;speed=data_->door_out_speed();break;case Transit::LoadIn:break;case Transit::RevealOut:anim=ContinueAnimation::CircleOut;p.fade.circle=p.fade.focused=true;break;default:return p;}
 p.fade.visible=true;p.fade.cut=float(data_->animation(anim).value(transit_time_*speed));float x=std::clamp(1-p.fade.cut,0.f,1.f);p.fade.alpha=x*x*(3-2*x);return p;
}
}
