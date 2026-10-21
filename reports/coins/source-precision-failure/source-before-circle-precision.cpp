/* Static-convex subset adapted from Godot Engine 3.6.2-stable:
 * servers/physics_2d/{collision_solver_2d_sat,shape_2d_sw,space_2d_sw}.cpp
 * scene/2d/physics_body_2d.cpp; commit 3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8.
 * Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md).
 * Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "encore/circle_motion.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
namespace encore::upstream {
namespace {
constexpr float epsilon=0.00001f;
Vec2 add(Vec2 a,Vec2 b) { return {a.x+b.x,a.y+b.y}; }
Vec2 sub(Vec2 a,Vec2 b) { return {a.x-b.x,a.y-b.y}; }
Vec2 mul(Vec2 a,float b) { return {a.x*b,a.y*b}; }
float dot(Vec2 a,Vec2 b) { return a.x*b.x+a.y*b.y; }
float cross(Vec2 a,Vec2 b) { return a.x*b.y-a.y*b.x; }
float length(Vec2 a) { return std::sqrt(dot(a,a)); }
Vec2 normalized(Vec2 a) { const float d=length(a);return d==0?Vec2{}:Vec2{a.x/d,a.y/d}; }
Vec2 div(Vec2 a,float b) { return {a.x/b,a.y/b}; }
Vec2 tangent(Vec2 a) { return {a.y,-a.x}; }
bool zero(Vec2 a) { return a.x==0&&a.y==0; }
bool bounded(Vec2 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::abs(a.x)<=1000000&&std::abs(a.y)<=1000000; }
Vec2 projected_slide(Vec2 a,Vec2 normal) { return sub(a,mul(normal,dot(a,normal))); }
struct Contact { Vec2 actor, obstacle; };
struct Contacts {
    std::array<Contact,32> data{};
    unsigned size=0;
    bool overflow=false;
    void append(Vec2 a,Vec2 b) {
        // Refuse unsupported contact density instead of silently losing shapes.
        if(size==data.size()){overflow=true;return;}
        data[size++]={a,b};
    }
};


struct Circle {float radius;Vec2 offset,minimum,maximum;};
void project(const CircleSolid&s,Vec2 axis,float&lo,float&hi){
 if(s.kind==CircleSolidKind::Rectangle){
  lo=1e20f;hi=-1e20f;
  for(unsigned i=0;i<4;++i){const auto p=add(s.center,{float(int(i&1)*2-1)*s.extents.x,float(int(i>>1)*2-1)*s.extents.y});const float d=dot(axis,p);lo=std::min(lo,d);hi=std::max(hi,d);}return;
 }
 lo=hi=dot(axis,s.points[0]);for(size_t i=1;i<s.points.size();++i){const float d=dot(axis,s.points[i]);lo=std::min(lo,d);hi=std::max(hi,d);}
}
unsigned supports(const CircleSolid&s,Vec2 normal,Vec2*out){
 normal=normalized(normal);
 if(s.kind==CircleSolidKind::Rectangle){
  if(std::abs(normal.x)>=0.99998f){const float x=s.extents.x*(normal.x>0?1.0f:-1.0f);out[0]=add(s.center,{x,s.extents.y});out[1]=add(s.center,{x,-s.extents.y});return 2;}
  if(std::abs(normal.y)>=0.99998f){const float y=s.extents.y*(normal.y>0?1.0f:-1.0f);out[0]=add(s.center,{s.extents.x,y});out[1]=add(s.center,{-s.extents.x,y});return 2;}
  out[0]=add(s.center,{normal.x<0?-s.extents.x:s.extents.x,normal.y<0?-s.extents.y:s.extents.y});return 1;
 }
 if(s.kind==CircleSolidKind::Segment){
  if(std::abs(dot(normal,s.normals[0]))>0.99998f){out[0]=s.points[0];out[1]=s.points[1];return 2;}
  out[0]=dot(normal,sub(s.points[1],s.points[0]))>0?s.points[1]:s.points[0];return 1;
 }
 size_t index=0;float best=-std::numeric_limits<float>::infinity();
 for(size_t i=0;i<s.points.size();++i){const float d=dot(normal,s.points[i]);if(d>best){best=d;index=i;}if(dot(s.normals[i],normal)>0.99998f){out[0]=s.points[i];out[1]=s.points[(i+1)%s.points.size()];return 2;}}
 out[0]=s.points[index];return 1;
}
Vec2 closest_on_line(Vec2 point,Vec2 a,Vec2 b){const Vec2 delta=sub(b,a);return add(a,mul(delta,dot(sub(point,a),delta)/dot(delta,delta)));}
bool collide(const Circle&c,Vec2 position,Vec2 motion,const CircleSolid&s,float margin,Contacts*contacts=nullptr){
 position=add(position,c.offset);
 float best_depth=1e15f;Vec2 best_axis{};const bool cast=!zero(motion),segment=s.kind==CircleSolidKind::Segment;
 const auto axis_test=[&](Vec2 axis){
  if(std::abs(axis.x)<epsilon&&std::abs(axis.y)<epsilon)axis={0,1};
  const float scale=length(axis),d=dot(axis,position);float cmin=d-c.radius*scale,cmax=d+c.radius*scale;
  if(cast){const float q=dot(axis,add(position,motion));cmin=std::min(cmin,q-c.radius*scale);cmax=std::max(cmax,q+c.radius*scale);}
  cmin-=margin;cmax+=margin;float smin,smax;project(s,axis,smin,smax);
  float amin=segment?smin:cmin,amax=segment?smax:cmax,bmin=segment?cmin:smin,bmax=segment?cmax:smax;
  bmin-=(amax-amin)*0.5f;bmax+=(amax-amin)*0.5f;
  float dmin=bmin-(amin+amax)*0.5f,dmax=bmax-(amin+amax)*0.5f;if(dmin>0||dmax<0)return false;dmin=std::abs(dmin);
  if(dmax<dmin){if(dmax<best_depth){best_depth=dmax;best_axis=axis;}}else if(dmin<best_depth){best_depth=dmin;best_axis=mul(axis,-1);}return true;
 };
 if(cast){const auto n=normalized(motion);if(!axis_test(n)||!axis_test(tangent(n)))return false;}
 const auto point_test=[&](Vec2 point){
  const auto delta=segment?sub(point,position):sub(position,point);
  return axis_test(normalized(delta))&&(!cast||axis_test(normalized(segment?sub(delta,motion):add(delta,motion))));
 };
 if(segment){
  if(!axis_test(tangent(normalized(sub(s.points[1],s.points[0]))))||!point_test(s.points[0])||!point_test(s.points[1]))return false;
 }else if(s.kind==CircleSolidKind::Rectangle){
  if(!axis_test({1,0})||!axis_test({0,1}))return false;
  const auto corner=[&](Vec2 p){const auto v=sub(p,s.center);return normalized(sub(add(s.center,{v.x<0?-s.extents.x:s.extents.x,v.y<0?-s.extents.y:s.extents.y}),p));};
  if(!axis_test(corner(position))||(cast&&!axis_test(corner(add(position,motion)))))return false;
 }else for(size_t i=0;i<s.points.size();++i)if(!point_test(s.points[i])||!axis_test(s.normals[i]))return false;
 if(zero(best_axis))return false;
 if(contacts){
  // Rest/recovery queries are stationary; source ordering makes the segment A
  // and circle B. Resolve its collector swap back to (circle, static solid).
  const auto normal=segment?best_axis:mul(best_axis,-1);Vec2 support[2];const unsigned count=supports(s,mul(normal,-1),support);
  const Vec2 p=add(add(position,mul(normalized(normal),c.radius)),mul(normal,margin));
  const Vec2 q=count==1?support[0]:closest_on_line(p,support[0],support[1]);contacts->append(p,q);
 }
 return true;
}
bool nearby(const Circle& actor,Vec2 position,Vec2 motion,const CircleSolid& obstacle,float grow) {
    if(obstacle.kind==CircleSolidKind::Segment) {
        // ConcavePolygonShape2DSW::cull: strict Rect2::intersects against the
        // actor's static projection, without motion or margin.
        const Vec2 lo{position.x+actor.minimum.x,position.y+actor.minimum.y},hi{position.x+actor.maximum.x,position.y+actor.maximum.y};
        return !(lo.x>=obstacle.maximum.x||hi.x<=obstacle.minimum.x||lo.y>=obstacle.maximum.y||hi.y<=obstacle.minimum.y);
    }
    const Vec2 lo{position.x+actor.minimum.x+std::min(0.0f,motion.x)-grow,position.y+actor.minimum.y+std::min(0.0f,motion.y)-grow};
    const Vec2 hi{position.x+actor.maximum.x+std::max(0.0f,motion.x)+grow,position.y+actor.maximum.y+std::max(0.0f,motion.y)+grow};
    return lo.x<=obstacle.maximum.x&&hi.x>=obstacle.minimum.x&&lo.y<=obstacle.maximum.y&&hi.y>=obstacle.minimum.y;
}
struct Motion { Vec2 position{},remainder{},normal{};bool collided=false; };
bool move(const Circle& actor,const std::vector<CircleSolid>& obstacles,Vec2 from,Vec2 motion,Motion& result,float margin) {
    const float minimum_depth=margin*0.05f;
    Vec2 position=from;bool recovered=false;
    for(unsigned attempt=0;attempt<4;++attempt) {
        Contacts contacts;
        for(const auto& obstacle:obstacles)if(nearby(actor,position,{},obstacle,margin))collide(actor,position,{},obstacle,margin,&contacts);
        if(contacts.overflow)return false;
        if(!contacts.size)break;
        recovered=true;Vec2 recovery{};
        for(unsigned i=0;i<contacts.size;++i) {
            const auto& c=contacts.data[i];const Vec2 n=normalized(sub(c.actor,c.obstacle));
            const float depth=dot(n,add(c.actor,recovery))-dot(n,c.obstacle);
            if(depth>minimum_depth+epsilon)recovery=sub(recovery,mul(mul(n,depth-minimum_depth),0.4f));
        }
        if(zero(recovery))break;
        position=add(position,recovery);
        if(!bounded(position))return false;
    }
    float safe=1,unsafe=1;
    for(const auto& obstacle:obstacles) {
        if(!nearby(actor,position,motion,obstacle,margin)||!collide(actor,position,motion,obstacle,0))continue;
        if(collide(actor,position,{},obstacle,0)){safe=unsafe=0;break;}
        float low=0,high=1,coefficient=0.5f;
        for(unsigned iteration=0;iteration<8;++iteration) {
            const float fraction=low+(high-low)*coefficient;
            if(collide(actor,position,mul(motion,fraction),obstacle,0)) {
                high=fraction;coefficient=(iteration==0||low>0)?0.5f:0.25f;
            } else {
                low=fraction;coefficient=(iteration==0||high<1)?0.5f:0.75f;
            }
        }
        if(low<safe){safe=low;unsafe=high;}
    }
    Vec2 normal{};float best_length=0;
    if(recovered||safe<1) {
        const Vec2 contact_position=add(position,mul(motion,unsafe));
        const float allowed_depth=std::min(length(motion),minimum_depth);
        for(const auto& obstacle:obstacles) {
            if(!nearby(actor,contact_position,{},obstacle,margin))continue;
            Contacts contacts;collide(actor,contact_position,{},obstacle,margin,&contacts);
            if(contacts.overflow)return false;
            for(unsigned i=0;i<contacts.size;++i) {
                const Vec2 relative=sub(contacts.data[i].obstacle,contacts.data[i].actor);const float d=length(relative);
                if(d<allowed_depth||d<=best_length)continue;
                best_length=d;normal=div(relative,d);
            }
        }
    }
    const bool collided=best_length!=0;
    // PhysicsServer returns travel plus recovery; Node2D adds that to its
    // original position. Preserve both rounded additions and their order.
    result.position=add(from,add(mul(motion,collided?safe:1.0f),sub(position,from)));
    result.remainder=collided?sub(motion,mul(motion,safe)):Vec2{};
    result.normal=normal;result.collided=collided;
    return bounded(result.position)&&bounded(result.remainder)&&bounded(normal);
}
}
bool CircleMotionSolver::validate(const CircleSolid&s){
 if(!bounded(s.minimum)||!bounded(s.maximum)||s.minimum.x>s.maximum.x||s.minimum.y>s.maximum.y)return false;
 if(s.kind==CircleSolidKind::Rectangle){
  if(!s.points.empty()||!s.normals.empty()||!bounded(s.center)||!bounded(s.extents)||s.extents.x<=0||s.extents.y<=0)return false;
  return s.minimum.x==s.center.x-s.extents.x&&s.maximum.x==s.center.x+s.extents.x&&s.minimum.y==s.center.y-s.extents.y&&s.maximum.y==s.center.y+s.extents.y;
 }
 if(s.kind!=CircleSolidKind::Convex&&s.kind!=CircleSolidKind::Segment)return false;
 const bool seg=s.kind==CircleSolidKind::Segment;
 if((seg?s.points.size()!=2:(s.points.size()<3||s.points.size()>64))||s.normals.size()!=(seg?1:s.points.size()))return false;
 for(auto p:s.points)if(!bounded(p)||p.x<s.minimum.x||p.x>s.maximum.x||p.y<s.minimum.y||p.y>s.maximum.y)return false;
 for(auto n:s.normals)if(!bounded(n)||(!zero(n)&&std::abs(length(n)-1)>0.0001f))return false;
 if(seg)return length(sub(s.points[1],s.points[0]))>epsilon;
 double area=0;for(size_t i=0;i<s.points.size();++i){const auto a=s.points[i],b=s.points[(i+1)%s.points.size()];area+=double(a.x)*b.y-double(a.y)*b.x;}
 if(area<=epsilon)return false;
 for(size_t i=0;i<s.points.size();++i){const auto edge=sub(s.points[(i+1)%s.points.size()],s.points[i]);if(length(edge)<=epsilon)return false;for(size_t j=0;j<s.points.size();++j)if(j!=i&&j!=(i+1)%s.points.size()&&cross(edge,sub(s.points[j],s.points[i]))<=epsilon)return false;if(length(sub(normalized(tangent(edge)),s.normals[i]))>0.0001f)return false;}
 return true;
}
bool CircleMotionSolver::configure(float radius,Vec2 offset,float margin,const std::vector<CircleSolid>&solids){
 valid_=false;solids_.clear();
 if(!std::isfinite(radius)||radius<=0||radius>10000||!bounded(offset)||!std::isfinite(margin)||margin<0||margin>100||solids.size()>4096)return false;
 for(const auto&s:solids)if(!validate(s))return false;
 radius_=radius;offset_=offset;margin_=margin;solids_=solids;valid_=true;return true;
}
bool CircleMotionSolver::slide(Vec2 position,Vec2 velocity,SlideResult&out)const{return advance(position,velocity,1.0f/60.0f,out);}
bool CircleMotionSolver::advance(Vec2 position,Vec2 velocity,float delta,SlideResult&out)const{
 if(!valid_||!bounded(position)||!bounded(velocity)||!std::isfinite(delta)||delta<=0||delta>1)return false;
 const Circle actor{radius_,offset_,{offset_.x-radius_,offset_.y-radius_},{offset_.x+radius_,offset_.y+radius_}};Vec2 motion=mul(velocity,delta);if(!bounded(motion))return false;
 const auto*solids=&solids_;
 if(source_){const float reach=length(motion)+margin_+1.0f;const Vec2 lo{position.x+actor.minimum.x-reach,position.y+actor.minimum.y-reach},hi{position.x+actor.maximum.x+reach,position.y+actor.maximum.y+reach};collected_.clear();if(!source_->collect(lo,hi,collected_)||collected_.size()+solids_.size()>4096)return false;
  for(const auto&s:collected_)if(!validate(s))return false;
  combined_.assign(solids_.begin(),solids_.end());combined_.insert(combined_.end(),collected_.begin(),collected_.end());solids=&combined_;
 }
 for(unsigned iteration=0;iteration<4;++iteration){Motion moved;if(!move(actor,*solids,position,motion,moved,margin_))return false;position=moved.position;if(!moved.collided)break;motion=projected_slide(moved.remainder,moved.normal);velocity=projected_slide(velocity,moved.normal);if(zero(motion))break;}
 if(!bounded(position)||!bounded(velocity))return false;out={position,velocity};return true;
}
}
