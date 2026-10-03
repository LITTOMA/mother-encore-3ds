#include "room_fixture.hpp"
#include "encore/movement.hpp"
#include "fixtures/movement_v0410.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(e) do{++checks;if(!(e)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);std::exit(1);}}while(0)
void compare(const WalkState& actual,const WalkState& expected) {
    CHECK(actual.position.x==expected.position.x);CHECK(actual.position.y==expected.position.y);
    CHECK(actual.direction.x==expected.direction.x);CHECK(actual.direction.y==expected.direction.y);
    CHECK(std::abs(actual.velocity.x-expected.velocity.x)<0.0001f);CHECK(std::abs(actual.velocity.y-expected.velocity.y)<0.0001f);
    CHECK(actual.speed==expected.speed);CHECK(actual.crouch==expected.crouch);CHECK(actual.tap_run==expected.tap_run);
    CHECK(actual.running==expected.running);CHECK(actual.substantial==expected.substantial);CHECK(actual.walking==expected.walking);
    CHECK(actual.previous_toggle==expected.previous_toggle);CHECK(actual.animation==expected.animation);CHECK(actual.moved_signals==expected.moved_signals);
}
class InvalidSolver final:public MotionSolver {
public:
    bool slide(Vec2,Vec2,SlideResult& result) const override {
        result.position.x=std::numeric_limits<float>::quiet_NaN();return true;
    }
};
int main() {
    FreeMotionSolver free;
    unsigned sequence_index=0;
    for(const auto& sequence:movement_reference::sequences) {
        WalkState state;state.speed=encore_test::room().rule_f32(RoomRuleKey::WalkSpeed);state.direction=encore_test::room().scene().start_direction;state.position={sequence.x,sequence.y};
        for(unsigned frame=0;frame<sequence.count;++frame) {
            const auto& sample=sequence.frames[frame];
            CHECK(advance_walk(state,sample.input,free,encore_test::room()));
            if(state.position.x!=sample.expected.position.x||state.position.y!=sample.expected.position.y||state.animation!=sample.expected.animation){
                std::fprintf(stderr,"Sequence %u frame %u: pos %.3f,%.3f expected %.3f,%.3f; animation %u vs %u\n",sequence_index,frame,state.position.x,state.position.y,sample.expected.position.x,sample.expected.position.y,unsigned(state.animation),unsigned(sample.expected.animation));
            }
            compare(state,sample.expected);
        }
        ++sequence_index;
    }
    for(const auto& sample:movement_reference::controls) {
        Vec2 output{23,45};CHECK(controls_vector(sample.input,sample.threshold,output));
        CHECK(output.x==sample.expected.x);CHECK(output.y==sample.expected.y);
    }
    WalkState state;state.speed=encore_test::room().rule_f32(RoomRuleKey::WalkSpeed);state.direction=encore_test::room().scene().start_direction;state.position={520,404};const auto original=state;
    CHECK(!advance_walk(state,{2,0},free,encore_test::room()));compare(state,original);
    InvalidSolver invalid;CHECK(!advance_walk(state,{1,0},invalid,encore_test::room()));compare(state,original);
    state.speed=100;const auto invalid_speed=state;CHECK(!advance_walk(state,{},free,encore_test::room()));compare(state,invalid_speed);
    Vec2 output{23,45};CHECK(!controls_vector({std::numeric_limits<float>::infinity(),0},0,output));CHECK(output.x==23&&output.y==45);
    CHECK(!controls_vector({},-1,output));CHECK(output.x==23&&output.y==45);
    std::printf("Upstream opening movement: %u checks, %u sequences and 605 control vectors against Godot 3.6.2\n",checks,sequence_index);
}
