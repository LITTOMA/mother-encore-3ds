#include "room_fixture.hpp"
#include "encore/animation.hpp"

#include "fixtures/animation_v0410.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(e) do{++checks;if(!(e)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);std::exit(1);}}while(0)
static FrameClip clip(unsigned i){FrameClip c; if(!encore_test::room().frame_clip(i,c))std::abort();return c;}
int main() {
    for(const auto& row:animation_reference::rows) {
        AnimationSample sample{199,row.initial,row.initial};
        CHECK(sample_clip(clip(row.clip),row.time,200,sample));
        if(sample.frame!=row.frame)std::fprintf(stderr,"Clip %u time %.9g: frame %u vs original %u\n",row.clip,row.time,sample.frame,row.frame);
        CHECK(sample.frame==row.frame);CHECK(sample.main_visible==row.main);CHECK(sample.special_visible==row.special);
    }
    AnimationPlayback clock{0,{199,true,true}};
    unsigned current=32;
    for(const auto& tick:animation_reference::ticks) {
        if(current!=tick.clip) {
            clock.sample={199,true,true};
            CHECK(begin_clip(clip(tick.clip),200,clock));current=tick.clip;
        }
        CHECK(advance_clip(clip(tick.clip),tick.delta,200,clock));
        if(clock.sample.frame!=tick.frame)std::fprintf(stderr,"Clock clip %u pos %.9g: frame %u vs original %u\n",tick.clip,clock.position,clock.sample.frame,tick.frame);
        CHECK(clock.sample.frame==tick.frame);CHECK(clock.sample.main_visible==tick.main);CHECK(clock.sample.special_visible==tick.special);
    }
    const FrameClip good{1,false,2,{{0,1},{0.5f,2}},1,0};
    AnimationSample sample{199,false,true};
    CHECK(sample_clip(good,2,200,sample));CHECK(sample.frame==2&&sample.main_visible&&!sample.special_visible);
    auto invalid=[&](const FrameClip& clip,float time,uint16_t count) {
        AnimationSample out{99,false,true};CHECK(!sample_clip(clip,time,count,out));
        CHECK(out.frame==99&&!out.main_visible&&out.special_visible);
    };
    invalid(good,-1,200);invalid(good,std::numeric_limits<float>::quiet_NaN(),200);invalid(good,0,0);
    auto bad=good;bad.count=9;invalid(bad,0,200);
    bad=good;bad.count=0;invalid(bad,0,200);
    bad=good;bad.length=0;invalid(bad,0,200);
    bad=good;bad.length=std::numeric_limits<float>::infinity();invalid(bad,0,200);
    bad=good;bad.keys[1].time=0;invalid(bad,0,200);
    bad=good;bad.keys[0].time=0.1f;invalid(bad,0,200);
    bad=good;bad.keys[1].frame=200;invalid(bad,0,200);
    bad=good;bad.main_visibility=2;invalid(bad,0,200);
    const auto before=clock;
    CHECK(!advance_clip(good,-1,200,clock));CHECK(clock.position==before.position&&clock.sample.frame==before.sample.frame);
    clock.position=2;CHECK(!advance_clip(good,0,200,clock));CHECK(clock.position==2);
    std::printf("Original Ninten animation: %u checks, 1216 samples and 2400 playback ticks against official AnimationPlayer\n",checks);
}
