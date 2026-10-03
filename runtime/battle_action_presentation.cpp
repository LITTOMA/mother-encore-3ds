#include "encore/battle_action_presentation.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cinttypes>
namespace encore::upstream {
namespace {
BattleValue lerp(BattleValue a,BattleValue b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
 }
float interpolation(float t,RoundInterpolation mode){
 const float pi=std::acos(-1.f);
 t=std::clamp(t,0.f,1.f);
 
 switch(mode){case RoundInterpolation::QuartOut:return 1-std::pow(1-t,4);
 case RoundInterpolation::QuartIn:return t*t*t*t;
 
 case RoundInterpolation::QuadOut:return 1-(1-t)*(1-t);
 case RoundInterpolation::QuadIn:return t*t;
 case RoundInterpolation::CircIn:return 1-std::sqrt(1-t*t);
 
 case RoundInterpolation::ElasticOut:return t==0||t==1?t:std::pow(2,-10*t)*std::sin((t-.075f)*(2*pi)/.3f)+1;
 
 case RoundInterpolation::SineOut:return std::sin(t*pi/2);
 default:return t;
 }
}
float cubic(float pre,float a,float b,float post,float t){return .5f*((2*a)+(-pre+b)*t+(2*pre-5*a+4*b-post)*t*t+(-pre+3*a-3*b+post)*t*t*t);
 }
}
bool BattleHpRoll::begin(RoundView c,int32_t hp,int32_t max){
 content_=c;
 const auto p=c.parameter(RoundParameter::HpDigits);
 radix_=int32_t(p.x);
 frames_=int32_t(p.y);
 maximum_=max;
 
 if(!c.valid()||radix_<2||frames_<2||max<0||hp<0||hp>max)return false;
 
 timer_=0;
 scrolling_=done_=false;
 instant(hp);
 return true;
 
}
void BattleHpRoll::instant(int32_t hp){current_=target_=hp;
 frame_=0;
 int32_t n=hp;
 for(auto&d:digits_){d=uint32_t(n%radix_*frames_);
 n/=radix_;
 }}
bool BattleHpRoll::set_instant(int32_t value){if(!content_.valid()||value<0||value>maximum_)return false;
 instant(value);scrolling_=false;done_=false;return true;
}
int32_t BattleHpRoll::stop_scrolling(){if(!content_.valid())return 0;scrolling_=false;instant(current_);done_=true;return current_;}
bool BattleHpRoll::set_target(int32_t hp){if(!content_.valid())return false;
 target_=std::clamp<int32_t>(hp,0,maximum_);
 increasing_=target_>current_;
 scrolling_=true;
 done_=false;
 return true;
 }
bool BattleHpRoll::digit_visible(uint32_t p)const{if(p>=digits_.size())return false;
 if(p==2)return digits_[2]!=0;
 if(p==1)return digits_[2]!=0||digits_[1]!=0;
 return true;
 }
bool BattleHpRoll::idle_frame(double dt,bool defending,bool fast){
 if(!std::isfinite(dt)||dt<0)return false;
 if(!scrolling_)return true;
 
 double speed=content_.rule(RoundRule::HpBaseSpeed);
 if(!increasing_&&defending)speed*=content_.rule(RoundRule::HpDefendingMultiplier);
 if(fast)speed*=content_.rule(RoundRule::HpFastMultiplier);
 
 const double quantum=content_.rule(RoundRule::HpFrameSeconds)/speed;
 if(!(quantum>0))return false;
 timer_+=dt;
 
 while(timer_>=quantum){timer_-=quantum;
 if(frame_==0&&current_==target_){timer_=0;
 scrolling_=false;
 done_=true;
 instant(target_);
 break;
 }
 const int increment=increasing_?1:-1;
 frame_=(frame_+increment+frames_)%frames_;
 if((increasing_&&frame_==0)||(!increasing_&&frame_==frames_-1))current_+=increment;
 
 const int ones=current_%radix_,tens=current_/radix_%radix_,hundreds=current_/radix_/radix_%radix_;
 digits_[0]=ones*frames_+frame_;
 
 if(ones==radix_-1||(ones==0&&frame_==0)){digits_[1]=tens*frames_+frame_;
 if(tens==radix_-1||(ones==0&&frame_==0))digits_[2]=hundreds*frames_+frame_;
 }
 }return true;
 
}
bool BattleTextPacer::begin(RoundView c,std::string_view s,bool auto_advance){content_=c;
 size_t count=0,cursor=0;uint32_t cp=0;while(cursor<s.size()){if(!encore::utf8_next(s,cursor,cp))return false;if(cp!=10)++count;}if(count>1024*1024)return false;characters_=uint32_t(count);
 auto_advance_=auto_advance;
 text_=std::string(s);
 visible_=0;
 elapsed_=0;
 remaining_=0;
 multiplier_=1;
 finished_=timer_=false;
 done_=false;
 empty_deferred_=s.empty();
 return c.valid();
 }
std::string BattleTextPacer::visible_text()const{std::string out;size_t cursor=0,count=0;uint32_t cp=0;while(cursor<text_.size()){const auto start=cursor;if(!encore::utf8_next(text_,cursor,cp))return{};if(cp!=10&&count>=visible_)break;if(cp!=10)++count;out+=text_.substr(start,cursor-start);}return out;}
bool BattleTextPacer::set_text_speed(double seconds){
 if(!std::isfinite(seconds)||seconds<=0)return false;
 text_seconds_=seconds;return true;
}
double BattleTextPacer::text_speed()const{
 return text_seconds_>0?text_seconds_:content_.valid()?content_.rule(RoundRule::TextSecondsPerChar):0;
}
bool BattleTextPacer::physics_frame(double dt){
 if(!std::isfinite(dt)||dt<0)return false;
 if(done_||finished_||empty_deferred_)return true;
 
 
 const double period=text_speed()/multiplier_;
 if(!std::isfinite(period)||!(period>0))return false;
 elapsed_+=dt;
 
 while(elapsed_>period&&!finished_){++visible_;
 if(visible_>characters_){finished_=true;
 timer_=auto_advance_;
 remaining_=content_.rule(RoundRule::TextAutoAdvanceSeconds);
 elapsed_=0;
 }elapsed_-=period;
 }
 return true;
 
}
bool BattleTextPacer::idle_frame(double dt){if(!std::isfinite(dt)||dt<0)return false;
 if(empty_deferred_){empty_deferred_=false;
 done_=true;
 return true;
 }if(timer_&&!done_){remaining_-=dt;
 if(remaining_<0){timer_=false;
 done_=true;
 }}return true;
 }
void BattleTextPacer::input(bool accept,bool cancel){if(done_||(!accept&&!cancel))return;
 if(!finished_){
 multiplier_=content_.rule(cancel?RoundRule::TextCancelMultiplier:RoundRule::TextAcceptMultiplier);
 }else done_=true;
 timer_=false;
 }
bool BattleActionPresentation::fail(const char*why){error_=why;
 return false;
 }
bool BattleActionPresentation::set_text_speed(double seconds){
 if(!content_.valid()||!text_.set_text_speed(seconds))return fail("Invalid battle dialogue text speed");
 return true;
}
BattleActionPresentation::Clip BattleActionPresentation::clip(uint32_t id,Vec2 origin,Vec2 input,std::string text)const{Clip c;
 c.media=id;
 c.origin=origin;
 c.input=input;
 c.text=std::move(text);
 c.active=id<content_.count(RoundSection::Media);
 return c;
 }
BattleActionPresentation::Clip BattleActionPresentation::slot(RoundPresentationSlot s,Vec2 o,Vec2 input,std::string t)const{return clip(content_.presentation(s),o,input,std::move(t));
 }
bool BattleActionPresentation::begin(RoundView c,BattleView e,SourceRandom&r,const BattleSessionStats*session){
 if(!c.valid()||!e.valid())return fail("Presentation requires checked source resources");
 content_=c;
 entry_=e;
 random_=&r;
 const auto b=c.binding();
 player_=b.player_participant;
 enemy_=b.enemy_participant;
 
 actors_.assign(e.count(BattleSection::Participants),{});
 if(player_>=actors_.size()||enemy_>=actors_.size())return fail("Presentation participant binding invalid");
 
 for(uint32_t i=0;i<actors_.size();++i){actors_[i].hp=e.participant(i).hp;
 actors_[i].base.visible=true;
 actors_[i].base.color={1,1,1,1};
 }
 actors_[player_].animation=slot(RoundPresentationSlot::PartyIdle);
 actors_[enemy_].animation=slot(RoundPresentationSlot::EnemyFlash);
 actors_[enemy_].animation.time=c.media(actors_[enemy_].animation.media).duration;
 
 auto player=e.participant(player_);if(session&&!apply_session_stats(player,*session))return fail("Invalid presentation session");
 player_maxhp_=player.maxhp;player_pp_=player.pp;actors_[player_].hp=player.hp;
 if(!hp_.begin(c,player.hp,player.maxhp))return fail("Invalid rolling HP data");
 
 text_=BattleTextPacer{};
 if(!text_.set_text_speed(c.rule(RoundRule::TextSecondsPerChar)))return fail("Invalid checked battle text speed");
 effects_.clear();
 pointer_={};
 dialogue_cursor_=slot(RoundPresentationSlot::DialogueCursor);
 background_={};
 quake_={};
 victory_banner_={};return_timeline_={};return_party_={};return_events_.clear();
 boss_shake_={};boss_flash_={};boss_defeat_=false;boss_time_=0;next_boss_shake_=0;
 victory_remaining_=0;victory_started_=return_started_=return_jump_requested_=false;
 battle_background_visible_=enemies_visible_=true;
 commands_=true;
 target_open_=false;
 slowmo_=false;
 time_scale_=1;
 error_="";
 return true;
 
}
bool BattleActionPresentation::set_actor_base(uint32_t i,const BattlePose&p){
 if(i>=actors_.size())return fail("Unknown actor base");
 auto&a=actors_[i];a.base=p;
 if(i!=player_)return true;
 // Commands becomes active when show_in starts, not when its tween finishes.
 // Carry the observed entry progress into that same source motion, while the
 // actor's permanent base is the external track's final shown position.
 for(uint32_t ti=0;ti<entry_.count(BattleSection::Tracks);++ti){
  const auto track=entry_.track(ti);
  if(track.clock!=uint32_t(BattleClock::PartyShow)||track.property!=uint32_t(BattleProperty::PositionY)||entry_.layout(track.target).role!=uint32_t(BattleRole::PartySprite))continue;
  if(track.count!=2||track.update!=0)return fail("Unreviewed entry show tween");
  const auto first=entry_.key(track.first),last=entry_.key(track.first+track.count-1);
  if(first.ease!=1||first.time!=0||last.time<=first.time||first.value.x==last.value.x)return fail("Unreviewed entry show interpolation");
  const auto layout=entry_.layout(track.target);
  const float centered=(layout.flags&2)?p.rect.w/2:0;
  const float from=first.value.x-centered,to=last.value.x-centered;
  const float progress=std::clamp((p.rect.y-from)/(to-from),0.f,1.f);
  a.base.rect.y=to;
  a.motion=slot(RoundPresentationSlot::PartyShow,{p.rect.x,from},{1,to-from});
  a.motion.time=content_.media(a.motion.media).duration*progress;
  a.hidden=false;
  return true;
 }
 return fail("Missing source entry show tween");
}
bool BattleActionPresentation::set_plate_base(BattleValue p){plate_=p;
 return true;
 }
BattleActionPose BattleActionPresentation::sample(uint32_t id,double seconds,Vec2 origin,Vec2 input)const{
 BattleActionPose p;
 if(id>=content_.count(RoundSection::Media)){p.visible=false;
 return p;
 }const auto m=content_.media(id);
 p.media=id;
 p.resource=m.resource;
 p.role=m.role;
 p.rect=m.rect;
 p.color=m.color;
 p.anchor=m.anchor;
 p.centered=(m.flags&2)!=0;
 p.rect.x+=origin.x;
 p.rect.y+=origin.y;
 
 float t=float(std::max(seconds,0.));
 if((m.flags&1)&&m.duration>0)t=std::fmod(t,m.duration);
 else t=std::min(t,m.duration);
 
 for(uint32_t i=0;i<m.track_count;++i){const auto tr=content_.track(m.first_track+i);
 if(!tr.count)continue;
 const auto first=content_.key(tr.first);
 if(t<first.time)continue;
 
 uint32_t k=0;
 // Godot discrete value tracks apply keys over the half-open playback
 // interval; an exact key boundary keeps the preceding value for one tick.
 while(k+1<tr.count&&(tr.update?content_.key(tr.first+k+1).time<t:content_.key(tr.first+k+1).time<=t))++k;
 auto a=content_.key(tr.first+k);
 BattleValue v=a.value;
 
 if(!tr.update&&k+1<tr.count){const auto b=content_.key(tr.first+k+1);
 float f=(t-a.time)/(b.time-a.time);
 f=battle_ease(f,a.ease);
 f=interpolation(f,RoundInterpolation(tr.interpolation));
 
 if(RoundInterpolation(tr.interpolation)==RoundInterpolation::Cubic){const auto before=content_.key(tr.first+(k?k-1:k)).value,after=content_.key(tr.first+std::min(k+2,tr.count-1)).value;
 v={cubic(before.x,a.value.x,b.value.x,after.x,f),cubic(before.y,a.value.y,b.value.y,after.y,f),cubic(before.z,a.value.z,b.value.z,after.z,f),cubic(before.w,a.value.w,b.value.w,after.w,f)};
 }else v=lerp(a.value,b.value,f);
 }
 const auto prop=RoundProperty(tr.property);
 const auto mode=RoundTrackMode(tr.mode);
 
 if(mode==RoundTrackMode::ScaleInput){if(prop==RoundProperty::PositionX)v.x=v.x*input.x+v.y*input.y+origin.x;
 else if(prop==RoundProperty::PositionY)v.x=v.x*input.y+v.y*input.x+origin.y;
 else{v.x*=input.x;
 v.y*=input.y;
 }}
 else if(mode==RoundTrackMode::AddOrigin){if(prop==RoundProperty::PositionX)v.x+=origin.x;
 else if(prop==RoundProperty::PositionY)v.x+=origin.y;
 else{v.x+=origin.x;
 v.y+=origin.y;
 }}
 switch(prop){case RoundProperty::Position:p.rect.x=v.x;
 p.rect.y=v.y;
 break;
 case RoundProperty::PositionX:p.rect.x=v.x;
 break;
 case RoundProperty::PositionY:p.rect.y=v.x;
 break;
 
 case RoundProperty::Radius:p.radius=v.x;break;
 case RoundProperty::Scale:p.scale={v.x,v.y};
 break;
 case RoundProperty::Alpha:p.color.w=v.x;
 break;
 case RoundProperty::Frame:p.frame=uint32_t(v.x);
 break;
 case RoundProperty::Visible:p.visible=v.x!=0;
 break;
 
 case RoundProperty::Color:p.color=v;
 break;
 case RoundProperty::FlashColor:p.flash_color=v;
 break;
 case RoundProperty::FlashModifier:p.flash_modifier=v.x;
 break;
 case RoundProperty::Offset:p.offset={v.x,v.y};
 break;
 
 case RoundProperty::Rotation:p.rotation=v.x;
 break;
 case RoundProperty::GlowColor:p.glow_color=v;
 break;
 case RoundProperty::GlowModifier:p.glow_modifier=v.x;
 break;
 case RoundProperty::Modulate:p.modulate=v;
 break;
 
 case RoundProperty::Size:p.rect.z=v.x;
 p.rect.w=v.y;
 break;
 case RoundProperty::OutlineWidth:break;
 }
 }
 if((m.flags&4)&&seconds>=m.duration)p.visible=false;
 return p;
 
}
void BattleActionPresentation::advance(Clip&c,double dt,Actor*a){if(!c.active)return;
 const auto m=content_.media(c.media);
 const double old=c.time;
 c.time+=dt;
 
 if(a)for(uint32_t i=0;i<m.event_count;++i){const auto ev=content_.event(m.first_event+i);
 if(old<ev.time&&c.time>=ev.time&&RoundEventKind(ev.kind)==RoundEventKind::ApplyDamage)a->apply_damage=true;
 }
}
void BattleActionPresentation::start_hide(Actor&a){const auto current=actor_pose(player_);
 const float target=a.base.rect.y+content_.parameter(RoundParameter::PartyShown).x;
 a.motion=slot(RoundPresentationSlot::PartyHide,{current.rect.x,current.rect.y},{1,target-current.rect.y});
 a.hidden=true;
 a.hide_pending=false;
 }
void BattleActionPresentation::start_show(Actor&a){const auto current=actor_pose(player_);
 if(a.hidden){a.motion=slot(RoundPresentationSlot::PartyShow,{current.rect.x,current.rect.y},{1,a.base.rect.y-current.rect.y});
 a.hidden=false;
 }a.hide_pending=false;
 }
Vec2 BattleActionPresentation::effect_position(uint32_t actor,bool top)const{if(actor==player_)return {plate_.x+plate_.z/2,plate_.y+(top?0:plate_.w/2)};
 const auto p=actor_pose(actor);
 return {p.rect.x+p.rect.z/2,p.rect.y+p.rect.w/2};
 }
bool BattleActionPresentation::emit(const BattleRoundCue&cue,SourceRandom&r){
 if(!content_.valid()||&r!=random_)return fail("Presentation must use the shared source RNG");
 const auto actor=cue.actor,target=cue.target;
 
 switch(cue.kind){
 case BattleRoundCueKind::ShowMenu:commands_=true;
 target_open_=false;
 if(actor<actors_.size()){auto&a=actors_[actor];
 a.defending=false;
 start_show(a);
 a.animation=slot(RoundPresentationSlot::PartyIdle);
 }break;
 
 case BattleRoundCueKind::TargetOpen:{commands_=false;
 target_open_=true;
 if(target>=actors_.size())return fail("Target pointer binding invalid");
 const auto b=actors_[target].base.rect;
 const auto m=content_.media(content_.presentation(RoundPresentationSlot::TargetPointer));
 const auto div=content_.parameter(RoundParameter::TargetPointerOffset).x;
 pointer_=slot(RoundPresentationSlot::TargetPointer,{b.x-m.rect.z/div,b.y-m.rect.w/div});
 background_=slot(RoundPresentationSlot::BackgroundDim);
 break;
 }
 case BattleRoundCueKind::TargetClose:target_open_=false;
 pointer_.active=false;
 background_=slot(RoundPresentationSlot::BackgroundUndim);
 break;
 
 case BattleRoundCueKind::PrepareAction:commands_=false;
 if(actor>=actors_.size())return fail("Prepare actor invalid");
 actors_[actor].animation=slot(cue.skill<content_.count(RoundSection::Skills)&&(content_.skill(cue.skill).traits&uint32_t(RoundTrait::Guard))?RoundPresentationSlot::PartyGuardPrepare:RoundPresentationSlot::PartyPrepare);
 break;
 
 case BattleRoundCueKind::Guard:if(actor>=actors_.size())return fail("Guard actor invalid");
 actors_[actor].defending=true;
 actors_[actor].animation=slot(RoundPresentationSlot::PartyGuardPrepare);
 break;
 
 case BattleRoundCueKind::TurnStart:if(actor>=actors_.size())return fail("Turn actor invalid");
 if(actor==enemy_)actors_[actor].animation=slot(RoundPresentationSlot::EnemyFlash);
 break;
 
 case BattleRoundCueKind::Dialogue:if(cue.text==round_no_index)return text_.begin(content_,{});
 if(cue.text>=content_.count(RoundSection::Texts))return fail("Dialogue record invalid");
 {std::string resolved;if(!resolve_text(cue.text,resolved))return fail("Source battle text resolver rejected record");return text_.begin(content_,resolved);}
 
 case BattleRoundCueKind::Attack:if(actor>=actors_.size()||cue.skill>=content_.count(RoundSection::Skills))return fail("Attack binding invalid");
 actors_[actor].apply_damage=false;
 if(actor==player_)actors_[actor].animation=clip(content_.skill(cue.skill).user_media);
 else actors_[actor].motion=slot(RoundPresentationSlot::EnemyAttack,{actors_[actor].base.rect.x,actors_[actor].base.rect.y});
 break;
 
 case BattleRoundCueKind::Hit:{if(target>=actors_.size()||cue.skill>=content_.count(RoundSection::Skills))return fail("Hit binding invalid");
 auto&a=actors_[target];
 a.hp=cue.hp_after;
 
 if(cue.smash){background_=slot(RoundPresentationSlot::SmashBackground);
 const auto o=content_.parameter(RoundParameter::SmashOffset);
 auto pos=effect_position(target,true);
 pos.x+=o.x;
 pos.y+=o.y;
 effects_.push_back(slot(RoundPresentationSlot::Smash,pos));
 effects_.back().target=target;
 slowmo_=true;
 slowmo_elapsed_=0;
 time_scale_=content_.rule(RoundRule::SmashTimeScale);
 }
 char buffer[32];
 std::snprintf(buffer,sizeof(buffer),"%" PRId32,cue.amount);
 std::string number=buffer;
 if(cue.adrenaline)number+='!';
 
 const auto ran=content_.parameter(RoundParameter::FlyingNumberRandom);
 double distance=r.rand_range(ran.x,ran.y);
 if(r.randi()%2==1)distance=-distance;
 
 auto pos=effect_position(target,true);
 const auto fm=content_.media(content_.presentation(RoundPresentationSlot::FlyingNumber));
 const auto grid=content_.parameter(RoundParameter::DamageGlyphGrid);
 const float width=std::max(fm.rect.z,float(number.size())*grid.w);
 pos.x-=width/2;
 pos.y-=fm.rect.w/2;
 effects_.push_back(slot(RoundPresentationSlot::FlyingNumber,pos,{float(distance),1},number));
 effects_.back().target=target;
 
 const auto skill=content_.skill(cue.skill);
 if(skill.hit_media!=round_no_index){effects_.push_back(clip(skill.hit_media,effect_position(target,false)));effects_.back().target=target;}
 
 if(target==enemy_){a.animation=slot(RoundPresentationSlot::EnemyHit);
 }else if(target==player_){hp_.set_target(cue.hp_after);
 const auto hit=content_.parameter(RoundParameter::PartyHit);
 const auto intensity=content_.parameter(RoundParameter::PlateHitIntensity);
 const auto current=actor_pose(target);
 
 if(a.defending){a.animation=slot(RoundPresentationSlot::PartyGuard);
 }else if(cue.amount>player_maxhp_/hit.x){const float scale=std::min(float(cue.amount)/(player_maxhp_/hit.y),hit.z);
 const auto b=content_.parameter(RoundParameter::PartyBounceMotion);
 const auto shown=content_.parameter(RoundParameter::PartyShown);
 a.restore_media=a.animation.media;
 
 const int selection=int(std::round(r.rand_range(1,hit.w)));
 RoundPresentationSlot hit_slot=selection==1?RoundPresentationSlot::PartyHit:selection==2?RoundPresentationSlot::PartyHit2:RoundPresentationSlot::PartyHit3;
 a.animation=slot(hit_slot);
 a.motion=slot(RoundPresentationSlot::PartyBounce,{current.rect.x,current.rect.y},{a.base.rect.y+shown.x-current.rect.y,(a.hidden?shown.x:0)+b.x*scale+b.y});
 quake_=slot(RoundPresentationSlot::PlateQuake,{0,0},{1,intensity.y});
 
 }else{a.motion=slot(RoundPresentationSlot::PartyShake,{current.rect.x,current.rect.y});
 quake_=slot(RoundPresentationSlot::PlateQuake,{0,0},{1,intensity.x});
 }}
 break;
 }
 case BattleRoundCueKind::Miss:{if(target>=actors_.size())return fail("Miss actor invalid");
 const auto fm=content_.parameter(RoundParameter::RisingNumberSize);
 auto pos=effect_position(target,true);
 pos.x-=fm.x/2;
 pos.y-=fm.y/2;
 effects_.push_back(slot(RoundPresentationSlot::RisingNumber,pos));
 effects_.back().target=target;
 const float sign=r.randi()%2==1?-1.f:1.f;
 const auto p=actor_pose(target);
 actors_[target].motion=slot(target==player_?RoundPresentationSlot::PartyDodge:RoundPresentationSlot::EnemyDodge,{p.rect.x,p.rect.y},{sign,1});
 break;
 }
 case BattleRoundCueKind::HideParty:if(actor>=actors_.size())return fail("Hide actor invalid");
 actors_[actor].hide_pending=true;
 break;
 
 case BattleRoundCueKind::Defeat:if(target>=actors_.size())return fail("Defeat actor invalid");
 actors_[target].dead=true;
 if(target==enemy_){actors_[target].animation=slot(RoundPresentationSlot::EnemyDefeat);
  if(content_.encounter().boss){boss_defeat_=true;boss_time_=0;next_boss_shake_=0;hp_.stop_scrolling();commands_=target_open_=false;}
 }
 else actors_[target].animation=slot(RoundPresentationSlot::PartyIdle);
 break;
 
 }
 return true;
 
}
bool BattleActionPresentation::refresh_session(const BattleSessionStats&s){
 auto player=entry_.participant(player_);if(!apply_session_stats(player,s))return fail("Invalid outcome session refresh");
 player_maxhp_=s.maxhp;player_pp_=s.pp;actors_[player_].hp=s.hp;return hp_.begin(content_,s.hp,s.maxhp);
}
bool BattleActionPresentation::begin_victory(){
 if(!content_.valid()||player_>=actors_.size()||victory_started_||return_started_)return fail("Invalid victory presentation state");
 commands_=target_open_=false;pointer_.active=false;
 auto&a=actors_[player_];start_show(a);a.hide_pending=false;a.restore_media=round_no_index;
 a.animation=slot(RoundPresentationSlot::PartyVictory);
 victory_banner_=slot(RoundPresentationSlot::VictoryBanner);
 victory_remaining_=double(float(content_.rule(RoundRule::VictoryBannerSeconds)));
 victory_started_=true;
 return true;
}
bool BattleActionPresentation::resolve_text(uint32_t index,std::string&value)const{
 if(index>=content_.count(RoundSection::Texts))return false;
 if(text_resolver_)return text_resolver_(text_context_,content_,index,value);
 value=std::string(content_.string(content_.text(index).text));return true;
}
bool BattleActionPresentation::begin_outcome_text(uint32_t index){
 std::string value;if(!resolve_text(index,value))return fail("Source outcome text resolver rejected record");return begin_outcome_text(value);
}
bool BattleActionPresentation::begin_outcome_text(std::string_view value){
 if(!victory_done()||return_started_)return fail("Outcome text before victory gate");
 victory_banner_.active=false;return text_.begin(content_,value,false);
}
bool BattleActionPresentation::begin_return(){
 if(!victory_done()||!text_.done()||return_started_)return fail("Return before acknowledged outcome");
 victory_banner_.active=false;commands_=target_open_=false;pointer_.active=false;
 return_timeline_=slot(RoundPresentationSlot::ReturnTimeline);return_timeline_.time=return_frame_delta_;return_party_={};
 return_events_.clear();return_started_=true;return true;
}
bool BattleActionPresentation::begin_party_return(Vec2 destination,Vec2 expansion){
 if(!return_started_||!return_jump_requested_||return_party_.active||!std::isfinite(destination.x)||!std::isfinite(destination.y)||!std::isfinite(expansion.x)||!std::isfinite(expansion.y))return fail("Invalid return party snapshot");
 const auto offset=content_.parameter(RoundParameter::ReturnPartyGeometry);
 const Vec2 origin{plate_.x+plate_.z/2+expansion.x/2,plate_.y+return_plate_offset()+expansion.y};
 destination.x+=offset.x;destination.y+=offset.y;
 return_party_=slot(RoundPresentationSlot::PartyJumpToWorld,origin,{destination.x-origin.x,destination.y-origin.y});
 return_party_.time=return_frame_delta_;
 return true;
}
bool BattleActionPresentation::return_done()const{return return_started_&&return_timeline_.time>=content_.media(return_timeline_.media).duration;}
std::vector<RoundEventKind> BattleActionPresentation::take_return_events(){auto result=std::move(return_events_);return_events_.clear();return result;}
float BattleActionPresentation::return_plate_offset()const{
 if(!return_started_)return 0;
 const auto id=content_.presentation(RoundPresentationSlot::ReturnPlate);
 return sample(id,return_timeline_.time).rect.y-content_.media(id).rect.y;
}
BattleActionPose BattleActionPresentation::return_party_pose()const{
 if(!return_party_.active){BattleActionPose p;p.visible=false;return p;}
 auto p=sample(return_party_.media,return_party_.time,return_party_.origin,return_party_.input);p.anchor={0,0};return p;
}
std::vector<BattleActionPose> BattleActionPresentation::return_overlays()const{
 std::vector<BattleActionPose>result;if(!return_started_)return result;
 result.push_back(sample(content_.presentation(RoundPresentationSlot::ReturnTop),return_timeline_.time));
 result.push_back(sample(content_.presentation(RoundPresentationSlot::ReturnBottom),return_timeline_.time));
 return result;
}
bool BattleActionPresentation::ready(BattleRoundGate gate,uint32_t actor)const{if(gate==BattleRoundGate::DialogueDone)return text_.done();
 if(gate==BattleRoundGate::BossDefeatDone)return boss_defeat_&&boss_flash_.active&&boss_flash_.time>=content_.media(boss_flash_.media).duration;
 return actor<actors_.size()&&actors_[actor].apply_damage;
 }
int32_t BattleActionPresentation::current_hp(uint32_t actor)const{return actor==player_?hp_.current_hp():actor<actors_.size()?actors_[actor].hp:0;
 }
bool BattleActionPresentation::physics_frame(double dt){
 if(!std::isfinite(dt)||dt<0)return fail("Invalid physics delta");
 if(boss_shake_.source!=round_no_index){auto&q=boss_shake_;const auto spec=content_.boss_shake(q.source);
  if(q.left>0){q.timer+=dt;if(q.timer>=spec.interval){--q.left;q.timer-=spec.interval;Vec2 next=q.direction;
   do {next={float(std::round(random_->rand_range(-1,1))),float(std::round(random_->rand_range(-1,1)))};}while(next.x==q.direction.x&&next.y==q.direction.y);
   q.direction=next;const Vec2 goal=q.left>1?Vec2{next.x*spec.magnitude,next.y*spec.magnitude}:Vec2{};
   q.offset={q.offset.x+(goal.x-q.offset.x)*spec.weight,q.offset.y+(goal.y-q.offset.y)*spec.weight};
  }}else q={};
 }
 return text_.physics_frame(dt);
 }
double BattleActionPresentation::advance_real_time(double dt){if(!std::isfinite(dt)||dt<0){fail("Invalid real delta");
 return time_scale_;
 }if(!slowmo_)return time_scale_=1;
 slowmo_elapsed_+=dt;
 const double duration=content_.rule(RoundRule::SmashRealSeconds),initial=content_.rule(RoundRule::SmashTimeScale);
 if(slowmo_elapsed_>=duration){slowmo_=false;
 return time_scale_=1;
 }const double fraction=std::floor(slowmo_elapsed_*1000)/1000/duration;
 return time_scale_=1-std::sqrt(1-fraction*fraction)+initial;
 }
bool BattleActionPresentation::idle_frame(double dt){
 if(!std::isfinite(dt)||dt<0)return fail("Invalid presentation delta");
 if(boss_defeat_){
  const double before=boss_time_;boss_time_=double(float(float(boss_time_)+float(dt)));
  while(next_boss_shake_<content_.count(RoundSection::BossShakes)&&content_.boss_shake(next_boss_shake_).time<boss_time_){
   const auto spec=content_.boss_shake(next_boss_shake_);boss_shake_={};boss_shake_.source=next_boss_shake_++;boss_shake_.left=int32_t(double(spec.length)/double(spec.interval));
  }
  const auto defeat=content_.media(content_.presentation(RoundPresentationSlot::EnemyDefeat));
  for(uint32_t i=0;i<defeat.event_count;++i){const auto ev=content_.event(defeat.first_event+i);
   if(before<=ev.time&&boss_time_>ev.time&&ev.kind==uint32_t(RoundEventKind::BossFlashStart))boss_flash_=clip(content_.encounter().boss_flash_media,effect_position(enemy_,false));
  }
  if(boss_flash_.active){const double old=boss_flash_.time;advance(boss_flash_,dt);boss_flash_.time=double(float(boss_flash_.time));const auto media=content_.media(boss_flash_.media);
   for(uint32_t i=0;i<media.event_count;++i){const auto ev=content_.event(media.first_event+i);if(old<=ev.time&&boss_flash_.time>ev.time&&ev.kind==uint32_t(RoundEventKind::BossKillEnemies))enemies_visible_=false;}
  }
 }
 if(victory_started_&&victory_remaining_>=0)victory_remaining_=double(float(float(victory_remaining_)-float(dt)));
 if(!text_.idle_frame(dt)||!hp_.idle_frame(dt,actors_[player_].defending,false))return fail("Presentation clock failed");
 
 for(uint32_t i=0;i<actors_.size();++i){auto&a=actors_[i];
 advance(a.animation,dt,&a);
 if(boss_defeat_&&i==enemy_)a.animation.time=boss_time_;
 advance(a.motion,dt,&a);
 
 if(a.hide_pending&&!a.hidden&&(!a.animation.active||a.animation.time>=content_.media(a.animation.media).duration))start_hide(a);
 
 if(a.restore_media!=round_no_index&&a.motion.active&&a.motion.time>=content_.media(a.motion.media).duration){a.animation=clip(a.restore_media);
 a.animation.time=content_.media(a.restore_media).duration;
 a.restore_media=round_no_index;
 if(!a.hidden){a.hidden=true;start_show(a);}
 }
 }
 // Return AnimationPlayer/Tween accumulate float32 engine delta in a double
 // playback cursor. Newly created scene tweens consume this same idle pass.
 return_frame_delta_=double(float(dt));
 if(return_party_.active){const double before=return_party_.time;advance(return_party_,return_frame_delta_);
  const auto media=content_.media(return_party_.media);
  for(uint32_t i=0;i<media.event_count;++i){const auto event=content_.event(media.first_event+i);
   if(before<event.time&&return_party_.time>=event.time)return_events_.push_back(RoundEventKind(event.kind));
  }
 }
 if(return_started_){const double before=return_timeline_.time;
  return_timeline_.time=std::min(double(content_.media(return_timeline_.media).duration),double(float(float(before)+float(return_frame_delta_))));
  const auto media=content_.media(return_timeline_.media);
  for(uint32_t i=0;i<media.event_count;++i){const auto event=content_.event(media.first_event+i);
   if(before<=event.time&&return_timeline_.time>event.time){const auto kind=RoundEventKind(event.kind);return_events_.push_back(kind);
    if(kind==RoundEventKind::TurnPartyToWorld){auto&a=actors_[player_];
     if(!a.hidden&&(!a.animation.active||a.animation.time>=content_.media(a.animation.media).duration)){start_hide(a);advance(a.motion,return_frame_delta_);}
     else a.hide_pending=true;
    }
    else if(kind==RoundEventKind::HideBattleBackground)battle_background_visible_=false;
    else if(kind==RoundEventKind::HideEnemies)enemies_visible_=false;
    else if(kind==RoundEventKind::JumpPartyToWorld)return_jump_requested_=true;
   }
  }
 }
 advance(victory_banner_,dt);
 advance(pointer_,dt);
 advance(dialogue_cursor_,dt);
 advance(background_,dt);
 advance(quake_,dt);
 for(auto&effect:effects_)advance(effect,dt);
 effects_.erase(std::remove_if(effects_.begin(),effects_.end(),[this](const Clip&c){return c.time>=content_.media(c.media).duration;}),effects_.end());
 return true;
 
}
BattleActionPose BattleActionPresentation::actor_pose(uint32_t i)const{
 BattleActionPose p;
 if(i>=actors_.size()){p.visible=false;
 return p;
 }const auto&a=actors_[i];
 const Vec2 base{a.base.rect.x,a.base.rect.y};
 p=sample(a.animation.media,a.animation.time,base);
 p.rect.z=a.base.rect.z;
 p.rect.w=a.base.rect.w;
 p.visible=p.visible&&a.base.visible&&(i!=enemy_||enemies_visible_);
 
 if(a.motion.active){const auto motion=sample(a.motion.media,a.motion.time,a.motion.origin,a.motion.input);
 p.rect.x=motion.rect.x;
 p.rect.y=motion.rect.y;
 p.offset.x+=motion.offset.x;
 p.offset.y+=motion.offset.y;
 p.scale.x*=motion.scale.x;
 p.scale.y*=motion.scale.y;
 }
 if(i==enemy_){p.offset.x+=boss_shake_.offset.x;p.offset.y+=boss_shake_.offset.y;}
 if(i==player_)p.rect.y+=return_plate_offset();
 if(target_open_&&i==enemy_){const auto g=content_.parameter(RoundParameter::TargetGlow);
 p.glow_color={g.x,g.y,g.z,1};
 p.glow_modifier=g.w;
 }
 return p;
 
}
float BattleActionPresentation::plate_offset()const{return quake_.active?sample(quake_.media,quake_.time,quake_.origin,quake_.input).rect.y:0;
 }
std::vector<BattleActionPose> BattleActionPresentation::overlays()const{
 std::vector<BattleActionPose>result;
 auto append=[&](const Clip&c){if(!c.active)return;
 auto p=sample(c.media,c.time,c.origin,c.input);
 p.text=c.text;
 if(c.target<actors_.size())p.anchor=actor_pose(c.target).anchor;
 if(p.visible)result.push_back(std::move(p));
 };
 append(background_);
 append(pointer_);
 for(const auto&c:effects_)append(c);
 
 if(target_open_){auto box=sample(content_.presentation(RoundPresentationSlot::TargetNameBox),0);
 result.push_back(box);
 auto label=sample(content_.presentation(RoundPresentationSlot::TargetNameText),0);
 if(!resolve_text(content_.binding().enemy_name,label.text))label.visible=false;
 result.push_back(label);
 }
 if(victory_banner_.active){result.push_back(sample(content_.presentation(RoundPresentationSlot::Dialogue),0));append(victory_banner_);}
 if(!text_.done()&&!victory_banner_.active){result.push_back(sample(content_.presentation(RoundPresentationSlot::Dialogue),0));
 auto p=sample(content_.presentation(RoundPresentationSlot::DialogueText),0);
 p.text=text_.visible_text();
 if(text_.finished())append(dialogue_cursor_);
 result.push_back(p);
 }if(boss_flash_.active){auto flash=sample(boss_flash_.media,boss_flash_.time);flash.offset=boss_flash_.origin;result.push_back(flash);}
 return result;
 
}
}
