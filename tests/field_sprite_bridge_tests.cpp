// Explicit manual suite. No CMake/CI registration in this independent slice.
#include "encore/field_sprite_bridge.hpp"
#include "encore/field_npc.hpp"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
using namespace encore::upstream;
namespace {
uint32_t get(const std::vector<uint8_t>&v,size_t p){return uint32_t(v[p])|uint32_t(v[p+1])<<8|uint32_t(v[p+2])<<16|uint32_t(v[p+3])<<24;}
void put(std::vector<uint8_t>&v,size_t p,uint32_t u){for(unsigned i=0;i<4;++i)v[p+i]=uint8_t(u>>(8*i));}
void checksum(std::vector<uint8_t>&v){uint32_t c=~0u;for(size_t i=0;i<v.size();++i){c^=i>=16&&i<20?0:v[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}put(v,16,~c);}
}
int main(int argc,char**argv){assert(argc==3);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>raw((std::istreambuf_iterator<char>(f)),{});FieldSpriteData data;FieldNpcData npcs;std::string error;assert(data.load(raw.data(),raw.size(),error));assert(npcs.load_file(argv[2],error));assert(field_sprite_npc_binding(data,npcs,error));assert(data.records().size()==150&&!data.reflector_exists());
 for(auto offset:{size_t(8),size_t(20),size_t(24),size_t(60)}){auto broken=raw;put(broken,offset,99);checksum(broken);FieldSpriteData rejected;assert(!rejected.load(broken.data(),broken.size(),error));assert(data.valid());}
 auto corrupt=raw;corrupt.back()^=1;assert(!data.load(corrupt.data(),corrupt.size(),error));assert(data.valid());auto trailing=raw;trailing.push_back(0);put(trailing,12,uint32_t(trailing.size()));checksum(trailing);assert(!data.load(trailing.data(),trailing.size(),error));
 size_t rebuilt=0,changed=0,trees=0,travelled=0;const FieldSpriteData*current=&data;std::map<uint32_t,FieldSpriteInstance>published;FieldSpriteHost host;
 host.create_tree=[&](uint32_t,const auto&,std::string&){++trees;return true;};host.rebuild_tree=[&](uint32_t,const auto&,const auto&,const auto&,const auto&,std::string&){++rebuilt;return true;};host.publish=[&](uint32_t id,const auto&s,std::string&){published[id]=s;return true;};host.sprite_changed=[&](uint32_t,const auto&,const auto&,std::string&){++changed;return true;};host.travel=[&](uint32_t,const std::string&,std::string&){++travelled;return true;};host.blend=[](uint32_t,Vec2,const auto&,std::string&){return true;};host.time_scale=[](uint32_t,float,const auto&,std::string&){return true;};host.resolve_sprite=[](uint32_t,const FieldSpriteDescriptor&d,bool&exists,uint32_t&id,std::string&){exists=true;id=d.target_id;return true;};host.current_scene=[&](const FieldSpriteData*&out,std::string&){out=current;return true;};host.sample=[](uint32_t,FieldSpriteSample&out,std::string&){out={0,1,1,0,true};return true;};
 FieldSpriteRuntime runtime;assert(!runtime.initialize(data,{},error));assert(runtime.initialize(data,host,error));assert(!runtime.create(0));for(const auto&d:data.records()){assert(runtime.create(d.id));assert(!runtime.create(d.id));assert(runtime.ready(d.id));}assert(trees==67&&rebuilt==1&&changed==1);assert(!runtime.ready(data.records().front().id));
 uint32_t first_character=0;for(const auto&d:data.records()){if(d.kind==FieldSpriteKind::Character){if(!first_character)first_character=d.id;assert(runtime.parent_setup(d.id));auto state=runtime.instance(d.id);assert(state&&state->parent_setup);assert(runtime.blend_position(d.id,{1,0}));assert(runtime.set_time_scale(d.id,1));assert(!runtime.frame(d.id,state->columns*state->rows));size_t before=travelled;assert(runtime.travel(d.id,"manual-missing-state"));assert(travelled==before);if(d.initial_animation){const auto*a=data.animation(d.initial_animation);const auto*b=data.animation(d.setup_animation);assert(state->directional_tags.size()==a->motions.size()+b->motions.size());}}else assert(runtime.process_fetcher(d.id));}assert(rebuilt==68&&changed==68);assert(runtime.instance(first_character));
 // Current-scene unknown capability must not inherit Podunk absence.
 current=nullptr;auto fetcher=std::find_if(data.records().begin(),data.records().end(),[](const auto&d){return d.kind==FieldSpriteKind::Fetcher;});assert(fetcher!=data.records().end());assert(!runtime.process_fetcher(fetcher->id));current=&data;
 FieldSpriteRuntime reverse;assert(reverse.initialize(data,host,error));assert(reverse.create(data.records().back().id)&&reverse.ready(data.records().back().id));assert(reverse.create(data.records().front().id));assert(!reverse.ready(data.records().front().id));
 // Schema fixture for explicit reflective Host admission; it does not certify
 // a production reflective scene or replace original gameplay with a fixture.
 auto reflection_fixture=raw;size_t p=64;p+=4+get(raw,p);p+=4+get(raw,p);put(reflection_fixture,p,1);checksum(reflection_fixture);FieldSpriteData reflection;assert(reflection.load(reflection_fixture.data(),reflection_fixture.size(),error));current=&reflection;assert(!runtime.process_fetcher(fetcher->id));
 size_t adds=0,queues=0;bool valid=true;host.reflection_create=[](uint32_t,const auto&,const auto&,uint32_t,uint64_t&handle,std::string&){handle=7;return true;};host.reflection_add_child=[&](uint64_t handle,uint32_t,std::string&){assert(handle==7);++adds;return true;};host.reflection_valid=[&](uint64_t,bool&out,std::string&){out=valid;return true;};host.reflection_queue_free=[&](uint64_t,std::string&){++queues;return true;};FieldSpriteRuntime reflecting;assert(reflecting.initialize(data,host,error));assert(reflecting.create(fetcher->id)&&reflecting.ready(fetcher->id));assert(reflecting.process_fetcher(fetcher->id));assert(adds==1&&reflecting.instance(fetcher->id)->has_reflection);current=&data;valid=false;assert(reflecting.process_fetcher(fetcher->id));assert(reflecting.instance(fetcher->id)->has_reflection&&queues==0);valid=true;assert(reflecting.process_fetcher(fetcher->id));assert(!reflecting.instance(fetcher->id)->has_reflection&&queues==1);
 return 0;
}
