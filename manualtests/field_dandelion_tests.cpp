// Manual source cases only; not scheduled or executed by this implementation.
#include "encore/field_dandelion.hpp"
#include <cassert>
using namespace encore::upstream;
void field_dandelion_manual_cases(const std::vector<uint8_t>&bytes,const FieldIdentity&id){
 FieldDandelionData data;std::string e;assert(data.load(bytes.data(),bytes.size(),id,e));auto old=data.spawner_count();
 auto damaged=bytes;damaged[8]^=1;assert(!data.load(damaged.data(),damaged.size(),id,e));assert(data.spawner_count()==old);
 damaged=bytes;damaged.back()^=1;assert(!data.load(damaged.data(),damaged.size(),id,e));assert(data.spawner_count()==old);
 assert(old==135&&!data.scene_admitted()&&!data.particle_emission_admitted());
 auto s=data.spawner(0);uint32_t queued=0,created=0,added=0,frames=0,emissions=0;std::vector<unsigned>order;
 FieldDandelionHost host;host.queue_source_sprite=[&](uint32_t source,std::string&){assert(source==s.sprite);++queued;return true;};
 host.instance=[&](const FieldDandelionData&actual,const FieldDandelionProfile&,Vec2 local,uint64_t&id,std::string&){assert(&actual==&data&&local.x==s.position.x&&local.y==s.position.y);id=37;++created;order.push_back(1);return true;};
 host.add_child=[&](uint64_t instance,uint32_t parent,Vec2&world,std::string&){assert(instance==37&&parent==s.parent);world={s.parent_transform.origin.x+s.position.x,s.parent_transform.origin.y+s.position.y};++added;order.push_back(2);return true;};
 host.abort_unparented=[](uint64_t,std::string&){return true;};host.sprite_frame=[&](uint64_t,uint32_t,std::string&){++frames;return true;};
 host.particle_backend_admitted=[](uint64_t,bool&value,std::string&){value=true;return true;};
 host.emit_particles=[&](uint64_t,const FieldDandelionData&,Vec2,std::string&){++emissions;return true;};host.player_direction=[](Vec2&direction,std::string&){direction={1,0};return true;};
 FieldDandelionRuntime runtime;assert(runtime.initialize(data,std::move(host),e));assert(runtime.ready(s.id,e));assert(queued==1);assert(!runtime.ready(s.id,e));
 assert(!runtime.screen_exited(s.id,e));assert(runtime.screen_entered(s.id,e));assert(created==1&&added==1&&order==std::vector<unsigned>({1,2}));
 assert(runtime.screen_entered(s.id,e));assert(runtime.screen_exited(s.id,e));assert(created==1&&runtime.instances().size()==1);
 assert(runtime.body_entered(37,false,e));assert(!runtime.body_entered(37,true,e));assert(frames==0&&emissions==0);
}
