#include "encore/field_ui_manager.hpp"
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


bool FieldUiManagerData::source_hash(std::string_view p,std::array<uint8_t,32>&h)const{auto i=sources_.find(std::string(p));if(i==sources_.end())return false;h=i->second;return true;}
const FieldNodeRecipeData*FieldUiManagerData::recipe(std::string_view p)const{for(const auto&r:recipes_)if(r.source_scene()==p)return &r;return nullptr;}
const FieldNodeRecipeData*FieldUiManagerData::recipe(uint32_t id)const{for(const auto&r:recipes_)if(r.identity().scene_id==id)return &r;return nullptr;}
bool FieldUiManagerData::load_file(const char*p,const FieldIdentity&id,std::string&e){
 if(!p||!*p){e="UI resource path rejected";return false;}FILE*f=std::fopen(p,"rb");if(!f){e="Cannot open UI resource";return false;}
 if(std::fseek(f,0,SEEK_END)){std::fclose(f);e="UI seek rejected";return false;}long n=std::ftell(f);if(n<128||n>32*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);e="UI size rejected";return false;}
 std::vector<uint8_t>b(static_cast<size_t>(n));auto got=std::fread(b.data(),1,b.size(),f);bool closed=std::fclose(f)==0;if(got!=b.size()||!closed){e="UI read rejected";return false;}return load(b.data(),b.size(),id,e);
}
bool FieldUiManagerData::load(const uint8_t*p,size_t n,const FieldIdentity&id,std::string&e){
 auto reject=[&](const char*s){e=s;return false;};
 if(!p||n<128||n>32*1024*1024||std::memcmp(p,"ENCFUIM1",8)||word(p+8)!=2||word(p+12)!=128||word(p+16)!=n||word(p+24)!=0x454e0045||word(p+28)!=1||word(p+32)!=9||!id.scene_id||word(p+36)!=id.scene_id||std::memcmp(p+40,id.upstream_commit.data(),20)||std::memcmp(p+60,id.source_sha256.data(),32)||word(p+124)||word(p+20)!=crc(p+128,n-128))return reject("UI identity/schema/capability/CRC rejected");
 FieldUiManagerData d;d.identity_=id;Reader r{p,n};d.script_=r.text();if(!path(d.script_))return reject("UI source script rejected");
 uint32_t count=r.integer();if(count!=20)return reject("UI preload roster rejected");std::set<uint32_t>ids;std::set<std::string>names;
 for(uint32_t i=0;i<count;++i){FieldUiPreload a;a.id=r.integer();uint32_t on=r.integer();a.onready=on!=0;a.name=r.text();a.path=r.text();a.native_class=r.text();a.sha=r.hash();if(!r.ok||!a.id||on>1||!ids.insert(a.id).second||!names.insert(a.name).second||a.name.empty()||!path(a.path)||empty_hash(a.sha)||(a.native_class!="PackedScene"&&a.native_class!="ShaderMaterial"))return reject("UI preload declaration rejected");d.preloads_.push_back(std::move(a));}
 count=r.integer();if(count!=7)return reject("UI onready source order rejected");names.clear();
 for(uint32_t i=0;i<count;++i){FieldUiInstance a;a.name=r.text();a.native_class=r.text();a.resource=r.integer();a.recipe=r.integer();if(!r.ok||a.name.empty()||!names.insert(a.name).second||!ids.count(a.resource)||!a.recipe||(a.native_class!="CanvasLayer"&&a.native_class!="Control"))return reject("UI actual instance declaration rejected");d.instances_.push_back(std::move(a));}
 count=r.integer();if(count!=7)return reject("UI flavor shape rejected");names.clear();
 for(uint32_t i=0;i<count;++i){auto name=r.text();std::array<uint32_t,8>colors{};for(auto&c:colors){c=r.integer();if(c>0xffffff)return reject("UI palette color rejected");}if(!r.ok||name.empty()||!names.insert(name).second)return reject("UI flavor identity rejected");d.flavors_.push_back(std::move(name));d.palette_.push_back(colors);}
 for(auto*array:{&d.old_,&d.fresh_})for(auto&c:*array){for(auto&v:c)v=r.scalar();if(!r.ok||std::any_of(c.begin(),c.end(),[](float v){return v<0||v>1;}))return reject("UI ShaderMaterial parameter rejected");}
 d.threshold_=r.scalar();if(!r.ok||d.threshold_<=0||d.threshold_>2)return reject("UI source shader color-distance threshold rejected");
 count=r.integer();if(count!=5)return reject("UI source methods rejected");for(uint32_t i=0;i<count;++i){auto name=r.text();auto h=r.hash();if(!r.ok||name.empty()||empty_hash(h)||!d.functions_.emplace(name,h).second)return reject("UI source method proof rejected");}
 count=r.integer();if(!count||count>100000)return reject("UI source proof bounds rejected");for(uint32_t i=0;i<count;++i){auto name=r.text();auto h=r.hash();if(!r.ok||!path(name)||empty_hash(h)||!d.sources_.emplace(name,h).second)return reject("UI source proof rejected");}
 auto si=d.sources_.find(d.script_);if(si==d.sources_.end()||si->second!=id.source_sha256)return reject("UI source identity rejected");
 for(const auto&a:d.preloads_){auto it=d.sources_.find(a.path);if(it==d.sources_.end()||it->second!=a.sha)return reject("UI preload source hash mismatch");}
 count=r.integer();if(count!=9)return reject("UI complete recipe count rejected");std::set<uint32_t>recipeids;
 for(uint32_t i=0;i<count;++i){auto len=r.integer();if(!r.ok||len<128||r.at>n||len>n-r.at)return reject("UI nested recipe size rejected");FieldIdentity ri;ri.scene_id=word(p+r.at+36);std::copy_n(p+r.at+40,20,ri.upstream_commit.begin());std::copy_n(p+r.at+60,32,ri.source_sha256.begin());if(ri.upstream_commit!=id.upstream_commit||!recipeids.insert(ri.scene_id).second)return reject("UI recipe pin/duplicate rejected");FieldNodeRecipeData recipe;if(!recipe.load(p+r.at,len,ri,e))return false;r.at+=len;auto source=d.sources_.find(recipe.source_scene());if(source==d.sources_.end()||source->second!=ri.source_sha256)return reject("UI recipe source identity rejected");d.recipes_.push_back(std::move(recipe));}
 for(const auto&a:d.instances_){auto recipe=d.recipe(a.recipe);auto pre=std::find_if(d.preloads_.begin(),d.preloads_.end(),[&](const auto&p){return p.id==a.resource;});if(!recipe||pre==d.preloads_.end()||recipe->source_scene()!=pre->path||recipe->records().front().native_class!=a.native_class)return reject("UI instance/recipe source class rejected");}
 if(!r.ok||r.at!=n)return reject("UI trailing/truncated resource rejected");
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
}
