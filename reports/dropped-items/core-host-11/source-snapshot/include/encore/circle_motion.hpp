#pragma once
#include "encore/movement.hpp"
#include <vector>

namespace encore::upstream {
// Keep the original shape kind: a RectangleShape2D and the same four-point
// ConvexPolygonShape2D use different SAT axes in Godot. Rectangles here admit
// translation only. The scene owner filters original layers/disabled shapes.
enum class CircleSolidKind:uint32_t {Convex=1,Segment=2,Rectangle=3};
struct CircleSolid {
 CircleSolidKind kind=CircleSolidKind::Convex;
 std::vector<Vec2>points,normals;
 Vec2 minimum{},maximum{},center{},extents{};
};
class CircleSolidSource {
public:
 virtual ~CircleSolidSource()=default;
 virtual bool collect(Vec2 minimum,Vec2 maximum,std::vector<CircleSolid>&)const=0;
};
// Translated CircleShape2D against static convex, concave segments, and
// translated rectangles. Uses the reviewed Godot recovery/cast/rest/slide
// subset. Dynamic-body velocities, one-way surfaces, rotated/scaled shapes and
// arbitrary broadphase ordering are not admitted by this interface.
class CircleMotionSolver final:public MotionSolver {
public:
 bool configure(float radius,Vec2 offset,float margin,const std::vector<CircleSolid>&);
 void attach_source(const CircleSolidSource*source){source_=source;}
 bool valid()const{return valid_;}
 bool matches(float radius,Vec2 offset,float margin)const{return valid_&&radius==radius_&&offset.x==offset_.x&&offset.y==offset_.y&&margin==margin_;}
 bool slide(Vec2 position,Vec2 velocity,SlideResult&)const override;
 bool advance(Vec2 position,Vec2 velocity,float delta,SlideResult&)const;
 static bool validate(const CircleSolid&);
private:
 bool valid_=false;float radius_=0,margin_=0;Vec2 offset_{};
 std::vector<CircleSolid>solids_;
 const CircleSolidSource*source_=nullptr;
 mutable std::vector<CircleSolid>collected_,combined_;
};
}
