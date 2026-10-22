#include "encore/breakables.hpp"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool finite(BreakablePoint p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1e6&&std::abs(p.y)<=1e6;}
bool flag(const std::map<std::string,bool>&flags,const std::string&key){const auto p=flags.find(key);return p!=flags.end()&&p->second;}
// Godot 3.6.2 real_t easing subset. Copyright (c) Godot contributors,
// Juan Linietsky, Ariel Manzur and Robert Penner. MIT notices are retained in
// docs/licenses/Godot-3.6.2-LICENSE.txt and docs/licenses/Penner-easing-MIT.txt.
// These are curve equations; offsets and durations come from checked content.
float cubic(float t,float b,float c,float d){t=t/d-1;return c*(t*t*t+1)+b;}
float bounce(float t,float b,float c,float d){
 t/=d;
 if(t<(1/2.75f))return c*(7.5625f*t*t)+b;
 if(t<(2/2.75f)){t-=1.5f/2.75f;return c*(7.5625f*t*t+0.75f)+b;}
 if(t<(2.5/2.75)){t-=2.25f/2.75f;return c*(7.5625f*t*t+0.9375f)+b;}
 t-=2.625f/2.75f;return c*(7.5625f*t*t+0.984375f)+b;
}
bool valid_content(const BreakableContentState&c){if(!finite(c.position)||!finite(c.start)||c.shapes.size()>256||c.interactions==UINT32_MAX)return false;for(const auto&s:c.shapes)if(!s.first)return false;return true;}
void commit_flags(std::map<std::string,bool>&live,std::map<std::string,bool>&staged){
 for(auto it=staged.begin();it!=staged.end();){auto found=live.find(it->first);if(found!=live.end()){found->second=it->second;++it;}else{auto next=std::next(it);live.insert(staged.extract(it));it=next;}}
}
void commit_services(BreakableServices&live,BreakableServices&&staged){
 // Preserve real content/shape handles and unrelated prepared storage. New
 // flag nodes were allocated during staging; node transfer cannot allocate.
 live.paused=staged.paused;commit_flags(live.story_flags,staged.story_flags);commit_flags(live.object_flags,staged.object_flags);
 for(auto&row:live.contents){auto&from=staged.contents.at(row.first);auto&to=row.second;
  to.visible=from.visible;to.physics=from.physics;to.changing=from.changing;to.position=from.position;to.start=from.start;to.interactions=from.interactions;
  if(to.parent!=from.parent)to.parent=std::move(from.parent);
  for(auto&shape:to.shapes)shape.second=from.shapes.at(shape.first);
 }
 if(live.requests.size()!=staged.requests.size())live.requests.swap(staged.requests);
}
}
BreakableContentState*BreakableActor::content(BreakableServices&s)const{const auto p=s.contents.find(content_path_);return p==s.contents.end()?nullptr:&p->second;}
bool BreakableActor::context(uint64_t epoch,const BreakableServices&s,std::string&e)const{
 if(!valid_||!epoch||epoch!=epoch_||s.epoch!=epoch||s.scene!=scene_||s.requests.size()>4096)return fail(e,"Breakable owner/scene epoch");
 if(!s.parents.count(owner_.parent)||!s.parents.count(owner_.drop_parent)||(!owner_.new_parent.empty()&&!s.parents.count(owner_.new_parent))||(!owner_.spawner.empty()&&!s.factories.count(owner_.spawner))||!s.sounds.count(policy_.sound))return fail(e,"Breakable prepared parent/factory/voice unavailable");
 if(!content_path_.empty()){const auto p=s.contents.find(content_path_);if(p==s.contents.end()||!valid_content(p->second))return fail(e,"Breakable content service binding");
  if(drop_==Drop::Timer){if(!p->second.present||p->second.shapes.size()!=captured_shapes_.size())return fail(e,"Breakable pending content lifecycle changed");for(auto id:captured_shapes_)if(!p->second.shapes.count(id))return fail(e,"Breakable captured shape handle retired");}}
 return true;
}
bool BreakableActor::request(BreakableServices&s,BreakableRequestKind kind,uint32_t identity,std::array<double,4>value,std::string&e){if(s.requests.size()>=4096)return fail(e,"Breakable prepared request capacity");s.requests.push_back({kind,epoch_,frame_,owner_.id,identity,value});return true;}
bool BreakableActor::prepare(const BreakablesData&d,uint32_t owner,uint64_t epoch,BreakableServices&s,std::string&e){
 const auto*o=d.owner(owner);if(!d.valid()||!o||!epoch||s.epoch!=epoch||s.scene!=d.scene()||s.requests.size()>4096)return fail(e,"Breakable prepare source/owner/epoch");
 BreakableActor v;BreakableServices staged=s;v.epoch_=epoch;v.scene_=d.scene();v.policy_=d.policy();v.owner_=*o;v.family_=*d.family(o->family);v.parent_=o->parent;v.visible_=(o->appear.empty()||flag(staged.story_flags,o->appear))&&(o->disappear.empty()||!flag(staged.story_flags,o->disappear));v.removed_=!v.visible_;v.body_layer_=v.family_.body_layer;v.hit_layer_=v.family_.hit_layer;v.hit_mask_=v.family_.hit_mask;v.body_disabled_=!v.family_.clips.initial().body;
 if(o->content!=UINT32_MAX){v.content_path_=d.contents()[o->content].path;v.content_parent_=d.contents()[o->content].parent;auto*c=v.content(staged);if(!c||!valid_content(*c))return fail(e,"Breakable content snapshot required before ready");if(!v.visible_&&o->drop_hidden){if(!c->present||c->parent!=v.content_parent_)return fail(e,"Breakable hidden-drop source null dereference");c->position=o->position;}}
 else if(!v.visible_&&o->drop_hidden)return fail(e,"Breakable hidden-drop source has no item");
 if(!v.animation_.initialize(v.family_.clips,e))return false;v.valid_=true;if(!v.context(epoch,staged,e))return false;*this=std::move(v);commit_services(s,std::move(staged));e.clear();return true;
}
bool BreakableActor::hit(uint64_t epoch,BreakablePoint emitter,BreakableServices&s,std::string&e){
 if(!context(epoch,s,e)||!finite(emitter))return fail(e,"Breakable hit context/position");
 if(removed_||hit_started_)return fail(e,"Breakable unadmitted removed/reentrant source callback");
 BreakableActor v=*this;BreakableServices staged=s;
 if(owner_.pause){staged.paused=true;if(!v.request(staged,BreakableRequestKind::Pause,0,{},e))return false;}
 const float x=owner_.position.x-emitter.x,y=owner_.position.y-emitter.y;const float distance=std::sqrt(x*x+y*y);if(!std::isfinite(distance))return fail(e,"Breakable hit distance overflow");v.hit_left_=float(std::min(policy_.maximum_delay,policy_.distance_delay*double(distance)));v.hit_pending_=true;v.hit_started_=true;*this=std::move(v);commit_services(s,std::move(staged));e.clear();return true;
}
bool BreakableActor::set_flag(bool value,BreakableServices&s,std::string&e){
 auto&flags=owner_.object_flag?s.object_flags:s.story_flags;const auto p=flags.find(owner_.flag);if(!owner_.object_flag&&p==flags.end())return true;flags[owner_.flag]=value;return !owner_.emit_flag||request(s,BreakableRequestKind::FlagsUpdated,0,{},e);
}
bool BreakableActor::start_break(BreakableServices&s,std::string&e){
 if(!request(s,BreakableRequestKind::Rumble,policy_.device,{policy_.weak,policy_.strong,policy_.rumble_seconds,0},e)||!animation_.play(family_.break_clip,e))return false;
 hit_layer_&=~policy_.clear_hit_layer;hit_mask_&=~policy_.clear_hit_mask;body_layer_&=~policy_.clear_body_layer;body_disabled_=true;
 auto*c=content(s);
 if(c&&c->present&&c->parent==content_parent_){const BreakablePoint point{owner_.position.x+family_.drop_offset.x,owner_.position.y+family_.drop_offset.y};if(c->has_start)c->start=point;if(c->has_changing)c->changing=true;captured_shapes_.clear();for(auto&shape:c->shapes){captured_shapes_.push_back(shape.first);shape.second=true;}c->parent=owner_.drop_parent;c->position=point;if(owner_.hide_drop)c->visible=false;drop_left_=family_.drop_seconds;drop_=Drop::Timer;if(!request(s,BreakableRequestKind::ContentReentered,owner_.content,{},e))return false;}
 const auto&flags=owner_.object_flag?s.object_flags:s.story_flags;
 if(!owner_.spawner.empty()&&owner_.coins&&!flag(flags,owner_.flag)){if(!set_flag(true,s,e)||!request(s,BreakableRequestKind::Coins,0,{double(owner_.coins),0,0,0},e))return false;}
 return true;
}
bool BreakableActor::tick(float delta,BreakableServices&s,std::string&e){
 if(removed_){++frame_;return true;}
 const bool was_playing=animation_.playing();const auto old=float(animation_.position());
 if(!animation_.advance(delta,e))return false;
 if(was_playing){const float next=float(animation_.position());const bool done=!animation_.playing();std::vector<const BreakableFamily::AudioKey*>keys;for(const auto&k:family_.audio)if(k.clip==animation_.assigned_clip())keys.push_back(&k);
  if(next==old){const BreakableFamily::AudioKey*chosen=nullptr;for(auto*k:keys)if(k->time<=next)chosen=k;if(keys.size()==1)chosen=keys.front();if(chosen&&!request(s,BreakableRequestKind::Audio,policy_.sound,{double(chosen->value),double(family_.spatial_audio),0,0},e))return false;while(audio_cursor_<keys.size()&&keys[audio_cursor_]->time<=next)++audio_cursor_;}
  else{while(audio_cursor_<keys.size()){const auto&k=*keys[audio_cursor_];if(k.time>next||(!done&&k.time==next))break;if(!request(s,BreakableRequestKind::Audio,policy_.sound,{double(k.value),double(family_.spatial_audio),0,0},e))return false;++audio_cursor_;}}
  body_disabled_=!animation_.pose().body;
  if(done){const auto completed=animation_.take_completed();if(completed.size()!=1||completed[0]!=family_.break_clip)return fail(e,"Breakable unexpected animation continuation");if(!owner_.new_parent.empty()&&parent_!=owner_.new_parent)parent_=owner_.new_parent;}
 }
 if(drop_==Drop::Timer){drop_left_-=double(delta);if(drop_left_<0){auto*c=content(s);if(!c||!c->present)return fail(e,"Breakable drop target retired");c->physics=false;for(auto id:captured_shapes_)c->shapes[id]=false;if(c->has_changing)c->changing=false;if(owner_.interact_content&&c->has_interact){if(!request(s,BreakableRequestKind::Interact,0,{},e))return false;++c->interactions;}
   if(owner_.tween_drop){c->visible=true;tween_start_=c->position;tween_elapsed_=0;drop_=Drop::Lift;}else{c->physics=true;drop_=Drop::Done;}}}
 // SceneTreeTimer processing follows internal AnimationPlayer/Timer nodes.
 if(hit_pending_){hit_left_=float(hit_left_-delta);if(hit_left_<0){hit_pending_=false;if(!start_break(s,e))return false;}}
 // A newly created first tween runs this frame; a bounce created by a
 // completed tween starts next frame (SceneTree::process_tweens last entry).
 if(drop_==Drop::Lift||drop_==Drop::Fall){auto*c=content(s);if(!c||!c->present)return fail(e,"Breakable tween target retired");const bool lift=drop_==Drop::Lift;const float duration=float(lift?policy_.lift_seconds:policy_.fall_seconds);const float offset=float(lift?policy_.lift:policy_.fall);tween_elapsed_=float(float(tween_elapsed_)+delta);const float time=std::min(float(tween_elapsed_),duration);const float final=float(double(tween_start_.y)+double(offset));const float change=final-tween_start_.y;c->position.x=tween_start_.x;
  if(time<duration)c->position.y=lift?cubic(time,tween_start_.y,change,duration):bounce(time,tween_start_.y,change,duration);
  else{c->position.y=final;if(lift){tween_start_=c->position;tween_elapsed_=0;drop_=Drop::Fall;c->physics=true;}else drop_=Drop::Done;}}
 ++frame_;return true;
}
bool BreakableActor::advance_idle(uint64_t epoch,double delta,BreakableServices&s,std::string&e){
 if(!context(epoch,s,e)||!std::isfinite(delta)||delta<0||delta>60||frame_==UINT64_MAX)return fail(e,"Breakable idle context/delta");
 // Untouched/finished scenery performs no copies or allocations per frame.
 if(removed_||(!animation_.playing()&&!hit_pending_&&(drop_==Drop::None||drop_==Drop::Done))){++frame_;e.clear();return true;}
 BreakableActor v=*this;BreakableServices staged=s;if(!v.tick(float(delta),staged,e))return false;*this=std::move(v);commit_services(s,std::move(staged));e.clear();return true;
}
bool BreakableActor::leave(uint64_t epoch,bool region,BreakableServices&s,std::string&e){
 if(!context(epoch,s,e))return false;BreakableActor v=*this;BreakableServices staged=s;if((owner_.reset_area||(region&&owner_.reset_region))&&!v.set_flag(false,staged,e))return false;*this=std::move(v);commit_services(s,std::move(staged));e.clear();return true;
}
}
