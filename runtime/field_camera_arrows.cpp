#include "encore/field_camera_arrows.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
namespace encore::upstream {namespace {
bool finite(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<1000000&&std::abs(v.y)<1000000;}
bool equal(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool approximate(float a,float b){return a==b||std::abs(a-b)<std::max(0.00001f,0.00001f*std::abs(a));}
int find(const std::vector<FieldArrowKey>&k,float t){int low=0,high=int(k.size())-1,m=0;while(low<=high){m=(low+high)/2;if(approximate(t,k[m].time))return m;if(t<k[m].time)high=m-1;else low=m+1;}if(k[m].time>t)--m;return m;}
float eased(float x,float c){double a=std::clamp(double(x),0.0,1.0),b=c;if(b>0)return float(b<1?1-std::pow(1-a,1/b):std::pow(a,b));if(b<0)return float(a<.5?std::pow(a*2,-b)*.5:(1-std::pow(1-(a-.5)*2,-b))*.5+.5);return 0;}
Vec2 transform(const std::array<Vec2,3>&m,Vec2 p){return {m[0].x*p.x+m[1].x*p.y+m[2].x,m[0].y*p.x+m[1].y*p.y+m[2].y};}
}
bool FieldCameraArrowsRuntime::fail(const char*s){error_=s;poisoned_=true;return false;}
FieldArrowRootState*FieldCameraArrowsRuntime::get(uint32_t id,bool ready){auto i=roots_.find(id);if(poisoned_||!data_||i==roots_.end()||!i->second.alive||(ready&&!i->second.ready)){fail("MapArrows lifetime/Ready rejected");return nullptr;}if(native_&&!native_->root(id,i->second.visible,i->second.position,error_)){poisoned_=true;return nullptr;}return &i->second;}
const FieldArrowRootState*FieldCameraArrowsRuntime::root_state(uint32_t id)const{auto i=roots_.find(id);return i==roots_.end()?nullptr:&i->second;}
const FieldArrowSpriteState*FieldCameraArrowsRuntime::sprite_state(uint32_t id)const{auto i=sprites_.find(id);return i==sprites_.end()?nullptr:&i->second;}
const FieldArrowPlayerState*FieldCameraArrowsRuntime::player_state(uint32_t id)const{auto i=players_.find(id);return i==players_.end()?nullptr:&i->second;}
bool FieldCameraArrowsRuntime::observe(uint32_t id,FieldArrowObservation&o){if(!host_.observe(id,o,error_)){poisoned_=true;return false;}if(!o.alive||!o.ancestors_admitted||!o.listeners_admitted)return fail("MapArrows live source ancestor/signal admission rejected");return true;}
bool FieldCameraArrowsRuntime::publish(FieldArrowRootState&s){if(!finite(s.position)||!finite(s.offset)||!finite(s.bounds))return fail("MapArrows root state overflow rejected");if(!host_.publish_root(s.id,s,error_)){poisoned_=true;return false;}return true;}
bool FieldCameraArrowsRuntime::publish(FieldArrowSpriteState&s){if(!finite(s.position)||!finite(s.offset)||!std::isfinite(s.timeout))return fail("MapArrows sprite state overflow rejected");if(!host_.publish_sprite(s.id,s,error_)){poisoned_=true;return false;}return true;}
bool FieldCameraArrowsRuntime::initialize(const FieldCameraArrowsData&d,FieldCameraArrowsHost h,std::string&e){if(native_){e="MapArrows borrowed owner cannot be replaced";return false;}if(!d.valid()||!h.bind||!h.observe||!h.connect_animation_finished||!h.publish_root||!h.publish_sprite||!h.animation_signal||!h.sprite_signal||!h.await_frame_changed||!h.cancel_frame_changed||!h.control_directions){e="MapArrows complete source host required";return false;}if(!h.bind(d,e))return false;data_=&d;host_=std::move(h);roots_.clear();sprites_.clear();players_.clear();waits_.clear();next_wait_=1;last_ready_=0;had_ready_=false;poisoned_=false;error_.clear();e.clear();return true;}
bool FieldCameraArrowsRuntime::initialize_borrowed(const FieldCameraArrowsData&d,FieldCameraArrowsHost h,FieldCameraArrowsNativeOwner&native,std::string&e){
 if(data_||native_||!native.bind(d,e))return false;
 if(!initialize(d,std::move(h),e))return false;
 native_=&native;return true;
}
float FieldCameraArrowsRuntime::duration(uint32_t id)const{auto*s=data_->sprite(id);return float(1.0/double(s->animation_speed*s->speed_scale));}
bool FieldCameraArrowsRuntime::create(uint32_t id) {
  return create_body(id, true);
}
bool FieldCameraArrowsRuntime::create_source_constructor(uint32_t id) {
  return create_body(id, false);
}
bool FieldCameraArrowsRuntime::create_body(uint32_t id, bool publish_native) {
  if (!data_ || poisoned_ || roots_.count(id))
    return fail("MapArrows create rejected");
  auto *d = data_->record(id);
  if (!d)
    return fail("MapArrows source root rejected");
  FieldArrowRootState r;
  r.id = id;
  r.visible = d->visible;
  r.position = d->position;
  roots_.emplace(id, r);
  if (native_)
    return !publish_native || publish(roots_.at(id));
  for (auto aid : d->arrows) {
    auto *a = data_->sprite(aid);
    FieldArrowSpriteState s;
    s.id = aid;
    s.position = a->position;
    s.offset = a->offset;
    s.frame = a->frame;
    s.visible = a->flags & 1;
    s.playing = a->flags & 2;
    s.timeout = s.playing ? duration(aid) : 0;
    sprites_.emplace(aid, s);
    if (publish_native && !publish(sprites_.at(aid)))
      return false;
  }
  for (const auto &p : data_->players()) {
    auto *a = data_->sprite(p.target_root);
    if (p.target_root != id && (!a || a->root_id != id))
      continue;
    FieldArrowPlayerState s;
    s.id = p.id;
    players_.emplace(p.id, s);
  }
  return !publish_native || publish(roots_.at(id));
}
bool FieldCameraArrowsRuntime::ready(uint32_t id) {
  auto *r = get(id, false);
  if (!r)
    return false;
  auto *d = data_->record(id);
  if (r->ready || (had_ready_ && d->ready <= last_ready_))
    return fail("MapArrows source Ready order rejected");
  FieldArrowObservation o;
  if (!observe(id, o) || !o.descendants_ready)
    return fail("MapArrows source native child Ready missing");
  r->ready = true;
  // Original Dictionary.values order UP,DOWN,LEFT,RIGHT. Connections are made
  // here during source _ready, not earlier while the factory tree is created.
  for (auto aid : d->arrows) {
    auto *p = data_->player_for_target(aid);
    if (!host_.connect_animation_finished(
            p->id,
            [this, id = aid](uint32_t role) {
              return on_animation_finished(id, role);
            },
            error_)) {
      poisoned_ = true;
      return false;
    }
  }
  r->show_arrows = r->visible;
  had_ready_ = true;
  last_ready_ = d->ready;
  return refresh(*r, true);
}
bool FieldCameraArrowsRuntime::play(uint32_t id,uint32_t role,float scale,bool from_end){auto i=players_.find(id);auto*d=data_->player(id);if(poisoned_||!d||(!native_&&(i==players_.end()||!i->second.alive))||!std::isfinite(scale)||scale<0||scale>1024)return fail("MapArrows play rejected");auto*c=data_->clip(d->profile,role);if(!c)return fail("MapArrows clip rejected");if(native_)return native_->play(id,c->name,scale,from_end,error_)||(poisoned_=true,false);auto&s=i->second;if(s.assigned!=role)s.time=from_end?c->length:0;else if(from_end&&s.time==0)s.time=c->length;else if(!from_end&&s.time==c->length)s.time=0;s.assigned=role;s.scale=scale;s.playing=true;if(!host_.animation_signal(id,true,role,error_)){poisoned_=true;return false;}return true;}
bool FieldCameraArrowsRuntime::refresh(FieldArrowRootState&r,bool instant){auto*p=data_->player_for_target(r.id);if(!play(p->id,r.show_arrows?1:2,instant?0:1,instant))return false;return publish(r);}
bool FieldCameraArrowsRuntime::show(uint32_t id){auto*r=get(id);if(!r)return false;if(!r->show_arrows){r->show_arrows=true;return refresh(*r,r->visible);}return true;}
bool FieldCameraArrowsRuntime::hide(uint32_t id){auto*r=get(id);if(!r)return false;if(r->show_arrows){r->show_arrows=false;return refresh(*r,!r->visible);}return true;}
bool FieldCameraArrowsRuntime::directions(uint32_t id,const std::vector<Vec2>&directions,uint32_t role){auto*r=get(id);if(!r)return false;auto*d=data_->record(id);for(auto dir:directions){if(!finite(dir))return fail("MapArrows nonfinite direction rejected");bool found=false;for(size_t i=0;i<4;++i)if(equal(dir,data_->directions()[i])){found=true;auto*p=data_->player_for_target(d->arrows[i]);if(native_){std::string assigned;if(!native_->assigned(p->id,assigned,error_))return false;auto*clip=data_->clip(p->profile,role);if(!clip)return fail("MapArrows native assigned role unknown");if(assigned!=clip->name&&!play(p->id,role))return false;}else if(players_.at(p->id).assigned!=role&&!play(p->id,role))return false;break;}if(!found)return fail("MapArrows unknown Dictionary direction rejected");}return true;}
bool FieldCameraArrowsRuntime::point_directions(uint32_t id,const std::vector<Vec2>&v){return directions(id,v,1);}
bool FieldCameraArrowsRuntime::unpoint_directions(uint32_t id,const std::vector<Vec2>&v){return directions(id,v,2);}
bool FieldCameraArrowsRuntime::handle_input_events(uint32_t id){auto*r=get(id);if(!r)return false;std::vector<Vec2>pressed,released;if(!host_.control_directions(pressed,released,error_)){poisoned_=true;return false;}if(pressed.size()>10000||released.size()>10000)return fail("MapArrows input array budget rejected");return point_directions(id,pressed)&&unpoint_directions(id,released);}
bool FieldCameraArrowsRuntime::point_dir_sum(uint32_t id,Vec2 sum){if(!get(id)||!finite(sum))return fail("MapArrows direction sum rejected");auto sign=[](float v){return float((v>0)-(v<0));};Vec2 x{sign(sum.x),0},y{0,sign(sum.y)};for(auto dir:data_->directions()){bool point=equal(dir,x)||equal(dir,y);if(point?!point_directions(id,{dir}):!unpoint_directions(id,{dir}))return false;}return true;}
bool FieldCameraArrowsRuntime::visibility(FieldArrowRootState&r){auto*d=data_->record(r.id);bool bounded=r.bounds.x!=0||r.bounds.y!=0;for(size_t i=0;i<4;++i){auto dir=data_->directions()[i];bool show=bounded&&(dir.x<0?r.offset.x> -r.bounds.x:dir.x>0?r.offset.x<r.bounds.x:dir.y<0?r.offset.y> -r.bounds.y:r.offset.y<r.bounds.y);if(native_){if(!native_->visible(d->arrows[i],show,error_))return fail("MapArrows native visibility write rejected");}else{auto&s=sprites_.at(d->arrows[i]);s.visible=show;if(!publish(s))return false;}}return publish(r);}
bool FieldCameraArrowsRuntime::set_offset(uint32_t id,Vec2 v){auto*r=get(id);if(!r||!finite(v))return fail("MapArrows offset rejected");r->offset=v;return visibility(*r);}
bool FieldCameraArrowsRuntime::set_bounds(uint32_t id,Vec2 v){auto*r=get(id);if(!r||!finite(v))return fail("MapArrows bounds rejected");r->bounds=v;return visibility(*r);}
bool FieldCameraArrowsRuntime::set_arrow_visible(uint32_t id,Vec2 dir,bool visible){auto*r=get(id);if(!r)return false;auto*d=data_->record(id);for(size_t i=0;i<4;++i)if(equal(dir,data_->directions()[i])){if(native_)return native_->visible(d->arrows[i],visible,error_);auto&s=sprites_.at(d->arrows[i]);s.visible=visible;return publish(s);}return fail("MapArrows visibility Dictionary direction rejected");}
bool FieldCameraArrowsRuntime::global_position(uint32_t id,Vec2 v){auto*r=get(id);if(!r||!finite(v))return fail("MapArrows global position rejected");FieldArrowObservation o;if(!observe(id,o))return false;for(auto p:o.parent)if(!finite(p))return fail("MapArrows parent transform overflow rejected");auto&m=o.parent;float det=m[0].x*m[1].y-m[0].y*m[1].x;if(!std::isfinite(det)||det==0)return fail("MapArrows singular actual parent rejected");Vec2 a{v.x-m[2].x,v.y-m[2].y};r->position={(m[1].y*a.x-m[1].x*a.y)/det,(-m[0].y*a.x+m[0].x*a.y)/det};return publish(*r);}
bool FieldCameraArrowsRuntime::frame(uint32_t id,int32_t value){if(native_)return native_->frame(id,value,error_);auto i=sprites_.find(id);if(i==sprites_.end()||!i->second.alive)return fail("MapArrows frame lifetime rejected");auto&s=i->second;uint32_t v=uint32_t(std::clamp<int32_t>(value,0,int32_t(data_->frames().size()-1)));if(s.frame==v)return true;s.frame=v;if(s.playing){s.timeout=duration(id);s.is_over=false;}if(!publish(s)||!host_.sprite_signal(id,FieldArrowSignal::FrameChanged,error_)){poisoned_=true;return false;}return true;}
bool FieldCameraArrowsRuntime::playing(uint32_t id,bool value){auto i=sprites_.find(id);if(i==sprites_.end()||!i->second.alive)return fail("MapArrows playing lifetime rejected");auto&s=i->second;if(value){s.is_over=false;if(!s.playing){s.playing=true;s.timeout=duration(id);}}else if(s.playing)s.playing=false;return publish(s);}
bool FieldCameraArrowsRuntime::on_animation_finished(uint32_t arrow,uint32_t role){auto*a=data_?data_->sprite(arrow):nullptr;if(!a||!get(a->root_id)||role<1||role>3)return fail("MapArrows animation finished source rejected");if(role!=2)return true;auto*r=data_->record(a->root_id);for(auto other:r->arrows){FieldArrowSpriteState borrowed;const FieldArrowSpriteState*value=nullptr;if(native_){if(!native_->sprite(other,borrowed,error_))return false;value=&borrowed;}else value=&sprites_.at(other);const auto&s=*value;if(other!=arrow&&s.visible&&s.playing){if(!next_wait_||waits_.size()>=10000)return fail("MapArrows resync subscription budget rejected");uint64_t token=next_wait_++;waits_.emplace(token,Wait{other,arrow,r->id});if(!host_.await_frame_changed(other,token,[this,token](){return resume_frame(token);},error_)){poisoned_=true;return false;}return true;}}return true;}
bool FieldCameraArrowsRuntime::resume_frame(uint64_t token){auto i=waits_.find(token);if(poisoned_||i==waits_.end())return fail("MapArrows unknown resync continuation rejected");auto wait=i->second;waits_.erase(i);if(!get(wait.root))return false;if(native_){FieldArrowSpriteState sender;if(!native_->sprite(wait.sender,sender,error_)||!sender.alive)return fail("MapArrows actual resync sender absent");return native_->frame(wait.receiver,int32_t(sender.frame),error_);}auto sender=sprites_.find(wait.sender);if(sender==sprites_.end()||!sender->second.alive)return fail("MapArrows resync sender lifetime rejected");return frame(wait.receiver,int32_t(sender->second.frame));}
bool FieldCameraArrowsRuntime::write(FieldArrowPlayerState&p,uint32_t target,FieldArrowProperty prop,Vec2 v){auto*d=data_->player(p.id);uint32_t id=d->target_root;if(d->kind==0&&target)id=data_->record(id)->arrows[target-1];if(!finite(v))return fail("MapArrows animated value overflow rejected");if(prop==FieldArrowProperty::Frame)return frame(id,int32_t(v.x));if(prop==FieldArrowProperty::Playing)return playing(id,v.x!=0);if(prop==FieldArrowProperty::Visible){auto*r=get(id);if(!r)return false;r->visible=v.x!=0;return publish(*r);}auto i=sprites_.find(id);if(i==sprites_.end()||!i->second.alive)return fail("MapArrows unsupported animated target rejected");if(prop==FieldArrowProperty::Offset)i->second.offset=v;else if(prop==FieldArrowProperty::Position)i->second.position=v;else return fail("MapArrows unknown animated property rejected");return publish(i->second);}
bool FieldCameraArrowsRuntime::apply(FieldArrowPlayerState&p,const FieldArrowClip&c,const FieldArrowTrack&t,float time,float delta){auto&k=t.keys;if(t.update==0||delta==0){if(k.size()==1)return write(p,t.target,t.property,k[0].value);int at=find(k,time);if(at<0)return true;int next=at+1<int(k.size())?at+1:at;if(t.update==1||k[at].transition==0||at==next)return write(p,t.target,t.property,k[at].value);float span=k[next].time-k[at].time,weight=std::abs(span)<.00001f?0:(time-k[at].time)/span;if(k[at].transition!=1)weight=eased(weight,k[at].transition);Vec2 a=k[at].value,b=k[next].value;return write(p,t.target,t.property,{a.x+(b.x-a.x)*weight,a.y+(b.y-a.y)*weight});}
 float from=std::clamp(time-delta,0.0f,c.length),to=std::clamp(time,0.0f,c.length);if(from>to)std::swap(from,to);if(from!=c.length&&to==c.length)to=c.length*1.001f;int last=find(k,to);if(last>=0&&from==to&&k[last].time==from)return write(p,t.target,t.property,k[last].value);if(last>=0&&k[last].time>=to)--last;int first=find(k,from);if(first<0||k[first].time<from)++first;for(int i=first;i<=last;++i)if(!write(p,t.target,t.property,k[i].value))return false;return true;
}
bool FieldCameraArrowsRuntime::animate(FieldArrowPlayerState&p,float delta){auto*d=data_->player(p.id);auto*c=data_->clip(d->profile,p.assigned);if(!c)return fail("MapArrows AnimationPlayer has no clip");float step=delta*d->speed*p.scale,old=p.time,raw=old+step;if(!std::isfinite(raw)||std::abs(raw)>=1000000)return fail("MapArrows animation overflow rejected");float next=std::clamp(raw,0.0f,c->length);step=next-old;bool end=next==c->length&&old<=c->length,notify=end&&old<c->length;p.time=next;
 // Native discrete setters execute during track evaluation. Continuous values
 // are accumulated and applied afterward, then the finished signal dispatches.
 for(const auto&t:c->tracks)if(t.update==1&&step!=0&&!apply(p,*c,t,next,step))return false;
 for(const auto&t:c->tracks)if((t.update==0||step==0)&&!apply(p,*c,t,next,step))return false;
 if(end){p.playing=false;if(notify&&!host_.animation_signal(p.id,false,p.assigned,error_)){poisoned_=true;return false;}}return true;
}
bool FieldCameraArrowsRuntime::sprite_idle(FieldArrowSpriteState&s,float delta){float remaining=delta;while(remaining){if(s.timeout<=0){s.timeout=duration(s.id);if(s.frame>=data_->frames().size()-1){s.frame=0;if(!host_.sprite_signal(s.id,FieldArrowSignal::AnimationFinished,error_)){poisoned_=true;return false;}}else ++s.frame;if(!publish(s)||!host_.sprite_signal(s.id,FieldArrowSignal::FrameChanged,error_)){poisoned_=true;return false;}}float process=std::min(s.timeout,remaining);if(!std::isfinite(process)||process<0)return fail("MapArrows reentrant clock invalid");remaining-=process;s.timeout-=process;}return publish(s);}
bool FieldCameraArrowsRuntime::idle_leaf(uint32_t id,float delta){if(native_)return fail("MapArrows borrowed native clock must be driven by its actual owner");if(!data_||poisoned_||!std::isfinite(delta)||delta<0||delta>1)return fail("MapArrows native idle interval rejected");FieldArrowObservation o;if(!observe(id,o))return false;auto p=players_.find(id);if(p!=players_.end()){auto*d=data_->player(id);auto*s=data_->sprite(d->target_root);if(!get(d->kind?s->root_id:d->target_root))return false;return !o.can_process||!p->second.playing?true:animate(p->second,delta);}auto s=sprites_.find(id);auto*d=data_->sprite(id);if(s==sprites_.end()||!d||!get(d->root_id))return fail("MapArrows unknown idle leaf rejected");return !o.can_process||!o.update_pending||!s->second.playing?true:sprite_idle(s->second,delta);}
bool FieldCameraArrowsRuntime::exit_tree(uint32_t id){auto*r=get(id,false);if(!r)return false;r->alive=false;for(auto i=waits_.begin();i!=waits_.end();){if(i->second.root!=id){++i;continue;}if(!host_.cancel_frame_changed(i->second.sender,i->first,error_)){poisoned_=true;return false;}i=waits_.erase(i);}if(native_)return true;for(auto a:data_->record(id)->arrows){sprites_.at(a).alive=false;players_.at(data_->player_for_target(a)->id).alive=false;}players_.at(data_->player_for_target(id)->id).alive=false;return true;}
bool FieldCameraArrowsRuntime::draw(uint32_t id,FieldArrowDraw&out){if(native_)return fail("MapArrows borrowed native draw belongs to the actual compositor");auto*d=data_?data_->sprite(id):nullptr;auto i=sprites_.find(id);if(!d||i==sprites_.end()||!get(d->root_id))return fail("MapArrows draw lifetime rejected");FieldArrowObservation o;if(!observe(id,o)||!o.material_admitted)return fail("MapArrows actual GPU material rejected");for(auto v:o.world)if(!finite(v))return fail("MapArrows world transform overflow rejected");for(float c:o.canvas_color)if(!std::isfinite(c)||c<0||c>1)return fail("MapArrows canvas color rejected");auto&s=i->second;auto f=data_->frames()[s.frame];Vec2 offset=s.offset;if(d->flags&4){offset.x-=f[2]/2;offset.y-=f[3]/2;}if(data_->pixel_snap()){offset.x=std::floor(offset.x);offset.y=std::floor(offset.y);}FieldArrowDraw draw;draw.id=id;draw.frame=s.frame;draw.visible=s.visible&&roots_.at(d->root_id).visible&&o.visible_in_tree;draw.color=o.canvas_color;draw.pixel_snap=data_->pixel_snap();draw.world_vertices={transform(o.world,offset),transform(o.world,{offset.x+f[2],offset.y}),transform(o.world,{offset.x+f[2],offset.y+f[3]}),transform(o.world,{offset.x,offset.y+f[3]})};for(auto v:draw.world_vertices)if(!finite(v))return fail("MapArrows vertex overflow rejected");out=draw;return true;}
}
