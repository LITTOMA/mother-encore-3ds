#pragma once
#include "encore/movement.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace encore::upstream {
struct FieldSpriteTexture {uint32_t id=0,width=0,height=0;std::string source;};
struct FieldSpriteKey {float time=0;uint32_t frame=0;};
struct FieldSpriteDirection {Vec2 vector{};float duration=0;std::vector<FieldSpriteKey>keys;};
struct FieldSpriteMotion {std::string name;bool loop=false;std::vector<FieldSpriteDirection>directions;};
struct FieldSpriteAnimation {uint32_t id=0,columns=0,rows=0;Vec2 offset{};std::string source;std::vector<FieldSpriteMotion>motions;};
struct FieldSpriteConnection {std::string from,to;uint32_t mode=0;};
enum class FieldSpriteKind:uint32_t {Character=1,Fetcher=2};
struct FieldSpriteDescriptor {
 uint32_t id=0,parent_id=0,ready_ordinal=0,flags=0,target_id=0,columns=0,rows=0,frame=0,texture=0,sprite=0,initial_animation=0,setup_texture=0,setup_animation=0;
 FieldSpriteKind kind{};Vec2 offset{},extra_offset{};float reflect_offset=0;std::string node,target_path;std::vector<FieldSpriteConnection>connections;
};
class FieldSpriteData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}uint32_t scene_id()const{return scene_;}bool reflector_exists()const{return reflector_;}
 const std::array<uint8_t,20>&source_pin()const{return pin_;}const std::string&reflector_path()const{return reflector_path_;}const std::string&fallback()const{return fallback_;}
 const std::vector<FieldSpriteDescriptor>&records()const{return records_;}
 const FieldSpriteDescriptor*record(uint32_t)const;const FieldSpriteTexture*texture(uint32_t)const;const FieldSpriteAnimation*animation(uint32_t)const;
private:
 bool valid_=false,reflector_=false;uint32_t scene_=0;std::array<uint8_t,20>pin_{};std::string reflector_path_,fallback_;std::vector<FieldSpriteTexture>textures_;std::vector<FieldSpriteAnimation>animations_;std::vector<FieldSpriteDescriptor>records_;
};
// Exact source accumulated tag sequence, including repeated names from rebuilds.
struct FieldSpriteTag {uint32_t animation_id=0,motion_index=0;};
struct FieldSpriteInstance {
 uint32_t id=0,descriptor_id=0,texture_id=0,sprite_id=0,animation_id=0,columns=0,rows=0,frame=0,target=0;
 uint64_t reflection=0;bool ready=false,tree_created=false,tree_active=false,visible=false,has_reflection=false,parent_setup=false;
 Vec2 offset{},default_offset{},direction{};float time_scale=1;std::string current_state;std::vector<FieldSpriteTag>directional_tags;std::vector<std::string>all_tags,states;
};
class FieldNpcData;
// Admission before the source Host borrows the existing NPC animation consumer.
bool field_sprite_npc_binding(const FieldSpriteData&,const FieldNpcData&,std::string&);
struct FieldSpriteSample {uint32_t texture_id=0,columns=0,rows=0,frame=0;bool visible=false;};
struct FieldSpriteHost {
 // Actual child Ready must create the tree before any source parent setup.
 std::function<bool(uint32_t,const FieldSpriteDescriptor&,std::string&)>create_tree;
 // Existing checked NPC motion consumer owns clip evaluation. The Host must
 // admit every supplied discrete motion/transition; mode 1 needs real sync.
 std::function<bool(uint32_t,const FieldSpriteAnimation&,const std::vector<FieldSpriteTag>&,const std::vector<FieldSpriteConnection>&,const std::vector<std::string>&,std::string&)>rebuild_tree;
 std::function<bool(uint32_t,const FieldSpriteInstance&,std::string&)>publish;
 // Implements the actual synchronous source sprite_changed -> emotes binding;
 // an unavailable emotes child adapter must reject instead of ignoring it.
 std::function<bool(uint32_t,const FieldSpriteDescriptor&,const FieldSpriteInstance&,std::string&)>sprite_changed;
 std::function<bool(uint32_t,const std::string&,std::string&)>travel;
 std::function<bool(uint32_t,Vec2,const std::vector<FieldSpriteTag>&,std::string&)>blend;
 std::function<bool(uint32_t,float,const std::vector<std::string>&,std::string&)>time_scale;
 std::function<bool(uint32_t,const FieldSpriteDescriptor&,bool&,uint32_t&,std::string&)>resolve_sprite;
 // Each current scene must supply loaded source data, not an unchecked bool.
 // The complete Podunk pack certifies absence; future reflective scenes need
 // their own checked scene pack plus all reflection lifecycle callbacks below.
 std::function<bool(const FieldSpriteData*&,std::string&)>current_scene;
 std::function<bool(uint32_t,FieldSpriteSample&,std::string&)>sample;
 std::function<bool(uint32_t,const FieldSpriteDescriptor&,const FieldSpriteData&,uint32_t,uint64_t&,std::string&)>reflection_create;
 std::function<bool(uint64_t,uint32_t,std::string&)>reflection_add_child;
 std::function<bool(uint64_t,bool&,std::string&)>reflection_valid;
 std::function<bool(uint64_t,std::string&)>reflection_queue_free;
};
class FieldSpriteRuntime {
public:
 const FieldSpriteData*data()const{return data_;}
 bool initialize(const FieldSpriteData&,FieldSpriteHost,std::string&);bool create(uint32_t);bool ready(uint32_t);
 // Invoke from source parent NPC _update_sprite_and_animations / Create.
 bool parent_setup(uint32_t);bool set_sprite(uint32_t,uint32_t);bool set_animation(uint32_t,uint32_t,const std::vector<FieldSpriteConnection>&);
 bool set_spritesheet(uint32_t);bool set_sprite_offset(uint32_t,Vec2);bool travel(uint32_t,const std::string&);bool blend_position(uint32_t,Vec2);bool set_time_scale(uint32_t,float);
 // Existing source animation owner supplies frames; no duplicate idle clock.
 bool frame(uint32_t,uint32_t);bool tree_active(uint32_t,bool);bool visibility(uint32_t,bool);
 bool process_fetcher(uint32_t);bool sample(uint32_t,FieldSpriteSample&);bool destroy(uint32_t);
 const FieldSpriteInstance*instance(uint32_t)const;const std::string&error()const{return error_;}
private:
 const FieldSpriteData*data_=nullptr;FieldSpriteHost host_;std::map<uint32_t,FieldSpriteInstance>instances_;std::string error_;uint32_t last_ready_=0;bool had_ready_=false;
 bool fail(const char*);FieldSpriteInstance*character(uint32_t);bool publish(FieldSpriteInstance&);bool scene(const FieldSpriteData*&);
};
}
