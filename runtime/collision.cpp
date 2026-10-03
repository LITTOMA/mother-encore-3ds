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
#include "encore/collision.hpp"
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

template<class Shape> void project(const Shape& shape,Vec2 position,Vec2 axis,float& lo,float& hi) {
    lo=hi=dot(axis,add(position,shape.vertices[0]));
    for(std::size_t i=1;i<shape.vertices.size();++i) {
        const float d=dot(axis,add(position,shape.vertices[i]));
        lo=std::min(lo,d);hi=std::max(hi,d);
    }
}
template<class Shape> unsigned supports(const Shape& shape,Vec2 position,Vec2 normal,Vec2* output) {
    std::size_t index=0;float best=-std::numeric_limits<float>::infinity();
    for(std::size_t i=0;i<shape.vertices.size();++i) {
        const float d=dot(normal,shape.vertices[i]);
        if(d>best){best=d;index=i;}
        if(dot(shape.normals[i],normal)>0.99998f) {
            output[0]=add(position,shape.vertices[i]);output[1]=add(position,shape.vertices[(i+1)%shape.vertices.size()]);return 2;
        }
    }
    output[0]=add(position,shape.vertices[index]);return 1;
}
Vec2 closest_on_line(Vec2 point,Vec2 a,Vec2 b) {
    const Vec2 delta=sub(b,a);return add(a,mul(delta,dot(sub(point,a),delta)/dot(delta,delta)));
}
void generate_contacts(Vec2* a,unsigned na,Vec2* b,unsigned nb,Vec2 normal,Contacts& out) {
    if(na==1&&nb==1){out.append(a[0],b[0]);return;}
    if(na==1){out.append(a[0],closest_on_line(a[0],b[0],b[1]));return;}
    if(nb==1){out.append(closest_on_line(b[0],a[0],a[1]),b[0]);return;}
    const Vec2 t=tangent(normal);
    const float da=dot(normal,a[0]),db=dot(normal,b[0]);
    struct Endpoint { float distance; bool actor; unsigned index; };
    std::array<Endpoint,4> points{{{dot(t,a[0]),true,0},{dot(t,a[1]),true,1},{dot(t,b[0]),false,0},{dot(t,b[1]),false,1}}};
    // Stable order for ties. Ordering among multiple shapes remains input order.
    for(unsigned i=1;i<points.size();++i) {
        const Endpoint value=points[i];unsigned j=i;
        while(j>0&&value.distance<points[j-1].distance){points[j]=points[j-1];--j;}
        points[j]=value;
    }
    for(unsigned i=1;i<=2;++i) {
        Vec2 pa,pb;
        if(points[i].actor){pa=a[points[i].index];pb=sub(pa,mul(normal,dot(normal,pa)-db));}
        else {pb=b[points[i].index];pa=sub(pb,mul(normal,dot(normal,pb)-da));}
        if(dot(normal,pa)>dot(normal,pb)-epsilon)continue;
        out.append(pa,pb);
    }
}

// The cast tests the Minkowski sweep, then uses Godot's eight adaptive fraction
// refinements. Margin is used for recovery/rest contacts, not the motion cast.
template<class Shape> bool solve(const Shape& a,Vec2 position,Vec2 motion,const Shape& b,float safe_margin,Contacts* contacts=nullptr) {
    float best_depth=1e15f;Vec2 best_axis{};
    const bool cast=!zero(motion);
    const auto test_axis=[&](Vec2 axis) {
        if(std::abs(axis.x)<epsilon&&std::abs(axis.y)<epsilon)axis={0,1};
        float amin,amax,bmin,bmax;
        project(a,position,axis,amin,amax);
        if(cast){float lo,hi;project(a,add(position,motion),axis,lo,hi);amin=std::min(amin,lo);amax=std::max(amax,hi);}
        project(b,{},axis,bmin,bmax);
        amin-=safe_margin;amax+=safe_margin;
        bmin-=(amax-amin)*0.5f;bmax+=(amax-amin)*0.5f;
        float dmin=bmin-(amin+amax)*0.5f,dmax=bmax-(amin+amax)*0.5f;
        if(dmin>0||dmax<0)return false;
        dmin=std::abs(dmin);
        if(dmax<dmin){if(dmax<best_depth){best_depth=dmax;best_axis=axis;}}
        else if(dmin<best_depth){best_depth=dmin;best_axis=mul(axis,-1);}
        return true;
    };
    if(cast){const Vec2 n=normalized(motion);if(!test_axis(n)||!test_axis(tangent(n)))return false;}
    for(Vec2 n:a.normals)if(!test_axis(n))return false;
    for(Vec2 n:b.normals)if(!test_axis(n))return false;
    if(safe_margin>0) {
        for(Vec2 av:a.vertices)for(Vec2 bv:b.vertices) {
            const Vec2 delta=sub(add(position,av),bv);
            if(!test_axis(normalized(delta)))return false;
            if(cast&&!test_axis(normalized(add(delta,motion))))return false;
        }
    }
    if(zero(best_axis))return false;
    if(contacts) {
        // This backend only requests contacts for stationary shape tests.
        Vec2 av[2],bv[2];const unsigned na=supports(a,position,mul(best_axis,-1),av),nb=supports(b,{},best_axis,bv);
        for(unsigned i=0;i<na;++i)av[i]=sub(av[i],mul(best_axis,safe_margin));
        generate_contacts(av,na,bv,nb,best_axis,*contacts);
    }
    return true;
}

