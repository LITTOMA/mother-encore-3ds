#pragma once
#include "encore/field_geometry.hpp"
#include <functional>

namespace encore::upstream {
struct FieldDandelionSpawner {uint32_t id=0,ready=0,node=0,parent=0,sprite=0,notifier=0;FieldGeometryTransform parent_transform{},notifier_transform{};Vec2 position{},visibility_origin{},visibility_size{};};
struct FieldDandelionProfile {uint32_t id=0,layer=0,mask=0,flags=0,sprite_texture=0,seed_texture=0,columns=0,rows=0,idle=0,blown=0,source=0;Vec2 collision_offset{},extents{},sprite_offset{},sprite_position{},particle_position{};float gravity_multiplier=0;};
struct FieldDandelionTexture {uint32_t id=0,source=0,path=0,width=0,height=0;std::array<uint8_t,32>source_sha{},output_sha{};};
struct FieldDandelionParameter {uint32_t name=0,kind=0;float value=0;};
class FieldDandelionData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);bool load_file(const char*,const FieldIdentity&,std::string&);
 bool valid()const{return !bytes_.empty();}bool scene_admitted()const{return false;}bool particle_emission_admitted()const{return false;}
 FieldIdentity identity()const;std::string_view source_scene()const;std::string_view string(uint32_t)const;
 uint32_t spawner_count()const;FieldDandelionSpawner spawner(uint32_t)const;FieldDandelionProfile profile()const;FieldDandelionTexture texture(uint32_t)const;
 uint32_t parameter_count()const;FieldDandelionParameter parameter(uint32_t)const;
 uint32_t ramp_count()const;std::array<float,5>ramp(uint32_t)const;std::array<uint32_t,5>material()const;
 std::string_view spawner_script()const;std::string_view plant_script()const;std::array<uint8_t,32>spawner_sha()const;std::array<uint8_t,32>plant_sha()const;
private:
 bool admit(std::vector<uint8_t>&&,const FieldIdentity&,std::string&);uint32_t count(uint32_t)const;const uint8_t*record(uint32_t,uint32_t)const;std::vector<uint8_t>bytes_;
};
struct FieldDandelionInstance {uint64_t id=0;uint32_t spawner=0,frame=0;bool added=false;Vec2 local{},world{};};
struct FieldDandelionHost {
 std::function<bool(uint32_t,std::string&)>queue_source_sprite;
 // Actual native instance starts parentless: source sets global_position to
 // spawner.position, then add_child applies the parent's live world transform.
 std::function<bool(const FieldDandelionData&,const FieldDandelionProfile&,Vec2,uint64_t&,std::string&)>instance;
 std::function<bool(uint64_t,uint32_t,Vec2&,std::string&)>add_child;
 std::function<bool(uint64_t,std::string&)>abort_unparented;
 std::function<bool(uint64_t,uint32_t,std::string&)>sprite_frame;
 // Exact CPUParticles backend must be admitted separately. Absent backend
 // rejects body interaction before any sprite/particle side effect occurs.
 std::function<bool(uint64_t,bool&,std::string&)>particle_backend_admitted;
 std::function<bool(uint64_t,const FieldDandelionData&,Vec2,std::string&)>emit_particles;
 std::function<bool(Vec2&,std::string&)>player_direction;
};
class FieldDandelionRuntime {
public:
 const FieldDandelionData*data()const{return data_;}
 bool source_body_unready(uint32_t id)const{if(!data_||spawners_.size()!=data_->spawner_count())return false;for(size_t i=0;i<spawners_.size();++i)if(data_->spawner(uint32_t(i)).id==id)return !spawners_[i].ready&&!spawners_[i].poisoned;return false;}
 bool initialize(const FieldDandelionData&,FieldDandelionHost,std::string&);
 bool ready(uint32_t,std::string&);bool screen_entered(uint32_t,std::string&);bool screen_exited(uint32_t,std::string&);
 bool body_entered(uint64_t,bool actual_party_player,std::string&);
 const std::vector<FieldDandelionInstance>&instances()const{return instances_;}
private:
 struct State {bool ready=false,poisoned=false;uint64_t current=0;};
 bool index(uint32_t,size_t&,std::string&)const;
 const FieldDandelionData*data_=nullptr;FieldDandelionHost host_;std::vector<State>spawners_;std::vector<FieldDandelionInstance>instances_;uint32_t last_ready_=0;bool had_ready_=false;
};
}
