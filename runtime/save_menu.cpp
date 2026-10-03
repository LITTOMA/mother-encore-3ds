#include "encore/save_menu.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace encore::upstream {
float SaveMenu::Tween::value()const{if(duration<=0||elapsed>=duration)return to;const double x=1-elapsed/duration;return from+(to-from)*float(1-x*x*x*x);}
void SaveMenu::Tween::start(float destination,double seconds){from=value();to=destination;duration=seconds;elapsed=0;}
void SaveMenu::Tween::tick(double dt){elapsed=std::min(duration,elapsed+dt);}
bool SaveMenu::validate_slot(const SaveSlotMetadata&s,std::string&e)const{
 auto fail=[&](const char*x){e=x;return false;};
 if(!s.occupied){if(!s.lead_name.empty()||!s.scene_label.empty()||!s.menu_flavor.empty()||s.highest_level||s.playtime_seconds||!s.party.empty())return fail("Empty save card contains metadata");return true;}
 if(s.lead_name.empty()||s.lead_name.size()>128||s.scene_label.empty()||s.scene_label.size()>256||!data_->supports_text(s.lead_name,false)||!data_->supports_text(s.scene_label,false)||data_->flavor_index(s.menu_flavor)<0||!s.highest_level||s.highest_level>999999||!std::isfinite(s.playtime_seconds)||s.playtime_seconds<0||s.playtime_seconds>1e15||s.party.empty()||s.party.size()>data_->icons().size())return fail("Unsupported save card metadata");
 std::set<std::string>seen;for(const auto&id:s.party)if(data_->icon_index(id)<0||!seen.insert(id).second)return fail("Unsupported/duplicate save party icon");return true;
}
bool SaveMenu::open(const SaveMenuData&d,const std::vector<SaveSlotMetadata>&slots,uint32_t initial,std::string&e){
 if(is_open()){e="Save menu already open";return false;}
 if(!d.valid()||slots.size()!=d.slot_count()||initial<1||initial>d.slot_count()){e="Save menu requires checked data, exact slots and valid initial selection";return false;}
 const auto*old=data_;data_=&d;for(const auto&s:slots)if(!validate_slot(s,e)){data_=old;return false;}
 slots_=slots;events_.clear();selected_=initial;choice_=0;cursor_top_=true;saved_=false;elapsed_=cursor_time_=arrow_time_=0;
 target_cards_y_=d.layout(SaveMenuLayout::Body).y-(initial-1)*d.slot_spacing();cards_={target_cards_y_,target_cards_y_,0,0};cursor_={};arrow_choice_={};phase_=SaveMenuPhase::Activating;e.clear();return true;
}
void SaveMenu::sound(SaveMenuSound s){events_.push_back({SaveMenuEventKind::SoundRequested,selected_,s,saved_});}
void SaveMenu::request_save(){phase_=SaveMenuPhase::Writing;events_.push_back({SaveMenuEventKind::SaveRequested,selected_,SaveMenuSound::Accept,saved_});}
void SaveMenu::activate(){phase_=SaveMenuPhase::Slots;cursor_time_=0;}
bool SaveMenu::step(double dt,const SaveMenuInput&i,std::string&e){
 if(!std::isfinite(dt)||dt<0||dt>1||i.vertical<-1||i.vertical>1||i.horizontal<-1||i.horizontal>1){e="Invalid save menu frame/input bounds";return false;}
 e.clear();if(!is_open())return true;
 // A bounded queue enforces the polling contract without dropping save/close events.
 if(events_.size()>64){e="Save menu events must be drained";return false;}
 elapsed_+=dt;cursor_time_+=dt;arrow_time_+=dt;cards_.tick(dt);cursor_.tick(dt);arrow_choice_.tick(dt);
 if(phase_==SaveMenuPhase::Activating){if(elapsed_+1e-12>=data_->activation_seconds())phase_=SaveMenuPhase::Slots;return true;}
 // Opening consumes the next idle boundary and cannot accept the opener again.
 if(phase_==SaveMenuPhase::OverwriteYield){phase_=SaveMenuPhase::Overwrite;choice_=0;arrow_choice_={};arrow_time_=0;return true;}
 if(phase_==SaveMenuPhase::Writing)return true;
 if(i.cancel){if(phase_==SaveMenuPhase::Overwrite){sound(SaveMenuSound::Back);activate();}else close();return true;}
 if(i.confirm){if(phase_==SaveMenuPhase::Overwrite){if(choice_==0){sound(SaveMenuSound::Accept);request_save();}else{sound(SaveMenuSound::Back);activate();}}else{sound(SaveMenuSound::Accept);if(slots_[selected_-1].occupied)phase_=SaveMenuPhase::OverwriteYield;else request_save();}return true;}
 if(phase_==SaveMenuPhase::Overwrite){const int n=int(choice_)+i.horizontal;if(n>=0&&n<2&&n!=int(choice_)){choice_=uint32_t(n);arrow_choice_.start(float(choice_),data_->arrow_move_seconds());sound(SaveMenuSound::Move);}return true;}
 if(!i.vertical)return true;
 sound(SaveMenuSound::Move);const auto n=data_->slot_count();const float h=data_->slot_spacing();
 if(i.vertical<0&&selected_>1){--selected_;if(!cursor_top_){cursor_top_=true;cursor_.start(0,data_->scroll_seconds());}else{target_cards_y_+=h;cards_.start(target_cards_y_,data_->scroll_seconds());}}
 else if(i.vertical<0){selected_=n;cursor_top_=false;target_cards_y_-=h*(n-2);cursor_.start(h,data_->scroll_seconds());cards_.start(target_cards_y_,data_->scroll_seconds());}
 else if(selected_<n){++selected_;if(cursor_top_){cursor_top_=false;cursor_.start(h,data_->scroll_seconds());}else{target_cards_y_-=h;cards_.start(target_cards_y_,data_->scroll_seconds());}}
 else{target_cards_y_+=h*(n-(cursor_top_?1:2));selected_=1;cursor_top_=true;cursor_.start(0,data_->scroll_seconds());cards_.start(target_cards_y_,data_->scroll_seconds());}
 return true;
}
bool SaveMenu::acknowledge_save(bool success,const SaveSlotMetadata*slot,std::string&e){
 if(phase_!=SaveMenuPhase::Writing){e="No pending save to acknowledge";return false;}
 if(success){if(!slot||!slot->occupied||!validate_slot(*slot,e)){if(e.empty())e="Successful save needs occupied validated metadata";return false;}slots_[selected_-1]=*slot;saved_=true;}
 else if(slot){e="Failed save must not replace metadata";return false;}
 activate();e.clear();return true;
}
bool SaveMenu::poll_event(SaveMenuEvent&e){if(events_.empty())return false;e=events_.front();events_.pop_front();return true;}
void SaveMenu::close(){if(!is_open()||phase_==SaveMenuPhase::Writing)return;phase_=SaveMenuPhase::Closed;events_.push_back({SaveMenuEventKind::Closed,selected_,SaveMenuSound::Back,saved_});}
SaveMenuPose SaveMenu::pose()const{
 SaveMenuPose p;p.phase=phase_;p.selected_slot=selected_;p.overwrite_choice=choice_;p.any_saved=saved_;if(!data_)return p;
 p.cards_y=cards_.value();p.cursor_y=cursor_.value();p.arrow_choice=arrow_choice_.value();p.cursor_visible=phase_!=SaveMenuPhase::Activating&&phase_!=SaveMenuPhase::Closed;
 const auto&keys=data_->cursor_keys();p.cursor_margins=keys.front().margins;
 if(phase_==SaveMenuPhase::Slots||phase_==SaveMenuPhase::Activating||phase_==SaveMenuPhase::OverwriteYield){double t=std::fmod(cursor_time_,data_->cursor_seconds());for(const auto&k:keys)if(k.time<=t+1e-12)p.cursor_margins=k.margins;}
 double t=std::fmod(arrow_time_,data_->arrow_seconds());for(const auto&k:data_->arrow_keys())if(k.time<=t+1e-12)p.arrow_frame=k.frame;return p;
}
}
