#include "encore/music_bank_routes.hpp"
#include "encore/music_scene_graph.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music library %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
std::vector<uint8_t>read(const char*p){std::ifstream f(p,std::ios::binary);CHECK(bool(f));return {std::istreambuf_iterator<char>(f),{}};}
void word(std::vector<uint8_t>&b,size_t at,uint32_t value){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(8*i));}
void crc(std::vector<uint8_t>&b){word(b,16,0);word(b,16,audio_crc32(b.data(),b.size()));}
}
int main(int argc,char**argv){
 CHECK(argc==8);auto bytes=read(argv[1]);MusicRegionData all;CHECK(all.load(bytes.data(),bytes.size(),error));CHECK(all.library()&&all.format_version()==2&&all.regions().empty()&&all.tracks().size()==13&&all.empty_player_gain_db()==0);
 const auto first=all.tracks()[0];
 for(size_t n=0;n<bytes.size();++n){CHECK(!all.load(bytes.data(),n,error));CHECK(all.library()&&all.tracks().size()==13&&all.tracks()[0].source_path==first.source_path);}
 auto reject=[&](std::vector<uint8_t>bad){crc(bad);CHECK(!all.load(bad.data(),bad.size(),error));CHECK(all.library()&&all.tracks().size()==13&&all.tracks()[0].id==first.id);};
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{8,3},{20,1},{24,0},{24,65},{28,1},{40,0x7fc00000u},{40,0x41c80000u},{32,0xc1980000u},{36,0x42740000u},{64,0}}){auto bad=bytes;word(bad,entry.first,entry.second);reject(std::move(bad));}
 auto bad=bytes;bad[44]=1;reject(bad);bad=bytes;std::fill(bad.begin()+68,bad.begin()+100,0);reject(bad);bad=bytes;std::fill(bad.begin()+100,bad.begin()+132,0);reject(bad);
 bad=bytes;word(bad,bad.size()-4,2);reject(bad);bad=bytes;bad[136]=0;reject(bad);
 const size_t second=64+4+32+32+4+first.source_path.size()+4;bad=bytes;word(bad,second,first.id);reject(bad);
 bad=bytes;bad.back()^=1;CHECK(!all.load(bad.data(),bad.size(),error));
 MusicRegionData intro,house,victory;AudioBank ib,hb,vb;CHECK(intro.load_file(argv[2],error)&&ib.load_file(argv[3],error)&&house.load_file(argv[4],error)&&hb.load_file(argv[5],error)&&victory.load_file(argv[6],error)&&vb.load_file(argv[7],error));
 CHECK(intro.matches(ib,error)&&house.matches(hb,error)&&victory.matches(vb,error));CHECK(!ib.asset(0).loops()&&hb.asset(0).loops()&&!vb.asset(0).loops());
 auto audio=read(argv[3]);audio[64+42]=1;crc(audio);AudioBank changed;CHECK(changed.load(audio.data(),audio.size(),error));CHECK(!intro.matches(changed,error));CHECK(!intro.matches(hb,error));
 MusicRegionController legacy;CHECK(!legacy.initialize(all,4,error));MusicSceneGraph scene;CHECK(scene.initialize({all.silence_db(),all.empty_player_gain_db(),all.fade_to_seconds(),4},error));CHECK(!scene.attach_scene(1,all,error)&&!scene.scene_epoch());
 MusicPlayers players;MusicBankRoutes routes;CHECK(players.initialize({all.silence_db(),all.empty_player_gain_db(),all.fade_to_seconds(),4},error));CHECK(routes.initialize({4,4,ib.master_db(),ib.silence_db()},error)&&routes.add_provider(1,intro,ib,error)&&routes.add_provider(2,house,hb,error)&&routes.add_provider(3,victory,vb,error));
 CHECK(players.play_track(0,intro.tracks()[0].source_path,house.tracks()[0].source_path,error));CHECK(routes.reconcile(players,{{0,1,1}},error)&&routes.routes()[0].track_id==intro.tracks()[0].id);
 CHECK(players.signal_finished(0,1,error)&&players.dispatch_finished(error)&&routes.reconcile(players,{{0,2,2}},error));CHECK(routes.routes()[0].track_id==house.tracks()[0].id&&routes.routes()[0].slot==0);
 std::printf("Music library: %u checks; version 2 source library, builtin loop identity, no scene admission, source one-shot/loop route, truncation/schema/flags/tuning/source rollback; no live EOF/audio claim\n",checks);
}
