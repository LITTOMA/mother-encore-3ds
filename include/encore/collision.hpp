#pragma once
#include "encore/movement.hpp"
#include "encore/room_data.hpp"
#include <vector>

namespace encore::upstream {
// Actor vertices are relative to its origin (including the source shape-node
// offset); obstacle vertices are in world coordinates. Both winding orders are
// accepted. Shapes must be strictly convex, finite, nondegenerate polygons.
struct ConvexPolygon { std::vector<Vec2> vertices; };
enum class StaticObstacleKind:uint8_t { Convex=1,Segment=2,NativeDegenerateConvex=3 };
struct StaticObstacle {StaticObstacleKind kind=StaticObstacleKind::Convex;std::vector<Vec2> vertices;};
struct MotionQueryBounds {Vec2 minimum{},maximum{};bool initialized=false;};

// Narrow Godot 3.6.2 static-convex motion backend: one translated actor hull,
// static solid polygons, 60 Hz, margin .08, no up direction, four slides.
// Callers filter collision layers/disabled shapes and decompose concave source
// shapes beforehand. Dynamic bodies, one-way shapes, rays and floor/snap state
// are outside this interface. Input polygon order is deterministic; it does not
// reproduce Godot's broadphase contact ordering for arbitrary overlapping sets.
class StaticMotionSolver final : public MotionSolver {
public:
    StaticMotionSolver()=default;
    StaticMotionSolver(const ConvexPolygon& actor,const std::vector<ConvexPolygon>& obstacles,float margin) { configure(actor,obstacles,margin); }
    StaticMotionSolver(const StaticMotionSolver&)=delete;
    StaticMotionSolver& operator=(const StaticMotionSolver&)=delete;
    // Failed configuration invalidates the solver, rather than using stale data.
    bool configure(const ConvexPolygon& actor,const std::vector<ConvexPolygon>& obstacles,float margin);
    // Segment shapes are source ConcavePolygon endpoint pairs. Degenerate
    // convex shapes retain the native repeated vertices and zero edge normals;
    // neither kind is inflated into a fabricated rectangle or discarded.
    bool configure_geometry(const ConvexPolygon& actor,const std::vector<StaticObstacle>& obstacles,float margin);
    bool configure_room(const RoomView& content,const std::vector<uint32_t>& active_polygons,const std::vector<Vec2>& polygon_offsets={});
    bool valid() const { return valid_; }
    bool slide(Vec2 position, Vec2 velocity, SlideResult& result) const override;
    // Track every broadphase query; spatial consumers repeat the original step
    // if recovery leaves the region from which they fetched the obstacles.
    bool slide_with_bounds(Vec2 position,Vec2 velocity,SlideResult&,MotionQueryBounds&) const;
private:
    struct PointRange {
        const RoomView* room=nullptr;const Vec2* flat=nullptr;
        uint32_t first=0,count=0;bool reverse=false;Vec2 offset{};
        size_t size() const { return count; }
        Vec2 operator[](size_t i) const {const uint32_t at=first+(reverse?count-1-uint32_t(i):uint32_t(i));const Vec2 p=room?room->vertex(at):flat[at];return offset.x||offset.y?Vec2{p.x+offset.x,p.y+offset.y}:p;}
        struct Iterator {const PointRange* range;size_t i;Vec2 operator*()const{return (*range)[i];}Iterator& operator++(){++i;return *this;}bool operator!=(const Iterator& b)const{return i!=b.i;}};
        Iterator begin()const{return {this,0};}Iterator end()const{return {this,count};}
    };
    struct Shape { PointRange vertices,normals;Vec2 minimum{},maximum{};bool segment=false; };
    bool prepare(PointRange points,Shape& result);
    bool prepare_source_obstacle(PointRange,StaticObstacleKind,Shape&);
    bool slide_impl(Vec2,Vec2,SlideResult&,MotionQueryBounds*) const;
    RoomView content_;
    std::vector<Vec2> vertex_cache_,normal_cache_;
    Shape actor_;
    std::vector<Shape> obstacles_;
    float margin_=0;
    bool valid_=false;
};
}
