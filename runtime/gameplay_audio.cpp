#include "encore/gameplay_audio.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstring>
#include <utility>
namespace encore::upstream {
namespace {uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}}
bool GameplayAudioData::load(const uint8_t*p,size_t n,std::string&e){
 auto fail=[&](const char*s){e=s;return false;};
 if(!p||n<24||n>16384||std::memcmp(p,"ENCCUE01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=uint32_t(GameplayCue::Count))return fail("Gameplay audio header rejected");
 if(~encore::crc32_update(~0u,p+24,n-24)!=u32(p+16))return fail("Gameplay audio checksum rejected");
 std::vector<std::string> paths;size_t off=24;
 for(uint32_t i=0;i<uint32_t(GameplayCue::Count);++i){
  if(n-off<4)return fail("Truncated gameplay cue");const auto len=u32(p+off);off+=4;
  if(!len||len>256||len>n-off)return fail("Gameplay cue length rejected");
  std::string path(reinterpret_cast<const char*>(p+off),len);off+=len;
  if(path.rfind("Audio/",0)!=0)return fail("Gameplay cue source rejected");
  for(unsigned char c:path)if(c<32||c>126||c==':'||c=='\\')return fail("Gameplay cue characters rejected");
  size_t start=0;while(start<=path.size()){auto end=path.find('/',start);if(end==path.npos)end=path.size();auto part=path.substr(start,end-start);if(part.empty()||part=="."||part=="..")return fail("Gameplay cue path rejected");if(end==path.size())break;start=end+1;}
  paths.push_back(std::move(path));
 }
 if(off!=n)return fail("Trailing gameplay cue bytes");paths_=std::move(paths);e.clear();return true;
}
bool GameplayAudioData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,16384,e)&&load(b.data(),b.size(),e);}
const std::string& GameplayAudioData::path(GameplayCue cue)const{static const std::string empty;auto i=size_t(cue);return i<paths_.size()?paths_[i]:empty;}
}
