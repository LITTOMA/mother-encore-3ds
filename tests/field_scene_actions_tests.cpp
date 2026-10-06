// Manual source lifecycle/core doubles only; no actual map or hardware claim.
#include "encore/field_scene_actions.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
using namespace encore::upstream;
int main(int argc,char**argv){
 assert(argc==2);FieldSceneActionsData d;std::string e;assert(d.load_file(argv[1],e)&&d.bindings().size()==18);
 std::map<uint64_t,FieldSceneActionNodeState>nodes;std::map<uint64_t,std::pair<uint32_t,uint32_t>>collision;std::map<std::string,bool>flags;uint64_t token=0;bool admit=true,player=true,stepping=true;unsigned actual_sets=0;
 for(const auto&r:d.references()){nodes[r.id]={true,r.kind,r.parent};collision[r.id]={r.mask,r.layer};}
 FieldSceneActionsHost h;h.admit_ready=[](const auto&,const auto&,auto&){return true;};h.resolve=[](auto,auto,auto expected,auto&out,auto&){out=expected;return true;};
 h.describe=[&](auto id,auto&out,auto&){out=nodes[id];return true;};h.body_is_player=[&](auto,auto&out,auto&){out=player;return true;};h.admit_reparent=[&](const auto&,const auto&,auto&err){if(!admit)err="Dynamic map capability unavailable";return admit;};
 h.remove_child=[&](auto parent,auto item,auto&){assert(nodes[item].parent==parent);nodes[item].parent=0;return true;};h.add_child=[&](auto parent,auto item,auto&){assert(nodes[item].parent==0);nodes[item].parent=parent;return true;};
 h.read_collision=[&](auto id,auto role,auto&out,auto&){out=role==1?collision[id].first:collision[id].second;return true;};h.set_collision=[&](auto id,auto role,auto value,auto&){assert(nodes[id].kind!=2);++actual_sets;(role==1?collision[id].first:collision[id].second)=value;return true;};h.enqueue=[&](const auto&,auto&out,auto&){out=++token;return true;};
 h.admit_event=[](const auto&,const auto&,auto&){return true;};h.has_method=[](auto,auto action,auto,auto&out,auto&){out=action==1||action==2;return true;};h.call_method=[&](auto,auto action,auto&){stepping=action==1;return true;};
 h.read_flag=[&](auto name,auto&exists,auto&value,auto&){auto it=flags.find(std::string(name));exists=it!=flags.end();value=exists&&it->second;return true;};h.write_existing_flag=[&](auto name,auto value,auto&){auto it=flags.find(std::string(name));assert(it!=flags.end());it->second=value;return true;};
 FieldSceneActionsRuntime r;assert(r.initialize(d,h,e));assert(!r.ready(d.bindings().back().id,e));for(const auto&b:d.bindings())assert(r.ready(b.id,e));const auto&b=d.bindings().front();assert(b.kind==1);
 admit=false;assert(!r.reparent(b.id,e)&&r.pending().empty());admit=true;player=false;assert(r.body_enter(b.id,2,e)&&r.pending().empty());player=true;assert(r.body_enter(b.id,1,e));assert(r.pending().size()==b.objects.size()*3);
 for(const auto&o:b.objects)assert(nodes[o.source_id].parent==0);assert(!r.flush_deferred(0,e));auto count=r.pending().size();assert(r.reparent(b.id,e)&&r.pending().size()==count);while(!r.pending().empty())assert(r.flush_deferred(r.pending().front().token,e));for(const auto&o:b.objects)assert(nodes[o.source_id].parent==b.parent_id);
 // Existing no-script CollisionPolygon absent property writes retain queue
 // positions and are consumed without changing collision or its Area parent.
 const FieldSceneActionBinding*poly=nullptr,*event=nullptr,*church=nullptr;
 for(const auto&v:d.bindings()){if(v.kind==1&&d.reference(v.objects.front().source_id)->kind==2)poly=&v;if(v.kind==2&&v.action==2)event=&v;if(v.kind==2&&!v.check_flag.empty())church=&v;}
 assert(poly&&event&&church);auto before=actual_sets;assert(r.reparent(poly->id,e)&&r.pending().size()==3);assert(!r.pending()[0].known_absent&&r.pending()[1].known_absent&&r.pending()[2].known_absent);while(!r.pending().empty())assert(r.flush_deferred(r.pending().front().token,e));assert(actual_sets==before);
 assert(r.activate_event(event->id,e)&&!stepping);assert(r.body_enter(church->id,1,e)&&flags.empty());flags[church->check_flag]=true;assert(r.body_enter(church->id,1,e)&&flags.size()==1);flags[church->set_flag]=false;assert(r.body_enter(church->id,1,e)&&flags[church->set_flag]==church->set_state);
 assert(!r.activate_event(b.id,e));assert(!r.reparent(event->id,e));assert(!r.body_enter(b.id,0,e));assert(r.exit_tree(b.id,e));assert(!r.reparent(b.id,e));assert(!FieldSceneActionsRuntime{}.initialize(d,{},e));
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>bytes((std::istreambuf_iterator<char>(f)),{});auto recalc=[](auto&v){uint32_t crc=~0u;for(size_t i=0;i<v.size();++i){crc^=i>=16&&i<20?0:v[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}crc=~crc;for(unsigned j=0;j<4;++j)v[16+j]=uint8_t(crc>>(j*8));};for(unsigned at:{8u,20u,24u,52u}){auto bad=bytes;bad[at]=99;recalc(bad);assert(!d.load(bad.data(),bad.size(),e)&&d.valid());}auto bad=bytes;bad.back()^=1;assert(!d.load(bad.data(),bad.size(),e));assert(!d.load(nullptr,0,e));
}
