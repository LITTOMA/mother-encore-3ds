#include "encore/field_node_recipe.hpp"
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
  bool name(std::string_view s){
   return !s.empty()&&s!="."&&s!=".."&&s.find_first_of("/:\\") ==s.npos;
  }
  bool native_canvas(uint32_t i){
   return i!=0&&i!=6&&i!=8&&i!=14&&i!=23&&i!=25;
  }
  // Class names form the native structural schema, not game-content bindings.
  const char*native_classes[]={
   "Node","Node2D","Sprite","VisibilityNotifier2D","Position2D","CollisionShape2D","AnimationPlayer","Area2D","Timer","KinematicBody2D","VisibilityEnabler2D","TextureRect","HBoxContainer","Label","AudioStreamPlayer","RayCast2D","AnimatedSprite","StaticBody2D","CollisionPolygon2D","TileMap","YSort","Camera2D","AudioStreamPlayer2D","Tween","ReferenceRect","CanvasLayer","Control","NinePatchRect","GridContainer","RichTextLabel","VScrollBar"
  }
  ;
 }
 const FieldNodeRecipeRecord*FieldNodeRecipeData::record(uint32_t id)const{
  auto it=index_.find(id);
  return it==index_.end()?nullptr:&records_[it->second];
 }
 const FieldRecipeCanvasLayer*FieldNodeRecipeData::canvas_layer(uint32_t id)const{
  auto i=layers_.find(id);return i==layers_.end()?nullptr:&i->second;
 }
 const FieldRecipeControl*FieldNodeRecipeData::control(uint32_t id)const{
  auto i=controls_.find(id);return i==controls_.end()?nullptr:&i->second;
 }
 bool FieldNodeRecipeData::source_hash(std::string_view p,std::array<uint8_t,32>&h)const{
  auto it=sources_.find(std::string(p));
  if(it==sources_.end())return false;
  h=it->second;
  return true;
 }
 bool FieldNodeRecipeData::load_file(const char*p,const FieldIdentity&id,std::string&e){
  if(!p||!*p){
   e="NodeRecipe path rejected";
   return false;
  }
  FILE*f=std::fopen(p,"rb");
  if(!f){
   e="Cannot open NodeRecipe resource";
   return false;
  }
  if(std::fseek(f,0,SEEK_END)){
   std::fclose(f);
   e="NodeRecipe seek rejected";
   return false;
  }
  long n=std::ftell(f);
  if(n<128||n>32*1024*1024||std::fseek(f,0,SEEK_SET)){
   std::fclose(f);
   e="NodeRecipe size rejected";
   return false;
  }
  std::vector<uint8_t>b(static_cast<size_t>(n));
  auto got=std::fread(b.data(),1,b.size(),f);
  bool closed=std::fclose(f)==0;
  if(got!=b.size()||!closed){
   e="NodeRecipe read rejected";
   return false;
  }
  return load(b.data(),b.size(),id,e);
 }
 bool FieldNodeRecipeData::load(const uint8_t*p,size_t n,const FieldIdentity&id,std::string&e){
  auto reject=[&](const char*s){
   e=s;
   return false;
  }
  ;
  if(!p||n<128||n>32*1024*1024)return reject("NodeRecipe size rejected");
  if(std::memcmp(p,"ENCFNRC1",8)||word(p+8)!=1||word(p+12)!=128||word(p+16)!=n||word(p+24)!=0x454e003d||word(p+28)!=3||!word(p+32)||word(p+32)>100000||!id.scene_id||word(p+36)!=id.scene_id||std::memcmp(p+40,id.upstream_commit.data(),20)||std::memcmp(p+60,id.source_sha256.data(),32)||word(p+124))return reject("NodeRecipe identity/version/capability rejected");
  if(crc(p+128,n-128)!=word(p+20)||std::all_of(p+92,p+124,[](uint8_t v){
   return !v;
  }
  ))return reject("NodeRecipe CRC/IR rejected");
  FieldNodeRecipeData d;
  d.identity_=id;
  std::copy_n(p+92,32,d.ir_.begin());
  Reader r{
   p,n
  }
  ;
  d.scene_=r.text();
  auto count=r.integer();
  if(!r.ok||!path(d.scene_)||count!=std::size(native_classes))return reject("NodeRecipe native class schema rejected");
  for(uint32_t i=0;i<count;++i){
   auto s=r.text();
   if(s!=native_classes[i])return reject("NodeRecipe class opcode rejected");
   d.classes_.push_back(std::move(s));
  }
  std::set<std::string>paths;
  std::vector<std::vector<uint32_t>>children(word(p+32));
  for(uint32_t i=0;i<word(p+32);++i){
   FieldNodeRecipeRecord a;
   a.id=r.integer();
   a.parent=r.integer();
   a.owner=r.integer();
   a.canvas_parent=r.integer();
   a.class_index=r.integer();
   if(a.class_index<count)a.native_class=d.classes_[a.class_index];
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
   if(!r.ok||groups>10000)return reject("NodeRecipe group count rejected");
   std::set<std::string>gs;
   for(uint32_t j=0;j<groups;++j){
    auto g=r.text();
    if(g.empty()||!gs.insert(g).second)return reject("NodeRecipe group identity rejected");
    a.groups.push_back(std::move(g));
   }
   auto generated=r.integer();
   a.native_generated=generated==1;
   if(generated>1)return reject("NodeRecipe generated constructor opcode rejected");
   if(!r.ok||!a.id||!d.index_.emplace(a.id,i).second||!paths.insert(a.path).second||!name(a.name)||a.class_index>=count||a.ready>=word(p+32)||a.pause>2||a.flags>1023||a.script_methods>255||bool(a.flags&1)!=native_canvas(a.class_index)||a.z<-4096||a.z>4096||a.script.empty()!=empty_hash(a.script_sha)||(a.script.empty()&&a.script_methods))return reject("NodeRecipe node fields rejected");
   if(!i){
    if(a.id!=id.scene_id||a.parent||a.owner||a.canvas_parent||a.path!="."||a.index!=-1)return reject("NodeRecipe root rejected");
   }
   else {
    auto parent=d.index_.find(a.parent);
    if(parent==d.index_.end()||parent->second>=i||!path(a.path))return reject("NodeRecipe parent/order rejected");
    auto&pr=d.records_[parent->second];
    auto expected=pr.path=="."?a.name:pr.path+"/"+a.name;
    if(a.path!=expected||a.index!=int32_t(children[parent->second].size()))return reject("NodeRecipe sibling index/path rejected");
    children[parent->second].push_back(i);
    if(a.owner){
     uint32_t at=a.parent;
     while(at&&at!=a.owner){
      at=d.records_[d.index_.at(at)].parent;
     }
     if(!at)return reject("NodeRecipe owner ancestry rejected");
    }
    if(a.native_generated&&(a.class_index!=30||pr.class_index!=29||a.owner||a.name.size()<3||a.name.substr(0,2)!="@@"||a.name.substr(2).find_first_not_of("0123456789")!=a.name.npos))return reject("NodeRecipe native internal constructor owner rejected");
    if(!a.native_generated&&a.name.find('@')!=a.name.npos)return reject("NodeRecipe unowned internal name rejected");
    auto cp=((a.flags&1)&&!(a.flags&4)&&(pr.flags&1))?a.parent:0;
    if(a.canvas_parent!=cp)return reject("NodeRecipe Canvas ancestry rejected");
   }
   if(!(a.flags&1)&&(a.flags||a.z||a.light_mask))return reject("NodeRecipe nonCanvas state rejected");
   d.records_.push_back(std::move(a));
  }
  uint32_t ordinal=0;
  std::function<bool(uint32_t)>visit=[&](uint32_t i){
   for(auto c:children[i])if(!visit(c))return false;
   return d.records_[i].ready==ordinal++;
  }
  ;
  if(!visit(0)||ordinal!=word(p+32))return reject("NodeRecipe native postorder Ready rejected");
  auto proofs=r.integer();
  if(!r.ok||!proofs||proofs>10000)return reject("NodeRecipe source proof count rejected");
  for(uint32_t i=0;i<proofs;++i){
   auto s=r.text();
   auto h=r.hash();
   if(!r.ok||!path(s)||empty_hash(h)||!d.sources_.emplace(s,h).second)return reject("NodeRecipe source proof rejected");
  }
  std::array<uint8_t,32>h{
  }
  ;
  auto layer_count=r.integer();
  if(!r.ok||layer_count>word(p+32))return reject("NodeRecipe CanvasLayer count rejected");
  for(uint32_t i=0;i<layer_count;++i){
   FieldRecipeCanvasLayer c;c.id=r.integer();c.layer=r.signed_integer();auto follow=r.integer(),custom=r.integer();c.world_2d_binding=r.integer();auto visible=r.integer();c.follow_viewport=follow!=0;c.custom_viewport=custom!=0;c.visible=visible!=0;c.follow_scale=r.scalar();for(auto&v:c.transform)v=r.vector();c.offset=r.vector();c.rotation=r.scalar();c.scale=r.vector();
   auto node=d.record(c.id);
   if(!r.ok||!node||node->class_index!=25||follow>1||custom||c.world_2d_binding||visible>1||!d.layers_.emplace(c.id,c).second)return reject("NodeRecipe CanvasLayer source/external binding rejected");
  }
  auto control_count=r.integer();
  if(!r.ok||control_count>word(p+32))return reject("NodeRecipe Control count rejected");
  for(uint32_t i=0;i<control_count;++i){
   FieldRecipeControl c;c.id=r.integer();auto clip=r.integer();c.clip=clip!=0;c.mouse=r.integer();c.focus=r.integer();for(auto&v:c.grow)v=r.signed_integer();for(auto&v:c.size_flags)v=r.integer();for(auto&v:c.anchors)v=r.scalar();for(auto&v:c.margins)v=r.scalar();c.position=r.vector();c.size=r.vector();c.scale=r.vector();c.rotation=r.scalar();c.pivot=r.vector();c.min_size=r.vector();c.stretch=r.scalar();auto node=d.record(c.id);
   if(!r.ok||!node||!(node->class_index==11||node->class_index==12||node->class_index==13||node->class_index==24||node->class_index>=26)||clip>1||c.mouse>2||c.focus>2||c.grow[0]<0||c.grow[0]>2||c.grow[1]<0||c.grow[1]>2||c.size_flags[0]>15||c.size_flags[1]>15||c.size.x<0||c.size.y<0||c.min_size.x<0||c.min_size.y<0||c.stretch<=0||!d.controls_.emplace(c.id,c).second)return reject("NodeRecipe Control source/layout rejected");
  }
  for(const auto&node:d.records_){
   bool control=node.class_index==11||node.class_index==12||node.class_index==13||node.class_index==24||node.class_index>=26;
   if((node.class_index==25)!=bool(d.layers_.count(node.id))||control!=bool(d.controls_.count(node.id)))return reject("NodeRecipe native owner properties missing");
  }
  if(!r.ok||r.at!=n||!d.source_hash(d.scene_,h)||h!=id.source_sha256)return reject("NodeRecipe trailing/source rejected");
  for(const auto&a:d.records_)if(!a.script.empty()){
   auto at=a.script.find("::");
   auto file=a.script.substr(0,at);
   if(!path(file)||!d.source_hash(file,h)||(at==a.script.npos&&h!=a.script_sha))return reject("NodeRecipe leaf source binding rejected");
   if(at!=a.script.npos){
    auto sub=a.script.substr(at+2);
    if(sub.empty()||sub.find_first_not_of("0123456789")!=sub.npos)return reject("NodeRecipe embedded source binding rejected");
   }
  }
  d.valid_=true;
  *this=std::move(d);
  e.clear();
  return true;
 }
}
