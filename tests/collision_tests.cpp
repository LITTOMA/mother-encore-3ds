#include "room_fixture.hpp"
#include "encore/collision.hpp"

#include "fixtures/collision_v0362.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(e) do{++checks;if(!(e)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);std::exit(1);}}while(0)
static bool near(Vec2 a,Vec2 b,float tolerance=0.0002f) { return std::abs(a.x-b.x)<=tolerance&&std::abs(a.y-b.y)<=tolerance; }
int main() {
 const auto content=encore_test::room();const float margin=content.rule_f32(RoomRuleKey::ActorCollisionSafeMargin);
    unsigned frames=0,mismatches=0;
    for(const auto& entry:collision_reference::cases) {
        StaticMotionSolver solver(entry.actor,entry.obstacles,margin);CHECK(solver.valid());Vec2 position=entry.start;
        for(const auto& frame:entry.frames) {
            SlideResult result;CHECK(solver.slide(position,frame.input,result));
            if(entry.round_and_recover) {
                SlideResult recovery;CHECK(solver.slide(result.position,{},recovery));
                result.position={std::round(recovery.position.x),std::round(recovery.position.y)};
            }
            if(!near(result.position,frame.position)||!near(result.velocity,frame.velocity)) {
                std::fprintf(stderr,"Collision %s frame %u: (%.9f,%.9f) vel(%.9f,%.9f); reference (%.9f,%.9f) vel(%.9f,%.9f)\n",entry.name,frames,result.position.x,result.position.y,result.velocity.x,result.velocity.y,frame.position.x,frame.position.y,frame.velocity.x,frame.velocity.y);++mismatches;
            }
            position=result.position;++frames;
        }
    }
    CHECK(mismatches==0);
    ConvexPolygon hull; const auto scene=content.scene();
    for(uint32_t i=0;i<scene.actor_hull_count;++i)hull.vertices.push_back(content.vec2(scene.actor_hull_first+i));
    std::vector<ConvexPolygon> house;
    for(uint32_t i=0;i<content.polygon_count();++i) {
        const auto polygon=content.polygon(i);ConvexPolygon p;
        for(uint32_t j=0;j<polygon.vertex_count;++j)p.vertices.push_back(content.vec2(polygon.first_vertex+j));
        StaticMotionSolver candidate(hull,{p},margin);CHECK(candidate.valid());house.push_back(p);
    }
    StaticMotionSolver whole_house(hull,house,margin);CHECK(whole_house.valid());
    SlideResult moved;CHECK(whole_house.slide({520,397},{64,0},moved));
    CHECK(near(moved.velocity,{64,0}));
    StaticMotionSolver invalid;SlideResult unchanged{{12,34},{56,78}};moved=unchanged;
    CHECK(!invalid.slide({},{},moved));CHECK(near(moved.position,unchanged.position));CHECK(near(moved.velocity,unchanged.velocity));
    CHECK(invalid.configure(hull,{},margin));
    CHECK(!invalid.slide({std::numeric_limits<float>::quiet_NaN(),0},{},moved));
    CHECK(!invalid.slide({},{std::numeric_limits<float>::infinity(),0},moved));
    CHECK(!invalid.slide({1000001,0},{},moved));
    CHECK(near(moved.position,unchanged.position));CHECK(near(moved.velocity,unchanged.velocity));
    const std::vector<ConvexPolygon> rejected={
        {},{{{0,0},{1,0}}},{{{0,0},{1,0},{2,0}}},
        {{{0,0},{2,0},{1,1},{2,2},{0,2}}},
        {{{0,0},{2,2},{0,2},{2,0}}},
        {{{0,0},{2,0},{2,0},{0,2}}},
        {{{0,0},{2,0},{0,std::numeric_limits<float>::infinity()}}}
    };
    for(const auto& bad:rejected) {
        CHECK(!invalid.configure(hull,{bad},margin));CHECK(!invalid.valid());CHECK(!invalid.slide({},{},moved));
        CHECK(!invalid.configure(bad,{},margin));
    }
    ConvexPolygon reversed=hull;std::reverse(reversed.vertices.begin(),reversed.vertices.end());
    CHECK(invalid.configure(reversed,{},margin));CHECK(invalid.slide({},{60,-60},moved));CHECK(near(moved.position,{1,-1}));CHECK(near(moved.velocity,{60,-60}));
    const auto& right=collision_reference::cases[1];
    ConvexPolygon reverse_wall=right.obstacles[0];std::reverse(reverse_wall.vertices.begin(),reverse_wall.vertices.end());
    CHECK(invalid.configure(reversed,{reverse_wall},margin));CHECK(invalid.slide(right.start,right.frames[0].input,moved));
    CHECK(near(moved.position,right.frames[0].position));CHECK(near(moved.velocity,right.frames[0].velocity));
    // Never silently drop recovery contacts when this bounded backend's
    // capacity is exceeded; keep the caller's output transaction untouched.
    const ConvexPolygon wall{{{12,-100},{32,-100},{32,100},{12,100}}};
    CHECK(invalid.configure(hull,std::vector<ConvexPolygon>(17,wall),margin));
    moved=unchanged;CHECK(!invalid.slide({8,0},{},moved));
    CHECK(near(moved.position,unchanged.position));CHECK(near(moved.velocity,unchanged.velocity));
    CHECK(invalid.configure(hull,{},margin));CHECK(invalid.slide({},{60,-60},moved));
    // A failed reconfiguration must never silently keep the previous world.
    CHECK(!invalid.configure(hull,{rejected[0]},margin));CHECK(!invalid.slide({},{60,0},moved));
    CHECK(near(moved.position,{1,-1}));
    // Manual-only invariants for native hollow segments and source degenerate
    // convex outlines. These do not claim a new engine-reference oracle.
    const ConvexPolygon square{{{-1,-1},{1,-1},{1,1},{-1,1}}};
    const StaticObstacle segment{StaticObstacleKind::Segment,{{4,-8},{4,8}}};
    CHECK(invalid.configure_geometry(square,{segment},margin));
    CHECK(invalid.slide({0,0},{600,0},moved));CHECK(moved.position.x<=3&&moved.position.x>0);CHECK(moved.velocity.x==0);
    CHECK(invalid.slide({8,0},{-600,0},moved));CHECK(moved.position.x>=5&&moved.position.x<8);CHECK(moved.velocity.x==0);
    CHECK(invalid.slide({0,20},{600,0},moved));CHECK(near(moved.position,{10,20}));CHECK(near(moved.velocity,{600,0}));
    const StaticObstacle repeated{StaticObstacleKind::NativeDegenerateConvex,{{4,-8},{4,8},{4,8},{4,-8}}};
    CHECK(invalid.configure_geometry(square,{repeated},margin));
    CHECK(invalid.slide({0,0},{600,0},moved));CHECK(moved.position.x<=3&&moved.velocity.x==0);
    CHECK(invalid.slide({0,20},{600,0},moved));CHECK(near(moved.position,{10,20}));
    CHECK(!invalid.configure_geometry(square,{{StaticObstacleKind::NativeDegenerateConvex,{{0,0},{2,0},{0,2}}}},margin));
    moved=unchanged;CHECK(!invalid.slide({},{60,0},moved));CHECK(near(moved.position,unchanged.position));
    CHECK(!invalid.configure_geometry(square,{{StaticObstacleKind(99),{{0,0},{0,1}}}},margin));
    CHECK(!invalid.configure_geometry(square,{{StaticObstacleKind::Segment,{{0,0},{0,1},{1,1}}}},margin));
    CHECK(!invalid.configure_geometry(square,{{StaticObstacleKind::Segment,{{0,0},{std::numeric_limits<float>::quiet_NaN(),1}}}},margin));
    std::printf("Static source collision: %u checks, %u engine-reference cases / %u frames, %zu source house pieces\n",checks,unsigned(collision_reference::cases.size()),frames,house.size());
}