template<class Shape> bool nearby(const Shape& actor,Vec2 position,Vec2 motion,const Shape& obstacle,float grow) {
    const Vec2 lo{position.x+actor.minimum.x+std::min(0.0f,motion.x)-grow,position.y+actor.minimum.y+std::min(0.0f,motion.y)-grow};
    const Vec2 hi{position.x+actor.maximum.x+std::max(0.0f,motion.x)+grow,position.y+actor.maximum.y+std::max(0.0f,motion.y)+grow};
    return lo.x<=obstacle.maximum.x&&hi.x>=obstacle.minimum.x&&lo.y<=obstacle.maximum.y&&hi.y>=obstacle.minimum.y;
}
struct Motion { Vec2 position{},remainder{},normal{};bool collided=false; };
template<class Shape> bool move(const Shape& actor,const std::vector<Shape>& obstacles,Vec2 from,Vec2 motion,Motion& result,float margin) {
    const float minimum_depth=margin*0.05f;
    Vec2 position=from;bool recovered=false;
    for(unsigned attempt=0;attempt<4;++attempt) {
        Contacts contacts;
        for(const auto& obstacle:obstacles)if(nearby(actor,position,{},obstacle,margin))solve(actor,position,{},obstacle,margin,&contacts);
        if(contacts.overflow)return false;
        if(!contacts.size)break;
        recovered=true;Vec2 recovery{};
        for(unsigned i=0;i<contacts.size;++i) {
            const auto& c=contacts.data[i];const Vec2 n=normalized(sub(c.actor,c.obstacle));
            const float depth=dot(n,add(c.actor,recovery))-dot(n,c.obstacle);
            if(depth>minimum_depth+epsilon)recovery=sub(recovery,mul(n,(depth-minimum_depth)*0.4f));
        }
        if(zero(recovery))break;
        position=add(position,recovery);
        if(!bounded(position))return false;
    }
    float safe=1,unsafe=1;
    for(const auto& obstacle:obstacles) {
        if(!nearby(actor,position,motion,obstacle,margin)||!solve(actor,position,motion,obstacle,0))continue;
        if(solve(actor,position,{},obstacle,0)){safe=unsafe=0;break;}
        float low=0,high=1,coefficient=0.5f;
        for(unsigned iteration=0;iteration<8;++iteration) {
            const float fraction=low+(high-low)*coefficient;
            if(solve(actor,position,mul(motion,fraction),obstacle,0)) {
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
            Contacts contacts;solve(actor,contact_position,{},obstacle,margin,&contacts);
            if(contacts.overflow)return false;
            for(unsigned i=0;i<contacts.size;++i) {
                const Vec2 relative=sub(contacts.data[i].obstacle,contacts.data[i].actor);const float d=length(relative);
                if(d<allowed_depth||d<=best_length)continue;
                best_length=d;normal=div(relative,d);
            }
        }
    }
    const bool collided=best_length!=0;
    result.position=add(position,mul(motion,collided?safe:1.0f));
    result.remainder=collided?sub(motion,mul(motion,safe)):Vec2{};
    result.normal=normal;result.collided=collided;
    return bounded(result.position)&&bounded(result.remainder)&&bounded(normal);
}
}

