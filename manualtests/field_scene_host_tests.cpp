// Manual cases only. This source is not scheduled in CI or executed by its
// implementation task. Call with the real source-backed resource and identity.
#include "encore/field_scene_host.hpp"
#include <cassert>
#include <map>
#include <cstring>
using namespace encore::upstream;
void field_scene_host_manual_cases(const std::vector<uint8_t>&bytes,const FieldIdentity&id){
 FieldSceneData data;std::string e;assert(data.load(bytes.data(),bytes.size(),id,e));
 auto damaged=bytes;damaged[8]^=1;assert(!data.load(damaged.data(),damaged.size(),id,e));assert(data.ready_count()==2157);
 damaged=bytes;damaged.back()^=1;assert(!data.load(damaged.data(),damaged.size(),id,e));assert(data.ready_count()==2157);
 assert(!data.scene_admitted());assert(data.landmark_count()==13);assert(data.flaggable_count()==19);
 // New typed roles require their declared capability. This header is outside
 // the section CRC, so lowering it exercises capability admission directly.
 for(uint32_t capability=1;capability<5;++capability){
  damaged=bytes;for(unsigned j=0;j<4;++j)damaged[32+j]=uint8_t(capability>>(8*j));
  assert(!data.load(damaged.data(),damaged.size(),id,e));assert(data.ready_count()==2157);
 }
 std::map<std::string,bool>normal,objects;for(uint32_t i=0;i<data.visit_count();++i)normal[std::string(data.visit(i).second)]=false;
 normal[std::string(data.string(data.area().flying_flag))]=false;
 std::vector<FieldSceneSignalSlot>slots;std::vector<FieldSceneAreaSlot>area_slots;uint32_t writes=0,emissions=0,teleports=0,queues=0;
 FieldSceneHostOps ops;
 ops.read_flag=[&](bool object,std::string_view key,bool&present,bool&value,std::string&){auto&dict=object?objects:normal;auto it=dict.find(std::string(key));present=it!=dict.end();value=present&&it->second;return true;};
 ops.write_flag=[&](bool object,std::string_view key,bool value,std::string&){(object?objects:normal)[std::string(key)]=value;++writes;return true;};
 ops.connect_flags=[&](uint32_t,FieldSceneSignalSlot slot,std::string&){slots.push_back(std::move(slot));return true;};
 ops.emit_flags=[&](std::string&error){++emissions;for(auto&slot:slots)if(!slot(error))return false;return true;};
 ops.connect_area_left=[&](uint32_t,FieldSceneAreaSlot slot,std::string&){area_slots.push_back(std::move(slot));return true;};
 ops.emit_area_left=[&](bool changed,std::string&error){for(auto&slot:area_slots)if(!slot(changed,error))return false;return true;};
 ops.connect_switches=[](uint32_t,std::function<void(bool)>,std::string&){return true;};
 ops.visibility=[](uint32_t,bool,std::string&){return true;};ops.queue_free=[&](uint32_t,std::string&){++queues;return true;};
 ops.map_possessed=[](std::string_view,bool&v,std::string&){v=false;return true;};
 ops.flyingman_present=[](bool&v,std::string&){v=false;return true;};ops.set_flyingman_present=[](bool,std::string&){return true;};
 ops.debug_context=[](bool&debug,bool&area,bool&paused,std::string&){debug=false;area=false;paused=false;return true;};ops.teleport_player=[&](Vec2,std::string&){++teleports;return true;};
 ops.current_scene=[&](const FieldSceneData*&current,std::string&){current=&data;return true;};
 FieldSceneHost runtime;assert(runtime.configure(data,{},std::move(ops),e));assert(runtime.ready_next(e));assert(teleports==0);
 FieldSceneScriptAdmission admitted;assert(runtime.script_admission(data.ready(0).id,admitted));assert(admitted.source_sha==data.ready(0).sha);
 assert(runtime.ready_next(e));auto landmark=data.landmark(data.ready(1).profile);assert(queues==1);assert(runtime.gate(landmark.id)==FieldMapGateState::Visible);
 assert(runtime.commit_deleted(landmark.id,e));assert(runtime.gate(landmark.id)==FieldMapGateState::Deleted);
 assert(runtime.set_flag(false,"unknown-manual-key",true,true,e));assert(writes==0&&emissions==0);
 assert(!runtime.ready_to_boundary(e));auto cursor=runtime.ready_cursor();assert(!runtime.scene_ready());assert(!runtime.ready_next(e));assert(runtime.ready_cursor()==cursor);
}
