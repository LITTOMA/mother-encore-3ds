#include "encore/music_zones.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <utility>
namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
struct Reader{const uint8_t*p;size_t n;bool ok=true;uint32_t u(){if(n<4){ok=false;return 0;}auto v=u32(p);p+=4;n-=4;return v;}float f(){auto bits=u();float v;std::memcpy(&v,&bits,4);if(!std::isfinite(v)||std::abs(v)>1e6)ok=false;return v;}std::string text(){auto len=u();if(!len||len>256||len>n){ok=false;return {};}std::string s(reinterpret_cast<const char*>(p),len);p+=len;n-=len;return s;}};
bool convex(const std::vector<Vec2>&p){double sign=0;for(size_t i=0;i<p.size();++i){auto a=p[i],b=p[(i+1)%p.size()];for(size_t j=0;j<p.size();++j){if(j==i||j==(i+1)%p.size())continue;auto c=p[j];double cross=double(b.x-a.x)*(c.y-a.y)-double(b.y-a.y)*(c.x-a.x);if(cross==0||sign*cross<0)return false;sign=cross;}}return sign!=0;}
bool separated(const std::vector<Vec2>&a,const std::vector<Vec2>&b){for(size_t i=0;i<a.size();++i){auto p=a[i],q=a[(i+1)%a.size()];const Vec2 axis{q.y-p.y,p.x-q.x};auto dot=[&](Vec2 v){return double(v.x)*axis.x+double(v.y)*axis.y;};double amin=dot(a[0]),amax=amin,bmin=dot(b[0]),bmax=bmin;for(auto v:a){amin=std::min(amin,dot(v));amax=std::max(amax,dot(v));}for(auto v:b){bmin=std::min(bmin,dot(v));bmax=std::max(bmax,dot(v));}if(amax<bmin||bmax<amin)return true;}return false;}
}
bool MusicZones::load(const uint8_t*p,size_t n,std::string&e){auto fail=[&](const char*s){e=s;return false;};
 if(!p||n<24||n>262144||std::memcmp(p,"ENCZON01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)==0||u32(p+20)>64)return fail("Music zones header rejected");
 if(~encore::crc32_update(~0u,p+24,n-24)!=u32(p+16))return fail("Music zones checksum rejected");
 Reader r{p+24,n-24};std::vector<MusicZone>zones;
 for(uint32_t i=0;i<u32(p+20);++i){MusicZone zone;zone.path=r.text();auto count=r.u();if(!r.ok||count==0||count>128||zone.path.rfind("Music/",0)!=0)return fail("Music zone binding rejected");
  for(unsigned char c:zone.path)if(c<32||c>126||c==':'||c=='\\')return fail("Music zone path rejected");
  size_t start=0;while(start<=zone.path.size()){auto end=zone.path.find('/',start);if(end==zone.path.npos)end=zone.path.size();auto part=zone.path.substr(start,end-start);if(part.empty()||part=="."||part=="..")return fail("Unsafe music zone path");if(end==zone.path.size())break;start=end+1;}
  for(const auto&old:zones)if(old.path==zone.path)return fail("Duplicate music zone");
  for(uint32_t j=0;j<count;++j){auto vertices=r.u();if(!r.ok||vertices<3||vertices>64||r.n<size_t(vertices)*8)return fail("Music zone polygon rejected");std::vector<Vec2>poly;for(uint32_t k=0;k<vertices;++k){auto x=r.f(),y=r.f();poly.push_back({x,y});}if(!r.ok||!convex(poly))return fail("Invalid music zone polygon");zone.polygons.push_back(std::move(poly));}
  zones.push_back(std::move(zone));
 }
 if(!r.ok||r.n)return fail("Trailing music zone bytes");zones_=std::move(zones);e.clear();return true;
}
bool MusicZones::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,262144,e)&&load(b.data(),b.size(),e);}
bool MusicZones::matches(const MusicRegionData&d,std::string&e)const{if(!d.valid()||d.regions().size()!=zones_.size()){e="Music zone coverage mismatch";return false;}for(size_t i=0;i<zones_.size();++i)if(zones_[i].path!=d.regions()[i].source_path){e="Music zone source order mismatch";return false;}e.clear();return true;}
bool MusicZones::overlaps(size_t i,RoomView room,Vec2 position)const{if(i>=zones_.size()||!room.valid()||!std::isfinite(position.x)||!std::isfinite(position.y))return false;const auto scene=room.scene();std::vector<Vec2>actor;for(uint32_t j=0;j<scene.actor_hull_count;++j){auto p=room.vertex(scene.actor_hull_first+j);actor.push_back({p.x+position.x,p.y+position.y});}if(actor.size()<3)return false;for(const auto&poly:zones_[i].polygons)if(!separated(actor,poly)&&!separated(poly,actor))return true;return false;}
}
