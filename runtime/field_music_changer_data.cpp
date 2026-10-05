#include "encore/field_music_changer.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=i>=16&&i<20?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return~c;}
struct Reader{
 const uint8_t*p;size_t n,at=64;bool ok=true;
 uint32_t integer(){if(!ok||at>n||n-at<4){ok=false;return 0;}auto v=u32(p+at);at+=4;return v;}
 bool boolean(){auto v=integer();if(v>1)ok=false;return v!=0;}
 float scalar(){auto u=integer();float v;std::memcpy(&v,&u,4);if(!std::isfinite(v)||std::abs(v)>1e6)ok=false;return v;}
 Vec2 point(){return{scalar(),scalar()};}
 std::string text(){auto len=integer();if(!ok||len>8192||at>n||len>n-at){ok=false;return{};}std::string v(reinterpret_cast<const char*>(p+at),len);at+=len;size_t i=0;uint32_t cp;while(i<v.size())if(!encore::utf8_next(v,i,cp)||cp<32){ok=false;break;}return v;}
 std::array<uint8_t,32>hash(){std::array<uint8_t,32>h{};if(!ok||at>n||n-at<h.size()){ok=false;return h;}std::copy(p+at,p+at+h.size(),h.begin());at+=h.size();if(std::all_of(h.begin(),h.end(),[](uint8_t b){return!b;}))ok=false;return h;}
};
bool path(std::string_view p){return!p.empty()&&p.front()!='/'&&p.back()!='/'&&p.find("..") ==p.npos&&p.find(':')==p.npos&&p.find('\\')==p.npos;}
bool symbol(std::string_view s){if(s.empty())return false;for(auto c:s)if(!(c=='_'||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')))return false;return true;}
}
const FieldMusicChangerBinding*FieldMusicChangerData::binding(uint32_t id)const{for(const auto&v:bindings_)if(v.id==id)return&v;return nullptr;}
bool FieldMusicChangerData::load_file(const char*p,std::string&e){
 if(!p){e="MusicChanger file path absent";return false;}std::ifstream f(p,std::ios::binary);if(!f){e="MusicChanger file unavailable";return false;}
 f.seekg(0,std::ios::end);auto n=f.tellg();if(n<64||n>4*1024*1024){e="MusicChanger file size";return false;}f.seekg(0);std::vector<uint8_t>b(static_cast<size_t>(n));if(!f.read(reinterpret_cast<char*>(b.data()),n)){e="MusicChanger file read";return false;}return load(b.data(),b.size(),e);
}
bool FieldMusicChangerData::matches_service(const MusicRegionData&service,const AudioBank&bank,std::string&e)const{
 auto fail=[&](const char*m){e=m;return false;};if(!valid_||!service.valid()||!bank.count()||!music_.matches(bank,e)||service.silence_db()!=music_.silence_db()||service.fade_to_seconds()!=music_.fade_to_seconds()||service.tracks().size()!=music_.tracks().size()||service.regions().size()!=music_.regions().size())return fail("MusicChanger actual service source bank/tuning mismatch");
 for(size_t i=0;i<service.tracks().size();++i){const auto&a=service.tracks()[i];const auto&b=music_.tracks()[i];if(a.id!=b.id||a.source_sha!=b.source_sha||a.import_sha!=b.import_sha||a.source_path!=b.source_path)return fail("MusicChanger actual service track/source mismatch");}
 for(size_t i=0;i<service.regions().size();++i){const auto&a=service.regions()[i];const auto&b=music_.regions()[i];if(a.id!=b.id||a.track_id!=b.track_id||a.disabled!=b.disabled||a.volume_db!=b.volume_db||a.fadein_seconds!=b.fadein_seconds||a.fadeout_seconds!=b.fadeout_seconds||a.source_path!=b.source_path||a.appear_flag!=b.appear_flag||a.disappear_flag!=b.disappear_flag||a.shape_paths!=b.shape_paths)return fail("MusicChanger actual service region/source mismatch");}e.clear();return true;
}
bool FieldMusicChangerData::source_hash(std::string_view path_,std::array<uint8_t,32>&out)const{auto i=sources_.find(std::string(path_));if(i==sources_.end())return false;out=i->second;return true;}
bool FieldMusicChangerData::load(const uint8_t*p,size_t n,std::string&e){
 auto reject=[&](const char*m){e=m;return false;};if(!p||n<64||n>4*1024*1024)return reject("MusicChanger pack size");
 if(std::memcmp(p,"ENCMCA01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+24)!=1||!u32(p+28)||u32(p+28)>64||u32(p+16)!=crc(p,n))return reject("MusicChanger header/version/capability/rules/CRC");
 for(size_t i=52;i<64;++i)if(p[i])return reject("MusicChanger reserved header");
 FieldMusicChangerData d;std::copy(p+32,p+52,d.pin_.begin());if(std::all_of(d.pin_.begin(),d.pin_.end(),[](uint8_t b){return!b;}))return reject("MusicChanger source pin absent");
 Reader r{p,n};d.scene_=r.text();d.script_=r.text();d.stop_default_=r.scalar();const auto marker=d.script_.find("::");
 if(!r.ok||!path(d.scene_)||marker==d.script_.npos||!path(std::string_view(d.script_).substr(0,marker))||!symbol(std::string_view(d.script_).substr(marker+2))||d.stop_default_<0||d.stop_default_>60)return reject("MusicChanger source identity/stop default");
 auto inner=r.integer();if(!r.ok||inner<64||inner>65536||r.at>n||inner>n-r.at)return reject("MusicChanger embedded service resource size");d.music_bytes_.assign(p+r.at,p+r.at+inner);r.at+=inner;if(!d.music_.load(d.music_bytes_.data(),d.music_bytes_.size(),e))return false;
 auto count=r.integer();if(count!=3)return reject("MusicChanger connection capability");std::set<std::string>signals;
 for(uint32_t i=0;i<count;++i){FieldMusicChangerConnection c;c.role=r.integer();c.signal=r.text();c.method=r.text();if(!r.ok||c.role!=i+1||!signals.insert(c.signal).second||!symbol(c.signal)||!symbol(c.method))return reject("MusicChanger source signal role/identity");d.connections_.push_back(std::move(c));}
 count=r.integer();if(count!=u32(p+28)||count!=d.music_.regions().size())return reject("MusicChanger source service instance count");std::set<uint32_t>ids,ordinals;std::set<std::string>nodes;uint32_t previous=0;
 for(uint32_t i=0;i<count;++i){
  FieldMusicChangerBinding b;b.id=r.integer();b.ready_ordinal=r.integer();b.region_id=r.integer();b.track_id=r.integer();b.collision_layer=r.integer();b.collision_mask=r.integer();b.flags=r.integer();b.node=r.text();auto owners=r.integer();const auto&region=d.music_.regions()[i];
  if(!r.ok||!b.id||!ids.insert(b.id).second||!ordinals.insert(b.ready_ordinal).second||(i&&b.ready_ordinal<=previous)||b.flags>3||!path(b.node)||!nodes.insert(b.node).second||region.id!=b.region_id||region.track_id!=b.track_id||region.source_path!=b.node||region.disabled||!owners||owners>16||owners!=region.shape_paths.size())return reject("MusicChanger source geometry/service identity");
  for(uint32_t j=0;j<owners;++j){
   FieldMusicChangerShape s;s.id=r.integer();s.order=r.integer();s.disabled=r.boolean();s.node=r.text();auto pieces=r.integer();
   if(!r.ok||!s.id||!ids.insert(s.id).second||s.order!=j||!path(s.node)||!nodes.insert(s.node).second||region.shape_paths[j]!=s.node||!pieces||pieces>128)return reject("MusicChanger source shape owner/order");
   for(uint32_t k=0;k<pieces;++k){auto vertices=r.integer();if(vertices<3||vertices>128)return reject("MusicChanger source convex shape size");std::vector<Vec2>points;for(uint32_t l=0;l<vertices;++l)points.push_back(r.point());if(!r.ok)return reject("MusicChanger source finite geometry");
    bool positive=false,negative=false;for(uint32_t l=0;l<vertices;++l){const auto a=points[l],x=points[(l+1)%vertices],y=points[(l+2)%vertices];const double cross=(double(x.x)-a.x)*(double(y.y)-x.y)-(double(x.y)-a.y)*(double(y.x)-x.x);positive|=cross>0;negative|=cross<0;}
    if(positive==negative)return reject("MusicChanger source convex decomposition invalid");
    s.parts.push_back(std::move(points));
   }b.shapes.push_back(std::move(s));
  }previous=b.ready_ordinal;d.bindings_.push_back(std::move(b));
 }
 count=r.integer();if(!count||count>2048)return reject("MusicChanger source closure count");std::map<std::string,std::array<uint8_t,32>>sources;
 for(uint32_t i=0;i<count;++i){auto name=r.text();auto hash=r.hash();if(!r.ok||!path(name)||!sources.emplace(std::move(name),hash).second)return reject("MusicChanger source closure hash/identity");}
 if(!sources.count(d.scene_)||!sources.count(d.script_.substr(0,marker)))return reject("MusicChanger source closure absent");
 for(const auto&t:d.music_.tracks()){if(t.source_path.substr(0,6)!="res://")return reject("MusicChanger source audio URI");auto s=sources.find(t.source_path.substr(6));auto import=sources.find(t.source_path.substr(6)+".import");if(s==sources.end()||import==sources.end()||s->second!=t.source_sha||import->second!=t.import_sha)return reject("MusicChanger genuine audio source closure");}
 if(!r.ok||r.at!=n)return reject("MusicChanger trailing bytes");
 d.sources_=std::move(sources);d.valid_=true;*this=std::move(d);e.clear();return true;
}
}
