#include "encore/music_zones.hpp"
#include "encore/audio_data.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace encore::upstream;
namespace{unsigned checks=0;std::string error;void check(bool x,const char*why){++checks;if(!x){std::fprintf(stderr,"Mayor music %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}}
#define CHECK(x) check(bool(x),#x)
int main(int argc,char**argv){
 CHECK(argc==3);const std::string root=argv[1];MusicRegionData data;MusicZones zones;AudioBank bank;RoomData room;
 CHECK(data.load_file((root+"/mayor.encmusic").c_str(),error));CHECK(bank.load_file((root+"/mayor.encaudio").c_str(),error));CHECK(zones.load_file((root+"/mayor.enczones").c_str(),error));CHECK(room.load_file((root+"/mayor-initial-story.encroom").c_str(),error));CHECK(data.matches(bank,error));CHECK(zones.matches(data,error));
 CHECK(data.tracks().size()==1&&data.regions().size()==1&&data.tracks()[0].source_path=="res://Audio/Music/Town Hall.mp3");const auto r=data.regions()[0];CHECK(r.source_path=="MusicArea"&&r.volume_db==6&&r.fadein_seconds==1&&r.fadeout_seconds==1.5f&&!r.disabled);CHECK(bank.count()==1&&bank.asset(0).loops()&&bank.asset(0).gain_db==0);CHECK(zones.overlaps(0,room.view(),{8,9})&&zones.overlaps(0,room.view(),{8,-519})&&!zones.overlaps(0,room.view(),{8,-900}));
 MusicRegionController controller;MusicRegionContext ctx;CHECK(controller.initialize(data,16,error)&&controller.attach_scene(10,error));CHECK(controller.enter(10,r.source_path,ctx,error));CHECK(controller.regions()[0].registered&&controller.regions()[0].inside);const int voice=controller.regions()[0].voice;CHECK(voice>=0);const auto generation=controller.voices()[voice].generation;CHECK(controller.voices()[voice].playing&&controller.voices()[voice].track_id==r.track_id&&controller.voices()[voice].gain_db==6);
 for(unsigned i=0;i<90;++i)CHECK(controller.advance(1.0/60,false,error));CHECK(controller.enter(10,r.source_path,ctx,error));CHECK(controller.voices()[voice].generation==generation);CHECK(controller.exit(10,r.source_path,ctx,error)&&controller.idle_frame(10,error));CHECK(controller.voices()[voice].tweening&&controller.voices()[voice].duration==1.5&&controller.voices()[voice].target_db==data.silence_db());CHECK(controller.advance(.75,false,error));CHECK(std::abs(controller.voices()[voice].gain_db-(6+(data.silence_db()-6)*.0625f))<1e-4);CHECK(controller.advance(.75,false,error));CHECK(!controller.voices()[voice].allocated&&!controller.voices()[voice].playing);CHECK(controller.idle_frame(10,error));CHECK(controller.enter(10,r.source_path,ctx,error));CHECK(controller.regions()[0].registered);
 std::vector<uint8_t>original;CHECK(encore::read_file((root+"/mayor.encmusic").c_str(),original,65536,error));for(size_t n=0;n<original.size();++n)CHECK(!data.load(original.data(),n,error));CHECK(data.tracks()[0].source_path=="res://Audio/Music/Town Hall.mp3");
 {
  std::vector<uint8_t>b;CHECK(encore::read_file((root+"/mayor.enczones").c_str(),b,262144,error));
  auto put=[](std::vector<uint8_t>&x,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)x[at+i]=uint8_t(v>>(i*8));};
  for(unsigned version:{0u,1u,3u}){auto bad=b;put(bad,8,version);CHECK(!zones.load(bad.data(),bad.size(),error));CHECK(zones.zones()[0].path=="MusicArea");}
  for(size_t n=0;n<b.size();++n)CHECK(!zones.load(b.data(),n,error));
  auto bad=b;CHECK(bad.size()>29);bad[28]=255;put(bad,16,encore::crc32(bad.data()+24,bad.size()-24));CHECK(!zones.load(bad.data(),bad.size(),error));CHECK(zones.zones()[0].path=="MusicArea");
  std::vector<uint8_t>legacy;CHECK(encore::read_file((std::string(argv[2])+"/sound/banks/podunk.enczones").c_str(),legacy,262144,error));MusicZones old;CHECK(old.load(legacy.data(),legacy.size(),error));put(legacy,8,2);CHECK(!old.load(legacy.data(),legacy.size(),error));CHECK(old.zones().size()==13);
 }
 std::printf("Mayor music: %u checks; actual Town Hall bank, +6dB region, geometry, same-source reentry and source fade; metadata/controller only, no audible/hardware claim\n",checks);
}
