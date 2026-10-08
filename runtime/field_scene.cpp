#include "encore/field_scene.hpp"
#include <algorithm>
#include <cmath>

namespace encore::upstream {
namespace {
bool intersects(Vec2 alo,Vec2 ahi,Vec2 blo,Vec2 bhi){return alo.x<=bhi.x&&ahi.x>=blo.x&&alo.y<=bhi.y&&ahi.y>=blo.y;}
Vec2 normalized(Vec2 v){const float d=std::sqrt(v.x*v.x+v.y*v.y);return d==0?Vec2{}:Vec2{v.x/d,v.y/d};}
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
    for(uint32_t i=0;i<layer_active.size();++i)if(layer_active[i]&&(map.layer(i).flags&uint32_t(FieldLayerFlag::Collides)))layers_.push_back(i);
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
    for(const auto li:layers_){
        const auto layer=map_.layer(li);
        for(uint32_t c=0;c<layer.chunk_count;++c){
            const auto chunk=map_.chunk(layer.chunk_first+c);
            if(!intersects(lo,hi,chunk.minimum,chunk.maximum))continue;
            for(uint32_t k=0;k<chunk.count;++k){
                const auto cell=map_.cell(chunk.first+k);const auto tile=map_.tile(cell.tile);
                if(!tile.shape_count)continue;
                const Vec2 offset{layer.position.x+float(cell.x)*16.f,layer.position.y+float(cell.y)*16.f};
                for(uint32_t s=0;s<tile.shape_count;++s)push(tile.shape_first+s,offset);
            }
        }
    }
    for(const auto bi:bodies_){const auto b=map_.body(bi);for(uint32_t s=0;s<b.shape_count;++s)push(b.shape_first+s,{});}
    if(out.size()>4096)return false;
    return true;
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
    const std::vector<bool> reviewed(room.flag_count(),true);
    if(!world.initialize_restored(room,story_flags,reviewed,position,direction,viewport))return reject(std::string("Field world rejected: ")+world.error());
    map_=map;links_=&links;
    const auto layers=map.count(FieldMapSection::Layers),sprites=map.count(FieldMapSection::Sprites),items=map.count(FieldMapSection::Items);
    const auto doors=map.count(FieldMapSection::Doors),boundaries=map.count(FieldMapSection::Boundaries),cameras=map.count(FieldMapSection::Cameras);
    const auto notices=map.count(FieldMapSection::Notices),bodies=map.count(FieldMapSection::Bodies),openables=map.count(FieldMapSection::Openables);
    // FlagLandmark conditions are evaluated once at Ready; no Podunk flag can change in this slice.
    layer_active_.assign(layers,false);for(uint32_t i=0;i<layers;++i)layer_active_[i]=conditions(map.layer(i).conditions);
    sprite_active_.assign(sprites,false);for(uint32_t i=0;i<sprites;++i)sprite_active_[i]=conditions(map.sprite(i).conditions);
    item_active_.assign(items,false);for(uint32_t i=0;i<items;++i)item_active_[i]=conditions(map.item(i).conditions);
    door_active_.assign(doors,false);for(uint32_t i=0;i<doors;++i)door_active_[i]=conditions(map.door(i).conditions);
    boundary_active_.assign(boundaries,false);for(uint32_t i=0;i<boundaries;++i)boundary_active_[i]=conditions(map.boundary(i).conditions);
    camera_active_.assign(cameras,false);for(uint32_t i=0;i<cameras;++i)camera_active_[i]=conditions(map.camera(i).conditions);
    notice_active_.assign(notices,false);for(uint32_t i=0;i<notices;++i)notice_active_[i]=conditions(map.notice(i).conditions);
    std::vector<bool> body_active(bodies,false);for(uint32_t i=0;i<bodies;++i)body_active[i]=conditions(map.body(i).conditions);
    std::vector<bool> collide(layers,false);for(uint32_t i=0;i<layers;++i)collide[i]=layer_active_[i];
    for(uint32_t i=0;i<doors;++i){const auto d=map.door(i);if(!d.route)continue;
        const auto r=links.find_route(d.route);
        if(r==WorldLinksData::kNotFound||links.route(r).from!=map.scene_id()||links.route(r).door!=map.text(d.path))return reject("Field door route binding rejected");}
    for(uint32_t i=0;i<openables;++i)if(!conditions(map.openable(i).conditions))return reject("Conditional openable doors are not reviewed");
    std::string why;
    if(!collision_.bind(map,collide,body_active,why))return reject(why);
    world.attach_obstacles(&collision_);
    door_inside_.assign(doors,0);boundary_inside_.assign(boundaries,0);camera_inside_.assign(cameras,0);
    openables_.assign(openables,Openable{});sounds_.clear();
    route_=pending_door_=boundary_=WorldLinksData::kNotFound;
    last_safe_position_=position;last_safe_direction_=direction;
    if(!world.pause_for_house())return reject("Field player pause rejected");
    phase_=FieldPhase::Transition;error_.clear();error.clear();return true;
}
bool FieldScene::finish_transition(){
    if(phase_!=FieldPhase::Transition)return fail("Field transition finish outside its boundary");
    if(!world.unpause_from_house())return fail("Field player unpause rejected");
    phase_=FieldPhase::Walking;return true;
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
    input.paused=input.paused||blocks_player();
    input.entering_door=input.entering_door||phase_==FieldPhase::DoorAwaitIdle||phase_==FieldPhase::Transition;
    return true;
}
bool FieldScene::after_physics(){
    if(phase_==FieldPhase::Error)return false;
    if(!world.healthy())return fail(std::string("Field world: ")+world.error());
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
        const auto b=map_.boundary(i);const bool inside=boundary_active_[i]&&overlaps(b.center,b.extents,position);
        const bool entered=inside&&!boundary_inside_[i];boundary_inside_[i]=inside;any|=inside;
        if(!entered||phase_!=FieldPhase::Walking)continue;
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
bool FieldScene::idle_frame(double delta,bool back){
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
