#include "encore/music_regions.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cmath>
using namespace encore::upstream;
namespace {unsigned checks=0;void check(bool x,const char*m){++checks;if(!x){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}std::vector<uint8_t>read(const char*p){std::ifstream f(p,std::ios::binary);return{std::istreambuf_iterator<char>(f),{}};}void crc(std::vector<uint8_t>&v){std::fill(v.begin()+16,v.begin()+20,0);auto c=audio_crc32(v.data(),v.size());for(int i=0;i<4;++i)v[16+i]=uint8_t(c>>(8*i));}unsigned active(const MusicRegionController&c){unsigned n=0;for(auto&v:c.voices())n+=v.allocated&&v.playing;return n;}int voice(const MusicRegionController&c,const MusicRegionData&d,const char*p){for(size_t i=0;i<d.regions().size();++i)if(d.regions()[i].source_path==p)return c.regions()[i].voice;return-1;}}
int main(int argc,char**argv){check(argc==3,"pack+bank arguments");auto bytes=read(argv[1]);std::string e;MusicRegionData d;check(d.load(bytes.data(),bytes.size(),e),e.c_str());check(d.regions().size()==13&&d.tracks().size()==5,"source scope");
 // Every truncation rejects while preserving the checked old resource.
 for(size_t n=0;n<bytes.size();++n){check(!d.load(bytes.data(),n,e),"truncated pack rejected");check(d.valid()&&d.regions().size()==13,"failed load transactional");}
 auto bad=bytes;bad[8]=2;crc(bad);check(!d.load(bad.data(),bad.size(),e),"unknown schema");bad=bytes;bad[40]=1;crc(bad);check(!d.load(bad.data(),bad.size(),e),"reserved bytes");bad=bytes;bad[24]=17;crc(bad);check(!d.load(bad.data(),bad.size(),e),"oversized tracks");bad=bytes;bad[35]=0x7f;bad[34]=0xc0;crc(bad);check(!d.load(bad.data(),bad.size(),e),"nonfinite tuning");bad=bytes;bad.back()=0;crc(bad);check(!d.load(bad.data(),bad.size(),e),"control byte path");bad=bytes;bad[100]^=1;check(!d.load(bad.data(),bad.size(),e),"bad crc");
 AudioBank bank;check(bank.load_file(argv[2],e)&&d.matches(bank,e),e.c_str());MusicRegionController c;MusicRegionContext ctx;ctx.flag=[](std::string_view){return false;};check(c.initialize(d,8,e)&&c.attach_scene(1,e),"fresh scene");check(!c.initialize(d,17,e)&&c.capacity()==8,"capacity capability");
 check(c.enter(1,"Music/MusicArea",ctx,e)&&active(c)==1,"first region starts without fade when silence");int a=voice(c,d,"Music/MusicArea");const auto gen=c.voices()[a].generation;check(c.voices()[a].gain_db==0&&!c.voices()[a].tweening,"first entry source immediate volume");
 check(c.enter(1,"Music/MusicArea3",ctx,e)&&active(c)==1,"same loop reuse");check(voice(c,d,"Music/MusicArea3")==a&&c.voices()[a].generation==gen,"same track no rewind");check(c.advance(.5,false,e)&&std::abs(c.voices()[a].gain_db+4)<1e-5,"source linear fadeto midpoint");
 check(c.exit(1,"Music/MusicArea3",ctx,e)&&c.enter(1,"Music/MusicArea3",ctx,e)&&c.idle_frame(1,e),"deferred exit cancelled on same-idle reentry");check(voice(c,d,"Music/MusicArea3")==a,"same-frame reentry retained");
 check(c.exit(1,"Music/MusicArea",ctx,e)&&c.idle_frame(1,e)&&active(c)==1,"shared song keeps voice on old area exit");
 ctx.in_cutscene=true;check(c.exit(1,"Music/MusicArea3",ctx,e)&&c.idle_frame(1,e)&&voice(c,d,"Music/MusicArea3")==a,"cutscene exit ignored");ctx.in_cutscene=false;ctx.has_collisions=false;check(c.exit(1,"Music/MusicArea3",ctx,e)&&c.idle_frame(1,e)&&voice(c,d,"Music/MusicArea3")==a,"collisionless exit ignored");ctx.has_collisions=true;
 ctx.in_battle=true;check(c.enter(1,"Music/MusicArea6",ctx,e)&&active(c)==1,"battle enter ignored");ctx.in_battle=false;check(c.enter(1,"Music/MusicArea2",ctx,e)&&active(c)==1,"false appear flag ignored");ctx.flag=[](std::string_view){return true;};check(c.enter(1,"Music/MusicArea10",ctx,e)&&active(c)==1,"true disappear flag ignored");ctx.flag=[](std::string_view){return false;};
 check(c.enter(1,"Music/MusicArea6",ctx,e)&&active(c)==2,"different loops concurrent");int b=voice(c,d,"Music/MusicArea6");check(c.voices()[b].gain_db==-80&&c.voices()[b].tweening,"new song source fadein");check(c.advance(.5,false,e)&&std::abs(c.voices()[b].gain_db-(-80+74*.9375))<1e-5,"quartic-out fadein midpoint");
 check(c.exit(1,"Music/MusicArea3",ctx,e)&&c.idle_frame(1,e)&&c.advance(.75,false,e),"last shared area fadeout");check(std::abs(c.voices()[a].gain_db-(-8-72*.0625))<1e-5,"quartic-in fadeout midpoint");check(c.advance(.75,false,e)&&active(c)==1,"silent voice removed after all tweens");
 // A->B->A before previous fades finish really creates two A voices.
 MusicRegionController rapid;check(rapid.initialize(d,8,e)&&rapid.attach_scene(9,e),"rapid scene");check(rapid.enter(9,"Music/MusicArea",ctx,e)&&rapid.exit(9,"Music/MusicArea",ctx,e)&&rapid.idle_frame(9,e),"rapid A leaves");check(rapid.enter(9,"Music/MusicArea6",ctx,e)&&rapid.exit(9,"Music/MusicArea6",ctx,e)&&rapid.idle_frame(9,e),"rapid B leaves");check(rapid.enter(9,"Music/MusicArea",ctx,e)&&active(rapid)==3,"source duplicate same-track fade voice retained");unsigned same=0;uint32_t ta=d.regions()[0].track_id;for(auto&v:rapid.voices())same+=v.allocated&&v.track_id==ta;check(same==2,"two independent A generations");
 MusicRegionController cap;check(cap.initialize(d,1,e)&&cap.attach_scene(1,e)&&cap.enter(1,"Music/MusicArea",ctx,e),"bounded capacity setup");auto oldgen=cap.voices()[0].generation;check(!cap.enter(1,"Music/MusicArea6",ctx,e)&&e.find("capacity exhausted")!=e.npos,"capacity failure explicit");check(active(cap)==1&&cap.voices()[0].generation==oldgen&&voice(cap,d,"Music/MusicArea6")==-1,"capacity failure old voices/ownership unchanged");
 MusicRegionController house;check(house.initialize(d,8,e)&&house.attach_scene(3,e),"house handoff setup");check(house.observe_external_player({1,true,true},e),"explicit House player creation observed");check(house.enter(3,"Music/MusicArea",ctx,e)&&house.voices()[0].gain_db==-80,"House continuing causes original crossfade entry");check(!house.attach_scene(4,e),"prepare cannot drop live area ownership");for(auto&r:d.regions())check(house.tree_exit(3,r.source_path,e),"ordered tree exit");check(house.attach_scene(4,e)&&house.enter(3,"Music/MusicArea",ctx,e)&&active(house)==1,"stale callback harmless across scene epochs");check(house.advance(2,true,e)&&active(house)==1,"source waits external manager tween before cleanup");check(house.advance(0,false,e)&&active(house)==0,"source all-tweens cleanup");check(house.enter(4,"Music/MusicArea",ctx,e)&&active(house)==1,"new epoch reentry works");
 // A newly created external House player is the global latest, even while
 // a Podunk voice survives a long tween. Reentry must not reuse that old voice.
 MusicRegionController revisit;
 check(revisit.initialize(d,8,e)&&revisit.attach_scene(1,e)&&revisit.enter(1,"Music/MusicArea",ctx,e)&&revisit.tree_exit(1,"Music/MusicArea",e)&&revisit.attach_scene(2,e),"revisit retains old fading region voice");
 auto old_revisit=revisit.voices()[0].generation;check(revisit.observe_external_player({2,true,true},e),"new global House identity observed in source order");
 check(revisit.enter(2,"Music/MusicArea",ctx,e)&&active(revisit)==2,"new external latest player creates source duplicate on reentry");
 check(revisit.voices()[0].generation==old_revisit&&revisit.voices()[1].gain_db==-80,"old voice untouched while new source voice fades in");
 check(!revisit.observe_external_player({1,true,true},e),"stale external identity rejected");
 check(revisit.observe_external_player({2,true,true},e),"same external identity observation does not reorder it");
 check(revisit.enter(2,"Music/MusicArea3",ctx,e)&&active(revisit)==2,"same song reuses existing source first match after notification");
 check(revisit.observe_external_player({2,false,false},e)&&!revisit.observe_external_player({2,true,true},e),"removed external identity cannot be resurrected");
 check(!revisit.attach_scene(1,e),"scene epoch cannot regress and accept an old scene callback");
 // audioManager.play_music replaces a stream on the same child. Repeated
 // observations, including an idle stopped child, cannot change child order.
 MusicRegionController replay;
 check(replay.initialize(d,8,e)&&replay.observe_external_player({1,true,true},e)&&replay.attach_scene(1,e)&&replay.enter(1,"Music/MusicArea",ctx,e),"external child precedes region child");
 bool external_target=false;
 check(replay.observe_external_player({1,true,true},e)&&replay.fade_index_zero(1,external_target,e)&&external_target,"same-child stream replacement preserves indexed fade target");
 check(replay.observe_external_player({1,true,false},e)&&replay.fade_index_zero(1,external_target,e)&&external_target,"stop preserves the idle source child and its order");
 check(replay.observe_external_player({1,true,true},e)&&replay.fade_index_zero(1,external_target,e)&&external_target,"replay on the same child does not poison indexed fades");
 check(replay.observe_external_player({1,false,false},e)&&replay.observe_external_player({2,true,true},e)&&replay.fade_index_zero(1,external_target,e)&&!external_target,"explicit remove then create puts replacement after surviving region child");
 check(!replay.observe_external_player({2,false,true},e),"absent child cannot be playing");
 check(!replay.observe_external_player({1,true,true},e),"retired child cannot replace the current child");
 MusicRegionController unknown_history;
 check(unknown_history.initialize(d,8,e)&&unknown_history.attach_scene(1,e)&&unknown_history.observe_external_player({1,true,true},e)&&unknown_history.observe_external_player({2,true,true},e),"unmapped overlapping external child history recorded");
 check(!unknown_history.fade_index_zero(1,external_target,e),"missing removal proof still rejects indexed fade");
 check(!c.advance(-1,false,e)&&!c.advance(INFINITY,false,e),"invalid timing rejected");check(!c.enter(1,"Music/Unknown",ctx,e),"unknown binding rejected");check(c.enter(999,"Music/Unknown",ctx,e),"stale scene callbacks ignored before path lookup");
 std::cout<<"Music regions PASS "<<checks<<" checks; controller and parser only, no audible output\n";
}
