#include "encore/field_battle_bg_resources.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <set>
namespace encore::upstream {
 namespace {
  uint32_t word(const uint8_t*p){
   return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
  }
  uint32_t crc(const uint8_t*p,size_t n){
   uint32_t c=~0u;
   while(n--){
    c^=*p++;
    for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320u:0u);
   }
   return ~c;
  }
  struct Reader {
   const uint8_t*p;
   size_t n,at=128;
   bool ok=true;
   uint32_t integer(){
    if(!ok||at>n||n-at<4){
     ok=false;
     return 0;
    }
    auto v=word(p+at);
    at+=4;
    return v;
   }
   int32_t signed_integer(){
    auto v=integer();
    int32_t out;
    std::memcpy(&out,&v,4);
    return out;
   }
   float scalar(){
    auto v=integer();
    float f;
    std::memcpy(&f,&v,4);
    if(!std::isfinite(f))ok=false;
    return f;
   }
   Vec2 vector(){
    float x=scalar(),y=scalar();
    return {
     x,y
    }
    ;
   }
   std::string text(){
    auto len=integer();
    if(!ok||len>65536||at>n||len>n-at){
     ok=false;
     return{
     }
     ;
    }
    std::string s(reinterpret_cast<const char*>(p+at),len);
    at+=len;
    size_t count=0;
    if(s.find('\0')!=s.npos||!utf8_count(s,count))ok=false;
    return s;
   }
   std::array<uint8_t,32>hash(){
    std::array<uint8_t,32>h{
    }
    ;
    if(!ok||at>n||n-at<32){
     ok=false;
     return h;
    }
    std::copy_n(p+at,32,h.begin());
    at+=32;
    return h;
   }
  }
  ;
  bool empty_hash(const std::array<uint8_t,32>&h){
   return std::all_of(h.begin(),h.end(),[](uint8_t v){
    return !v;
   }
   );
  }
  bool path(std::string_view s){
   if(s.empty()||s.front()=='/'||s.back()=='/'||s.find('\\')!=s.npos||s.find(':')!=s.npos)return false;
   size_t b=0;
   while(b<s.size()){
    auto e=s.find('/',b);
    if(e==s.npos)e=s.size();
    auto v=s.substr(b,e-b);
    if(v.empty()||v=="."||v=="..")return false;
    b=e+1;
   }
   return true;
  }
 FieldBgValue value(Reader&r,unsigned depth,size_t&budget){
  FieldBgValue v;if(depth>64||!budget--||!r.ok||r.at>=r.n){r.ok=false;return v;}uint8_t tag=r.p[r.at++];if(tag>6){r.ok=false;return v;}v.kind=static_cast<FieldBgValueKind>(tag);
  switch(v.kind){
   case FieldBgValueKind::Nil:break;
   case FieldBgValueKind::Boolean:if(r.at>=r.n||r.p[r.at]>1){r.ok=false;break;}v.boolean=r.p[r.at++]!=0;break;
   case FieldBgValueKind::Integer:case FieldBgValueKind::Real:{if(r.at>r.n||r.n-r.at<8){r.ok=false;break;}uint64_t bits=uint64_t(word(r.p+r.at))|(uint64_t(word(r.p+r.at+4))<<32);r.at+=8;if(v.kind==FieldBgValueKind::Integer)std::memcpy(&v.integer,&bits,8);else{std::memcpy(&v.real,&bits,8);if(!std::isfinite(v.real))r.ok=false;}break;}
   case FieldBgValueKind::String:v.text=r.text();break;
   case FieldBgValueKind::Array:{auto n=r.integer();if(n>100000||n>budget){r.ok=false;break;}for(uint32_t i=0;i<n&&r.ok;++i)v.array.push_back(value(r,depth+1,budget));break;}
   case FieldBgValueKind::Map:{auto n=r.integer();std::set<std::string>keys;if(n>100000||n>budget){r.ok=false;break;}for(uint32_t i=0;i<n&&r.ok;++i){auto key=value(r,depth+1,budget);if(key.kind!=FieldBgValueKind::String||!keys.insert(key.text).second){r.ok=false;break;}auto item=value(r,depth+1,budget);v.map.emplace_back(std::move(key.text),std::move(item));}break;}
  }return v;
 }
 const FieldBgValue*member(const FieldBgValue&v,const char*k,FieldBgValueKind kind){auto*p=v.member(k);return p&&p->kind==kind?p:nullptr;}
 bool variant(const FieldBgValue&v,size_t resources,unsigned depth=0){
  if(depth>64)return false;
  if(v.kind==FieldBgValueKind::Nil||v.kind==FieldBgValueKind::Boolean||v.kind==FieldBgValueKind::String||v.kind==FieldBgValueKind::Integer||v.kind==FieldBgValueKind::Real)return true;
  if(v.kind!=FieldBgValueKind::Map)return false;
  auto type=member(v,"type",FieldBgValueKind::String);if(!type)return false;const auto&t=type->text;
  if(t=="ResourceReference"){auto id=member(v,"id",FieldBgValueKind::Integer);return v.map.size()==2&&id&&id->integer>=0&&uint64_t(id->integer)<resources;}
  if(t=="NodeReference"||t=="NodePath"){auto text=member(v,t=="NodePath"?"value":"path",FieldBgValueKind::String);return v.map.size()==2&&text&&text->text.find('\\')==text->text.npos;}
  if(t=="real"||t=="int64"){
   auto text=member(v,"value",FieldBgValueKind::String);if(v.map.size()!=2||!text||text->text.empty())return false;char*end=nullptr;errno=0;if(t=="real"){auto x=std::strtod(text->text.c_str(),&end);return !errno&&std::isfinite(x)&&end&&*end==0;}std::strtoll(text->text.c_str(),&end,10);return !errno&&end&&*end==0;
  }
  if(t=="Vector2"||t=="Vector3"||t=="Color"||t=="Rect2"||t=="Transform2D"){
   std::vector<const char*>keys;if(t=="Vector2")keys={"x","y"};else if(t=="Vector3")keys={"x","y","z"};else if(t=="Color")keys={"r","g","b","a"};else if(t=="Rect2")keys={"position","size"};else keys={"x","y","origin"};if(v.map.size()!=keys.size()+1)return false;
   for(auto k:keys){auto item=v.member(k);if(!item||!variant(*item,resources,depth+1))return false;}return true;
  }
  if(t=="Array"||t=="PoolByteArray"||t=="PoolIntArray"||t=="PoolRealArray"||t=="PoolStringArray"||t=="PoolVector2Array"||t=="PoolVector3Array"||t=="PoolColorArray"){
   auto list=member(v,"value",FieldBgValueKind::Array);if(v.map.size()!=2||!list)return false;for(const auto&item:list->array)if(!variant(item,resources,depth+1))return false;return true;
  }
  if(t=="Dictionary"){auto list=member(v,"pairs",FieldBgValueKind::Array);if(v.map.size()!=2||!list)return false;for(const auto&pair:list->array)if(pair.kind!=FieldBgValueKind::Array||pair.array.size()!=2||!variant(pair.array[0],resources,depth+1)||!variant(pair.array[1],resources,depth+1))return false;return true;}
  return false;
 }
 bool properties(const FieldBgValue&v,size_t resources){if(v.kind!=FieldBgValueKind::Map)return false;for(const auto&p:v.map)if(p.first.empty()||!variant(p.second,resources))return false;auto*script=v.member("script");return !script||script->kind==FieldBgValueKind::Nil;}
 uint32_t bigword(const uint8_t*p){return uint32_t(p[3])|uint32_t(p[2])<<8|uint32_t(p[1])<<16|uint32_t(p[0])<<24;}
 bool png(FieldBgTexturePayload&t){
  static const uint8_t signature[]={137,80,78,71,13,10,26,10};if(t.png.size()<33||std::memcmp(t.png.data(),signature,8))return false;size_t at=8;bool header=false,end=false,data=false;
  while(at<t.png.size()){
   if(t.png.size()-at<12)return false;
   auto len=bigword(t.png.data()+at);if(len>t.png.size()-at-12)return false;auto type=t.png.data()+at+4;auto payload=type+4;if(bigword(payload+len)!=crc(type,size_t(len)+4))return false;
   if(!header){if(std::memcmp(type,"IHDR",4)||len!=13)return false;t.width=bigword(payload);t.height=bigword(payload+4);if(!t.width||!t.height||t.width>4096||t.height>4096)return false;header=true;}
   else if(!std::memcmp(type,"IHDR",4))return false;
   if(!std::memcmp(type,"IDAT",4))data=true;
   if(!std::memcmp(type,"IEND",4)){if(len||!data)return false;end=true;at+=12;break;}
   at+=size_t(len)+12;
  }return end&&at==t.png.size();
 }
 bool alias_equal(const FieldBgValue&a,const FieldBgValue&as,const FieldBgValue&b,const FieldBgValue&bs){
  if(a.kind!=b.kind)return false;
  if(a.kind==FieldBgValueKind::Map){
   auto ta=a.member("type"),tb=b.member("type");
   if(ta&&tb&&ta->text=="ResourceReference"&&tb->text=="ResourceReference"){
    auto ia=a.member("id"),ib=b.member("id");if(!ia||!ib||ia->integer<0||ib->integer<0||uint64_t(ia->integer)>=as.array.size()||uint64_t(ib->integer)>=bs.array.size())return false;
    auto sa=as.array[size_t(ia->integer)].member("source_object_id"),sb=bs.array[size_t(ib->integer)].member("source_object_id");return sa&&sb&&sa->text==sb->text;
   }
   if(a.map.size()!=b.map.size())return false;
   for(const auto&item:a.map){auto other=b.member(item.first);if(!other||!alias_equal(item.second,as,*other,bs))return false;}return true;
  }
  if(a.kind==FieldBgValueKind::Array){if(a.array.size()!=b.array.size())return false;for(size_t i=0;i<a.array.size();++i)if(!alias_equal(a.array[i],as,b.array[i],bs))return false;return true;}
  return a.boolean==b.boolean&&a.integer==b.integer&&a.real==b.real&&a.text==b.text;
 }
 bool graph(const FieldBgPackedGraph&s){
  auto source=member(s.graph,"source",FieldBgValueKind::String),key=member(s.graph,"key",FieldBgValueKind::String),nodes=member(s.graph,"nodes",FieldBgValueKind::Array),resources=member(s.graph,"resources",FieldBgValueKind::Array),states=member(s.graph,"scene_states",FieldBgValueKind::Array);if(!source||!key||!nodes||!resources||!states||source->text!="res://"+s.path||key->text!=s.key||nodes->array.empty()||nodes->array.size()>10000||states->array.size()!=1||resources->array.size()>10000)return false;
  std::map<std::string,std::string>paths;
  for(const auto&n:nodes->array){auto p=member(n,"path",FieldBgValueKind::String),c=member(n,"class",FieldBgValueKind::String),props=member(n,"properties",FieldBgValueKind::Map);if(!p||!c||!props||!paths.emplace(p->text,c->text).second||(c->text!="PanelContainer"&&c->text!="TextureRect"&&c->text!="BackBufferCopy")||!properties(*props,resources->array.size()))return false;}
  if(!paths.count(".")||paths.at(".")!="PanelContainer")return false;
  for(size_t i=0;i<resources->array.size();++i){const auto&r=resources->array[i];auto id=member(r,"id",FieldBgValueKind::Integer),c=member(r,"class",FieldBgValueKind::String),p=member(r,"path",FieldBgValueKind::String);if(!id||!c||!p||id->integer!=int64_t(i)||(c->text!="StyleBoxEmpty"&&c->text!="Shader"&&c->text!="ShaderMaterial"&&c->text!="StreamTexture"))return false;auto props=r.member("properties");if(c->text=="StreamTexture"){if(p->text.substr(0,6)!="res://"||p->text.size()<10||p->text.substr(p->text.size()-4)!=".png"||!r.member("payload")||!r.member("size"))return false;}else if(!props||!properties(*props,resources->array.size()))return false;}
  const auto&state=states->array[0];auto state_source=member(state,"source",FieldBgValueKind::String),state_nodes=member(state,"nodes",FieldBgValueKind::Array),connections=member(state,"connections",FieldBgValueKind::Array);if(!state_source||state_source->text!=source->text||!state_nodes||state_nodes->array.size()!=nodes->array.size()||!connections||!connections->array.empty())return false;
  std::set<std::string>statepaths;for(const auto&n:state_nodes->array){auto p=member(n,"path",FieldBgValueKind::String),c=member(n,"class",FieldBgValueKind::String),props=member(n,"properties",FieldBgValueKind::Map),owner=member(n,"owner",FieldBgValueKind::String);if(!p||!c||!props||!owner)return false;auto path=p->text;if(path.substr(0,2)=="./")path=path.substr(2);auto it=paths.find(path);if(it==paths.end()||it->second!=c->text||!statepaths.insert(path).second||!properties(*props,resources->array.size()))return false;auto instance=n.member("instance");if(!instance||instance->kind!=FieldBgValueKind::Nil||(path=="."?!owner->text.empty():owner->text!="."))return false;}
  return true;
 }
 }

