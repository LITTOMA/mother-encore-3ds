#include "encore/gameplay_audio.hpp"
#include "encore/music_zones.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace encore::upstream;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"audio admission line %d: %s: %s\n",__LINE__,#x,e.c_str());return 1;}}while(0)
static void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(i*8));}
static void fix(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+24,b.size()-24));}
int main(int argc,char**argv){
 const std::string root=argc>1?argv[1]:"romfs";std::string e;std::vector<uint8_t>c,z;
 CHECK(encore::read_file((root+"/sound/banks/gameplay.enccues").c_str(),c,16384,e));
 CHECK(encore::read_file((root+"/sound/banks/podunk.enczones").c_str(),z,262144,e));
 GameplayAudioData cues;MusicZones zones;MusicRegionData regions;
 CHECK(cues.load(c.data(),c.size(),e));const auto saved=cues.path(GameplayCue::MenuOpen);
 CHECK(!saved.empty());CHECK(zones.load(z.data(),z.size(),e));const auto count=zones.zones().size();
 CHECK(regions.load_file((root+"/sound/banks/podunk.encmusic").c_str(),e));CHECK(zones.matches(regions,e));
 for(size_t size=0;size<c.size();++size){CHECK(!cues.load(c.data(),size,e));CHECK(cues.path(GameplayCue::MenuOpen)==saved);}
 for(size_t size=0;size<z.size();++size){CHECK(!zones.load(z.data(),size,e));CHECK(zones.zones().size()==count);}
 for(size_t off:{size_t(0),size_t(8),size_t(16),size_t(20)}){auto bad=c;bad[off]^=0x7f;CHECK(!cues.load(bad.data(),bad.size(),e));auto b=z;b[off]^=0x7f;CHECK(!zones.load(b.data(),b.size(),e));}
 auto bad=c;put(bad,24,257);fix(bad);CHECK(!cues.load(bad.data(),bad.size(),e));
 bad=c;bad[28]='.';fix(bad);CHECK(!cues.load(bad.data(),bad.size(),e));
 bad=c;bad.push_back(0);fix(bad);CHECK(!cues.load(bad.data(),bad.size(),e));
 bad=z;put(bad,24,257);fix(bad);CHECK(!zones.load(bad.data(),bad.size(),e));
 bad=z;bad.push_back(0);fix(bad);CHECK(!zones.load(bad.data(),bad.size(),e));
 // The first polygon's vertex count and first coordinate have independent bounds.
 auto u=[](const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;};
 const size_t polygon=28+u(z.data()+24)+4;
 bad=z;put(bad,polygon,2);fix(bad);CHECK(!zones.load(bad.data(),bad.size(),e));
 bad=z;put(bad,polygon+4,0x7fc00000);fix(bad);CHECK(!zones.load(bad.data(),bad.size(),e));
 bad=z;std::memcpy(bad.data()+polygon+4+8,bad.data()+polygon+4,8);fix(bad);CHECK(!zones.load(bad.data(),bad.size(),e));
  CHECK(cues.path(GameplayCue::MenuOpen)==saved&&zones.zones().size()==count);
 return 0;
}
