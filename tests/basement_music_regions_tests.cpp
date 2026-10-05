#include "encore/music_regions.hpp"
#include "encore/basement_progression.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Basement music manual failure %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
int main(int argc,char**argv){
 const std::string root=argc>1?argv[1]:"romfs";std::string error;MusicRegionData data;BasementProgressionData progression;
 CHECK(data.load_file((root+"/sound/banks/house.encmusic").c_str(),error));CHECK(progression.load_file((root+"/data/house.encbasement").c_str(),error));
 const auto&binding=progression.music();CHECK(data.regions().size()==4&&data.tracks().size()==3&&progression.music_regions().size()==4);const auto&r=data.regions().front();const auto&t=data.tracks().front();CHECK(r.id==binding.region_id&&r.track_id==binding.track_id&&r.source_path==binding.node&&t.source_path==binding.music);CHECK(r.volume_db==binding.volume_db&&r.fadein_seconds==binding.fadein_seconds&&r.fadeout_seconds==binding.fadeout_seconds);
 MusicRegionController core;CHECK(core.initialize(data,4,error)&&core.attach_scene(1,error));MusicRegionContext context;context.in_cutscene=true;context.flag=[](std::string_view){return false;};
 CHECK(core.enter(1,binding.node,context,error));CHECK(!core.regions().front().inside&&!core.regions().front().registered);
 // Explicit diary play bypasses the callback guards, exactly as the source
 // public method does. It registers and sets player_inside even in cutscene.
 CHECK(core.play_explicit(1,binding.node,error));CHECK(core.regions().front().inside&&core.regions().front().registered);const auto voice=core.regions().front().voice;CHECK(voice>=0);CHECK(core.voices()[voice].gain_db==binding.volume_db);
 const auto generation=core.voices()[voice].generation;CHECK(!core.play_explicit(1,"Unknown",error)&&core.voices()[voice].generation==generation);CHECK(!core.stop_explicit(1,binding.node,NAN,error)&&core.regions().front().registered);CHECK(!core.stop_explicit(1,binding.node,-1,error)&&core.regions().front().registered);
 CHECK(core.stop_explicit(1,binding.node,binding.default_stop_seconds,error));CHECK(core.regions().front().inside&&!core.regions().front().registered&&core.regions().front().voice<0);CHECK(core.voices()[voice].curve==MusicRegionCurve::QuartIn&&core.voices()[voice].duration==binding.default_stop_seconds);
 CHECK(core.advance(binding.default_stop_seconds/2,false,error));const auto expected=binding.volume_db+(data.silence_db()-binding.volume_db)*.0625f;CHECK(std::abs(core.voices()[voice].gain_db-expected)<.0001f);
 CHECK(core.play_explicit(1,binding.node,error));CHECK(core.regions().front().voice==voice&&core.voices()[voice].generation==generation&&core.voices()[voice].curve==MusicRegionCurve::Linear);
 context.in_cutscene=false;CHECK(core.exit(1,binding.node,context,error)&&core.regions().front().pending_exit);CHECK(core.play_explicit(1,binding.node,error)&&core.regions().front().inside);CHECK(core.idle_frame(1,error)&&core.regions().front().registered&&!core.regions().front().pending_exit);
 CHECK(core.stop_explicit(1,binding.node,0,error));CHECK(!core.voices()[voice].playing&&core.regions().front().inside);CHECK(core.tree_exit(1,binding.node,error)&&core.attach_scene(2,error));CHECK(core.play_explicit(1,"Unknown",error)&&!core.regions().front().registered);CHECK(!core.stop_explicit(2,"Unknown",0,error));
 // Root House and basement must share the original owned player, including
 // reentry while its exit fade is still in flight. No external song shortcut.
 const auto&root_binding=progression.music_regions()[1];MusicRegionController handoff;CHECK(handoff.initialize(data,4,error)&&handoff.attach_scene(1,error));context.flag=[](std::string_view){return true;};CHECK(handoff.play_explicit(1,root_binding.node,error));const auto root_voice=handoff.regions()[1].voice;const auto root_generation=handoff.voices()[root_voice].generation;CHECK(handoff.exit(1,root_binding.node,context,error)&&handoff.idle_frame(1,error)&&handoff.advance(.1,false,error));CHECK(handoff.play_explicit(1,binding.node,error));CHECK(handoff.regions()[0].voice==root_voice&&handoff.voices()[root_voice].generation==root_generation&&handoff.voices()[root_voice].target_db==binding.volume_db);unsigned voices=0;for(const auto&v:handoff.voices())voices+=v.allocated;CHECK(voices==1);
 // Source DialogueBox music="" targets global child0, including a region
 // older than the latest external Music player. It must not fade every song.
 MusicRegionController indexed;CHECK(indexed.initialize(data,4,error)&&indexed.attach_scene(1,error));CHECK(indexed.play_explicit(1,root_binding.node,error));const auto first_slot=indexed.regions()[1].voice;CHECK(indexed.play_explicit(1,progression.music_regions()[3].node,error));const auto later_slot=indexed.regions()[3].voice;const auto later_curve=indexed.voices()[later_slot].curve;const auto later_duration=indexed.voices()[later_slot].duration;bool external_target=true;
 CHECK(indexed.fade_index_zero(2,external_target,error)&&!external_target);CHECK(indexed.voices()[first_slot].curve==MusicRegionCurve::QuartIn&&indexed.voices()[first_slot].duration==2);CHECK(indexed.voices()[later_slot].curve==later_curve&&indexed.voices()[later_slot].duration==later_duration);CHECK(indexed.regions()[1].registered&&indexed.regions()[1].inside);
 CHECK(indexed.observe_external_player({1,true,true},error));CHECK(indexed.fade_index_zero(1,external_target,error)&&!external_target);CHECK(!indexed.fade_index_zero(NAN,external_target,error));CHECK(indexed.observe_external_player({2,true,true},error));CHECK(!indexed.fade_index_zero(1,external_target,error));
 MusicRegionController external_first;CHECK(external_first.initialize(data,4,error)&&external_first.attach_scene(1,error));CHECK(external_first.observe_external_player({1,true,true},error)&&external_first.play_explicit(1,root_binding.node,error));const auto retained_curve=external_first.voices()[external_first.regions()[1].voice].curve;CHECK(external_first.fade_index_zero(2,external_target,error)&&external_target);CHECK(external_first.voices()[external_first.regions()[1].voice].curve==retained_curve);
 std::puts("Manual explicit House MusicChanger source ownership/fade checks passed; no audible or hardware result");
}
