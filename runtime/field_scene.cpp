#include "encore/field_scene.hpp"
#include <algorithm>
#include <cmath>

namespace encore::upstream {
namespace {
bool intersects(Vec2 alo,Vec2 ahi,Vec2 blo,Vec2 bhi){return alo.x<=bhi.x&&ahi.x>=blo.x&&alo.y<=bhi.y&&ahi.y>=blo.y;}
Vec2 normalized(Vec2 v){const float d=std::sqrt(v.x*v.x+v.y*v.y);return d==0?Vec2{}:Vec2{v.x/d,v.y/d};}
float sign(float v){return v<0?-1.f:v>0?1.f:0.f;}
bool ray_rect(Vec2 origin,Vec2 direction,float length,Vec2 center,Vec2 extents,float& distance){
 float low=0,high=length;
 for(unsigned axis=0;axis<2;++axis){const float o=axis?origin.y:origin.x,d=axis?direction.y:direction.x,c=axis?center.y:center.x,e=axis?extents.y:extents.x;
  if(d==0){if(o<c-e||o>c+e)return false;continue;}
  float a=(c-e-o)/d,b=(c+e-o)/d;if(a>b)std::swap(a,b);low=std::max(low,a);high=std::min(high,b);if(low>high)return false;
 }distance=low;return true;
}
}

bool FieldCollision::bind(FieldMapView map,const std::vector<bool>& layer_active,const std::vector<bool>& body_active,std::string& error){
    *this=FieldCollision{};
    if(!map.valid()||layer_active.size()!=map.count(FieldMapSection::Layers)||body_active.size()!=map.count(FieldMapSection::Bodies)){error="Field collision binding rejected";return false;}
    map_=map;
    const auto shapes=map.count(FieldMapSection::Shapes);
    first_.reserve(shapes);
    for(uint32_t i=0;i<shapes;++i){
        const auto s=map.shape(i);first_.push_back(uint32_t(points_.size()));
        std::vector<Vec2> pts;pts.reserve(s.count);
        for(uint32_t k=0;k<s.count;++k)pts.push_back(map.point(s.first+k));
        if(s.kind==uint16_t(FieldShapeKind::Segment)){points_.insert(points_.end(),pts.begin(),pts.end());normals_.push_back(s.normal);normals_.push_back({});continue;}
        // ConvexPolygonShape2D::_update_shape inverts clockwise point lists.
        double sum=0;for(size_t k=0;k<pts.size();++k){const auto a=pts[k],b=pts[(k+1)%pts.size()];sum+=double(b.x-a.x)*double(b.y+a.y);}
        if(pts.size()>=3&&sum>0)std::reverse(pts.begin(),pts.end());
        for(size_t k=0;k<pts.size();++k){const auto a=pts[k],b=pts[(k+1)%pts.size()];normals_.push_back(normalized({b.y-a.y,-(b.x-a.x)}));}
        points_.insert(points_.end(),pts.begin(),pts.end());
    }
    for(uint32_t i=0;i<layer_active.size();++i){
        if(!layer_active[i])continue;
        const auto layer=map.layer(i);
        if(!(layer.flags&uint32_t(FieldLayerFlag::Collides)))continue;
        for(uint32_t c=0;c<layer.chunk_count;++c){
            const auto chunk=map.chunk(layer.chunk_first+c);
            const auto first=uint32_t(cells_.size());
            for(uint32_t k=0;k<chunk.count;++k){
                const auto cell=map.cell(chunk.first+k);const auto tile=map.tile(cell.tile);
                if(!tile.shape_count)continue;
                cells_.push_back({{layer.position.x+float(cell.x)*16.f,layer.position.y+float(cell.y)*16.f},tile.shape_first,tile.shape_count});
            }
            const auto count=uint32_t(cells_.size())-first;
            if(count)chunks_.push_back({chunk.minimum,chunk.maximum,first,count});
        }
    }
    for(uint32_t i=0;i<body_active.size();++i)if(body_active[i])bodies_.push_back(i);
    error.clear();return true;
}
bool FieldCollision::collect(Vec2 lo,Vec2 hi,std::vector<MotionObstacle>& out)const{
    if(!map_.valid())return false;
    auto push=[&](uint32_t shape_index,Vec2 offset){
        const auto s=map_.shape(shape_index);const Vec2 mn{s.minimum.x+offset.x,s.minimum.y+offset.y},mx{s.maximum.x+offset.x,s.maximum.y+offset.y};
        if(!intersects(lo,hi,mn,mx))return;
        const auto first=first_[shape_index];
        out.push_back({points_.data()+first,normals_.data()+first,s.count,offset,mn,mx,s.kind==uint16_t(FieldShapeKind::Segment)});
    };
    for(const auto& chunk:chunks_){
        if(!intersects(lo,hi,chunk.minimum,chunk.maximum))continue;
        for(uint32_t k=0;k<chunk.count;++k){
            const auto& cell=cells_[chunk.first+k];
            for(uint32_t j=0;j<cell.count;++j)push(cell.first+j,cell.offset);
        }
    }
    for(const auto bi:bodies_){const auto b=map_.body(bi);for(uint32_t s=0;s<b.shape_count;++s)push(b.shape_first+s,{});}
    for(uint32_t i=0;i<extra_.size();++i){const auto& b=extra_[i];if(i!=excluded_&&b.enabled&&intersects(lo,hi,b.minimum,b.maximum))
        out.push_back({b.points,b.normals,4,{},b.minimum,b.maximum,false});}
    if(out.size()>4096)return false;
    return true;
}
void FieldCollision::set_body(Vec2 center,Vec2 extents,uint32_t slot,bool enabled){
    if(slot>=65)return;
    if(extra_.size()<=slot)extra_.resize(slot+1);
    auto& body=extra_[slot];body.enabled=enabled;
    Vec2 pts[4]={{center.x-extents.x,center.y-extents.y},{center.x+extents.x,center.y-extents.y},{center.x+extents.x,center.y+extents.y},{center.x-extents.x,center.y+extents.y}};
    double sum=0;for(int k=0;k<4;++k){const auto a=pts[k],b=pts[(k+1)%4];sum+=double(b.x-a.x)*double(b.y+a.y);}
    if(sum>0)std::reverse(pts,pts+4);
    for(int k=0;k<4;++k){body.points[k]=pts[k];const auto a=pts[k],b=pts[(k+1)%4];body.normals[k]=normalized({b.y-a.y,-(b.x-a.x)});}
    body.minimum={center.x-extents.x,center.y-extents.y};body.maximum={center.x+extents.x,center.y+extents.y};
}

uint32_t SceneDoorTransition::kind()const{return phase_==SceneDoorPhase::FadeIn?route_.in_kind:route_.out_kind;}
float SceneDoorTransition::cut()const{
    if(!fades_)return 0;
    if(phase_==SceneDoorPhase::FadeIn)return door_transition_cut(*fades_,route_.in_kind,false,time_);
    if(phase_==SceneDoorPhase::FadeOut)return door_transition_cut(*fades_,route_.out_kind,true,time_);
    return door_transition_cut(*fades_,route_.out_kind,true,0);
}
bool SceneDoorTransition::begin(const WorldLinksData& links,uint32_t index,const IntroductionData& fades,std::string& error){
    *this=SceneDoorTransition{};
    if(!links.valid()||index>=links.route_count()||!fades.valid()){error="Scene door transition rejected";phase_=SceneDoorPhase::Error;return false;}
    const auto r=links.route(index);
    if(r.in_kind>2||r.out_kind>2||door_transition_length(fades,r.in_kind,false)<=0||door_transition_length(fades,r.out_kind,true)<=0||
       door_transition_mostly(fades,r.out_kind,true)<0||door_transition_mostly(fades,r.out_kind,true)>door_transition_length(fades,r.out_kind,true)){
        error="Scene door transition clip rejected";phase_=SceneDoorPhase::Error;return false;}
    fades_=&fades;route_=r;index_=index;phase_=SceneDoorPhase::FadeIn;
    // Door.enter: _set_flag, then SceneTransition: stop music, door sound, fade-in.
    if(!r.flag.empty())events_.push_back({SceneDoorEventKind::SetFlag,r.flag,0,r.flag_value});
    if(r.flags&uint32_t(WorldRouteFlag::FadeMusic))events_.push_back({SceneDoorEventKind::FadeMusic,{},r.music_fade,false});
    if(!r.sound.empty())events_.push_back({SceneDoorEventKind::PlaySound,r.sound,0,false});
    error.clear();return true;
}
bool SceneDoorTransition::idle_frame(double delta,std::string& error){
    if(!std::isfinite(delta)||delta<0||delta>1){error="Scene door transition time rejected";phase_=SceneDoorPhase::Error;return false;}
    switch(phase_){
    case SceneDoorPhase::FadeIn:
        // Same float-quantized clock as HouseRuntime's same-scene doors.
        time_=double(float(time_+double(float(delta*route_.in_speed))));
        if(time_>=door_transition_length(*fades_,route_.in_kind,false)){time_=0;phase_=SceneDoorPhase::SwapRequested;}
        break;
    case SceneDoorPhase::Settle:
        // scene_changed follows the deferred change, tree_changed and one idle
        // frame; the door yields one more idle frame before the fade-out.
        if(settle_)--settle_;
        if(!settle_){time_=0;phase_=SceneDoorPhase::FadeOut;}
        break;
    case SceneDoorPhase::FadeOut:{
        const double old=time_;
        time_=double(float(time_+double(float(delta*route_.out_speed))));
        const double mostly=door_transition_mostly(*fades_,route_.out_kind,true);
        if(!unpaused_&&old<=mostly&&time_>mostly){
            unpaused_=true;
            if(!route_.end_sound.empty())events_.push_back({SceneDoorEventKind::PlaySound,route_.end_sound,0,false});
            events_.push_back({SceneDoorEventKind::Unpause,{},0,false});
        }
        if(time_>=door_transition_length(*fades_,route_.out_kind,true)){
            if(!unpaused_){unpaused_=true;if(!route_.end_sound.empty())events_.push_back({SceneDoorEventKind::PlaySound,route_.end_sound,0,false});events_.push_back({SceneDoorEventKind::Unpause,{},0,false});}
            phase_=SceneDoorPhase::Done;
        }
        break;}
    default:break;
    }
    error.clear();return true;
}
bool SceneDoorTransition::swapped(std::string& error){
    if(phase_!=SceneDoorPhase::SwapRequested){error="Scene door swap outside its boundary";phase_=SceneDoorPhase::Error;return false;}
    phase_=SceneDoorPhase::Settle;settle_=2;error.clear();return true;
}

bool FieldScene::conditions(FieldSpan span)const{
    for(uint32_t i=0;i<span.count;++i){const auto c=map_.condition(span.first+i);if(world.story_flag(map_.text(c.flag))!=c.expected)return false;}
    return true;
}
bool FieldScene::prepare(RoomView room,FieldMapView map,const WorldLinksData& links,const std::vector<bool>& story_flags,Vec2 position,Vec2 direction,Vec2 viewport,std::string& error){
    auto reject=[&](std::string message){error=message;phase_=FieldPhase::Error;error_=std::move(message);return false;};
    if(!room.valid()||!map.valid()||!links.valid()||room.scene().stable_id!=map.scene_id())return reject("Field scene resources do not match");
    const auto scene_index=links.find_scene(map.scene_id());
    if(scene_index==WorldLinksData::kNotFound||!links.scene(scene_index).map_role||links.scene(scene_index).source!=room.string(room.scene().source_scene_string))
        return reject("Field scene is not a linked map scene");
    if(story_flags.size()!=room.flag_count())return reject("Field scene flag table mismatch");
    for(uint32_t i=0;i<map.count(FieldMapSection::Items);++i){const auto item=map.item(i);
        if(item.kind==uint16_t(FieldItemKind::Actor)&&(item.first>=room.actor_instance_count()||item.first==room.scene().player_instance_index))return reject("Field map actor is not a room NPC");}
    if(map.count(FieldMapSection::Area)){
        const auto a=map.area();
        if(map.text(a.source)!=room.string(room.scene().source_scene_string))return reject("Field AreaRoom source differs from the room");
        uint32_t visit=kRoomNoIndex,guard=kRoomNoIndex;
        for(uint32_t i=0;i<room.flag_count();++i){const auto name=room.string(room.flag(i).name_string);if(name==map.text(a.visit_flag))visit=i;if(name==map.text(a.party_guard_flag))guard=i;}
        if(visit==kRoomNoIndex||guard==kRoomNoIndex)return reject("Field AreaRoom ready flag is unregistered");
        if(story_flags[guard])return reject("Field AreaRoom flying NPC party is not yet admitted");
    }
    const std::vector<bool> reviewed(room.flag_count(),true);
    if(!world.initialize_restored(room,story_flags,reviewed,position,direction,viewport))return reject(std::string("Field world rejected: ")+world.error());
    // A candidate world owns this Ready write; a failed scene publication cannot
    // mutate the old owner. Source set_flag emits even when already true.
    if(map.count(FieldMapSection::Area)&&!world.set_story_flag(map.text(map.area().visit_flag),true,true))return reject("Field AreaRoom visit flag write failed");
    map_=map;links_=&links;viewport_=viewport;
    const auto doors=map.count(FieldMapSection::Doors),boundaries=map.count(FieldMapSection::Boundaries),cameras=map.count(FieldMapSection::Cameras);
    const auto openables=map.count(FieldMapSection::Openables);
    for(uint32_t i=0;i<doors;++i){const auto d=map.door(i);if(!d.route)continue;
        const auto r=links.find_route(d.route);
        if(r==WorldLinksData::kNotFound||links.route(r).from!=map.scene_id()||links.route(r).door!=map.text(d.path))return reject("Field door route binding rejected");}
    for(uint32_t i=0;i<openables;++i)if(!conditions(map.openable(i).conditions))return reject("Conditional openable doors are not reviewed");
    std::string why;
    if(!refresh_conditions(why))return reject(why);
    door_inside_.assign(doors,0);boundary_inside_.assign(boundaries,0);camera_inside_.assign(cameras,0);
    openables_.assign(openables,Openable{});sounds_.clear();
    route_=pending_door_=boundary_=WorldLinksData::kNotFound;
    last_safe_position_=position;last_safe_direction_=direction;
    if(!world.pause_for_house())return reject("Field player pause rejected");
    phase_=FieldPhase::Transition;error_.clear();error.clear();return true;
}
bool FieldScene::refresh_conditions(std::string& error){
    if(!map_.valid()||!links_){error="Field conditions require a prepared map";return false;}
    const auto layers=map_.count(FieldMapSection::Layers),sprites=map_.count(FieldMapSection::Sprites),items=map_.count(FieldMapSection::Items);
    const auto doors=map_.count(FieldMapSection::Doors),boundaries=map_.count(FieldMapSection::Boundaries),cameras=map_.count(FieldMapSection::Cameras);
    const auto notices=map_.count(FieldMapSection::Notices),bodies=map_.count(FieldMapSection::Bodies);
    layer_active_.assign(layers,false);for(uint32_t i=0;i<layers;++i)layer_active_[i]=conditions(map_.layer(i).conditions);
    sprite_active_.assign(sprites,false);for(uint32_t i=0;i<sprites;++i)sprite_active_[i]=conditions(map_.sprite(i).conditions);
    item_active_.assign(items,false);for(uint32_t i=0;i<items;++i)item_active_[i]=conditions(map_.item(i).conditions);
    door_active_.assign(doors,false);for(uint32_t i=0;i<doors;++i)door_active_[i]=conditions(map_.door(i).conditions);
    boundary_active_.assign(boundaries,false);for(uint32_t i=0;i<boundaries;++i)boundary_active_[i]=conditions(map_.boundary(i).conditions);
    camera_active_.assign(cameras,false);for(uint32_t i=0;i<cameras;++i)camera_active_[i]=conditions(map_.camera(i).conditions);
    notice_active_.assign(notices,false);for(uint32_t i=0;i<notices;++i)notice_active_[i]=conditions(map_.notice(i).conditions);
    std::vector<bool> body_active(bodies,false);for(uint32_t i=0;i<bodies;++i)body_active[i]=conditions(map_.body(i).conditions);
    std::vector<bool> collide(layers,false);for(uint32_t i=0;i<layers;++i)collide[i]=layer_active_[i];
    std::string why;
    if(!collision_ready_||collision_layers_!=collide||collision_bodies_!=body_active){
        if(!collision_.bind(map_,collide,body_active,why)){error=why;return false;}
        collision_layers_=collide;collision_bodies_=body_active;collision_ready_=true;
    }
    world.attach_obstacles(&collision_);
    if(boundary_inside_.size()==boundaries){
        for(uint32_t i=0;i<boundaries;++i)if(!boundary_active_[i])boundary_inside_[i]=0;
        if(boundary_!=WorldLinksData::kNotFound&&(boundary_&0x80000000u)!=0){
            const auto index=boundary_&~0x80000000u;
            if(index<boundaries&&!boundary_active_[index]&&phase_==FieldPhase::Unsupported){
                boundary_=WorldLinksData::kNotFound;error_.clear();
                if(!world.unpause_from_house()){error="Field boundary clear unpause rejected";return false;}
                phase_=FieldPhase::Walking;
            }
        }
    }
    if(mick_bound_)apply_mick_body();
    apply_npc_bodies();
    story_revision_=world.story_revision();error.clear();return true;
}
bool FieldScene::bind_story(FieldStoryView view,ItemView items,FieldStoryHost& host,std::string& error){
    if(!view.bind(world.content(),map_,items,error))return false;
    story_=view;story_host_=&host;error.clear();return true;
}
void FieldScene::apply_mick_body(){
    const auto a=mick_.view().actor();
    collision_.set_body(mick_.collision_center(),a.collision_extents);
}
struct FieldMickWorld final:MickWorld {
    FieldScene& scene;
    uint32_t slot;StaticMotionSolver* solver;
    explicit FieldMickWorld(FieldScene& scene,uint32_t slot=0,StaticMotionSolver* solver=nullptr):scene(scene),slot(slot),solver(solver?solver:&scene.mick_solver_){}
    bool probe(Vec2 from,Vec2 to,bool& clear)override{
        if(!scene.mick_solver_ready_&&slot==0){clear=true;return true;}
        scene.collision_.set_excluded(slot);
        SlideResult result;const Vec2 delta{to.x-from.x,to.y-from.y};
        const bool ok=solver->slide(from,{delta.x*60.f,delta.y*60.f},result);
        scene.collision_.set_excluded(UINT32_MAX);
        if(!ok)return false;
        const float dx=result.position.x-to.x,dy=result.position.y-to.y;
        clear=dx*dx+dy*dy<1.f;return true;
    }
    bool slide(Vec2 position,Vec2 velocity,Vec2& out)override{
        if(!scene.mick_solver_ready_&&slot==0){out={position.x+velocity.x/60.f,position.y+velocity.y/60.f};return true;}
        scene.collision_.set_excluded(slot);
        SlideResult result;const bool ok=solver->slide(position,velocity,result);
        scene.collision_.set_excluded(UINT32_MAX);
        if(!ok)return false;out=result.position;return true;
    }
    bool player_near(Vec2 center,Vec2 extents)override{return scene.overlaps(center,extents,scene.world.player().position);}
    bool player_in_view(Vec2 center,float radius)override{return scene.circle_overlaps(center,radius);}
    bool shove_player(Vec2 position,Vec2 direction,bool walking)override{return scene.world.script_move(position,direction,walking);}
};
bool FieldScene::bind_npc(MickView view,MickHost& host,std::string& error){
    if(phase_==FieldPhase::Error||!view.valid()||!view.motion_only()){error="Field NPC requires a checked motion owner";return false;}
    const auto room=world.content();uint32_t actor=UINT32_MAX,item=UINT32_MAX;
    for(uint32_t i=0;i<room.actor_instance_count();++i)if(room.actor_instance(i).stable_id==view.owner_id())actor=i;
    for(uint32_t i=0;i<map_.count(FieldMapSection::Items);++i)if(map_.item(i).kind==uint16_t(FieldItemKind::Actor)&&map_.item(i).first==actor)item=i;
    if(actor==UINT32_MAX||item==UINT32_MAX){error="Field NPC has no room/map owner";return false;}
    for(const auto& n:npcs_)if(n->actor==actor){error="Duplicate field NPC motion owner";return false;}
    const auto instance=room.actor_instance(actor);const auto profile=room.actor_profile(instance.profile_index);const auto texture=room.resource(profile.primary_resource);const auto t=view.texture();const auto a=view.actor();
    // Eight-direction motion must use the checked NPC PartyMember replacement.
    if(profile.execution_kind!=(view.direction_count()==8?4:3)||profile.direction_count!=view.direction_count()){error="Field NPC replacement direction profile differs";return false;}
    if(room.string(texture.path_string)!=view.string(t.path)||texture.width!=t.width||texture.height!=t.height||texture.columns!=t.columns||texture.rows!=t.rows||instance.position.x!=a.position.x||instance.position.y!=a.position.y){error="Field NPC room/motion binding differs";return false;}
    auto candidate=std::make_unique<Npc>();candidate->actor=actor;candidate->item=item;
    if(!candidate->motion.initialize(view,host,error))return false;
    if(view.interaction_policy()){
        if(!initialize_actor(candidate->sprite,room,instance.profile_index,a.position,a.initial_direction)||!npc_sprite_blend(candidate->sprite,a.initial_direction)){error="Field NPC source sprite initialization rejected";return false;}
    }
    const Vec2 o{a.collision_center.x-a.position.x,a.collision_center.y-a.position.y},e=a.collision_extents;
    ConvexPolygon foot;foot.vertices={{o.x-e.x,o.y-e.y},{o.x+e.x,o.y-e.y},{o.x+e.x,o.y+e.y},{o.x-e.x,o.y+e.y}};
    if(!candidate->solver.configure(foot,{},0.08f)){error="Field NPC collision solver rejected";return false;}
    candidate->solver.attach_source(&collision_);npcs_.push_back(std::move(candidate));apply_npc_bodies();error.clear();return true;
}
bool FieldScene::actor_visible(uint32_t index)const{
    for(const auto& n:npcs_)if(n->actor==index)return item_active(n->item)&&world.instance_visible(index);
    return false;
}
bool FieldScene::npcs_ready()const{
    if(!map_.valid())return false;
    for(uint32_t i=0;i<map_.count(FieldMapSection::Items);++i){const auto item=map_.item(i);if(item.kind!=uint16_t(FieldItemKind::Actor))continue;
        bool found=false;for(const auto& n:npcs_)found|=n->actor==item.first;
        if(!found)return false;
    }return true;
}
ActorActionState FieldScene::actor_pose(uint32_t index)const{
    if(index>=world.actor_count())return {};
    auto pose=world.actor(index);if(world.actor_bound(index))return pose;
    for(const auto& n:npcs_)if(n->actor==index){pose.position=n->motion.position();pose.direction=n->motion.facing();pose.frame=n->motion.view().interaction_policy()?n->sprite.frame:uint16_t(n->motion.sheet_frame());break;}
    return pose;
}
void FieldScene::apply_npc_bodies(){
    for(uint32_t i=0;i<npcs_.size();++i){const auto& n=*npcs_[i];const auto a=n.motion.view().actor();const auto p=actor_pose(n.actor).position;
        collision_.set_body({p.x+a.collision_center.x-a.position.x,p.y+a.collision_center.y-a.position.y},a.collision_extents,i+1,actor_visible(n.actor)&&!world.actor_bound(n.actor));}
}
bool FieldScene::finish_transition(){
    if(phase_!=FieldPhase::Transition)return fail("Field transition finish outside its boundary");
    if(!world.unpause_from_house())return fail("Field player unpause rejected");
    phase_=FieldPhase::Walking;return true;
}
bool FieldScene::warp_route(uint32_t index,std::string&error){
 if(phase_!=FieldPhase::Transition||!links_||!map_.valid()||index>=links_->route_count()){error="Field door relocation requires a checked transition";return false;}
 const auto route=links_->route(index);
 if(route.from!=scene_id()||route.to!=scene_id()){error="Field door relocation cannot change scene ownership";return false;}
 const auto direction=(route.direction.x||route.direction.y)?route.direction:world.player().direction;
 if(!world.warp_same_scene(route.destination,direction)){error="Field door relocation pose rejected";return false;}
 last_safe_position_=route.destination;last_safe_direction_=direction;
 for(uint32_t i=0;i<door_inside_.size();++i){const auto door=map_.door(i);door_inside_[i]=door_active_[i]&&overlaps(door.center,door.extents,route.destination);}
 route_=pending_door_=WorldLinksData::kNotFound;error.clear();return true;
}
bool FieldScene::bind_mick(MickView view,MickHost& host,std::string& error){
    if(phase_==FieldPhase::Error||!map_.valid()){error="Field Mick bind requires a prepared scene";return false;}
    if(!mick_.initialize(view,host,error))return false;
    const auto a=view.actor();const auto origin=mick_.position();const auto center=mick_.collision_center();
    const Vec2 o{center.x-origin.x,center.y-origin.y};const auto e=a.collision_extents;
    ConvexPolygon foot;foot.vertices={{o.x-e.x,o.y-e.y},{o.x+e.x,o.y-e.y},{o.x+e.x,o.y+e.y},{o.x-e.x,o.y+e.y}};
    if(!mick_solver_.configure(foot,{},0.08f)){error="Field Mick foot solver rejected";return false;}
    mick_solver_.attach_source(&collision_);mick_solver_ready_=mick_solver_.valid();
    if(!mick_solver_ready_){error="Field Mick foot solver rejected";return false;}
    mick_bound_=true;apply_mick_body();error.clear();return true;
}
bool FieldScene::mick_contact()const{
    if(!mick_bound_||phase_!=FieldPhase::Walking)return false;
    const auto a=mick_.view().actor();const auto p=world.player();const auto center=mick_.interact_center();
    float distance=0;
    return ray_rect(p.position,normalized(p.direction),a.ray_length,center,a.interact_extents,distance)||overlaps(center,a.interact_extents,p.position);
}
bool FieldScene::try_mick(bool accept,std::string& error){
    if(!mick_bound_||phase_!=FieldPhase::Walking||!accept||!mick_contact()){error.clear();return true;}
    const auto p=world.player();const auto at=mick_.position();
    mick_.face_toward(p.position);
    Vec2 facing{at.x-p.position.x,at.y-p.position.y};
    if(std::abs(facing.x)>std::abs(facing.y)&&facing.x!=0)facing={sign(facing.x),0};else if(facing.y!=0)facing={0,sign(facing.y)};else facing=p.direction;
    if(!world.set_house_direction(facing)||!world.pause_for_house())return fail("Field Mick turn/pause rejected");
    phase_=FieldPhase::MickTalk;mick_ignore_accept_=true;
    if(!mick_.start_talk(error))return fail(error.empty()?"Field Mick programme rejected":error.c_str());
    error.clear();return true;
}
bool FieldScene::try_interaction(bool accept,std::string& error){
    if(world.stage()!=OpeningStage::Walking){error.clear();return true;}
    if(!try_mick(accept,error))return false;
    if(!accept||phase_!=FieldPhase::Walking||world.house_paused()||world.player().crouch)return true;
    for(const auto& ptr:npcs_){auto& n=*ptr;if(!actor_visible(n.actor)||!n.motion.view().has_dialogue())continue;
        const auto a=n.motion.view().actor();const auto p=world.player();const auto at=actor_pose(n.actor).position;
        const Vec2 center{at.x+a.interact_center.x-a.position.x,at.y+a.interact_center.y-a.position.y};float distance=0;
        if(!ray_rect(p.position,normalized(p.direction),a.ray_length,center,a.interact_extents,distance)&&!overlaps(center,a.interact_extents,p.position))continue;
        FieldInteraction selected;for(uint32_t i=0;i<story_.interaction_count();++i){const auto rule=story_.interaction(i);if(rule.actor==n.actor&&(!rule.flag.length||world.story_flag(story_.text(rule.flag))))selected=rule;}
        if(selected.status){
            if(!world.pause_for_house())return fail("Unavailable source dialogue pause rejected");
            phase_=FieldPhase::Unsupported;boundary_=WorldLinksData::kNotFound;error_=std::string(story_.text(selected.source))+" not ported; B returns";error.clear();return true;
        }
        if(selected.programme!=UINT32_MAX){
            if(!story_host_||!story_host_->can_start(story_,selected.programme,error)){
                boundary_=WorldLinksData::kNotFound;phase_=FieldPhase::Unsupported;error_=error.empty()?"Field interaction admission rejected; B returns":error+"; B returns";
                if(!world.pause_for_house())return fail("Field interaction admission pause rejected");error.clear();return true;
            }
            if(selected.turns&2)n.motion.face_toward(p.position);
            if(!world.sync_unbound_actor_pose(n.actor,at,n.motion.facing()))return fail("Field interaction pose handoff rejected");
            if((selected.turns&1)&&!world.face_interaction(at))return fail("Field interaction player turn rejected");
            if(n.motion.view().interaction_policy()&&(selected.turns&2)&&!npc_sprite_blend(n.sprite,n.motion.facing()))return fail("Field NPC interaction sprite facing rejected");
            if(!world.begin_house_program(selected.programme,n.motion.view().interaction_policy()?n.actor:kRoomNoIndex))return fail("Field interaction programme start rejected");
            if(n.motion.view().interaction_policy()){
                if(!n.motion.begin_interaction(error))return fail(error);
                interaction_actor_=n.actor;
            }
            error.clear();return true;
        }
        if(!world.pause_for_house())return fail("Field NPC interaction pause rejected");
        phase_=FieldPhase::Unsupported;boundary_=WorldLinksData::kNotFound;error_="Field NPC dialogue not ported; B returns";error.clear();return true;
    }
    for(uint32_t i=0;i<boundary_active_.size();++i){
        const auto b=map_.boundary(i);
        if(!boundary_active_[i]||b.kind!=uint16_t(FieldBoundaryKind::Confirm)||!overlaps(b.center,b.extents,world.player().position))continue;
        if(!world.pause_for_house())return fail("Field action boundary pause rejected");
        boundary_=0x80000000u|i;phase_=FieldPhase::Unsupported;
        error_=std::string(map_.text(b.label))+" not ported; B returns";
        return true;
    }
    error.clear();return true;
}
bool FieldScene::try_telepathy(Vec2 offset,float length,std::string& error){
    if(!mick_bound_||phase_!=FieldPhase::Walking||world.house_paused()||!std::isfinite(offset.x)||!std::isfinite(offset.y)||!std::isfinite(length)||length<=0){error="Field Telepathy state/ray rejected";return false;}
    const auto player=world.player();const auto direction=normalized(player.direction);
    const Vec2 origin{player.position.x+offset.x,player.position.y+offset.y};
    const auto actor=mick_.view().actor();float target_distance=INFINITY,hit=0;
    bool target=ray_rect(origin,direction,length,mick_.interact_center(),actor.interact_extents,hit);
    if(target)target_distance=hit;
    if(ray_rect(origin,direction,length,mick_.collision_center(),actor.collision_extents,hit)){target=true;target_distance=std::min(target_distance,hit);}
    const Vec2 end{origin.x+direction.x*length,origin.y+direction.y*length};
    std::vector<MotionObstacle> obstacles;
    if(!collision_.collect({std::min(origin.x,end.x),std::min(origin.y,end.y)},{std::max(origin.x,end.x),std::max(origin.y,end.y)},obstacles)){error="Field Telepathy collider query rejected";return false;}
    auto cross=[](Vec2 a,Vec2 b){return a.x*b.y-a.y*b.x;};
    for(const auto&o:obstacles){
        const uint32_t edges=o.segment?1:o.count;
        for(uint32_t i=0;i<edges;++i){const auto p=o.points[i],q=o.points[(i+1)%o.count];
            const Vec2 a{p.x+o.offset.x-origin.x,p.y+o.offset.y-origin.y},edge{q.x-p.x,q.y-p.y};
            const float denominator=cross(direction,edge);if(denominator==0)continue;
            const float distance=cross(a,edge)/denominator,u=cross(a,direction)/denominator;
            if(distance>=0&&distance<=length&&u>=0&&u<=1&&distance<target_distance){error="Telepathy hit an unported field collider";return false;}
        }
    }
    // Other source actors are represented as notices rather than executable
    // thought targets. Conservatively reject nearby unknown actors, never invent
    // their thoughts or silently report an empty ray for unsupported content.
    for(uint32_t i=0;i<notice_active_.size();++i)if(notice_active_[i]){const auto n=map_.notice(i);
        if(map_.text(n.path)==mick_.view().string(actor.path))continue;
        const float x=n.position.x-origin.x,y=n.position.y-origin.y;
        if(x*x+y*y<=length*length){error="Nearby Telepathy target is not ported";return false;}
    }
    if(target){const auto at=mick_.position();Vec2 facing{at.x-player.position.x,at.y-player.position.y};
        if(std::abs(facing.x)>std::abs(facing.y)&&facing.x!=0)facing={sign(facing.x),0};else if(facing.y!=0)facing={0,sign(facing.y)};else facing=player.direction;
        if(!world.set_house_direction(facing)){error="Telepathy player turn rejected";return false;}
    }
    if(!world.pause_for_house()){error="Telepathy dialogue pause rejected";return false;}
    phase_=FieldPhase::MickTalk;mick_ignore_accept_=true;
    if(!mick_.start_thought(target,error))return fail(error.empty()?"Telepathy programme rejected":error.c_str());
    error.clear();return true;
}
bool FieldScene::circle_overlaps(Vec2 center,float radius)const{
    const auto& room=world.content();const auto scene=room.scene();const auto player=world.player().position;
    const float r2=radius*radius;bool inside=true;
    for(uint32_t i=0;i<scene.actor_hull_count;++i){
        const auto p=room.vertex(scene.actor_hull_first+i);const float x=p.x+player.x-center.x,y=p.y+player.y-center.y;
        if(x*x+y*y<=r2)return true;
        const auto q=room.vertex(scene.actor_hull_first+(i+1)%scene.actor_hull_count);
        const float ex=q.x-p.x,ey=q.y-p.y,len=ex*ex+ey*ey;
        if(len>0){float t=((center.x-player.x-p.x)*ex+(center.y-player.y-p.y)*ey)/len;t=std::clamp(t,0.f,1.f);
            const float dx=player.x+p.x+ex*t-center.x,dy=player.y+p.y+ey*t-center.y;if(dx*dx+dy*dy<=r2)return true;}
        const float cross=(p.x+player.x-center.x)*(q.y+player.y-center.y)-(p.y+player.y-center.y)*(q.x+player.x-center.x);
        if(cross<0)inside=false;
    }
    return inside&&scene.actor_hull_count>=3;
}
bool FieldScene::overlaps(Vec2 center,Vec2 extents,Vec2 player)const{
    // Same actor-hull SAT as HouseRuntime::overlaps; touching counts as contact.
    const auto& room=world.content();const auto scene=room.scene();
    const Vec2 rect[4]={{center.x-extents.x,center.y-extents.y},{center.x+extents.x,center.y-extents.y},{center.x+extents.x,center.y+extents.y},{center.x-extents.x,center.y+extents.y}};
    auto separated=[&](Vec2 axis){float amin=INFINITY,amax=-INFINITY,bmin=INFINITY,bmax=-INFINITY;
        for(uint32_t i=0;i<scene.actor_hull_count;++i){auto p=room.vertex(scene.actor_hull_first+i);const float dot=(p.x+player.x)*axis.x+(p.y+player.y)*axis.y;amin=std::min(amin,dot);amax=std::max(amax,dot);}
        for(auto p:rect){const float dot=p.x*axis.x+p.y*axis.y;bmin=std::min(bmin,dot);bmax=std::max(bmax,dot);}return amax<bmin||bmax<amin;};
    if(separated({1,0})||separated({0,1}))return false;
    for(uint32_t i=0;i<scene.actor_hull_count;++i){auto a=room.vertex(scene.actor_hull_first+i),b=room.vertex(scene.actor_hull_first+(i+1)%scene.actor_hull_count);if(separated({a.y-b.y,b.x-a.x}))return false;}
    return true;
}
bool FieldScene::before_physics(WalkInput& input){
    if(phase_==FieldPhase::Error)return false;
    if(story_revision_!=world.story_revision()){std::string why;if(!refresh_conditions(why))return fail(why);}
    if(mick_bound_){
        FieldMickWorld motion{*this};std::string why;const auto player=world.player().position;
        if(!mick_.simulate(1.f/60.f,player,camera_origin(player,viewport_),viewport_,world.house_paused()||world.stage()!=OpeningStage::Walking,motion,why))
            return fail(why.empty()?"Field Mick simulation rejected":why.c_str());
        apply_mick_body();
    }
    for(uint32_t i=0;i<npcs_.size();++i){auto& n=*npcs_[i];std::string why;
        if(n.motion.view().interaction_policy()){
            const auto a=n.motion.view().actor();const auto p=n.motion.position();
            if(!n.motion.observe_player(circle_overlaps({p.x+a.view_offset.x,p.y+a.view_offset.y},a.view_radius),why))return fail(why);
        }
        if(!actor_visible(n.actor))continue;
        if(world.actor_restore_requested(n.actor)){
            const auto pose=world.actor(n.actor);
            if(!n.motion.restore_motion_pose(pose.position,pose.direction,why)||!world.complete_actor_restore(n.actor,world.actor_binding_revision(n.actor)))return fail("Field NPC restoration rejected");
        }
        if(world.actor_bound(n.actor)){apply_npc_bodies();continue;}
        FieldMickWorld motion{*this,i+1,&n.solver};const auto p=world.player().position;
        const bool paused=world.house_paused()||world.stage()!=OpeningStage::Walking;
        if(n.motion.view().interaction_policy()){
            const auto talker=world.story_talker();const bool talking=talker.kind==DialogueTalkerKind::OriginalNpc&&talker.index==n.actor&&world.story_talking();
            n.motion.set_dialogue_talking(talking);
            // Source NPC blends before its stare callback updates the input.
            if(!npc_sprite_physics(n.sprite,n.motion.facing(),talking,n.motion.view().animated_idle()))return fail("Field NPC source sprite physics rejected");
        }
        if(!n.motion.simulate(1.f/60.f,p,camera_origin(p,viewport_),viewport_,paused,motion,why))return fail(why);
        if(world.stage()==OpeningStage::Walking&&!world.sync_unbound_actor_pose(n.actor,n.motion.position(),n.motion.facing()))return fail("Field NPC live pose handoff rejected");
        apply_npc_bodies();
    }
    input.paused=input.paused||blocks_player();
    input.entering_door=input.entering_door||phase_==FieldPhase::DoorAwaitIdle||phase_==FieldPhase::Transition;
    return true;
}
bool FieldScene::after_physics(){
    if(phase_==FieldPhase::Error)return false;
    if(!world.healthy())return fail(std::string("Field world: ")+world.error());
    if(world.stage()!=OpeningStage::Walking)return true;
    const auto position=world.player().position;bool any=false;
    const bool paused=world.house_paused();
    for(uint32_t i=0;i<door_inside_.size();++i){
        const auto d=map_.door(i);const bool inside=door_active_[i]&&overlaps(d.center,d.extents,position);
        const bool entered=inside&&!door_inside_[i];door_inside_[i]=inside;any|=inside;
        if(!entered||phase_!=FieldPhase::Walking)continue;
        if(d.route){pending_door_=i;phase_=FieldPhase::DoorAwaitIdle;}
        else {boundary_=i;phase_=FieldPhase::Unsupported;error_="Unported route to "+std::string(map_.text(d.target).empty()?"another scene":map_.text(d.target))+"; B returns";}
        if(!world.pause_for_house())return fail("Field door pause rejected");
    }
    for(uint32_t i=0;i<boundary_inside_.size();++i){
        const auto b=map_.boundary(i);
        if(b.kind==uint16_t(FieldBoundaryKind::Confirm))continue;
        const bool inside=boundary_active_[i]&&overlaps(b.center,b.extents,position);
        const bool entered=inside&&!boundary_inside_[i];boundary_inside_[i]=inside;any|=inside;
        if(!entered||phase_!=FieldPhase::Walking)continue;
        const auto bark=mick_bound_?mick_.view().string(mick_.view().actor().bark_path):std::string_view{};
        if(!bark.empty()&&map_.text(b.path)==bark){
            if(!world.pause_for_house())return fail("Field Mick bark pause rejected");
            phase_=FieldPhase::MickTalk;mick_ignore_accept_=true;
            std::string why;if(!mick_.start_bark(why))return fail(why.empty()?"Field Mick bark rejected":why.c_str());
            continue;
        }
        bool handled=false;
        if(story_.valid())for(uint32_t k=0;k<story_.programme_count();++k){const auto p=story_.programme(k);
            if(story_.text(p.boundary)!=map_.text(b.path))continue;
            std::string why;
            if(!story_host_||!story_host_->can_start(story_,p.programme,why)){
                boundary_=0x80000000u|i;phase_=FieldPhase::Unsupported;error_=why.empty()?"Field story admission rejected; B returns":why+"; B returns";
                if(!world.pause_for_house())return fail("Field story admission pause rejected");
            }else if(!world.begin_house_program(p.programme))return fail("Field story programme rejected");
            handled=true;break;
        }
        if(handled)continue;
        boundary_=0x80000000u|i;phase_=FieldPhase::Unsupported;error_=std::string(map_.text(b.label))+" not ported; B returns";
        if(!world.pause_for_house())return fail("Field boundary pause rejected");
    }
    for(uint32_t i=0;i<openables_.size();++i){
        const auto o=map_.openable(i);auto& s=openables_[i];const bool inside=overlaps(o.center,o.extents,position);
        if(inside&&!s.inside){
            // Openable Door.gd: open when unlocked, idle ("Normal") and its timer is stopped.
            if(!s.open&&s.timer<0){s.open=true;if(phase_!=FieldPhase::DoorAwaitIdle&&phase_!=FieldPhase::Transition&&!map_.text(o.sound).empty())sounds_.push_back(map_.text(o.sound));}
        }else if(!inside&&s.inside){
            if(paused){s.open=false;s.timer=-1;}else s.timer=o.timer;
        }
        s.inside=inside;
    }
    for(uint32_t i=0;i<camera_inside_.size();++i){const auto c=map_.camera(i);camera_inside_[i]=camera_active_[i]&&overlaps(c.center,c.extents,position);}
    if(!any&&phase_==FieldPhase::Walking){last_safe_position_=position;last_safe_direction_=world.player().direction;}
    return true;
}
bool FieldScene::finish_talk(){
    if(phase_!=FieldPhase::MickTalk||mick_.state()!=MickProgramState::Complete)return fail("Field Mick talk is still running");
    std::string why;
    if(!refresh_conditions(why))return fail(why.empty()?"Field Mick condition refresh rejected":why.c_str());
    if(!world.unpause_from_house())return fail("Field Mick unpause rejected");
    error_.clear();phase_=FieldPhase::Walking;return true;
}
bool FieldScene::idle_frame(double delta,bool back,bool accept,int choice){
    if(phase_==FieldPhase::Error)return false;
    if(!std::isfinite(delta)||delta<0||delta>1)return fail("Field idle time rejected");
    if(phase_==FieldPhase::DoorAwaitIdle){
        // Door body_entered pauses, then yields one idle frame before enter().
        route_=links_->find_route(map_.door(pending_door_).route);phase_=FieldPhase::Transition;
    }else if(phase_==FieldPhase::Unsupported&&back){
        if(!world.warp_same_scene(last_safe_position_,last_safe_direction_)||!world.unpause_from_house())return fail("Field boundary return rejected");
        boundary_=WorldLinksData::kNotFound;error_.clear();phase_=FieldPhase::Walking;
        for(uint32_t i=0;i<door_inside_.size();++i)door_inside_[i]=0;
        for(uint32_t i=0;i<boundary_inside_.size();++i)boundary_inside_[i]=0;
    }else if(phase_==FieldPhase::MickTalk){
        std::string why;
        const bool ignore=mick_ignore_accept_;mick_ignore_accept_=false;
        if(mick_.waiting_choice()){
            if(choice)mick_.move_choice(choice);
            if((back||(accept&&!ignore))&&!mick_.confirm_choice(back,why))return fail(why.empty()?"Field Mick choice rejected":why.c_str());
        }else if(mick_.state()==MickProgramState::WaitingText&&accept&&!ignore){
            if(!mick_.advance_text(why))return fail(why.empty()?"Field Mick text advance rejected":why.c_str());
        }
    }
    if(interaction_actor_!=UINT32_MAX&&world.stage()==OpeningStage::Walking&&!world.house_paused()){
        bool found=false;std::string why;
        for(auto& n:npcs_)if(n->actor==interaction_actor_){found=true;if(!n->motion.end_interaction(why))return fail(why);break;}
        if(!found)return fail("Field interaction owner missing at completion");
        interaction_actor_=UINT32_MAX;
    }
    // SceneTreeTimers run during player/menu pause and while an Actor hides its
    // replaced NPC. They are distinct from the NPC physics processing toggle.
    for(auto& n:npcs_){std::string why;
        if(n->motion.view().interaction_policy()&&!actor_idle_animations(n->sprite,delta))return fail("Field NPC source sprite idle rejected");
        if(!n->motion.idle_frame(delta,why))return fail(why);
    }
    const bool paused=world.house_paused();
    for(uint32_t i=0;i<openables_.size();++i){auto& s=openables_[i];if(s.timer<0)continue;s.timer-=delta;
        if(s.timer>0)continue;s.timer=-1;
        if(s.inside)continue;
        s.open=false;const auto o=map_.openable(i);if(!paused&&!map_.text(o.end_sound).empty())sounds_.push_back(map_.text(o.end_sound));}
    return true;
}
bool FieldScene::sprite_visible(uint32_t i)const{
    if(i>=sprite_active_.size()||!sprite_active_[i])return false;
    const auto openable=map_.sprite(i).openable;
    return openable==kFieldNoOpenable||!openable_open(openable);
}
Vec2 FieldScene::camera_origin(Vec2 player,Vec2 viewport)const{
    if(world.stage()!=OpeningStage::Walking){const auto center=world.cutscene_camera().center();return {std::floor(center.x-viewport.x*.5f),std::floor(center.y-viewport.y*.5f)};}
    float x=std::floor(player.x-viewport.x*.5f),y=std::floor(player.y-viewport.y*.5f);
    for(uint32_t i=0;i<camera_inside_.size();++i){
        if(!camera_inside_[i])continue;
        const auto c=map_.camera(i);
        // camarea.gd: limits from the ReferenceRect (or shape), never smaller than the view.
        const float left=c.has_rect?c.left:c.center.x-c.width*.5f,top=c.has_rect?c.top:c.center.y-c.height*.5f;
        const float w=std::max(c.width,viewport.x),h=std::max(c.height,viewport.y);
        x=std::floor(std::clamp(player.x-viewport.x*.5f,left,left+w-viewport.x))+c.offset.x;
        y=std::floor(std::clamp(player.y-viewport.y*.5f,top,top+h-viewport.y))+c.offset.y;
        break;
    }
    return {x,y};
}
std::vector<FieldNoticeView> FieldScene::nearby_notices(Vec2 center,float radius,uint32_t limit)const{
    std::vector<FieldNoticeView> out;
    for(uint32_t i=0;i<notice_active_.size();++i){if(!notice_active_[i])continue;const auto n=map_.notice(i);
        const float d=std::hypot(n.position.x-center.x,n.position.y-center.y);if(d<=radius)out.push_back({i,d});}
    std::stable_sort(out.begin(),out.end(),[](const FieldNoticeView&a,const FieldNoticeView&b){return a.distance<b.distance;});
    if(out.size()>limit)out.resize(limit);
    return out;
}
}
