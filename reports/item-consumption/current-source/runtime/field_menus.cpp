#include "encore/field_menus.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace encore::upstream {namespace {
using Phase=FieldMenusPhase;using Text=FieldMenuText;
bool fail(std::string&e,const char*s){e=s;return false;}
bool flag(const SessionSnapshot&s,std::string_view id){for(const auto&f:s.flags)if(f.id==id)return f.value;return false;}
uint32_t wrap(uint32_t at,int delta,uint32_t n){return n?uint32_t((int64_t(at)+delta+int64_t(n)*2)%n):0;}
bool same_status(const std::vector<SessionStatus>&a,const std::vector<SessionStatus>&b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(a[i].status_id!=b[i].status_id||a[i].passive_healing_turns!=b[i].passive_healing_turns)return false;return true;}
bool same_boosts(const std::vector<SessionIntegerValue>&a,const std::vector<SessionIntegerValue>&b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(a[i].id!=b[i].id||a[i].value!=b[i].value)return false;return true;}
}
bool expand_field_item_text(std::string_view input,std::string_view owner,uint32_t value,std::string&out,std::string&e){
 std::string intermediate;
 for(size_t i=0;i<input.size();){
  if(input[i]=='{'){
   if(input.substr(i,7)=="{owner}"){intermediate+="{owner}";i+=7;}
   else if(input.substr(i,7)=="{value}"){intermediate+=std::to_string(value);i+=7;}
   else return fail(e,"Unknown field item message token");
  }else{if(input[i]=='}')return fail(e,"Unbalanced field item message token");intermediate+=input[i++];}
 }
 return expand_field_menu_text(intermediate,owner,std::vector<std::string>(8),out,e);
}
bool consume_field_menu_item(const FieldMenusData&data,const FieldMenusState&before,uint32_t at,FieldMenusState&out,FieldMenuConsumeResult&result,std::string&e){
 if(!data.valid()||!data.has_consume_effects()||data.consume_lane!=uint32_t(FieldMenuConsumeAudioLane::Effect)||!validate_session_snapshot(before.session,e)||before.session.party.size()!=1||before.session.characters.empty()||before.session.characters[0].character_id!=data.consume_actor||before.session.party[0]!=data.consume_actor||before.stats[0]<=0||before.stats[1]<0)return fail(e,"Field consume context/resource rejected");
 const auto&prior=before.session.characters[0];
 if(prior.hp<0||prior.hp>before.stats[0]||prior.pp<0||prior.pp>before.stats[1]||at>=prior.inventory.size())return fail(e,"Field consume health/selection rejected");
 for(const auto&s:prior.status)if(std::find(data.consume_statuses.begin(),data.consume_statuses.end(),s.status_id)==data.consume_statuses.end())return fail(e,"Field consume status behavior is unsupported");
 const auto&item=prior.inventory[at];const auto*rule=data.item(item.item_id);const auto*effect=data.consume_effect(item.item_id);
 if(!rule||rule->action!=2||rule->key||!effect||item.equipped||item.doses<=0||item.doses>rule->doses)return fail(e,"Field consume item identity/doses/effect rejected");
 if((effect->hp==0)==effect->cure_status.empty()||effect->hp>65535||!data.cure_refresh_hp||data.cure_refresh_hp>65535)return fail(e,"Field consume effect domain rejected");
 auto next=before;auto&character=next.session.characters[0];FieldMenuConsumeResult applied;unsigned message=0;
 if(effect->hp){
  const auto requested=character.hp+effect->hp;character.hp=std::min<int64_t>(requested,before.stats[0]);
  applied.signals.push_back({FieldMenuConsumeSignalKind::HP,requested,before.stats[0]});
  const bool maximum=character.hp>=before.stats[0];message=maximum?1:0;
  applied.kind=maximum?FieldMenuConsumeKind::HPMax:FieldMenuConsumeKind::HPUp;
  applied.value=maximum?1:effect->hp;applied.eat_sound=true;
 }else{
  if(std::find(data.consume_statuses.begin(),data.consume_statuses.end(),effect->cure_status)==data.consume_statuses.end())return fail(e,"Field consume cure status is unbound");
  auto status=std::find_if(character.status.begin(),character.status.end(),[&](const SessionStatus&s){return s.status_id==effect->cure_status;});
  const bool healed=status!=character.status.end();message=healed?2:3;
  applied.kind=healed?FieldMenuConsumeKind::Healed:FieldMenuConsumeKind::HealFailed;
  if(healed){
   character.status.erase(status);applied.signals.push_back({FieldMenuConsumeSignalKind::Status,0,0});
   if(character.hp==0){character.hp=std::min<int64_t>(data.cure_refresh_hp,before.stats[0]);applied.signals.push_back({FieldMenuConsumeSignalKind::HP,data.cure_refresh_hp,before.stats[0]});}
  }
  applied.eat_sound=healed;
 }
 applied.message_key=effect->message_keys[message];
 if(applied.message_key.empty()||!expand_field_item_text(effect->messages[message].get(before.chinese),prior.nickname,applied.value,applied.message,e))return fail(e,"Field consume message rejected");
 // Source drop_item removes the instance without changing its remaining
 // doses. reduce_or_drop_item decrements only an instance which stays held.
 applied.retained_doses=uint32_t(item.doses);
 if(item.doses>1){--character.inventory[at].doses;--applied.retained_doses;}
 else character.inventory.erase(character.inventory.begin()+at);
 if(!validate_session_snapshot(next.session,e))return false;
 out=std::move(next);result=std::move(applied);e.clear();return true;
}
std::string FieldMenus::text(Text id)const{return data_->text(id,state_.chinese);}
bool FieldMenus::refresh(std::string&e){FieldMenusState s;if(!host_.read(s,e))return false;if(!std::isfinite(s.world_x)||!std::isfinite(s.world_y)||(s.area_map&&(!std::isfinite(s.area_map_position[0])||!std::isfinite(s.area_map_position[1])||s.area_map_item.empty())))return fail(e,"Field menu world position rejected");if(!validate_session_snapshot(s.session,e))return false;if(s.stats[0]<=0||std::any_of(s.stats.begin(),s.stats.end(),[](int32_t n){return n<0;}))return fail(e,"Field menu derived stats rejected");if(s.session.party.size()!=1||s.session.characters.empty()||s.session.characters[0].character_id!=s.session.party[0]||!data_->supports(s.preferences)||!settings_->supports(s.session.settings))return fail(e,"Field menus live session/preferences rejected");const auto&c=s.session.characters[0];if(c.hp>s.stats[0]||c.pp>s.stats[1])return fail(e,"Field menu HP/PP rejected");for(const auto&skill:c.learned_skills)if(std::none_of(data_->skills.begin(),data_->skills.end(),[&](const FieldMenuSkill&s){return s.category!=0&&s.id==skill;}))return fail(e,"Field menu learned skill rejected");for(const auto&i:s.session.characters[0].inventory)if(!data_->item(i.item_id)||data_->item(i.item_id)->key||i.doses<1||i.doses>data_->item(i.item_id)->doses)return fail(e,"Field menus unmapped ordinary item");for(const auto&i:s.session.key_items)if(!data_->item(i.item_id)||!data_->item(i.item_id)->key||i.doses!=data_->item(i.item_id)->doses)return fail(e,"Field menus unmapped key item");state_=std::move(s);return true;}
bool FieldMenus::initialize(const FieldMenusData&d,const StartupSettingsData&s,FieldMenusHost h,std::string&e){if(!d.valid()||!s.valid()||!h.read||!h.commit||!h.equip||!h.locale||d.setting_labels[0].size()!=s.speeds.size()||d.setting_labels[1].size()!=s.flavors.size()||d.setting_labels[2].size()!=s.prompts.size())return fail(e,"Field menus require checked resources and complete host");FieldMenus next;next.data_=&d;next.settings_=&s;next.host_=std::move(h);if(!next.refresh(e))return false;*this=std::move(next);e.clear();return true;}
bool FieldMenus::open(uint32_t command,std::string&e){if(!data_||active()||!refresh(e))return fail(e,"Field menu open state rejected");selection_=item_selection_=action_selection_=sort_source_=skill_tab_=choice_=0;key_=false;changed_=false;exit_=FieldMenusExit::None;message_.clear();if(command==1)phase_=Phase::Goods;else if(command==4)phase_=Phase::Status;else if(command==5){map_parent_=Phase::Closed;if(!open_map(false,e))return false;}else if(command==6)phase_=Phase::Options;else return fail(e,"Field menu command rejected");sounds_.push_back(3);e.clear();return true;}
const std::vector<SessionItem>&FieldMenus::goods()const{return key_?state_.session.key_items:state_.session.characters[0].inventory;}
std::vector<SessionItem>&FieldMenus::goods(FieldMenusState&s)const{return key_?s.session.key_items:s.session.characters[0].inventory;}
const FieldMenuItem*FieldMenus::selected_item()const{return item_selection_<goods().size()?data_->item(goods()[item_selection_].item_id):nullptr;}
std::vector<uint32_t>FieldMenus::actions()const{std::vector<uint32_t>v;const auto*i=selected_item();if(!i)return v;if(i->action)v.push_back(i->action);v.push_back(4);if(i->drop)v.push_back(5);return v;}
std::vector<const FieldMenuSkill*>FieldMenus::skills()const{std::vector<const FieldMenuSkill*>v;const auto&known=state_.session.characters[0].learned_skills;for(const auto&s:data_->skills){if(s.category!=skill_tab_)continue;if(!s.flag.empty()&&!flag(state_.session,s.flag))continue;if(!s.battle_skill.empty()&&std::find(known.begin(),known.end(),s.battle_skill)==known.end())continue;v.push_back(&s);}return v;}
bool merge_field_menu_session(const FieldMenusState&before,const FieldMenusChanges&changes,const SessionSnapshot&current,SessionSnapshot&out,std::string&e){
 const auto&prior=before.session;if(prior.party.size()!=1||current.party!=prior.party||prior.characters.empty()||current.characters.empty()||prior.characters[0].character_id!=current.characters[0].character_id||current.characters[0].character_id!=current.party[0]||current.scene_id!=prior.scene_id||current.scene_label!=prior.scene_label)return fail(e,"Field menu context changed");
 auto same=[](const std::vector<SessionItem>&a,const std::vector<SessionItem>&b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(a[i].item_id!=b[i].item_id||a[i].uid!=b[i].uid||a[i].doses!=b[i].doses||a[i].equipped!=b[i].equipped)return false;return true;};
 const auto&a=prior.settings;const auto&b=current.settings;if(!same(prior.characters[0].inventory,current.characters[0].inventory)||!same(prior.key_items,current.key_items)||a.text_speed!=b.text_speed||a.menu_flavor!=b.menu_flavor||a.button_prompts!=b.button_prompts||a.description!=b.description)return fail(e,"Field menu inventory/settings changed since observation");
 const auto&observed=prior.characters[0];const auto&live=current.characters[0];
 if(changes.health_observed&&(observed.hp!=live.hp||observed.pp!=live.pp||observed.level!=live.level||!same_status(observed.status,live.status)||!same_boosts(observed.permanent_boosts,live.permanent_boosts)))return fail(e,"Field consume health/status changed since observation");
 if((changes.hp||changes.status)&&!changes.health_observed)return fail(e,"Field consume writes require an observed health context");
 auto next=current;next.settings=changes.settings;next.characters[0].inventory=changes.inventory;next.key_items=changes.keys;
 if(changes.hp)next.characters[0].hp=*changes.hp;
 if(changes.status)next.characters[0].status=*changes.status;
 if(!validate_session_snapshot(next,e))return false;out=std::move(next);e.clear();return true;
}
bool FieldMenus::commit(FieldMenusState next,std::string&e,bool health){FieldMenusChanges changes{next.session.characters[0].inventory,next.session.key_items,next.session.settings,next.preferences};changes.health_observed=health;if(next.session.characters[0].hp!=state_.session.characters[0].hp)changes.hp=next.session.characters[0].hp;if(!same_status(next.session.characters[0].status,state_.session.characters[0].status))changes.status=next.session.characters[0].status;if(!host_.commit(state_,changes,e))return false;changed_=true;return refresh(e);}
void FieldMenus::back(){sounds_.push_back(10);selection_=0;switch(phase_){
 case Phase::Goods:case Phase::Status:case Phase::Options:phase_=Phase::Closed;exit_=FieldMenusExit::Pause;sounds_.push_back(4);break;
 case Phase::Actions:case Phase::SortTarget:phase_=Phase::Goods;selection_=item_selection_;break;
 case Phase::Sort:case Phase::Drop:case Phase::Target:phase_=Phase::Actions;selection_=action_selection_;break;
 case Phase::Message:phase_=Phase::Goods;selection_=item_selection_;break;
 case Phase::SkillTabs:phase_=Phase::Status;break;
 case Phase::Skills:phase_=Phase::SkillTabs;selection_=skill_tab_;break;
 case Phase::Map:phase_=map_parent_;if(phase_==Phase::Closed)exit_=FieldMenusExit::Pause;selection_=item_selection_;break;
 case Phase::File:case Phase::General:selection_=phase_==Phase::File?0:1;phase_=Phase::Options;break;
 case Phase::Audio:case Phase::Video:case Phase::Controls:selection_=phase_==Phase::Audio?0:phase_==Phase::Video?1:2;phase_=Phase::General;break;
 case Phase::Choice:phase_=parent_;selection_=choice_previous_;break;
 case Phase::Confirm:phase_=parent_;selection_=choice_previous_;break;
 case Phase::Capture:phase_=Phase::Controls;selection_=choice_previous_;break;
 default:break;}}
