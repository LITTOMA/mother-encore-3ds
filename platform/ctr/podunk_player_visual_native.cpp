#include "podunk_player_visual_native.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e,const char *s) { e=s;return false; }
using Value=std::shared_ptr<const GlobalYamlValue>;
Value get(const Value &v,std::string_view k) { return v && v->kind==6 ? v->get(k) : nullptr; }
bool boolean(const Value &v,bool &out) { if(!v || v->kind!=1)return false;out=v->boolean;return true; }
bool text(const Value &v,std::string &out) { if(!v || v->kind!=4)return false;out=v->string;return true; }
bool native_number(const Value &v,std::string_view type,double &out) {
  std::string t,s;if(!text(get(v,"type"),t)||t!=type||!text(get(v,"value"),s))return false;
  char *end=nullptr;out=std::strtod(s.c_str(),&end);
  return end==s.c_str()+s.size()&&!s.empty()&&std::isfinite(out);
}
bool native_uint(const Value &v,uint32_t &out) {
  double n=0;if(!native_number(v,"int64",n)||n<0||n>std::numeric_limits<uint32_t>::max()||std::floor(n)!=n)return false;
  out=uint32_t(n);return true;
}
bool vector(const Value &v,Vec2 &out) {
  std::string t;double x=0,y=0;
  if(!text(get(v,"type"),t)||t!="Vector2"||!native_number(get(v,"x"),"real",x)||!native_number(get(v,"y"),"real",y)||!std::isfinite(float(x))||!std::isfinite(float(y)))return false;
  out={float(x),float(y)};return true;
}
bool ref(const Value &v,uint32_t &out) {
  std::string t;auto id=get(v,"id");
  if(!text(get(v,"type"),t)||t!="ResourceReference"||!id||id->kind!=2||id->integer<=0||uint64_t(id->integer)>std::numeric_limits<uint32_t>::max())return false;
  out=uint32_t(id->integer);return true;
}
Vec2 transform(const FieldTransform &t,Vec2 v) { return {t[0].x*v.x+t[1].x*v.y+t[2].x,t[0].y*v.x+t[1].y*v.y+t[2].y}; }
}
bool PodunkPlayerVisualNative::live(std::string &e) const {
  const auto *s=tree_?tree_->state(object_):nullptr;
  if(!player_||!data_||!player_->valid()||!data_->valid()||!registry_||registry_->poisoned()||!registry_->object_exists(object_)||!s||!s->alive||s->queued||tree_->object_domain()!=registry_->kernel())return fail(e,"Player native Sprite actual owner expired");
  return true;
}
bool PodunkPlayerVisualNative::prepare(const PlayerInitializationData &p,
    const PlayerVisualScriptsData &d, FieldNodeTreeRuntime &t,
    FieldGlobalRegistry &r, PodunkPlayerVisualTextures &textures, std::string &e) {
  if(player_||object_||!p.valid()||!d.valid()||d.player_ir_sha256()!=p.ir_sha256()||
     !r.root()||r.poisoned()||t.object_domain()!=r.kernel())
    return fail(e,"Player visual constructor domain rejected");
  player_=&p;data_=&d;tree_=&t;registry_=&r;textures_=&textures;
  e.clear();return true;
}
bool PodunkPlayerVisualNative::construct(const PlayerInitializationData &p,const PlayerVisualScriptsData &d,
                                        FieldNodeTreeRuntime &t,FieldGlobalRegistry &r,FieldObjectId id,
                                        PodunkPlayerVisualTextures &textures,std::string &e) {
  auto *n=t.state(id);auto *desc=t.descriptor(id);FieldIdentity identity;
  if(object_||(player_&&(player_!=&p||data_!=&d||tree_!=&t||registry_!=&r||textures_!=&textures))||!p.valid()||!d.valid()||d.player_ir_sha256()!=p.ir_sha256()||!n||!desc||!r.object_exists(id)||t.object_domain()!=r.kernel()||!t.object_identity(id,identity)||identity.scene_id!=p.identity().scene_id||identity.source_sha256!=p.identity().source_sha256||identity.upstream_commit!=p.identity().upstream_commit||n->inside||n->parent||!n->name.empty())return fail(e,"Player Sprite requires actual source attachment cursor");
  bool shadow=desc->id==d.shadow().id;
  if((!shadow && desc->id!=d.bat().id)||desc->native_class!=(shadow?"AnimatedSprite":"Sprite")||desc->script!=(shadow?d.shadow().script:d.bat().script)||desc->script_sha!=(shadow?d.shadow().script_sha:d.bat().script_sha))return fail(e,"Player native Sprite source identity differs");
  auto nodes=get(p.native_source(),"nodes");Value props;
  if(!nodes||nodes->kind!=5)return fail(e,"Player native snapshot nodes unavailable");
  auto *record=p.recipe().record(desc->id);
  if(!record)return fail(e,"Player native Sprite source record absent");
  for(const auto &row:nodes->array) { std::string path;if(!text(get(row,"path"),path))return fail(e,"Player native snapshot path malformed");if(path==record->path) { if(props)return fail(e,"Player native snapshot duplicate path");props=get(row,"properties"); } }
  PlayerVisualNativeState v;bool centered=false,flip_h=false,flip_v=false;
  if(!props||!boolean(get(props,"visible"),v.visible)||!boolean(get(props,"show_behind_parent"),v.behind_parent)||!boolean(get(props,"centered"),centered)||!boolean(get(props,"flip_h"),flip_h)||!boolean(get(props,"flip_v"),flip_v)||!vector(get(props,"offset"),offset_))return fail(e,"Player native Sprite ordered fields unavailable");
  // Neither supported source node has a material or normal map. A new such
  // dependency requires its real rendering owner, never an unlit substitute.
  auto material=get(props,"material"),normal=get(props,"normal_map");
  if(!material||material->kind!=0||(normal&&normal->kind!=0))return fail(e,"Player native Sprite material/normal rendering unsupported");
  if(shadow) {
    double speed=0;
    if(!ref(get(props,"frames"),v.frames_resource)||v.frames_resource!=d.shadow().frames_resource||!text(get(props,"animation"),v.animation)||!boolean(get(props,"playing"),v.playing)||!native_number(get(props,"speed_scale"),"real",speed)||speed<0||!std::isfinite(float(speed)))return fail(e,"Player native AnimatedSprite frames/state differs");
    speed_scale_=float(speed);v.frame=0; // Audited native constructor default.
    auto c=std::find_if(d.shadow().animations.begin(),d.shadow().animations.end(),[&](const auto &x){return x.name==v.animation;});
    if(c==d.shadow().animations.end()||c->frames.empty())return fail(e,"Player native AnimatedSprite source clip absent");
  } else {
    bool region=false;
    if(!ref(get(props,"texture"),v.texture)||v.texture!=d.bat().texture||!native_uint(get(props,"hframes"),v.columns)||!native_uint(get(props,"vframes"),v.rows)||!native_uint(get(props,"frame"),v.frame)||v.columns!=d.bat().columns||v.rows!=d.bat().rows||v.frame!=d.bat().initial_frame||!boolean(get(props,"region_enabled"),region)||region)return fail(e,"Player native Sprite source atlas/region differs");
  }
  if(!(n->flags&1u)||bool(n->flags&2u)!=v.visible||bool(n->flags&8u)!=v.behind_parent)return fail(e,"Player native Canvas source properties differ");
  // The original PackedScene owns these textures before its nodes run Ready.
  // Require the actual loaded GPU owner now, rather than approving a resource
  // path and discovering a missing image only after entering gameplay.
  auto image_ok=[&](std::string_view source,const std::array<uint8_t,32> &sha,const std::array<float,4> *rect) {
    C2D_Image image{};if(!textures.image(source,sha,image,e))return false;
    if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||!image.subtex->width||!image.subtex->height)return fail(e,"Player native Texture GPU owner invalid");
    if(rect&&((*rect)[0]<0||(*rect)[1]<0||(*rect)[2]<=0||(*rect)[3]<=0||(*rect)[0]+(*rect)[2]>image.subtex->width||(*rect)[1]+(*rect)[3]>image.subtex->height))return fail(e,"Player native AtlasTexture source bounds differ");
    if(!rect&&(image.subtex->width%v.columns||image.subtex->height%v.rows))return fail(e,"Player native Texture source grid differs");
    return true;
  };
  if(shadow) {for(const auto &c:d.shadow().animations)for(const auto &f:c.frames)if(!image_ok(f.source,f.source_sha,&f.rect))return false;}
  else if(!image_ok(d.bat().texture_source,d.bat().texture_sha,nullptr))return false;
  v.constructed=true;player_=&p;data_=&d;tree_=&t;registry_=&r;textures_=&textures;object_=id;state_=std::move(v);shadow_=shadow;centered_=centered;flip_h_=flip_h;flip_v_=flip_v;
  if(shadow_&&state_.playing){
    timeout_=duration();
    if(!tree_->add_group(object_,"idle_process_internal",e))return false;
  }
  e.clear();return true;
}
bool PodunkPlayerVisualNative::state(PlayerVisualNativeState &out,std::string &e) const {
  if(!live(e))return false;
  out=state_;
  const auto *n=tree_->state(object_);
  // These classes have no additional native Ready body. Node's actual
  // NOTIFICATION_READY cursor sets ready_first before its script/onready call.
  out.native_ready=n->inside&&n->ready_notified&&!n->ready_first;
  out.visible=(n->flags&2u)!=0;out.behind_parent=(n->flags&8u)!=0;e.clear();return true;
}
const PlayerVisualAnimation *PodunkPlayerVisualNative::clip() const {
  if(!data_||!shadow_)return nullptr;
  auto i=std::find_if(data_->shadow().animations.begin(),data_->shadow().animations.end(),[&](const auto &x){return x.name==state_.animation;});
  return i==data_->shadow().animations.end()?nullptr:&*i;
}
float PodunkPlayerVisualNative::duration() const {
  const auto *c=clip();float speed=c?float(c->speed)*speed_scale_:0;return speed>0?float(1.0/speed):0;
}
bool PodunkPlayerVisualNative::sprite_frames(const std::vector<PlayerVisualAnimation> *&out,std::string &e) const {
  if(!live(e)||!shadow_)return fail(e,"Native Sprite has no SpriteFrames resource");
  out=&data_->shadow().animations;e.clear();return true;
}
bool PodunkPlayerVisualNative::play(std::string_view name,std::string &e) {
  if(!live(e)||!shadow_)return fail(e,"Native Sprite cannot play AnimatedSprite clips");
  const auto &clips=data_->shadow().animations;
  if(!name.empty()&&std::none_of(clips.begin(),clips.end(),[&](const auto &x){return x.name==name;}))return fail(e,"Native AnimatedSprite source animation unavailable");
  if(!name.empty()&&state_.animation!=name) {
    state_.animation=std::string(name);if(state_.playing){timeout_=duration();over_=false;}
    if(!set_frame(0,e))return false;
  }
  over_=false;
  if(!state_.playing){
    state_.playing=true;timeout_=duration();
    if(!tree_->add_group(object_,"idle_process_internal",e))return false;
  }
  e.clear();return true;
}
bool PodunkPlayerVisualNative::set_behind_parent(bool value,std::string &e) {
  if(!live(e)||!tree_->set_behind_parent(object_,value,e))return false;
  e.clear();return true;
}
bool PodunkPlayerVisualNative::set_visible(bool value,std::string &e) {
  if(!live(e)||!tree_->set_visible(object_,value,e))return false;
  state_.visible=value;e.clear();return true;
}
bool PodunkPlayerVisualNative::set_frame(uint32_t frame,std::string &e) {
  if(!live(e))return false;
  if(shadow_) {
    const auto *c=clip();if(!c||c->frames.empty())return fail(e,"Native AnimatedSprite source frame set absent");
    frame=std::min(frame,uint32_t(c->frames.size()-1));if(frame==state_.frame){e.clear();return true;}
    state_.frame=frame;if(state_.playing){timeout_=duration();over_=false;}
  } else {
    if(!state_.columns||!state_.rows||uint64_t(frame)>=uint64_t(state_.columns)*state_.rows)return fail(e,"Native Sprite frame outside source grid");
    state_.frame=frame; // Sprite emits even when setting the same frame.
  }
  return emit("frame_changed",e);
}
bool PodunkPlayerVisualNative::connect(std::string_view signal,FieldObjectId target,std::string method,uint32_t flags,std::string &e) {
  if(!live(e)||!registry_->object_exists(target)||method.empty()||(signal!="frame_changed"&&(signal!="animation_finished"||!shadow_))||(flags&~5u))return fail(e,"Player native Sprite signal connection unsupported");
  for(const auto &c:connections_)if(c.signal==signal&&c.target==target&&c.method==method)return fail(e,"Player native Sprite duplicate signal connection");
  connections_.push_back({std::string(signal),std::move(method),target,flags});e.clear();return true;
}
bool PodunkPlayerVisualNative::emit(std::string_view signal,std::string &e) {
  auto current=connections_;std::vector<Connection> one_shot;
  for(const auto &c:current)if(c.signal==signal) {
    if(!registry_->object_exists(c.target))return fail(e,"Player Sprite signal target expired");
    FieldDeferredMessage call;call.object=c.target;call.member=c.method;
    if(c.flags&1u) { if(!registry_->enqueue(std::move(call),e))return false; }
    else if(!registry_->dispatch(call,e))return false;
    if(c.flags&4u)one_shot.push_back(c);
  }
  // Native Object disconnects one-shot connections after synchronous delivery.
  for(const auto &c:one_shot)connections_.erase(std::remove_if(connections_.begin(),connections_.end(),[&](const auto &x){return x.signal==c.signal&&x.target==c.target&&x.method==c.method;}),connections_.end());
  e.clear();return true;
}
bool PodunkPlayerVisualNative::process_internal(float delta,bool paused,bool update_pending,std::string &e) {
  if(!live(e)||!shadow_||!std::isfinite(delta)||delta<0)return fail(e,"Native AnimatedSprite actual process delta rejected");
  if(!tree_->can_process(object_,paused)||!state_.playing||!update_pending){e.clear();return true;}
  float remaining=delta;
  while(remaining) {
    const auto *c=clip();if(!c||c->frames.empty())return fail(e,"Native AnimatedSprite process clip missing");
    float speed=float(c->speed)*speed_scale_;if(speed==0){e.clear();return true;}
    if(timeout_<=0) {
      timeout_=duration();uint32_t count=uint32_t(c->frames.size());
      if(state_.frame>=count-1) {
        if(c->loop){state_.frame=0;if(!emit("animation_finished",e))return false;}
        else {state_.frame=count-1;if(!over_){over_=true;if(!emit("animation_finished",e))return false;}}
      } else ++state_.frame;
      if(!emit("frame_changed",e))return false;
    }
    float amount=std::min(timeout_,remaining);
    if(amount<=0)return fail(e,"Native AnimatedSprite frame clock cannot advance");
    remaining-=amount;timeout_-=amount;
  }
  e.clear();return true;
}
bool PodunkPlayerVisualNative::draw(const FieldTransform &viewport,bool snap,std::string &e) const {
  if(!live(e))return false;
  if(!tree_->visible_in_tree(object_)){e.clear();return true;}
  const auto *n=tree_->state(object_);if(!n->inside)return fail(e,"Player native Sprite draw before enter");
  C2D_Image image{};std::array<float,4> rect{};
  if(shadow_) {
    const auto *c=clip();if(!c||state_.frame>=c->frames.size())return fail(e,"Player shadow actual frame unavailable");
    const auto &f=c->frames[state_.frame];rect=f.rect;
    if(!textures_->image(f.source,f.source_sha,image,e))return false;
  } else {
    const auto &b=data_->bat();if(!textures_->image(b.texture_source,b.texture_sha,image,e))return false;
    if(!image.subtex||!state_.columns||!state_.rows||image.subtex->width%state_.columns||image.subtex->height%state_.rows)return fail(e,"Player native Sprite GPU source grid differs");
    float w=float(image.subtex->width/state_.columns),h=float(image.subtex->height/state_.rows);
    rect={float(state_.frame%state_.columns)*w,float(state_.frame/state_.columns)*h,w,h};
  }
  if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||rect[0]<0||rect[1]<0||rect[2]<=0||rect[3]<=0||rect[0]+rect[2]>image.subtex->width||rect[1]+rect[3]>image.subtex->height)return fail(e,"Player native Sprite GPU frame region rejected");
  // C2D images support orthogonal scale/rotation. Skew must receive a real
  // quad renderer before being supported; no axis-aligned approximation.
  FieldTransform world;if(!tree_->world_transform(object_,world,e))return false;
  Vec2 axis_x=transform(viewport,world[0]),axis_y=transform(viewport,world[1]),origin=transform(viewport,world[2]);
  axis_x.x-=viewport[2].x;axis_x.y-=viewport[2].y;axis_y.x-=viewport[2].x;axis_y.y-=viewport[2].y;
  float sx=std::hypot(axis_x.x,axis_x.y),sy=std::hypot(axis_y.x,axis_y.y);
  if(sx==0||sy==0){e.clear();return true;}
  if(std::fabs(axis_x.x*axis_y.x+axis_x.y*axis_y.y)>1e-5f*sx*sy)return fail(e,"Player native Sprite skew renderer unavailable");
  bool negative=(axis_x.x*axis_y.y-axis_x.y*axis_y.x)<0;
  auto sub=*image.subtex;float du=(sub.right-sub.left)/sub.width,dv=(sub.bottom-sub.top)/sub.height;
  float u=sub.left+rect[0]*du,v=sub.top+rect[1]*dv;
  sub.left=u;sub.right=u+rect[2]*du;sub.top=v;sub.bottom=v+rect[3]*dv;
  sub.width=uint16_t(rect[2]);sub.height=uint16_t(rect[3]);
  if(flip_h_)std::swap(sub.left,sub.right);
  if(flip_v_!=negative)std::swap(sub.top,sub.bottom);
  Vec2 offset=offset_;if(centered_){offset.x-=rect[2]/2;offset.y-=rect[3]/2;}
  if(snap){
    float left=origin.x+axis_x.x*offset.x+axis_y.x*offset.y;
    float top=origin.y+axis_x.y*offset.x+axis_y.y*offset.y;
    origin.x+=std::floor(left+.5f)-left;origin.y+=std::floor(top+.5f)-top;
  }
  Vec2 center{offset.x+rect[2]/2,offset.y+rect[3]/2};
  origin.x+=axis_x.x*center.x+axis_y.x*center.y;origin.y+=axis_x.y*center.x+axis_y.y*center.y;
  FieldColor color;if(!tree_->effective_color(object_,color,e))return false;
  C2D_ImageTint tint;C2D_PlainImageTint(&tint,C2D_Color32f(color[0],color[1],color[2],color[3]),0);
  C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);image.subtex=&sub;
  if(!C2D_DrawImageAtRotated(image,origin.x,origin.y,0,std::atan2(axis_x.y,axis_x.x),&tint,sx,sy))return fail(e,"Player native Sprite GPU draw rejected");
  e.clear();return true;
}
} // namespace encore::ctr