bool FieldBattleBgData::load_file(const char*p,const FieldIdentity&id,std::string&e){
 if(!p||!*p){e="BG resource path rejected";return false;}FILE*f=std::fopen(p,"rb");if(!f){e="Cannot open BG resource";return false;}if(std::fseek(f,0,SEEK_END)){std::fclose(f);e="BG seek rejected";return false;}long n=std::ftell(f);if(n<128||n>64*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);e="BG size rejected";return false;}std::vector<uint8_t>b(static_cast<size_t>(n));auto got=std::fread(b.data(),1,b.size(),f);bool closed=std::fclose(f)==0;if(got!=b.size()||!closed){e="BG read rejected";return false;}return load(b.data(),b.size(),id,e);
}
bool FieldBattleBgData::load(const uint8_t*p,size_t n,const FieldIdentity&id,std::string&e){
 auto fail=[&](const char*s){e=s;return false;};if(!p||n<128||n>64*1024*1024||std::memcmp(p,"ENCFBGR1",8)||word(p+8)!=1||word(p+12)!=128||word(p+16)!=n||word(p+24)!=0x454e004a||word(p+28)!=1||word(p+32)!=50||!id.scene_id||word(p+36)!=id.scene_id||std::memcmp(p+40,id.upstream_commit.data(),20)||std::memcmp(p+60,id.source_sha256.data(),32)||word(p+124)||word(p+20)!=crc(p+128,n-128))return fail("BG identity/schema/capability/CRC rejected");
 FieldBattleBgData d;d.identity_=id;Reader r{p,n};d.directory_=r.text();if(!path(d.directory_.substr(0,d.directory_.size()-1))||d.directory_.back()!='/')return fail("BG source directory rejected");auto count=r.integer();if(!count||count>10000)return fail("BG source proof count rejected");for(uint32_t i=0;i<count;++i){auto name=r.text();auto h=r.hash();if(!r.ok||!path(name)||empty_hash(h)||!d.sources_.emplace(name,h).second)return fail("BG source file proof rejected");}
 bool found=false;for(const auto&source:d.sources_)if(source.second==id.source_sha256)found=true;if(!found)return fail("BG original importer source proof missing");
 count=r.integer();if(count!=50)return fail("BG full original fifty resource roster rejected");std::set<uint32_t>ids;std::set<std::string>keys;
 for(uint32_t i=0;i<count;++i){FieldBgPackedGraph s;s.id=r.integer();s.path=r.text();s.key=r.text();s.sha=r.hash();auto len=r.integer();auto proof=d.sources_.find(s.path);if(!r.ok||!s.id||!ids.insert(s.id).second||s.key.empty()||!keys.insert(s.key).second||!path(s.path)||proof==d.sources_.end()||proof->second!=s.sha||len<5||len>8*1024*1024||r.at>n||len>n-r.at)return fail("BG complete source PackedScene identity rejected");Reader nested{p+r.at,len,0};size_t budget=2000000;s.graph=value(nested,0,budget);r.at+=len;if(!nested.ok||nested.at!=len||!graph(s))return fail("BG full source node/property/resource/connection graph rejected");d.scenes_.push_back(std::move(s));}
 std::map<std::string,std::pair<size_t,size_t>>source_aliases;
 for(size_t si=0;si<d.scenes_.size();++si){const auto&scene=d.scenes_[si];const auto&rows=scene.graph.member("resources")->array;
  for(size_t ri=0;ri<rows.size();++ri){const auto&resource=rows[ri];auto source=member(resource,"source_object_id",FieldBgValueKind::String);if(!source||source->text.empty()||source->text=="0"||source->text.find_first_not_of("0123456789")!=source->text.npos)return fail("BG source resource alias identity rejected");auto entry=source_aliases.emplace(source->text,std::make_pair(si,ri));const auto&first=d.scenes_[entry.first->second.first];const auto&firstrows=*first.graph.member("resources");const auto&original=firstrows.array[entry.first->second.second];if(!entry.second){
    if(resource.map.size()!=original.map.size())return fail("BG shared source Resource property count changed");
    for(const auto&item:resource.map){if(item.first=="id")continue;auto other=original.member(item.first);if(!other||!alias_equal(item.second,*scene.graph.member("resources"),*other,firstrows))return fail("BG shared source Resource properties/refs changed");}
   }
   d.aliases_.emplace(std::make_pair(scene.id,uint32_t(ri)),FieldBgResourceAlias{first.id,uint32_t(entry.first->second.second),source->text});
  }
 }
 count=r.integer();if(!count||count>10000)return fail("BG native texture resource payload count rejected");std::set<std::string>texturepaths;
 for(uint32_t i=0;i<count;++i){FieldBgTexturePayload t;t.path=r.text();t.sha=r.hash();auto len=r.integer();auto proof=d.sources_.find(t.path);if(!r.ok||!path(t.path)||!texturepaths.insert(t.path).second||proof==d.sources_.end()||proof->second!=t.sha||len<33||len>16*1024*1024||r.at>n||len>n-r.at)return fail("BG original texture codec payload rejected");t.png.assign(p+r.at,p+r.at+len);r.at+=len;if(!png(t))return fail("BG PNG native source dimensions/chunk integrity rejected");d.textures_.push_back(std::move(t));}
 for(const auto&s:d.scenes_){auto resources=s.graph.member("resources");for(const auto&v:resources->array){auto c=v.member("class");if(c&&c->text=="StreamTexture"){auto path=v.member("path");if(!path||!d.texture(path->text.substr(6)))return fail("BG source graph texture payload reference missing");}}}
 if(!r.ok||r.at!=n)return fail("BG trailing/truncated binary rejected");
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
}
