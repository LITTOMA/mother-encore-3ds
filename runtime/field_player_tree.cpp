#include "encore/field_player_tree.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
void clear_motion(SwingMotionFlags&f,uint32_t mask){if(mask&1)f.crouch=false;if(mask&2)f.idle=false;if(mask&4)f.tap_run=false;if(mask&8)f.running=false;if(mask&16)f.walking=false;}
bool skill(std::string_view s){return s.size()<=64&&std::all_of(s.begin(),s.end(),[](unsigned char c){return c>=32&&c<127;});}
}
float FieldPlayerTree::position()const{
 if(!data_)return 0;
 if(state_==data_->attack())return player_.position();
 const auto*c=data_->select(state_,facing_);
 // The discrete-key helper visits end keys before the tree wraps its clock.
 return c&&c->clip.loop&&playback_.position==c->clip.length?0:playback_.position;
}
bool FieldPlayerTree::initialize(const FieldSwingEntryData&d,const FieldSwingActionData&a,const FieldSwingData&s,uint32_t state,Vec2 facing,float position,std::string_view selected,std::string&e){
 if(d.capability()!=2||!d.bind(a,s,e))return fail(e,"Player tree needs the complete checked graph");
 const auto*c=d.select(state,facing);if(!c||!std::isfinite(position)||position<0||position>c->clip.length||!skill(selected))return fail(e,"Player tree initial snapshot rejected");
 FieldPlayerTree next;next.data_=&d;next.action_=&a;next.swing_=&s;next.state_=state;next.facing_=facing;next.selected_=std::string(selected);
 if(!next.player_.initialize(s,e))return false;
 next.pose_=s.initial();next.playback_.sample={0,next.pose_.main,next.pose_.special};
 if(!begin_clip(c->clip,d.frames(),next.playback_)||!advance_clip(c->clip,position,d.frames(),next.playback_))return fail(e,"Player tree initial clip rejected");
 next.pose_.main=next.playback_.sample.main_visible;next.pose_.special=next.playback_.sample.special_visible;*this=std::move(next);e.clear();return true;
}
bool FieldPlayerTree::travel(uint32_t target,std::string&e){if(!data_||!data_->route(state_,target))return fail(e,"Player tree travel target rejected");target_=target==state_?0:target;e.clear();return true;}
bool FieldPlayerTree::input(std::string_view name,bool pressed,const SwingActionContext&c,SwingMotionFlags&flags,bool&accepted,std::string&e){
 if(!data_)return fail(e,"Player tree input owner unprepared");
 const auto mask=action_->policy()[1];const bool blocked=((mask&1)&&(paused_||c.paused))||((mask&2)&&(!c.moving||attack_))||((mask&4)&&c.entering_door)||((mask&8)&&c.climbing)||((mask&16)&&c.spinning);
 if(name!=action_->strings()[2]||!pressed||blocked){accepted=false;e.clear();return true;}
 if((mask&32)&&!c.skill_available){selected_.clear();accepted=false;e.clear();return true;}
 const auto*clip=swing_->direction(c.direction);if(!clip||c.direction.x!=facing_.x||c.direction.y!=facing_.y||starts_==UINT32_MAX)return fail(e,"Player tree facing/counter rejected");
 if(!player_.set_incapacitated(c.incapacitated,e))return false;
 incap_=c.incapacitated;attack_=true;target_=state_==data_->attack()?0:data_->attack();selected_=action_->strings()[3];++starts_;clear_motion(flags,action_->policy()[2]);accepted=true;e.clear();return true;
}
bool FieldPlayerTree::pause(bool stop_running,bool start_idle,bool incap,SwingMotionFlags&flags,std::string&e){
 if(!data_)return fail(e,"Player tree pause owner unprepared");
 if(start_idle){const auto target=incap?data_->incapacitated_idle():data_->idle();target_=target==state_?0:target;}
 if(!paused_state_&&data_->state(state_)->pausable)paused_state_=state_;
 attack_=false;paused_=true;flags.crouch=false;flags.walking=false;if(stop_running||!flags.tap_run){flags.tap_run=false;flags.running=false;}
 e.clear();return true;
}
bool FieldPlayerTree::unpause(std::string&e){if(!data_)return fail(e,"Player tree resume owner unprepared");paused_state_=0;paused_=false;attack_=false;e.clear();return true;}
bool FieldPlayerTree::advance(double delta,std::string&e){
 if(!data_||!std::isfinite(delta)||delta<0||delta>60)return fail(e,"Player tree clock rejected");
 const auto*state=data_->state(state_);bool ended=false;
 if(state_==data_->attack()){
  if(!player_.advance(delta,e))return false;
  pose_=player_.pose();ended=!player_.playing();
 }else{
  const auto*clip=data_->select(state_,facing_);if(!clip)return fail(e,"Player tree normal clip lost");
  if(new_node_){playback_.sample.main_visible=pose_.main;playback_.sample.special_visible=pose_.special;if(!begin_clip(clip->clip,data_->frames(),playback_))return fail(e,"Player tree new clip rejected");new_node_=false;}
  if(!advance_clip(clip->clip,state_==paused_state_?0:float(delta),data_->frames(),playback_))return fail(e,"Player tree frame advance rejected");
  pose_.main=playback_.sample.main_visible;pose_.special=playback_.sample.special_visible;ended=playback_.position>=clip->clip.length;
  if(clip->clip.loop&&ended)playback_.position=0;
 }
 const auto destination=target_?target_:state->automatic;
 if(destination&&destination!=state_){
  const auto*route=data_->route(state_,destination);if(!route)return fail(e,"Player tree route lost");
  if(route->mode==0||ended){const bool from_bat=state_==data_->attack();const auto next=route->next;
   if(next==data_->attack()){const auto*c=swing_->direction(facing_);if(!c||!player_.play(c->id,incap_,e))return false;}
   state_=next;playback_.position=0;new_node_=next!=data_->attack();if(next==destination)target_=0;
   // At Bat's end the source evaluates the immediate destination's reset pose
   // in this pass; ordinary incoming paths keep the outgoing pose until next.
   if(from_bat&&new_node_){const auto*c=data_->select(next,facing_);playback_.sample.main_visible=pose_.main;playback_.sample.special_visible=pose_.special;if(!c||!begin_clip(c->clip,data_->frames(),playback_))return fail(e,"Player tree end pose lost");pose_.main=playback_.sample.main_visible;pose_.special=playback_.sample.special_visible;new_node_=false;}
  }
 }
 e.clear();return true;
}
bool FieldPlayerTree::flush(SwingMotionFlags&flags,std::string&e){if(!data_)return fail(e,"Player tree callback owner unprepared");const bool pending=player_.pending_callbacks()!=0;if(!player_.flush(e))return false;if(pending){pose_.hitbox=false;attack_=false;clear_motion(flags,action_->policy()[3]);}e.clear();return true;}
}