void FieldMenus::clamp_map(){const auto&im=data_->images[missing_map_?1:0];const auto&view=data_->display_sizes[1];map_x_=std::clamp(map_x_,0.f,std::max(0.f,float(im.width)-view[0]));map_y_=std::clamp(map_y_,0.f,std::max(0.f,float(im.height)-view[1]));}
std::array<float,2>field_menu_map_position(const FieldMenusData&d,const FieldMenusState&s){
 if(s.area_map)return s.area_map_position;
 return {s.in_field?d.map_offset_x+s.world_x/d.map_divisor:d.map_home_x,s.in_field?d.map_offset_y+s.world_y/d.map_divisor:d.map_home_y};
}
bool FieldMenus::open_map(bool force,std::string&e){
 if(state_.area_map&&state_.area_map_item!=data_->map_item)return fail(e,"Field map region resource is not yet ported");
 missing_map_=!force&&std::none_of(state_.session.key_items.begin(),state_.session.key_items.end(),[&](const SessionItem&i){return i.item_id==data_->map_item;});markers_=true;map_fraction_=0;
 const auto&view=data_->display_sizes[1];const auto point=field_menu_map_position(*data_,state_);map_x_=point[0]-view[0]/2.f;map_y_=point[1]-view[1]/2.f;clamp_map();phase_=Phase::Map;return true;
}
bool FieldMenus::tick_map(int x,int y,bool fast,double dt,std::string&e){if(x<-1||x>1||y<-1||y>1||!std::isfinite(dt)||dt<0||dt>1)return fail(e,"Field map direction/delta rejected");if(phase_!=Phase::Map)return true;map_fraction_+=dt*data_->map_speed*(fast?data_->map_fast:1);auto pixels=std::floor(map_fraction_);map_fraction_-=pixels;map_x_+=x*float(pixels);map_y_+=y*float(pixels);clamp_map();return true;}
bool FieldMenus::apply_choice(std::string&e){
 auto next=state_;if(choice_kind_<3){if(choice_>=data_->setting_labels[choice_kind_].size())return fail(e,"Field setting choice out of range");if(choice_kind_==0)next.session.settings.text_speed=settings_->speeds[choice_];if(choice_kind_==1)next.session.settings.menu_flavor=settings_->flavors[choice_];if(choice_kind_==2)next.session.settings.button_prompts=settings_->prompts[choice_];}
 else if(choice_kind_==3){if(choice_>=data_->locales.size()||!host_.locale(data_->locales[choice_],e))return false;if(!refresh(e))return false;phase_=parent_;selection_=choice_previous_;return true;}
 else if(choice_kind_==4)next.preferences.display=choice_;else return fail(e,"Field setting action rejected");
 if(!commit(std::move(next),e))return false;phase_=parent_;selection_=choice_previous_;return true;
}
bool FieldMenus::input(int x,int y,bool confirm,bool cancel,bool scope,int tab,uint32_t raw,std::string&e){
 if(x<-1||x>1||y<-1||y>1||tab<-1||tab>1)return fail(e,"Field menu direction rejected");if(!active())return true;
 if(phase_==Phase::Capture){
  // D-pad left cancels capture independently of any remappable action.
  if(raw&32){back();return true;}uint32_t mask=0;for(auto b:data_->logical_buttons)if(raw&b){if(mask)return true;mask=b;}if(!mask)return true;
  if(choice_previous_>=state_.preferences.buttons.size())return fail(e,"Field control action out of range");auto next=state_;auto at=std::find(next.preferences.buttons.begin(),next.preferences.buttons.end(),mask);if(at==next.preferences.buttons.end())return fail(e,"Field physical button rejected");std::swap(next.preferences.buttons[choice_previous_],*at);if(!commit(std::move(next),e))return false;phase_=Phase::Controls;selection_=choice_previous_;sounds_.push_back(6);return true;
 }
 if(cancel){back();return true;}
 if(phase_==Phase::Map){if(scope)markers_=!markers_;return true;}
 if(phase_==Phase::Goods||phase_==Phase::SortTarget){
  if(tab&&phase_==Phase::Goods){key_=!key_;selection_=item_selection_=0;sounds_.push_back(3);return true;}
  if(scope&&data_->description_toggle){auto next=state_;next.session.settings.description=!next.session.settings.description;return commit(std::move(next),e);}
  const auto n=uint32_t(goods().size());if(!n)return true;
  if(x||y){selection_=wrap(selection_,y?y*int(data_->columns):x,n);sounds_.push_back(5);}
  if(confirm){if(selection_>=n)return fail(e,"Field selected item is stale");if(phase_==Phase::SortTarget){if(sort_source_>=n)return fail(e,"Field sort source is stale");auto next=state_;std::swap(goods(next)[sort_source_],goods(next)[selection_]);if(!commit(std::move(next),e))return false;phase_=Phase::Goods;item_selection_=selection_;}else{item_selection_=selection_;selection_=0;phase_=Phase::Actions;}sounds_.push_back(6);}return true;
 }
 if(phase_==Phase::Status){if(confirm){phase_=Phase::SkillTabs;selection_=skill_tab_=0;sounds_.push_back(3);}return true;}
 if(phase_==Phase::SkillTabs){if(x||tab){skill_tab_=wrap(skill_tab_,tab?tab:x,3);selection_=skill_tab_;sounds_.push_back(5);}if(confirm&&!skills().empty()){phase_=Phase::Skills;selection_=0;sounds_.push_back(6);}return true;}
 if(phase_==Phase::Skills){if(x||y){selection_=wrap(selection_,y?y*int(data_->columns):x,uint32_t(skills().size()));sounds_.push_back(5);}return true;}
 if(phase_==Phase::Message){if(confirm)back();return true;}
 if(phase_==Phase::Choice){uint32_t count=choice_kind_<3?uint32_t(data_->setting_labels[choice_kind_].size()):choice_kind_==3?uint32_t(data_->locales.size()):2;if(x||y){choice_=wrap(choice_,y?y:x,count);sounds_.push_back(5);}if(confirm){if(!apply_choice(e))return false;sounds_.push_back(6);}return true;}
 auto p=page();const auto n=uint32_t(p.lines.size());if(y||(x&&phase_!=Phase::Audio)){selection_=wrap(selection_,y?y:x,n);sounds_.push_back(5);}
 if(phase_==Phase::Audio&&(x||confirm)){if(selection_>=state_.preferences.volume.size())return fail(e,"Field audio bus rejected");auto next=state_;auto&v=next.preferences.volume[selection_];const int value=std::clamp(int(v)+(x?x:1),0,int(data_->volume_db.size()-1));v=uint32_t(value);if(!commit(std::move(next),e))return false;return true;}
 if(!confirm)return true;sounds_.push_back(6);
 if(phase_==Phase::Actions){auto a=actions();if(selection_>=a.size())return fail(e,"Field item action is stale");action_selection_=selection_;const auto action=a[selection_];const auto*item=selected_item();if(!item)return fail(e,"Field item is absent");
  if(action==1){const auto instance=goods()[item_selection_];if(!host_.equip(item->slot,instance.equipped,instance.uid,e)||!refresh(e))return false;phase_=Phase::Goods;selection_=item_selection_;sounds_.push_back(instance.equipped?8:9);}
  else if(action==2){phase_=Phase::Target;selection_=0;}
  else if(action==3){map_parent_=Phase::Goods;if(!open_map(true,e))return false;}
  else if(action==4){phase_=Phase::Sort;selection_=0;}
  else if(action==5){phase_=Phase::Drop;selection_=1;}else return fail(e,"Field item opcode rejected");return true;
 }
 if(phase_==Phase::Sort){if(selection_==0){sort_source_=item_selection_;selection_=item_selection_;phase_=Phase::SortTarget;}else{auto next=state_;auto&v=goods(next);std::stable_sort(v.begin(),v.end(),[&](const SessionItem&a,const SessionItem&b){const auto*pa=data_->item(a.item_id),*pb=data_->item(b.item_id);const auto sa=a.equipped?pa->equipped_score:pa->score,sb=b.equipped?pb->equipped_score:pb->score;return sa!=sb?sa>sb:pa->sorting.get(state_.chinese)<pb->sorting.get(state_.chinese);});if(!commit(std::move(next),e))return false;phase_=Phase::Goods;selection_=item_selection_=0;}return true;}
 if(phase_==Phase::Drop){if(selection_){back();return true;}const auto*i=selected_item();if(!i||!i->drop||i->key)return fail(e,"Field protected item drop rejected");auto next=state_;auto&v=goods(next);v.erase(v.begin()+item_selection_);const auto selected=v.empty()?0:std::min(item_selection_,uint32_t(v.size()-1));if(!commit(std::move(next),e))return false;phase_=Phase::Goods;selection_=item_selection_=selected;return true;}
 if(phase_==Phase::Target){
  if(data_->has_consume_effects()){
   FieldMenusState next;FieldMenuConsumeResult result;
   if(key_||!consume_field_menu_item(*data_,state_,item_selection_,next,result,e)||!commit(std::move(next),e,true))return false;
   message_=std::move(result.message);if(result.eat_sound)sounds_.push_back(11);
  }else{
   const auto*i=selected_item();if(!i||i->action!=2||i->key||!state_.session.characters[0].status.empty())return fail(e,"Field consumable effect/status is unsupported");std::string result;if(!expand_field_menu_text(i->result.get(state_.chinese),state_.session.characters[0].nickname,std::vector<std::string>(8),result,e))return false;auto next=state_;auto&v=goods(next);if(v[item_selection_].doses<=0||v[item_selection_].doses>i->doses)return fail(e,"Field consumable doses rejected");if(--v[item_selection_].doses==0)v.erase(v.begin()+item_selection_);if(!commit(std::move(next),e))return false;message_=result;
  }
  phase_=Phase::Message;selection_=0;item_selection_=goods().empty()?0:std::min(item_selection_,uint32_t(goods().size()-1));return true;
 }
 if(phase_==Phase::Options){if(selection_<2){phase_=selection_?Phase::General:Phase::File;selection_=0;}else{choice_kind_=selection_;choice_previous_=selection_;parent_=Phase::Options;phase_=Phase::Confirm;selection_=1;}return true;}
 if(phase_==Phase::File){parent_=phase_;choice_kind_=selection_;choice_previous_=selection_;choice_=selection_==0?uint32_t(settings_->speed_index(state_.session.settings.text_speed)):selection_==1?uint32_t(settings_->flavor_index(state_.session.settings.menu_flavor)):uint32_t(settings_->prompt_index(state_.session.settings.button_prompts));phase_=Phase::Choice;return true;}
 if(phase_==Phase::General){if(selection_==3){parent_=phase_;choice_previous_=selection_;choice_kind_=3;choice_=state_.chinese?1:0;phase_=Phase::Choice;}else{phase_=selection_==0?Phase::Audio:selection_==1?Phase::Video:Phase::Controls;selection_=0;}return true;}
 if(phase_==Phase::Video){parent_=phase_;choice_previous_=selection_;choice_kind_=4;choice_=state_.preferences.display;phase_=Phase::Choice;return true;}
 if(phase_==Phase::Controls){choice_previous_=selection_;if(selection_<state_.preferences.buttons.size()){phase_=Phase::Capture;}else{parent_=phase_;phase_=Phase::Confirm;choice_kind_=4;selection_=1;}return true;}
 if(phase_==Phase::Confirm){if(selection_){back();return true;}if(choice_kind_==4){auto next=state_;next.preferences.buttons=data_->defaults.buttons;if(!commit(std::move(next),e))return false;phase_=Phase::Controls;selection_=choice_previous_;}else{exit_=choice_kind_==2?FieldMenusExit::Title:FieldMenusExit::Quit;phase_=Phase::Closed;}return true;}
 return fail(e,"Field submenu opcode rejected");
}
FieldMenuPage FieldMenus::page()const{
 FieldMenuPage p;if(!data_||state_.session.characters.empty())return p;const auto&c=state_.session.characters[0];p.selected=selection_;auto line=[&](Text id,std::string value={}){p.lines.push_back({text(id),std::move(value),false,0});};
 auto description=[&](const SessionItem&i){const auto*r=data_->item(i.item_id);if(r->key)p.description=r->description.get(state_.chinese);else{p.has_item_description=true;p.described_item={i.uid,r->definition,uint32_t(i.equipped),uint32_t(i.doses)};}};
 auto button=[&](size_t i){auto at=std::find(data_->logical_buttons.begin(),data_->logical_buttons.end(),state_.preferences.buttons[i]);return data_->button_names[size_t(at-data_->logical_buttons.begin())];};
 p.footer=button(0)+": "+text(Text::ControlAccept)+"   "+button(1)+": "+text(Text::Back);
 switch(phase_){
 case Phase::Goods:case Phase::SortTarget:{p.title=text(Text::Goods);p.subtitle=key_?text(Text::KeyItems):c.nickname;p.grid=true;for(const auto&i:goods()){const auto*r=data_->item(i.item_id);p.lines.push_back({r->name.get(state_.chinese),r->doses>1?std::to_string(i.doses):std::string{},false,0,i.equipped});}if(p.lines.empty()){p.cursor=false;p.description=text(Text::Empty);}else if(selection_<goods().size()&&(state_.session.settings.description||phase_==Phase::SortTarget))description(goods()[selection_]);if(phase_==Phase::SortTarget)p.title=text(Text::Manual);else p.subtitle+="   "+button(5)+" / "+button(6);break;}
 case Phase::Actions:{p.title=selected_item()->name.get(state_.chinese);for(auto a:actions())line(a==1?(goods()[item_selection_].equipped?Text::Unequip:Text::Equip):a==2||a==3?Text::Use:a==4?Text::Sort:Text::Drop);if(state_.session.settings.description)description(goods()[item_selection_]);break;}
 case Phase::Sort:p.title=text(Text::Sort);line(Text::Manual);line(Text::Auto);break;
 case Phase::Drop:p.title=text(Text::DropConfirm);p.description=selected_item()->name.get(state_.chinese);line(Text::Yes);line(Text::No);break;
 case Phase::Target:p.title=text(Text::Target);p.lines.push_back({c.nickname,{},false,0});break;
 case Phase::Message:p.title=text(Text::Goods);p.description=message_;p.cursor=false;break;
 case Phase::Status:{p.title=text(Text::Status);p.subtitle=c.nickname;p.grid=true;p.cursor=false;line(Text::HP,std::to_string(c.hp)+"/"+std::to_string(state_.stats[0]));line(Text::PP,std::to_string(c.pp)+"/"+std::to_string(state_.stats[1]));line(Text::Level,std::to_string(c.level));line(Text::Exp,std::to_string(c.experience)+"/"+std::to_string(state_.next_exp));for(uint32_t i=2;i<7;++i)line(Text(uint32_t(Text::Offense)+i-2),std::to_string(state_.stats[i]));for(uint32_t slot=0;slot<4;++slot){std::string value=text(Text::None);for(const auto&i:c.inventory)if(i.equipped){const auto*r=data_->item(i.item_id);if(r&&r->action==1&&r->slot==slot)value=r->name.get(state_.chinese);}line(Text(uint32_t(Text::Weapon)+slot),value);}p.description=button(0)+": "+text(Text::Skills);break;}
 case Phase::SkillTabs:case Phase::Skills:{p.title=text(Text::Skills);p.subtitle=text(Text::Field)+" / "+text(Text::Battle)+" / "+text(Text::Psi);if(phase_==Phase::SkillTabs){line(Text::Field);line(Text::Battle);line(Text::Psi);p.selected=skill_tab_;}else{p.grid=true;const auto list=skills();for(const auto*s:list)p.lines.push_back({s->name.get(state_.chinese),s->cost?std::to_string(s->cost):std::string{},!s->weapons.empty()&&std::none_of(c.inventory.begin(),c.inventory.end(),[&](const SessionItem&i){return i.equipped&&std::find(s->weapons.begin(),s->weapons.end(),i.item_id)!=s->weapons.end();}),0});if(selection_<list.size()){std::vector<std::string>buttons;for(size_t i=0;i<state_.preferences.buttons.size();++i)buttons.push_back(button(i));std::string error;if(!expand_field_menu_text(list[selection_]->description.get(state_.chinese),c.nickname,buttons,p.description,error))p.description=error;}}break;}
 case Phase::Map:p.title=text(Text::Map);p.cursor=false;p.description=missing_map_?text(Text::MapMissing):std::string{};p.footer=button(4)+": "+text(Text::MapFast)+"  "+button(3)+": "+text(Text::MapMarkers)+"  "+button(7)+": "+text(Text::MapClose);break;
 case Phase::Options:p.title=text(Text::Options);line(Text::File);line(Text::General);line(Text::Title);line(Text::Quit);break;
 case Phase::File:{p.title=text(Text::File);line(Text::TextSpeed,data_->setting_labels[0][size_t(settings_->speed_index(state_.session.settings.text_speed))].get(state_.chinese));line(Text::Flavor,data_->setting_labels[1][size_t(settings_->flavor_index(state_.session.settings.menu_flavor))].get(state_.chinese));line(Text::Prompts,data_->setting_labels[2][size_t(settings_->prompt_index(state_.session.settings.button_prompts))].get(state_.chinese));break;}
 case Phase::General:p.title=text(Text::General);line(Text::Audio);line(Text::Video);line(Text::Controls);line(Text::Language);break;
 case Phase::Audio:p.title=text(Text::Audio);for(size_t i=0;i<state_.preferences.volume.size();++i)line(Text(uint32_t(Text::Master)+uint32_t(i)),std::to_string(state_.preferences.volume[i]));break;
 case Phase::Video:p.title=text(Text::Video);line(Text::Display,data_->display_names[state_.preferences.display]);p.description=text(Text::PlatformVideo);break;
 case Phase::Controls:p.title=text(Text::Controls);for(size_t i=0;i<state_.preferences.buttons.size();++i)line(Text(uint32_t(Text::ControlAccept)+uint32_t(i)),button(i));line(Text::Reset);break;
 case Phase::Capture:p.title=text(Text::Controls);p.description=text(Text::Capture)+"\n"+data_->capture_cancel.get(state_.chinese);p.cursor=false;break;
 case Phase::Choice:{p.title=choice_kind_<3?text(Text(uint32_t(Text::TextSpeed)+choice_kind_)):choice_kind_==3?text(Text::Language):text(Text::Display);p.selected=choice_;if(choice_kind_<3)for(const auto&l:data_->setting_labels[choice_kind_])p.lines.push_back({l.get(state_.chinese),{},false,0});else if(choice_kind_==3)for(const auto&l:data_->locale_names)p.lines.push_back({l.get(state_.chinese),{},false,0});else for(const auto&l:data_->display_names)p.lines.push_back({l,{},false,0});break;}
 case Phase::Confirm:p.title=text(choice_kind_==4?Text::Reset:choice_kind_==2?Text::Title:Text::Quit);p.description=text(choice_kind_==4?Text::ResetConfirm:choice_kind_==2?Text::TitleConfirm:Text::QuitConfirm);line(Text::Yes);line(Text::No);break;
 default:break;
 }
 const auto capacity=data_->rows*(p.grid?data_->columns:1);p.offset=capacity?p.selected/capacity*capacity:0;if(capacity&&p.lines.size()>capacity)p.footer+="  "+std::to_string(p.offset/capacity+1)+"/"+std::to_string((p.lines.size()+capacity-1)/capacity);return p;
}
}
