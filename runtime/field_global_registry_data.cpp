#include "encore/field_global_registry.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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
 }

 bool FieldGlobalRegistryData::source_hash(std::string_view p,std::array<uint8_t,32>&h)const{auto i=sources_.find(std::string(p));if(i==sources_.end())return false;h=i->second;return true;}
 bool FieldGlobalRegistryData::engine_hash(std::string_view p,std::array<uint8_t,32>&h)const{auto i=engine_sources_.find(std::string(p));if(i==engine_sources_.end())return false;h=i->second;return true;}
 bool FieldGlobalRegistryData::load_file(const char*p,const FieldIdentity&id,std::string&e){
  if(!p||!*p){e="Global registry path rejected";return false;}FILE*f=std::fopen(p,"rb");if(!f){e="Cannot open global registry";return false;}
  if(std::fseek(f,0,SEEK_END)){std::fclose(f);e="Global registry seek rejected";return false;}long n=std::ftell(f);if(n<128||n>8*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);e="Global registry size rejected";return false;}
  std::vector<uint8_t>b(static_cast<size_t>(n));auto got=std::fread(b.data(),1,b.size(),f);bool closed=std::fclose(f)==0;if(got!=b.size()||!closed){e="Global registry read rejected";return false;}return load(b.data(),b.size(),id,e);
 }
 bool FieldGlobalRegistryData::load(const uint8_t*p,size_t n,const FieldIdentity&id,std::string&e){
  auto reject=[&](const char*s){e=s;return false;};
  if(!p||n<128||n>8*1024*1024)return reject("Global registry size rejected");
  if(std::memcmp(p,"ENCFGRG1",8)||word(p+8)!=1||word(p+12)!=128||word(p+16)!=n||word(p+24)!=0x454e0042||word(p+28)!=3||!word(p+32)||word(p+32)>4096||word(p+36)!=id.scene_id||!id.scene_id||std::memcmp(p+40,id.upstream_commit.data(),20)||std::memcmp(p+60,id.source_sha256.data(),32)||word(p+124))return reject("Global registry identity/version/capability rejected");
  if(word(p+20)!=crc(p+128,n-128)||std::all_of(p+92,p+124,[](uint8_t v){return !v;}))return reject("Global registry CRC/source IR rejected");
  FieldGlobalRegistryData d;d.identity_=id;Reader r{p,n};auto scene=r.text();d.root_name_=r.text();d.root_native_=r.text();d.kernel_native_=r.text();uint64_t low=r.integer(),high=r.integer();d.object_counter_=low|(high<<32);low=r.integer();high=r.integer();d.fast_counter_=low|(high<<32);d.ui_=r.integer();d.global_=r.integer();d.main_scene_=r.text();auto canvas_scene=r.text();FieldIdentity canvas_identity;canvas_identity.scene_id=r.integer();canvas_identity.upstream_commit=id.upstream_commit;canvas_identity.source_sha256=r.hash();
  if(!r.ok||!path(scene)||d.root_name_.empty()||d.root_name_.find_first_of("/:@\\")!=d.root_name_.npos||d.root_native_!="Viewport"||d.kernel_native_!="SceneTree"||d.object_counter_!=1||d.fast_counter_!=1||!path(d.main_scene_)||!path(canvas_scene)||!canvas_identity.scene_id||empty_hash(canvas_identity.source_sha256)||!d.ui_||!d.global_||d.ui_==d.global_)return reject("Global registry native root/counter/source fields rejected");
  auto parents=r.integer();if(parents!=2)return reject("Global registry player parent schema rejected");
  for(uint32_t i=0;i<parents;++i){auto s=r.text();if(s.empty()||s.find_first_of("/:@\\")!=s.npos)return reject("Global registry parent name rejected");d.player_parents_.push_back(s);}if(d.player_parents_[0]==d.player_parents_[1])return reject("Global registry duplicate parent rejected");
  auto count=r.integer();if(count!=word(p+32))return reject("Global registry autoload count rejected");std::set<uint32_t>ids;std::set<std::string>names;
  for(uint32_t i=0;i<count;++i){FieldGlobalAutoload a;a.id=r.integer();a.kind=r.integer();a.ordinal=r.integer();a.name=r.text();a.path=r.text();a.native_class=r.text();a.script=r.text();a.source_sha=r.hash();a.script_sha=r.hash();if(!r.ok||!a.id||!ids.insert(a.id).second||!names.insert(a.name).second||a.name.empty()||a.name.find_first_of("/:@\\")!=a.name.npos||!path(a.path)||a.native_class.empty()||a.kind<1||a.kind>2||a.ordinal!=i||empty_hash(a.source_sha)||a.script.empty()!=empty_hash(a.script_sha)||(!a.script.empty()&&!path(a.script)))return reject("Global registry autoload source/class/order rejected");d.autoloads_.push_back(std::move(a));}
  if(!ids.count(d.ui_)||!ids.count(d.global_))return reject("Global registry actual singleton identity missing");
  auto functions=r.integer();if(!r.ok||!functions||functions>4096)return reject("Global registry source operation proof count rejected");std::set<std::pair<std::string,std::string>>methods;
  for(uint32_t i=0;i<functions;++i){FieldGlobalFunctionProof f;f.source=r.text();f.method=r.text();f.line=r.integer();f.sha=r.hash();if(!r.ok||!path(f.source)||f.method.empty()||f.method.find_first_of("/:@\\")!=f.method.npos||!f.line||empty_hash(f.sha)||!methods.emplace(f.source,f.method).second)return reject("Global registry source operation proof rejected");d.functions_.push_back(std::move(f));}
  auto proofs=r.integer();if(!r.ok||!proofs||proofs>16384)return reject("Global registry source proof count rejected");for(uint32_t i=0;i<proofs;++i){auto f=r.text();auto h=r.hash();if(!r.ok||!path(f)||empty_hash(h)||!d.sources_.emplace(f,h).second)return reject("Global registry source proof rejected");}
  d.engine_commit_=r.text();if(d.engine_commit_.size()!=40||d.engine_commit_.find_first_not_of("0123456789abcdef")!=d.engine_commit_.npos)return reject("Global registry native engine pin rejected");auto engines=r.integer();if(engines!=4)return reject("Global registry native source proof count rejected");for(uint32_t i=0;i<engines;++i){auto f=r.text();auto h=r.hash();if(!r.ok||!path(f)||empty_hash(h)||!d.engine_sources_.emplace(f,h).second)return reject("Global registry native source proof rejected");}
  for(const auto*f:{"core/object.cpp","core/message_queue.cpp","scene/main/node.cpp","scene/main/scene_tree.cpp"})if(!d.engine_sources_.count(f))return reject("Global registry native mechanism proof missing");
  auto nested=r.integer();if(!r.ok||nested<128||r.at>n||nested!=n-r.at||!d.canvas_.load(p+r.at,nested,canvas_identity,e))return false;r.at+=nested;
  std::array<uint8_t,32>h{};if(!r.ok||r.at!=n||!d.source_hash(scene,h)||h!=id.source_sha256||d.canvas_.source_scene()!=canvas_scene||d.canvas_.records().size()!=1||d.canvas_.records()[0].native_class!="CanvasLayer"||!d.source_hash(canvas_scene,h)||h!=canvas_identity.source_sha256)return reject("Global registry nested canvas/source binding rejected");
  for(const auto&a:d.autoloads_)if(!d.source_hash(a.path,h)||h!=a.source_sha||(!a.script.empty()&&(!d.source_hash(a.script,h)||h!=a.script_sha)))return reject("Global registry autoload source owner rejected");
  for(const auto&f:d.functions_)if(!d.sources_.count(f.source))return reject("Global registry function file source missing");
  d.valid_=true;*this=std::move(d);e.clear();return true;
 }
}