bool StaticMotionSolver::prepare(PointRange points,Shape& result) {
    if(points.size()<3||points.size()>64)return false;
    Shape shape;shape.vertices=points;
    for(Vec2 v:shape.vertices)if(!bounded(v))return false;
    double area=0;
    for(size_t i=0;i<points.size();++i){const auto a=points[i],b=points[(i+1)%points.size()];area+=double(a.x)*b.y-double(a.y)*b.x;}
    if(std::abs(area)<epsilon)return false;
    shape.vertices.reverse=area<0;
    shape.minimum=shape.maximum=shape.vertices[0];
    const uint32_t first=uint32_t(normal_cache_.size());
    for(size_t i=0;i<shape.vertices.size();++i){
        const Vec2 v=shape.vertices[i],edge=sub(shape.vertices[(i+1)%shape.vertices.size()],v);
        if(length(edge)<epsilon)return false;
        for(size_t j=0;j<shape.vertices.size();++j)if(j!=i&&j!=(i+1)%shape.vertices.size())if(cross(edge,sub(shape.vertices[j],v))<=epsilon)return false;
        normal_cache_.push_back(normalized(tangent(edge)));
        shape.minimum.x=std::min(shape.minimum.x,v.x);shape.minimum.y=std::min(shape.minimum.y,v.y);
        shape.maximum.x=std::max(shape.maximum.x,v.x);shape.maximum.y=std::max(shape.maximum.y,v.y);
    }
    shape.normals={nullptr,normal_cache_.data(),first,uint32_t(shape.vertices.size()),false};
    result=shape;return true;
}
bool StaticMotionSolver::configure(const ConvexPolygon& actor,const std::vector<ConvexPolygon>& obstacles,float margin) {
    valid_=false;actor_={};obstacles_.clear();vertex_cache_.clear();normal_cache_.clear();content_={};
    if(!std::isfinite(margin)||margin<0||margin>100||obstacles.size()>4096)return false;
    size_t total=actor.vertices.size();for(const auto& p:obstacles)total+=p.vertices.size();
    if(total>4097*64)return false;
    vertex_cache_.reserve(total);normal_cache_.reserve(total);obstacles_.reserve(obstacles.size());
    auto append=[&](const ConvexPolygon& polygon,Shape& out){const auto first=uint32_t(vertex_cache_.size());vertex_cache_.insert(vertex_cache_.end(),polygon.vertices.begin(),polygon.vertices.end());return prepare({nullptr,vertex_cache_.data(),first,uint32_t(polygon.vertices.size()),false},out);};
    if(!append(actor,actor_))return false;
    for(const auto& polygon:obstacles){Shape shape;if(!append(polygon,shape))return false;obstacles_.push_back(shape);}
    margin_=margin;valid_=true;return true;
}
bool StaticMotionSolver::configure_room(const RoomView& content,const std::vector<uint32_t>& active,const std::vector<Vec2>& offsets) {
    valid_=false;actor_={};obstacles_.clear();vertex_cache_.clear();normal_cache_.clear();
    if(!content.valid()||active.size()>4096||(!offsets.empty()&&offsets.size()!=content.polygon_count()))return false;
    for(const auto offset:offsets)if(!bounded(offset))return false;
    content_=content;
    const auto scene=content.scene();size_t total=scene.actor_hull_count;
    for(auto index:active){if(index>=content.polygon_count())return false;total+=content.polygon(index).vertex_count;}
    normal_cache_.reserve(total);obstacles_.reserve(active.size());
    if(!prepare({&content_,nullptr,scene.actor_hull_first,scene.actor_hull_count,false},actor_))return false;
    for(auto index:active){const auto polygon=content.polygon(index);Shape shape;if(!prepare({&content_,nullptr,polygon.first_vertex,polygon.vertex_count,false,offsets.empty()?Vec2{}:offsets[index]},shape))return false;obstacles_.push_back(shape);}
    margin_=content.rule_f32(RoomRuleKey::ActorCollisionSafeMargin);valid_=true;return true;
}
bool StaticMotionSolver::slide(Vec2 position,Vec2 velocity,SlideResult& result) const {
    if(!valid_||!bounded(position)||!bounded(velocity))return false;
    Vec2 motion=mul(velocity,1.0f/60.0f);
    for(unsigned iteration=0;iteration<4;++iteration) {
        Motion moved;
        if(!move(actor_,obstacles_,position,motion,moved,margin_))return false;
        position=moved.position;
        if(!moved.collided)break;
        motion=projected_slide(moved.remainder,moved.normal);
        velocity=projected_slide(velocity,moved.normal);
        if(zero(motion))break;
    }
    if(!bounded(position)||!bounded(velocity))return false;
    result={position,velocity};return true;
}
}
