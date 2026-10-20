#include "encore/field_swing_graphics.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint16_t half(const uint8_t*p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
float real(const uint8_t*p){const auto bits=word(p);float v;std::memcpy(&v,&bits,4);return v;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool path(const std::string&s,const char*prefix,const char*suffix){
 if(s.empty()||s.size()>512||s.compare(0,std::strlen(prefix),prefix)||s.size()<std::strlen(suffix)||s.compare(s.size()-std::strlen(suffix),std::strlen(suffix),suffix))return false;
 size_t first=0;for(size_t i=0;i<=s.size();++i){if(i<s.size()&&(uint8_t(s[i])<32||uint8_t(s[i])>=127||s[i]==':'||s[i]=='\\'))return false;
  if(i==s.size()||s[i]=='/'){const auto part=s.substr(first,i-first);if(part.empty()||part=="."||part=="..")return false;first=i+1;}}
 return true;
}
}
bool FieldSwingGraphicsData::load_file(const char*p,std::string&e){std::vector<uint8_t>b;return encore::read_file(p,b,64*1024,e)&&load(b.data(),b.size(),e);}
bool FieldSwingGraphicsData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<128||n>64*1024)return fail(e,"Swing graphics size rejected");
 if(std::memcmp(p,"ENCSGX01",8)||word(p+8)!=1||word(p+12)!=n||word(p+20)!=1||word(p+24)||word(p+28)||word(p+52)!=4||word(p+56)||word(p+60))return fail(e,"Swing graphics schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};
 for(unsigned i=0;i<20;++i)if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Swing graphics source pin rejected");
 if(encore::crc32(p+32,n-32)!=word(p+16))return fail(e,"Swing graphics checksum rejected");
 const uint32_t strides[]={32,128,12,1},limits[]={1,2,1024,2048};uint32_t offsets[4],counts[4];size_t end=128;
 for(unsigned i=0;i<4;++i){const auto*d=p+64+i*16;offsets[i]=word(d+4);counts[i]=word(d+8);
  if(word(d)!=i+1||word(d+12)!=strides[i]||!counts[i]||counts[i]>limits[i]||offsets[i]%4||offsets[i]<end||offsets[i]>n||counts[i]>(n-offsets[i])/strides[i])return fail(e,"Swing graphics directory/span rejected");
  for(size_t j=end;j<offsets[i];++j)if(p[j])return fail(e,"Swing graphics padding rejected");end=offsets[i]+counts[i]*strides[i];
 }
 if(end!=n||counts[1]!=2)return fail(e,"Swing graphics trailing bytes/layers rejected");
 FieldSwingGraphicsData v;auto&m=v.meta_;const auto*b=p+offsets[0];m={word(b),word(b+4),word(b+8),word(b+12),word(b+16),word(b+20)};
 if(!m.frame_width||m.frame_width>1024||!m.frame_height||m.frame_height>1024||!m.source_columns||m.source_columns>256||!m.source_rows||m.source_rows>256||m.source_columns*m.source_rows>65535||m.initial_frame>=m.source_columns*m.source_rows||!m.atlas_columns||m.atlas_columns>16||word(b+24)!=counts[2]||word(b+28))return fail(e,"Swing graphics meta rejected");
 const uint32_t width=m.frame_width*m.atlas_columns,height=((counts[2]+m.atlas_columns-1)/m.atlas_columns)*m.frame_height;
 if(width>1024||height>1024)return fail(e,"Swing graphics atlas capacity rejected");
 uint32_t string_end=0;
 auto text=[&](const uint8_t*r,std::string&out,const char*prefix,const char*suffix){const auto first=word(r),size=word(r+4);if(first!=string_end||!size||size>512||first>counts[3]||size>counts[3]-first)return false;out.assign(reinterpret_cast<const char*>(p+offsets[3]+first),size);string_end+=size;return path(out,prefix,suffix);};
 for(unsigned i=0;i<2;++i){auto&l=v.layers_[i];const auto*r=p+offsets[1]+i*128;
  l.role=word(r);l.width=word(r+4);l.height=word(r+8);l.parent=word(r+12);l.flags=word(r+16);
  if(l.role!=i+1||l.parent!=i||l.width!=width||l.height!=height||l.flags>15||word(r+20))return fail(e,"Swing graphics layer ownership rejected");
  l.position={real(r+24),real(r+28)};l.offset={real(r+32),real(r+36)};
  for(float value:{l.position.x,l.position.y,l.offset.x,l.offset.y})if(!std::isfinite(value)||std::abs(value)>4096)return fail(e,"Swing graphics sprite origin rejected");
  if(!text(r+40,l.path,"graphics/",".t3x")||!text(r+48,l.source,"Graphics/",".png"))return fail(e,"Swing graphics texture identity/span rejected");
  std::copy(r+56,r+88,l.sha256.begin());std::copy(r+88,r+120,l.source_sha256.begin());l.bytes=word(r+120);l.crc=word(r+124);
  if(!l.bytes||l.bytes>8*1024*1024||std::all_of(l.sha256.begin(),l.sha256.end(),[](uint8_t x){return !x;})||std::all_of(l.source_sha256.begin(),l.source_sha256.end(),[](uint8_t x){return !x;}))return fail(e,"Swing graphics texture receipt rejected");
 }
 if(string_end!=counts[3]||v.layers_[0].path==v.layers_[1].path||v.layers_[0].source==v.layers_[1].source)return fail(e,"Swing graphics duplicate/unowned identities rejected");
 bool initial=false;uint32_t previous=0;
 for(uint32_t i=0;i<counts[2];++i){const auto*r=p+offsets[2]+i*12;SwingGraphicsFrame frame{word(r),half(r+4),half(r+6),half(r+8),half(r+10)};
  if(frame.source_frame>=m.source_columns*m.source_rows||(i&&frame.source_frame<=previous)||frame.x!=(i%m.atlas_columns)*m.frame_width||frame.y!=(i/m.atlas_columns)*m.frame_height||frame.width!=m.frame_width||frame.height!=m.frame_height)return fail(e,"Swing graphics frame mapping rejected");
  initial|=frame.source_frame==m.initial_frame;previous=frame.source_frame;v.frames_.push_back(frame);
 }
 if(!initial)return fail(e,"Swing graphics initial frame missing");v.valid_=true;*this=std::move(v);e.clear();return true;
}
const SwingGraphicsFrame*FieldSwingGraphicsData::frame(uint32_t id)const{for(const auto&f:frames_)if(f.source_frame==id)return &f;return nullptr;}
bool FieldSwingGraphicsData::bind(const FieldSwingData&action,std::string&e)const{
 if(!valid_||!action.valid())return fail(e,"Swing graphics/action unprepared");
 if(action.initial().frame!=meta_.initial_frame||action.initial().special!=bool(layers_[0].flags&8)||action.policy()[2]!=layers_[0].position.x||action.policy()[3]!=layers_[0].position.y||action.strings()[0]!=layers_[0].source||action.strings()[1]!=layers_[1].source)return fail(e,"Swing graphics/action source binding differs");
 std::set<uint32_t>required{action.initial().frame};for(const auto&k:action.keys())if(k.kind==SwingTrack::Frame)required.insert(uint32_t(k.value));
 if(required.size()!=frames_.size())return fail(e,"Swing graphics/action frame coverage differs");for(auto id:required)if(!frame(id))return fail(e,"Swing graphics/action frame missing");e.clear();return true;
}
bool FieldSwingGraphicsData::texture_matches(uint32_t role,const uint8_t*p,size_t n,std::string&e)const{
 if(!valid_||role<1||role>layers_.size()||!p)return fail(e,"Swing graphics texture owner rejected");const auto&l=layers_[role-1];if(n!=l.bytes||encore::crc32(p,n)!=l.crc)return fail(e,"Swing graphics texture bytes differ");e.clear();return true;
}
}
