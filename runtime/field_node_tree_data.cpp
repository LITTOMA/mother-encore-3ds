#include "encore/field_node_tree.hpp"
#include "encore/utf8.hpp"
#include "field_node_native_classes.hpp"
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
  bool name(std::string_view s){
   return !s.empty()&&s!="."&&s!=".."&&s.find_first_of("/:@\\") ==s.npos;
  }
 }
 const FieldNodeDescriptor*FieldNodeTreeData::record(uint32_t id)const{
  auto it=index_.find(id);
  return it==index_.end()?nullptr:&records_[it->second];
 }
 bool FieldNodeTreeData::source_hash(std::string_view p,std::array<uint8_t,32>&h)const{
  auto it=sources_.find(std::string(p));
  if(it==sources_.end())return false;
  h=it->second;
  return true;
 }
 bool FieldNodeTreeData::load_file(const char*p,const FieldIdentity&id,std::string&e){
  if(!p||!*p){
   e="NodeTree path rejected";
   return false;
  }
  FILE*f=std::fopen(p,"rb");
  if(!f){
   e="Cannot open NodeTree resource";
   return false;
  }
  if(std::fseek(f,0,SEEK_END)){
   std::fclose(f);
   e="NodeTree seek rejected";
   return false;
  }
  long n=std::ftell(f);
  if(n<128||n>32*1024*1024||std::fseek(f,0,SEEK_SET)){
   std::fclose(f);
   e="NodeTree size rejected";
   return false;
  }
  std::vector<uint8_t>b(static_cast<size_t>(n));
  auto got=std::fread(b.data(),1,b.size(),f);
  bool closed=std::fclose(f)==0;
  if(got!=b.size()||!closed){
   e="NodeTree read rejected";
   return false;
  }
  return load(b.data(),b.size(),id,e);
 }
 bool FieldNodeTreeData::load(const uint8_t*p,size_t n,const FieldIdentity&id,std::string&e){
  auto reject=[&](const char*s){
   e=s;
   return false;
  }
  ;
  if(!p||n<128||n>32*1024*1024)return reject("NodeTree size rejected");
  if(std::memcmp(p,"ENCFNTR1",8)||word(p+8)!=1||word(p+12)!=128||word(p+16)!=n||word(p+24)!=0x454e003c||word(p+28)!=3||!word(p+32)||word(p+32)>100000||!id.scene_id||word(p+36)!=id.scene_id||std::memcmp(p+40,id.upstream_commit.data(),20)||std::memcmp(p+60,id.source_sha256.data(),32)||word(p+124))return reject("NodeTree identity/version/capability rejected");
  if(crc(p+128,n-128)!=word(p+20)||std::all_of(p+92,p+124,[](uint8_t v){
   return !v;
  }
  ))return reject("NodeTree CRC/IR rejected");
  FieldNodeTreeData d;
  d.identity_=id;
  Reader r{
   p,n
  }
  ;
  d.scene_=r.text();
  auto count=r.integer();
  if(!r.ok||!path(d.scene_)||!detail::field_native_class_schema(count))return reject("NodeTree native class schema rejected");
  for(uint32_t i=0;i<count;++i){
   auto s=r.text();
   if(s!=detail::field_native_classes[i])return reject("NodeTree class opcode rejected");
   d.classes_.push_back(std::move(s));
  }
  std::set<std::string>paths;
  std::vector<std::vector<uint32_t>>children(word(p+32));
  for(uint32_t i=0;i<word(p+32);++i){
   FieldNodeDescriptor a;
   a.id=r.integer();
   a.parent=r.integer();
   a.owner=r.integer();
   a.canvas_parent=r.integer();
   a.class_index=r.integer();
   a.ready=r.integer();
   a.pause=r.integer();
   a.flags=r.integer();
   a.light_mask=r.integer();
   a.script_methods=r.integer();
   a.index=r.signed_integer();
   a.priority=r.signed_integer();
   a.z=r.signed_integer();
   for(auto&v:a.local)v=r.vector();
   for(auto&v:a.world)v=r.vector();
   for(auto&v:a.modulate)v=r.scalar();
   for(auto&v:a.self_modulate)v=r.scalar();
   a.path=r.text();
   a.name=r.text();
   a.script=r.text();
   a.script_sha=r.hash();
   auto groups=r.integer();
   if(!r.ok||groups>10000)return reject("NodeTree group count rejected");
   std::set<std::string>gs;
   for(uint32_t j=0;j<groups;++j){
    auto g=r.text();
    if(g.empty()||!gs.insert(g).second)return reject("NodeTree group identity rejected");
    a.groups.push_back(std::move(g));
   }
   if(!r.ok||!a.id||!d.index_.emplace(a.id,i).second||!paths.insert(a.path).second||!name(a.name)||a.class_index>=count||a.ready>=word(p+32)||a.pause>2||a.flags>1023||a.script_methods>255||bool(a.flags&1)!=detail::field_native_canvas(a.class_index)||a.z<-4096||a.z>4096||a.script.empty()!=empty_hash(a.script_sha)||(a.script.empty()&&a.script_methods))return reject("NodeTree node fields rejected");
   if(!i){
    if(a.id!=id.scene_id||a.parent||a.owner||a.canvas_parent||a.path!="."||a.index!=-1)return reject("NodeTree root rejected");
   }
   else {
    auto parent=d.index_.find(a.parent);
    if(parent==d.index_.end()||parent->second>=i||!path(a.path))return reject("NodeTree parent/order rejected");
    auto&pr=d.records_[parent->second];
    auto expected=pr.path=="."?a.name:pr.path+"/"+a.name;
    if(a.path!=expected||a.index!=int32_t(children[parent->second].size()))return reject("NodeTree sibling index/path rejected");
    children[parent->second].push_back(i);
    if(a.owner){
     uint32_t at=a.parent;
     while(at&&at!=a.owner){
      at=d.records_[d.index_.at(at)].parent;
     }
     if(!at)return reject("NodeTree owner ancestry rejected");
    }
    auto cp=((a.flags&1)&&!(a.flags&4)&&(pr.flags&1))?a.parent:0;
    if(a.canvas_parent!=cp)return reject("NodeTree Canvas ancestry rejected");
   }
   if(!(a.flags&1)&&(a.flags||a.z||a.light_mask))return reject("NodeTree nonCanvas state rejected");
   d.records_.push_back(std::move(a));
  }
  uint32_t ordinal=0;
  std::function<bool(uint32_t)>visit=[&](uint32_t i){
   for(auto c:children[i])if(!visit(c))return false;
   return d.records_[i].ready==ordinal++;
  }
  ;
  if(!visit(0)||ordinal!=word(p+32))return reject("NodeTree native postorder Ready rejected");
  auto proofs=r.integer();
  if(!r.ok||!proofs||proofs>10000)return reject("NodeTree source proof count rejected");
  for(uint32_t i=0;i<proofs;++i){
   auto s=r.text();
   auto h=r.hash();
   if(!r.ok||!path(s)||empty_hash(h)||!d.sources_.emplace(s,h).second)return reject("NodeTree source proof rejected");
  }
  std::array<uint8_t,32>h{
  }
  ;
  if(!r.ok||r.at!=n||!d.source_hash(d.scene_,h)||h!=id.source_sha256)return reject("NodeTree trailing/source rejected");
  for(const auto&a:d.records_)if(!a.script.empty()){
   auto at=a.script.find("::");
   auto file=a.script.substr(0,at);
   if(!path(file)||!d.source_hash(file,h)||(at==a.script.npos&&h!=a.script_sha))return reject("NodeTree leaf source binding rejected");
   if(at!=a.script.npos){
    auto sub=a.script.substr(at+2);
    if(sub.empty()||sub.find_first_not_of("0123456789")!=sub.npos)return reject("NodeTree embedded source binding rejected");
   }
  }
  // The binary stores the validated class opcode once. Both immutable source
  // consumers and live factories must observe that same native class; leaving
  // this derived field empty makes a correct source receiver appear untyped.
  for(auto&node:d.records_)node.native_class=d.classes_[node.class_index];
  d.valid_=true;
  *this=std::move(d);
  e.clear();
  return true;
 }
}
