#include "encore/field_motion.hpp"
#include <algorithm>
#include <cmath>

namespace encore::upstream {namespace {
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1000000&&std::abs(p.y)<=1000000;}
bool contains(FieldMapRect a,MotionQueryBounds b){return b.initialized&&a.minimum.x<=b.minimum.x&&a.minimum.y<=b.minimum.y&&a.maximum.x>=b.maximum.x&&a.maximum.y>=b.maximum.y;}
Vec2 transform(FieldMapShapeTransform t,Vec2 p){return {t.x.x*p.x+t.y.x*p.y+t.origin.x,t.x.y*p.x+t.y.y*p.y+t.origin.y};}
bool inverse_query(FieldMapShapeTransform t,FieldMapRect world,FieldMapRect& local){
    const float det=t.x.x*t.y.y-t.x.y*t.y.x;
    if(!std::isfinite(det)||det==0)return false;
    bool first=true;
    for(float x:{world.minimum.x,world.maximum.x})for(float y:{world.minimum.y,world.maximum.y}){
        const Vec2 d{x-t.origin.x,y-t.origin.y};
        const Vec2 p{(t.y.y*d.x-t.y.x*d.y)/det,(-t.x.y*d.x+t.x.x*d.y)/det};
        if(!finite(p))return false;
        if(first){local={p,p};first=false;}else{
            local.minimum.x=std::min(local.minimum.x,p.x);local.minimum.y=std::min(local.minimum.y,p.y);
            local.maximum.x=std::max(local.maximum.x,p.x);local.maximum.y=std::max(local.maximum.y,p.y);
        }
    }return true;
}
}
bool FieldMapMotion::configure(const FieldMapView& map,const RoomView& source,uint32_t mask,FieldMapGateQuery gates,std::string& e){
    map_=nullptr;commit_.clear();
    if(!map.valid()||map.source_scene().empty()||!source.valid()||!mask){e="Field collision requires checked map, actor and mask";return false;}
    const auto identity=map.identity();
    if(!std::equal(identity.upstream_commit.begin(),identity.upstream_commit.end(),source.bytes()+56)){e="Field collision source pin mismatch";return false;}
    const auto scene=source.scene();ConvexPolygon actor;
    for(uint32_t i=0;i<scene.actor_hull_count;++i)actor.vertices.push_back(source.vertex(scene.actor_hull_first+i));
    const float margin=source.rule_f32(RoomRuleKey::ActorCollisionSafeMargin);
    if(!solver_.configure_geometry(actor,{},margin)){e="Field actor collision hull rejected";return false;}
    if(map.gate_count()&&!gates){e="Field collision requires source flag lifecycle";return false;}
    minimum_=maximum_=actor.vertices.front();
    for(auto p:actor.vertices){minimum_.x=std::min(minimum_.x,p.x);minimum_.y=std::min(minimum_.y,p.y);maximum_.x=std::max(maximum_.x,p.x);maximum_.y=std::max(maximum_.y,p.y);}
    actor_=std::move(actor);mask_=mask;margin_=margin;gates_=std::move(gates);
    constexpr char hex[]="0123456789abcdef";
    for(auto b:identity.upstream_commit){commit_+=hex[b>>4];commit_+=hex[b&15];}
    map_=&map;error_.clear();e.clear();return true;
}
bool FieldMapMotion::geometry(FieldMapRect area)const{
    if(!map_->collision_polygons(area,mask_,gates_,4096,polygons_,error_))return false;
    obstacles_.clear();
    for(auto index:polygons_){
        const auto p=map_->polygon(index);
        if(p.kind==2){
            FieldMapRect local;const auto t=map_->shape_transform(p.transform);
            if(!inverse_query(t,area,local)){error_="Field collision transform rejected";return false;}
            if(!map_->concave_segments(index,local,4096-obstacles_.size(),pairs_,error_))return false;
            const auto g=map_->local_geometry(p.geometry);
            for(auto pair:pairs_){
                const auto a=transform(t,map_->local_point(g.point_first+pair*2)),b=transform(t,map_->local_point(g.point_first+pair*2+1));
                obstacles_.push_back({StaticObstacleKind::Segment,{a,b}});
            }
        }else{
            if(obstacles_.size()==4096||p.point_count>64){error_="Field collision shape capacity exceeded";return false;}
            StaticObstacle shape;shape.kind=p.kind==1?StaticObstacleKind::Convex:StaticObstacleKind::NativeDegenerateConvex;
            for(uint32_t j=0;j<p.point_count;++j)shape.vertices.push_back(map_->point(p.point_first+j));
            obstacles_.push_back(std::move(shape));
        }
    }
    if(!solver_.configure_geometry(actor_,obstacles_,margin_)){error_="Field static collision geometry rejected";return false;}
    return true;
}
bool FieldMapMotion::slide(Vec2 position,Vec2 velocity,SlideResult& result)const{
    if(!admitted()||!finite(position)||!finite(velocity)){error_="Field movement input/backend rejected";return false;}
    const Vec2 motion{velocity.x/60,velocity.y/60};
    FieldMapRect area{{position.x+minimum_.x+std::min(0.0f,motion.x)-margin_,position.y+minimum_.y+std::min(0.0f,motion.y)-margin_},
                      {position.x+maximum_.x+std::max(0.0f,motion.x)+margin_,position.y+maximum_.y+std::max(0.0f,motion.y)+margin_}};
    // An overlap recovery can enter a new spatial leaf. Re-run from the original
    // position after fetching every newly visited region; never commit the
    // tentative result while an unqueried wall could affect the path.
    for(unsigned attempt=0;attempt<16;++attempt){
        if(!finite(area.minimum)||!finite(area.maximum)||!geometry(area))return false;
        SlideResult candidate;MotionQueryBounds visited;
        if(!solver_.slide_with_bounds(position,velocity,candidate,visited)){error_="Field source movement rejected";return false;}
        if(contains(area,visited)){result=candidate;error_.clear();return true;}
        area.minimum.x=std::min(area.minimum.x,visited.minimum.x);area.minimum.y=std::min(area.minimum.y,visited.minimum.y);
        area.maximum.x=std::max(area.maximum.x,visited.maximum.x);area.maximum.y=std::max(area.maximum.y,visited.maximum.y);
    }
    error_="Field collision query closure exceeded capacity";return false;
}
}
