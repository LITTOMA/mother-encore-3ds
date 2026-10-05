#include "encore/present_sparkles.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=i>=16&&i<20?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return~c;}
struct Reader {const uint8_t*p;size_t n,at=64;bool ok=true;
 uint32_t integer(){if(!ok||at>n||n-at<4){ok=false;return 0;}auto r=u32(p+at);at+=4;return r;}
 float scalar(){uint32_t v=integer();float f;std::memcpy(&f,&v,4);if(!std::isfinite(f))ok=false;return f;}
 std::string text(){auto size=integer();if(!ok||size>8192||at>n||size>n-at){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p+at),size);at+=size;size_t i=0;uint32_t cp=0;while(i<s.size()){if(!encore::utf8_next(s,i,cp)||cp<32){ok=false;break;}}return s;}
 std::array<uint8_t,32>hash(){std::array<uint8_t,32>h{};if(!ok||at>n||n-at<32){ok=false;return h;}std::copy(p+at,p+at+32,h.begin());at+=32;if(std::all_of(h.begin(),h.end(),[](uint8_t v){return !v;}))ok=false;return h;}
};
bool path(std::string_view s){return!s.empty()&&s.front()!='/'&&s.back()!='/'&&s.find("..") ==s.npos&&s.find('\\')==s.npos&&s.find(':')==s.npos;}
bool identity(std::string_view s){if(s.empty())return false;for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
}
bool PresentSparklesData::load_file(const char*path,std::string&e){if(!path){e="Sparkles pack path missing";return false;}std::ifstream f(path,std::ios::binary);if(!f){e="Sparkles pack unavailable";return false;}f.seekg(0,std::ios::end);auto n=f.tellg();if(n<64||n>256*1024){e="Sparkles file size";return false;}f.seekg(0);std::vector<uint8_t>b(static_cast<size_t>(n));if(!f.read(reinterpret_cast<char*>(b.data()),n)){e="Sparkles file read failed";return false;}return load(b.data(),b.size(),e);}
bool PresentSparklesData::load(const uint8_t*p,size_t n,std::string&e){
 auto reject=[&](const char*s){e=s;return false;};if(!p||n<64||n>256*1024)return reject("Sparkles pack size");if(std::memcmp(p,"ENCSPL01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+24)!=1||!u32(p+28)||u32(p+28)>1024||u32(p+16)!=crc(p,n))return reject("Sparkles header/version/capability/rules/CRC");for(size_t i=52;i<64;++i)if(p[i])return reject("Sparkles reserved header");
 PresentSparklesData next;const char*hex="0123456789abcdef";for(unsigned i=32;i<52;++i){next.commit_+=hex[p[i]>>4];next.commit_+=hex[p[i]&15];}Reader r{p,n};next.id_=r.integer();next.parent_id_=r.integer();next.width_=r.integer();next.height_=r.integer();next.serialized_frame_=r.integer();next.flags_=r.integer();const auto frames=r.integer(),ready_before_parent=r.integer();next.low_=r.scalar();next.high_=r.scalar();next.speed_=r.scalar();next.speed_scale_=r.scalar();next.parent_position_={r.scalar(),r.scalar()};next.position_={r.scalar(),r.scalar()};next.offset_={r.scalar(),r.scalar()};
 if(!r.ok||!next.id_||next.id_==0xffffffff||!next.parent_id_||next.parent_id_==0xffffffff||!next.width_||!next.height_||next.width_>1024||next.height_>1024||frames!=u32(p+28)||next.serialized_frame_>=frames||next.flags_!=15||ready_before_parent!=1||next.low_<0||next.high_<next.low_||next.high_>=65536||next.speed_<=0||next.speed_>1024||next.speed_scale_<=0||next.speed_scale_>1024)return reject("Sparkles source parameters");
 for(auto v:{next.parent_position_.x,next.parent_position_.y,next.position_.x,next.position_.y,next.offset_.x,next.offset_.y})if(std::abs(v)>=1e6)return reject("Sparkles source transform");
 next.node_=r.text();next.parent_=r.text();next.flag_=r.text();next.animation_=r.text();next.texture_=r.text();next.source_=r.text();const auto image_hash=r.hash();r.hash();
 if(!r.ok||!path(next.node_)||!path(next.parent_)||next.node_.rfind('/')!=next.parent_.size()||next.node_.substr(0,next.parent_.size())!=next.parent_||!identity(next.flag_)||next.animation_.empty()||!path(next.texture_)||next.texture_.find("graphics/")!=0||next.texture_.size()<4||next.texture_.substr(next.texture_.size()-4)!=".t3x"||!path(next.source_)||next.source_.find("Graphics/")!=0||next.source_.size()<4||next.source_.substr(next.source_.size()-4)!=".png")return reject("Sparkles strings/source binding");
 for(uint32_t i=0;i<frames;++i){PresentSparklesFrame f{r.integer(),r.integer(),r.integer(),r.integer()};if(!r.ok||!f.width||!f.height||f.x>next.width_||f.y>next.height_||f.width>next.width_-f.x||f.height>next.height_-f.y)return reject("Sparkles frame rectangle");next.frames_.push_back(f);}
 const auto source_count=r.integer();if(!source_count||source_count>128)return reject("Sparkles source count");std::set<std::string>sources;bool image_found=false;for(uint32_t i=0;i<source_count;++i){const auto source=r.text();const auto h=r.hash();if(!r.ok||!path(source)||!sources.insert(source).second)return reject("Sparkles source receipt");if(source==next.source_){if(h!=image_hash)return reject("Sparkles PNG lineage");image_found=true;}}
 if(!r.ok||r.at!=n||!image_found)return reject("Sparkles trailing data/source closure");next.valid_=true;*this=std::move(next);e.clear();return true;
}
bool PresentSparklesRuntime::ready(const PresentSparklesData&data,SourceRandom&random,std::string&e){
 if(data_||!data.valid()){e="Sparkles Ready requires one admitted child instance";return false;}
 // Bounds are checked before this shared draw. Source int() truncates before
 // AnimatedSprite.set_frame clamps; overshoot is not a modulo operation.
 const auto sample=random.rand_range(data.random_low(),data.random_high());
 if(!std::isfinite(sample)||sample<0||sample>=65536){e="Sparkles source RNG result invalid";return false;}
 const auto selected=std::min(uint32_t(sample),uint32_t(data.frames().size()-1));
 data_=&data;frame_=selected;playing_=data.playing();visible_=true;timeout_=float(1.0/double(float(data.speed()*data.speed_scale())));frame_changes_=selected!=data.serialized_frame();loops_=0;e.clear();return true;
}
bool PresentSparklesRuntime::set_opened(bool opened,std::string&e){if(!data_){e="Sparkles parent Ready before child Ready";return false;}const bool next=!opened;visible_=next;if(playing_!=next){playing_=next;if(playing_)timeout_=float(1.0/double(float(data_->speed()*data_->speed_scale())));}e.clear();return true;}
bool PresentSparklesRuntime::idle_frame(double delta,bool update_pending,std::string&e){
 if(!data_||!std::isfinite(delta)||delta<0||delta>double(.1f)){e="Sparkles idle inputs rejected";return false;}if(!playing_||!update_pending){e.clear();return true;}float remaining=float(delta);
 // Godot INTERNAL_PROCESS uses float remaining/timeout and changes frame at
 // the top of the next nonempty iteration, preserving exact-boundary behavior.
 while(remaining){if(timeout_<=0){timeout_=float(1.0/double(float(data_->speed()*data_->speed_scale())));if(frame_>=data_->frames().size()-1){frame_=0;++loops_;}else ++frame_;++frame_changes_;}const float process=std::min(timeout_,remaining);remaining-=process;timeout_-=process;}
 e.clear();return true;
}
const PresentSparklesFrame*PresentSparklesRuntime::frame()const{return data_&&frame_<data_->frames().size()?&data_->frames()[frame_]:nullptr;}
}
