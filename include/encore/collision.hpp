#pragma once
#include "encore/movement.hpp"
#include "encore/room_data.hpp"
#include <vector>

namespace encore::upstream {
// Actor vertices are relative to its origin (including the source shape-node
// offset); obstacle vertices are in world coordinates. Both winding orders are
// accepted. Shapes must be strictly convex, finite, nondegenerate polygons.
struct ConvexPolygon { std::vector<Vec2> vertices; };

// Obstacles supplied per query by a spatial source (TileMap cells, static bodies).
// Convex: points/normals follow Godot ConvexPolygonShape2DSW after its clockwise
// inversion; zero-length edges keep zero normals (SAT then uses a (0, 1) axis).
// Segment: two points and one normal from a ConcavePolygonShape2D, culled by the
// actor's static AABB as Godot's concave solver does. Bounds include offset.
struct MotionObstacle {
    const Vec2* points=nullptr; const Vec2* normals=nullptr; uint32_t count=0;
    Vec2 offset{}, minimum{}, maximum{}; bool segment=false;
};
class MotionObstacleSource {
public:
    virtual ~MotionObstacleSource()=default;
    // Append obstacles whose bounds may touch [minimum, maximum], in a
    // deterministic order. Returning false rejects the motion.
    virtual bool collect(Vec2 minimum,Vec2 maximum,std::vector<MotionObstacle>& out)const=0;
};

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
    bool configure_room(const RoomView& content,const std::vector<uint32_t>& active_polygons,const std::vector<Vec2>& polygon_offsets={});
    // The source must outlive the solver binding; configure* keeps it attached.
    void attach_source(const MotionObstacleSource* source) { source_=source; }
    bool valid() const { return valid_; }
    bool slide(Vec2 position, Vec2 velocity, SlideResult& result) const override;
    struct PointRange {
        const RoomView* room=nullptr;const Vec2* flat=nullptr;
        uint32_t first=0,count=0;bool reverse=false;Vec2 offset{};
        size_t size() const { return count; }
        Vec2 operator[](size_t i) const {const uint32_t at=first+(reverse?count-1-uint32_t(i):uint32_t(i));const Vec2 p=room?room->vertex(at):flat[at];return offset.x||offset.y?Vec2{p.x+offset.x,p.y+offset.y}:p;}
        struct Iterator {const PointRange* range;size_t i;Vec2 operator*()const{return (*range)[i];}Iterator& operator++(){++i;return *this;}bool operator!=(const Iterator& b)const{return i!=b.i;}};
        Iterator begin()const{return {this,0};}Iterator end()const{return {this,count};}
    };
    struct Shape { PointRange vertices,normals;Vec2 minimum{},maximum{};bool segment=false; };
private:
    bool prepare(PointRange points,Shape& result);
    RoomView content_;
    std::vector<Vec2> vertex_cache_,normal_cache_;
    Shape actor_;
    std::vector<Shape> obstacles_;
    const MotionObstacleSource* source_=nullptr;
    mutable std::vector<MotionObstacle> collected_;
    mutable std::vector<Shape> combined_;
    float margin_=0;
    bool valid_=false;
};
}
