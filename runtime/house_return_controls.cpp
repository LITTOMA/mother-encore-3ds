#include "encore/house_return_controls.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int k=0;k<8;++k)c=(c>>1)^((c&1)?0xedb88320u:0u);}return ~c;}
bool same(FieldIdentity a,FieldIdentity b){return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;}
bool path(std::string_view s){return !s.empty()&&s.front()!='/'&&s.find_first_of("\\:")==s.npos&&s.find("..") == s.npos;}
bool nz(const std::array<uint8_t,32>&h){return std::any_of(h.begin(),h.end(),[](auto b){return b;});}
uint32_t rot(uint32_t v,unsigned n){return(v>>n)|(v<<(32-n));}
// SHA-256 integrity constants, unrelated to game content or rule tuning.
std::array<uint8_t,32> digest(std::string_view s){
 static constexpr uint32_t k[]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t h[]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 const size_t blocks=(s.size()+72)/64;
 for(size_t b=0;b<blocks;++b){uint8_t raw[64]{};for(size_t j=0;j<64;++j){size_t a=b*64+j;if(a<s.size())raw[j]=uint8_t(s[a]);else if(a==s.size())raw[j]=128;}if(b+1==blocks)for(unsigned j=0;j<8;++j)raw[63-j]=uint8_t(uint64_t(s.size())*8>>(j*8));
  uint32_t w[64];for(unsigned j=0;j<16;++j)w[j]=uint32_t(raw[j*4])<<24|uint32_t(raw[j*4+1])<<16|uint32_t(raw[j*4+2])<<8|raw[j*4+3];for(unsigned j=16;j<64;++j){auto a=w[j-15],c=w[j-2];w[j]=w[j-16]+(rot(a,7)^rot(a,18)^(a>>3))+w[j-7]+(rot(c,17)^rot(c,19)^(c>>10));}
  uint32_t a=h[0],c=h[1],d=h[2],e=h[3],f=h[4],g=h[5],v=h[6],z=h[7];for(unsigned j=0;j<64;++j){auto t=z+(rot(f,6)^rot(f,11)^rot(f,25))+((f&g)^(~f&v))+k[j]+w[j],q=(rot(a,2)^rot(a,13)^rot(a,22))+((a&c)^(a&d)^(c&d));z=v;v=g;g=f;f=e+t;e=d;d=c;c=a;a=t+q;}uint32_t values[]={a,c,d,e,f,g,v,z};for(unsigned j=0;j<8;++j)h[j]+=values[j];
 }
 std::array<uint8_t,32>out{};for(unsigned i=0;i<8;++i)for(unsigned j=0;j<4;++j)out[i*4+j]=uint8_t(h[i]>>(24-j*8));return out;
}
struct Reader {
 const uint8_t*p;size_t n,at=128;bool ok=true;
 uint32_t u(){if(!ok||at>n||n-at<4){ok=false;return 0;}auto v=word(p+at);at+=4;return v;}
 std::array<uint8_t,32>hash(){std::array<uint8_t,32>h{};if(!ok||at>n||n-at<32){ok=false;return h;}std::copy_n(p+at,32,h.begin());at+=32;return h;}
 std::string text(){auto count=u();if(!ok||count>65536||at>n||n-at<count){ok=false;return{};}std::string v(reinterpret_cast<const char*>(p+at),count);at+=count;size_t c=0;if(v.find('\0')!=v.npos||!utf8_count(v,c))ok=false;return v;}
};
// Parses only the closed native scalar/vector/Color/Resource property schema.
// No arbitrary object, expression, dictionary or script may execute here.
struct ValueReader {
 std::string_view s;size_t at=0;
 bool token(std::string_view v){if(s.substr(at,v.size())!=v)return false;at+=v.size();return true;}
 bool number(float&out,bool integral=false,int32_t*integer=nullptr){
  const auto begin=at;if(at<s.size()&&s[at]=='-')++at;
  if(at==s.size())return false;
  if(s[at]=='0')++at;else{if(s[at]<'1'||s[at]>'9')return false;while(at<s.size()&&s[at]>='0'&&s[at]<='9')++at;}
  if(!integral&&at<s.size()&&s[at]=='.'){++at;auto q=at;while(at<s.size()&&s[at]>='0'&&s[at]<='9')++at;if(at==q)return false;}
  if(!integral&&at<s.size()&&(s[at]=='e'||s[at]=='E')){++at;if(at<s.size()&&(s[at]=='+'||s[at]=='-'))++at;auto q=at;while(at<s.size()&&s[at]>='0'&&s[at]<='9')++at;if(at==q)return false;}
  std::string v(s.substr(begin,at-begin));char*end=nullptr;double d=std::strtod(v.c_str(),&end);if(end!=v.c_str()+v.size()||!std::isfinite(d)||std::abs(d)>std::numeric_limits<float>::max())return false;
  if(integral&&(d<INT32_MIN||d>INT32_MAX||std::floor(d)!=d))return false;out=float(d);if(integer)*integer=int32_t(d);return true;
 }
 bool string(std::string&out){
  if(!token("\""))return false;out.clear();while(at<s.size()){unsigned char c=s[at++];if(c=='"'){size_t n=0;return utf8_count(out,n);}if(c<32)return false;
   if(c=='\\'){if(at==s.size())return false;char q=s[at++];switch(q){case '"':case '\\':case '/':out+=q;break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;default:return false;}}
   else out+=char(c);
  }return false;
 }
};
bool value(HouseControlProperty&v){
 ValueReader r{v.canonical};bool ok=false;
 switch(v.type){
 case HouseControlValue::Nil:ok=r.token("null");break;
 case HouseControlValue::Bool:if(r.token("true")){v.boolean=true;ok=true;}else if(r.token("false")){v.boolean=false;ok=true;}break;
 case HouseControlValue::Integer:ok=r.number(v.numbers[0],true,&v.integer);break;
 case HouseControlValue::Real:ok=r.number(v.numbers[0]);break;
 case HouseControlValue::Text:ok=r.string(v.text);break;
 case HouseControlValue::Path:ok=r.token("{\"type\":\"NodePath\",\"value\":")&&r.string(v.text)&&v.text.empty()&&r.token("}");break;
 case HouseControlValue::Vector:ok=r.token("[")&&r.number(v.numbers[0])&&r.token(",")&&r.number(v.numbers[1])&&r.token("]");break;
 case HouseControlValue::Color:ok=r.token("{\"a\":")&&r.number(v.numbers[3])&&r.token(",\"b\":")&&r.number(v.numbers[2])&&r.token(",\"g\":")&&r.number(v.numbers[1])&&r.token(",\"r\":")&&r.number(v.numbers[0])&&r.token(",\"type\":\"Color\"}");break;
 case HouseControlValue::Resource:ok=r.token("{\"id\":")&&r.number(v.numbers[0],true,&v.integer)&&v.integer>0&&r.token(",\"type\":\"ResourceReference\"}");break;
 case HouseControlValue::EditorMetadata:ok=r.token("{\"pairs\":[[\"_edit_lock_\",true]],\"type\":\"Dictionary\"}");v.boolean=ok;break;
 }
 return ok&&r.at==r.s.size();
}
using V=HouseControlValue;
std::map<std::string,V>schema(std::string_view cls,uint32_t flags){
 std::map<std::string,V>s;
 for(auto k:{"_import_path","focus_neighbour_left","focus_neighbour_top","focus_neighbour_right","focus_neighbour_bottom","focus_next","focus_previous"})s[k]=V::Path;
 for(auto k:{"pause_mode","physics_interpolation_mode","process_priority","light_mask","grow_horizontal","grow_vertical","focus_mode","mouse_filter","mouse_default_cursor_shape","size_flags_horizontal","size_flags_vertical"})s[k]=V::Integer;
 for(auto k:{"unique_name_in_owner","visible","show_behind_parent","use_parent_material","rect_clip_content","input_pass_on_modal_close_click"})s[k]=V::Bool;
 for(auto k:{"anchor_left","anchor_top","anchor_right","anchor_bottom","margin_left","margin_top","margin_right","margin_bottom","rect_rotation","size_flags_stretch_ratio"})s[k]=V::Real;
 for(auto k:{"rect_min_size","rect_scale","rect_pivot_offset"})s[k]=V::Vector;
 for(auto k:{"material","theme","script"})s[k]=V::Nil;
 for(auto k:{"hint_tooltip","theme_type_variation"})s[k]=V::Text;
 for(auto k:{"modulate","self_modulate"})s[k]=V::Color;
 if(cls=="Label"){
  s["custom_fonts/font"]=V::Resource;s["text"]=V::Text;
  for(auto k:{"align","valign","lines_skipped","max_lines_visible"})s[k]=V::Integer;
  for(auto k:{"autowrap","clip_text","uppercase"})s[k]=V::Bool;
  s["percent_visible"]=V::Real;
 }else if(cls=="HBoxContainer")s["alignment"]=V::Integer;
 else if(cls=="TextureRect"){
  s["texture"]=V::Resource;s["expand"]=V::Bool;s["stretch_mode"]=V::Integer;
  s["flip_h"]=V::Bool;s["flip_v"]=V::Bool;
 }else if(cls=="ColorRect"){s["color"]=V::Color;s["__meta__"]=V::EditorMetadata;}
 if(flags&32)s["material"]=V::Resource;
 return s;
}
bool supported(const HouseControlRecord&r){
 auto i=[&](const char*k){return r.properties.at(k).integer;};auto f=[&](const char*k){return r.properties.at(k).numbers[0];};
 if(i("pause_mode")<0||i("pause_mode")>2||i("physics_interpolation_mode")<0||i("physics_interpolation_mode")>2||i("grow_horizontal")<0||i("grow_horizontal")>2||i("grow_vertical")<0||i("grow_vertical")>2||i("focus_mode")!=0||i("mouse_filter")<0||i("mouse_filter")>2||i("mouse_default_cursor_shape")!=0||i("size_flags_horizontal")<0||i("size_flags_horizontal")>15||i("size_flags_vertical")<0||i("size_flags_vertical")>15||f("size_flags_stretch_ratio")<=0)return false;
 if(!r.properties.at("hint_tooltip").text.empty()||!r.properties.at("theme_type_variation").text.empty()||r.properties.at("rect_clip_content").boolean)return false;
 if(r.properties.at("use_parent_material").boolean&&r.native_class!="TextureRect")return false;
 if(r.native_class=="TextureRect"&&(i("stretch_mode")<0||i("stretch_mode")>6))return false;
 if(r.native_class=="Label")return i("align")>=0&&i("align")<=3&&i("valign")>=0&&i("valign")<=3&&i("lines_skipped")==0&&i("max_lines_visible")==-1&&!r.properties.at("autowrap").boolean&&!r.properties.at("clip_text").boolean&&!r.properties.at("uppercase").boolean&&f("percent_visible")==1;
 return r.native_class!="HBoxContainer"||(i("alignment")>=0&&i("alignment")<=2);
}
}
const HouseControlProperty*HouseControlRecord::property(std::string_view s)const{auto i=properties.find(std::string(s));return i==properties.end()?nullptr:&i->second;}
const HouseControlRecord*HouseReturnControlsData::record(uint32_t id)const{for(const auto&r:records_)if(r.id==id)return&r;return nullptr;}
bool HouseReturnControlsData::source_hash(std::string_view s,std::array<uint8_t,32>&h)const{auto i=sources_.find(std::string(s));if(i==sources_.end())return false;h=i->second;return true;}
bool HouseReturnControlsData::matches(const FieldNodeTreeData&t,const FieldCanvasArtData&c,std::string&e)const{
 size_t count=c.control_boundaries().size();for(const auto&r:c.records())if(r.kind==1||r.kind==3)++count;
 if(!valid_||!t.valid()||!c.valid()||c.format()!=2||!same(identity_,t.identity())||!same(identity_,c.identity())||t.source_scene()!=scene_||c.source_scene()!=scene_||c.ir_sha256()!=canvas_ir_||c.tree_ir_sha()!=tree_ir_||count!=records_.size())return fail(e,"House Control actual Tree/Canvas resource identity differs");
 std::array<uint8_t,32>h{},th{};
 for(const auto&r:records_){auto*n=t.record(r.id);auto*b=c.control_boundary(r.id);auto*owner=t.record(r.owner);auto*draw=c.record(r.id);
  if(!n||n->class_index>=t.classes().size()||t.classes()[n->class_index]!=r.native_class||n->path!=r.node||n->parent!=r.parent||n->flags!=r.flags)return fail(e,"House Control native tree structure differs");
  if(r.owner){if(!owner||owner->script!=r.owner_script||owner->script_sha!=r.owner_sha||!source_hash(r.owner_script,h)||h!=r.owner_sha||!t.source_hash(r.owner_script,th)||th!=h)return fail(e,"House Control original source owner proof differs");}
  else if(!r.owner_script.empty()||nz(r.owner_sha))return fail(e,"House Control absent source owner fabricated");
  if(!r.draw_kind){if(!b||b->node!=r.node||b->native_class!=r.native_class||b->flags!=r.flags||b->owner_id!=r.owner||b->owner_script!=r.owner_script||b->owner_sha!=r.owner_sha||b->native_properties_sha!=r.properties_sha||r.texture||!r.texture_source.empty()||nz(r.texture_sha)||(r.native_class=="Control"?r.pose_owner!=0:r.pose_owner!=1))return fail(e,"House Control complete boundary/property proof differs");}
  else{
   if(!draw||b||draw->kind!=r.draw_kind||draw->flags!=r.flags||draw->owner_id!=r.owner||draw->owner_script!=r.owner_script||draw->owner_sha!=r.owner_sha||draw->texture!=r.texture||draw->node!=r.node)return fail(e,"House Control actual Canvas draw proof differs");
   if(r.native_class=="TextureRect"){
    auto*tex=c.texture(r.texture);if(r.draw_kind!=1||!tex||tex->source!=r.texture_source||tex->source_sha!=r.texture_sha||!source_hash(r.texture_source,h)||h!=r.texture_sha||draw->stretch!=uint32_t(r.properties.at("stretch_mode").integer)||draw->flip_h!=r.properties.at("flip_h").boolean||draw->flip_v!=r.properties.at("flip_v").boolean||r.pose_owner!=(draw->owner==FieldCanvasOwner::Prompt?1u:2u))return fail(e,"House native TextureRect source/material/texture proof differs");
   }else if(r.native_class!="ColorRect"||r.draw_kind!=3||r.texture||!r.texture_source.empty()||nz(r.texture_sha)||r.pose_owner!=2||draw->color!=r.properties.at("color").numbers)return fail(e,"House native ColorRect source color/owner proof differs");
   const auto width=r.properties.at("margin_right").numbers[0]-r.properties.at("margin_left").numbers[0],height=r.properties.at("margin_bottom").numbers[0]-r.properties.at("margin_top").numbers[0];if(draw->size.x!=width||draw->size.y!=height)return fail(e,"House native Control source draw/rect size differs");
  }
  if(r.native_class=="Label"&&(b->font_source!=r.font_source||b->font_source_sha!=r.font_sha||b->text!=r.properties.at("text").text||!source_hash(r.font_source,h)||h!=r.font_sha))return fail(e,"House Label original native font/text differs");
  const auto&a=r.properties.at("modulate").numbers;const auto&self=r.properties.at("self_modulate").numbers;
  if(a!=n->modulate||self!=n->self_modulate||uint32_t(r.properties.at("pause_mode").integer)!=n->pause||r.properties.at("process_priority").integer!=n->priority||uint32_t(r.properties.at("light_mask").integer)!=n->light_mask||r.properties.at("visible").boolean!=bool(n->flags&2)||r.properties.at("show_behind_parent").boolean!=bool(n->flags&8)||r.properties.at("use_parent_material").boolean!=bool(n->flags&16))return fail(e,"House Control native Canvas/Node constructor fields differ");
  if(r.native_class=="Label"){auto*p=record(r.parent);if(!p||p->native_class!="HBoxContainer"||p->owner!=r.owner)return fail(e,"House Label real HBox/source parent differs");}
  if(r.native_class=="Control"){auto*p=t.record(r.parent);if(!p||p->class_index>=t.classes().size()||t.classes()[p->class_index]!="Node2D")return fail(e,"House Control native parent anchor class rejected");}
 }
 e.clear();return true;
}
bool HouseReturnControlsData::load(const uint8_t*p,size_t n,const FieldIdentity&expected,const FieldNodeTreeData&t,const FieldCanvasArtData&c,std::string&e){
 if(!p||n<128||n>1024*1024||std::memcmp(p,"ENCHCTL1",8)||word(p+8)!=1||word(p+12)!=128||word(p+16)!=n||word(p+20)!=crc(p+128,n-128)||word(p+24)!=0x454e0081||word(p+28)!=1||word(p+32)!=1||word(p+124))return fail(e,"House Control header/format/capability/rules rejected");
 HouseReturnControlsData d;d.identity_.scene_id=word(p+36);std::copy_n(p+40,20,d.identity_.upstream_commit.begin());std::copy_n(p+60,32,d.identity_.source_sha256.begin());std::copy_n(p+92,32,d.ir_.begin());if(!same(d.identity_,expected)||!nz(d.ir_))return fail(e,"House Control source identity rejected");
 Reader rd{p,n};d.scene_=rd.text();d.canvas_ir_=rd.hash();d.tree_ir_=rd.hash();
 const auto engines=rd.u();if(engines!=9)return fail(e,"House Control native engine source proof extent rejected");
 for(uint32_t j=0;j<engines;++j){auto pth=rd.text();auto h=rd.hash();if(!path(pth)||!nz(h)||!d.engine_.emplace(pth,h).second)return fail(e,"House Control native engine source proof malformed");}
 for(auto name:{"scene/gui/control.cpp","scene/gui/box_container.cpp","scene/gui/container.cpp","scene/gui/label.cpp","scene/2d/canvas_item.h","scene/2d/node_2d.h","scene/2d/node_2d.cpp","scene/main/viewport.cpp","core/math/rect2.h"})if(!d.engine_.count(name))return fail(e,"House Control native engine source proof unknown");
 size_t expected_count=c.control_boundaries().size();for(const auto&r:c.records())if(r.kind==1||r.kind==3)++expected_count;
 auto count=rd.u();if(!path(d.scene_)||!nz(d.canvas_ir_)||!nz(d.tree_ir_)||count!=expected_count||count>4096)return fail(e,"House Control source/record closure rejected");
 for(uint32_t j=0;j<count;++j){HouseControlRecord r;r.id=rd.u();r.parent=rd.u();r.owner=rd.u();r.flags=rd.u();r.node=rd.text();r.native_class=rd.text();r.owner_script=rd.text();r.owner_sha=rd.hash();r.properties_sha=rd.hash();r.font_source=rd.text();r.font_sha=rd.hash();
  r.draw_kind=rd.u();r.texture=rd.u();r.texture_source=rd.text();r.texture_sha=rd.hash();r.pose_owner=rd.u();
  if(!r.id||!r.parent||d.record(r.id)||!path(r.node)||(r.owner?(!path(r.owner_script)||!nz(r.owner_sha)):(!r.owner_script.empty()||nz(r.owner_sha)))||!nz(r.properties_sha)||r.pose_owner>2||(r.draw_kind!=0&&r.draw_kind!=1&&r.draw_kind!=3)||(r.native_class!="Control"&&r.native_class!="HBoxContainer"&&r.native_class!="Label"&&r.native_class!="TextureRect"&&r.native_class!="ColorRect"))return fail(e,"House Control unknown native class/duplicate source record");
  auto s=schema(r.native_class,r.flags);if(rd.u()!=s.size())return fail(e,"House Control complete property schema rejected");std::string canonical="{";bool first=true;std::string last;
  for(size_t k=0;k<s.size();++k){auto key=rd.text();auto kind=rd.u();auto raw=rd.text();auto found=s.find(key);HouseControlProperty v;v.type=V(kind);v.canonical=raw;if(!rd.ok||found==s.end()||kind>uint32_t(V::EditorMetadata)||found->second!=v.type||(!last.empty()&&key<=last)||!value(v)||!r.properties.emplace(key,std::move(v)).second)return fail(e,"House Control unknown/malformed native property rejected");if(!first)canonical+=',';first=false;canonical+='"'+key+"\":"+raw;last=key;}
  canonical+='}';if(digest(canonical)!=r.properties_sha||!supported(r)||(r.native_class=="Label"?(!path(r.font_source)||!nz(r.font_sha)):(!r.font_source.empty()||nz(r.font_sha))))return fail(e,"House Control property digest/unsupported native policy rejected");d.records_.push_back(std::move(r));
 }
 count=rd.u();if(!count||count>8192)return fail(e,"House Control source closure extent rejected");for(uint32_t j=0;j<count;++j){auto pth=rd.text();auto h=rd.hash();if(!path(pth)||!nz(h)||!d.sources_.emplace(pth,h).second)return fail(e,"House Control duplicate/unknown source proof");}
 std::array<uint8_t,32>h{};if(!rd.ok||rd.at!=n||!d.source_hash(d.scene_,h)||h!=d.identity_.source_sha256)return fail(e,"House Control incomplete/trailing source payload");d.valid_=true;if(!d.matches(t,c,e))return false;*this=std::move(d);e.clear();return true;
}
bool HouseReturnControlsData::load_file(const char*file,const FieldIdentity&id,const FieldNodeTreeData&t,const FieldCanvasArtData&c,std::string&e){
 if(!file)return fail(e,"House Control resource path absent");FILE*f=std::fopen(file,"rb");if(!f)return fail(e,"House Control resource unavailable");if(std::fseek(f,0,SEEK_END)){std::fclose(f);return fail(e,"House Control resource seek failed");}auto n=std::ftell(f);if(n<128||n>1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);return fail(e,"House Control resource extent rejected");}std::vector<uint8_t>b(static_cast<size_t>(n));bool ok=std::fread(b.data(),1,b.size(),f)==b.size();ok=std::fclose(f)==0&&ok;return ok?load(b.data(),b.size(),id,t,c,e):fail(e,"House Control incomplete resource read");
}
}
