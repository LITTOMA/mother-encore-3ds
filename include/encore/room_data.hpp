#pragma once
#include "encore/room_schema.hpp"
#include "encore/movement.hpp"
#include "encore/animation.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
struct RoomResource { uint32_t stable_id=0,path_string=0; uint16_t width=0,height=0,columns=0,rows=0,kind=0,flags=0; std::array<uint8_t,32> sha256{}; };
struct RoomPolygon { uint32_t body_id=0,owner_id=0,first_vertex=0; uint16_t vertex_count=0,flags=0; float minx=0,miny=0,maxx=0,maxy=0; };
struct RoomBodyRule { uint32_t body_id=0,source_path_string=0; bool initially_enabled=false; uint8_t flags=0; };
// Foreground overlays preserve a later source CanvasItem sibling pass.
enum class RoomOverlayFlag : uint16_t { Foreground=1 };
struct RoomOverlay { uint32_t stable_id=0,resource_index=0; float x=0,y=0,sort_y=0; uint16_t u=0,v=0,w=0,h=0,flags=0; };
struct RoomMapDraw { uint32_t stable_id=0,resource_index=0; float x=0,y=0; uint16_t w=0,h=0; uint32_t flags=0; };
struct RoomClip { uint32_t stable_id=0; float length=0; uint32_t first_key=0; uint16_t key_count=0; uint8_t flags=0,visibility=0; uint16_t frame_count=0,channel=0; bool loop() const { return (flags&1)!=0; } };
struct RoomKey { float time=0; uint16_t frame=0; };
struct RoomActorProfile {
    uint32_t stable_id=0; uint16_t execution_kind=0,flags=0;
    uint32_t primary_resource=0,shadow_resource=0,emote_resource=0,animation_binding_first=0;
    uint16_t animation_binding_count=0,initial_frame=0,emote_initial_frame=0;
    Vec2 sprite_position{},sprite_offset{},emote_offset{},shadow_offset{};
    uint32_t direction_first=0; uint16_t direction_count=0; uint32_t idle_clip=0,emote_clip=0;
};
struct RoomActorInstance { uint32_t stable_id=0,profile_index=0; uint16_t binding_kind=0,flags=0; uint32_t display_name_string=0; Vec2 position{},direction{}; uint32_t initial_clip=0; };
struct RoomCameraArea { uint32_t stable_id=0,flags=0; Vec2 center{},extents{}; };
struct RoomFlag { uint32_t stable_id=0,name_string=0; bool default_value=false; uint8_t flags=0; };
struct RoomInitialFlag { uint32_t flag_index=0; bool value=false; };
struct RoomCondition { uint32_t flag_index=0; bool expected_value=false; uint8_t domain=0; };
struct RoomTrigger { uint32_t stable_id=0,first_vertex=0; uint16_t vertex_count=0,flags=0; uint32_t condition_first=0,condition_count=0,program_index=0,actor_instance_index=0; };
struct RoomProgram { uint32_t stable_id=0,first_command=0,command_count=0,phrase_count=0,source_path_string=0; };
struct RoomCommand { uint16_t opcode=0,actor_index=kRoomNoActor; uint32_t phrase=0,target_index=kRoomNoIndex,flags=0; Vec2 vector{}; double value=0,duration=0; uint32_t auxiliary_index=kRoomNoIndex; };
struct RoomBinding { uint32_t stable_id=0; uint16_t kind=0,flags=0; uint32_t target_index=kRoomNoIndex,auxiliary_index=kRoomNoIndex; double value=0,duration=0; };
struct RoomBattle { uint32_t stable_id=0,enemy_string=0,actor_instance_index=0,win_flag_index=0; int32_t advantage=0; uint32_t flags=0,win_cutscene_string=0,battle_resource_index=kRoomNoIndex; };
struct RoomRule { RoomRuleKey key{}; RoomScalarType scalar_type{}; float f32=0; double f64=0; uint32_t u32=0; };
struct RoomScene { uint32_t stable_id=0,display_name_string=0,version_string=0,source_scene_string=0,player_instance_index=0,initial_program_index=0,actor_hull_first=0,actor_hull_count=0; Vec2 spawn{},start_direction{}; uint16_t initial_motion_state=0,initial_frame=0; uint32_t initial_flag_first=0,initial_flag_count=0,body_rule_first=0,body_rule_count=0,default_camera_area=0,rule_profile_id=0,flags=0; };
struct RoomAnimationBinding { uint16_t actor_profile_index=0; uint8_t motion_state=0,direction=0; uint32_t clip_index=0; };

// flags: step vectors=1, moonwalk=2, loop=4, queue=8 (rules7+).
// animation_motion NONE or actor walk=4. Paths contain at most16 entries.
struct RoomMovementPath { uint32_t stable_id=0,first_entry=0; uint16_t entry_count=0,flags=0,animation_motion=kRoomNoActor; double speed=0; };
// kind: absolute/step vector=0, timed wait=1. Ownership belongs to one path.
struct RoomMovementPathEntry { uint16_t kind=0; Vec2 vector{}; double duration=0; };

class RoomData;
// A non-owning immutable view. It may be copied, but must not outlive its
// RoomData or a successful reload of that owner. Failed loads preserve views.
// Records are small decoded values, never pointers to packed C++ objects.
class RoomView {
public:
    RoomView()=default;
    bool valid() const { return bytes_!=nullptr; }
    explicit operator bool() const { return valid(); }
    const uint8_t* bytes() const { return bytes_; }
    size_t byte_size() const { return size_; }
    uint32_t count(RoomSection section) const;
    uint32_t section_offset(RoomSection section) const;
#define ENCORE_ROOM_COUNT(name,section) uint32_t name##_count() const { return count(RoomSection::section); }
    ENCORE_ROOM_COUNT(string,StringRef) ENCORE_ROOM_COUNT(resource,Resource)
    ENCORE_ROOM_COUNT(vertex,Vertex) ENCORE_ROOM_COUNT(polygon,Polygon)
    ENCORE_ROOM_COUNT(body_rule,BodyRule) ENCORE_ROOM_COUNT(overlay,Overlay)
    ENCORE_ROOM_COUNT(map_draw,MapDraw) ENCORE_ROOM_COUNT(clip,Clip)
    ENCORE_ROOM_COUNT(key,Key) ENCORE_ROOM_COUNT(direction_frame,DirectionFrame)
    ENCORE_ROOM_COUNT(actor_profile,ActorProfile) ENCORE_ROOM_COUNT(actor_instance,ActorInstance)
    ENCORE_ROOM_COUNT(camera_area,CameraArea) ENCORE_ROOM_COUNT(flag,Flag)
    ENCORE_ROOM_COUNT(initial_flag,InitialFlag) ENCORE_ROOM_COUNT(condition,Condition)
    ENCORE_ROOM_COUNT(trigger,Trigger) ENCORE_ROOM_COUNT(program,Program)
    ENCORE_ROOM_COUNT(command,Command) ENCORE_ROOM_COUNT(binding,Binding)
    ENCORE_ROOM_COUNT(battle,Battle) ENCORE_ROOM_COUNT(rule,Rule)
    ENCORE_ROOM_COUNT(experience,Experience) ENCORE_ROOM_COUNT(animation_binding,AnimationBinding)
    ENCORE_ROOM_COUNT(movement_path,MovementPath) ENCORE_ROOM_COUNT(movement_path_entry,MovementPathEntry)
#undef ENCORE_ROOM_COUNT
    // Invalid indexes return empty/zero records; valid runtime references were
    // checked transactionally at load. string_data always returns a NUL string.
    std::string_view string(uint32_t index) const;
    const char* string_data(uint32_t index) const;
    RoomResource resource(uint32_t index) const;
    Vec2 vertex(uint32_t index) const;
    Vec2 vec2(uint32_t index) const { return vertex(index); }
    RoomPolygon polygon(uint32_t index) const;
    RoomBodyRule body_rule(uint32_t index) const;
    RoomOverlay overlay(uint32_t index) const;
    RoomMapDraw map_draw(uint32_t index) const;
    RoomClip clip(uint32_t index) const;
    RoomKey key(uint32_t index) const;
    // Legacy zero-start sampler; delayed-start clips use actor-specific sampling.
    bool frame_clip(uint32_t index,FrameClip& result) const;
    uint16_t direction_frame(uint32_t index) const;
    RoomActorProfile actor_profile(uint32_t index) const;
    RoomActorInstance actor_instance(uint32_t index) const;
    RoomCameraArea camera_area(uint32_t index) const;
    RoomFlag flag(uint32_t index) const;
    RoomInitialFlag initial_flag(uint32_t index) const;
    RoomCondition condition(uint32_t index) const;
    RoomTrigger trigger(uint32_t index) const;
    RoomProgram program(uint32_t index) const;
    RoomCommand command(uint32_t index) const;
    RoomBinding binding(uint32_t index) const;
    RoomBattle battle(uint32_t index) const;
    RoomRule rule(uint32_t index) const;
    RoomRule rule(RoomRuleKey key) const;
    float rule_f32(RoomRuleKey key) const;
    double rule_f64(RoomRuleKey key) const;
    uint32_t rule_u32(RoomRuleKey key) const;
    uint32_t experience(uint32_t index) const;
    RoomScene scene() const;
    RoomAnimationBinding animation_binding(uint32_t index) const;
    RoomMovementPath movement_path(uint32_t index) const;
    RoomMovementPathEntry movement_path_entry(uint32_t index) const;
private:
    friend class RoomData;
    RoomView(const uint8_t* bytes,size_t size):bytes_(bytes),size_(size) {}
    const uint8_t* record(RoomSection section,uint32_t index) const;
    const uint8_t* bytes_=nullptr;
    size_t size_=0;
};

// Exactly one owned byte buffer; no expanded per-node/record heap graph.
// Callers explicitly load platform-selected bytes/path. There is no default
// room, embedded fallback, or shared-core lookup of a platform resource path.
class RoomData {
public:
    RoomData()=default;
    RoomData(const RoomData&)=delete;
    RoomData& operator=(const RoomData&)=delete;
    RoomData(RoomData&&)=default;
    RoomData& operator=(RoomData&&)=default;
    bool load(const uint8_t* bytes,size_t size,std::string& error);
    bool load_file(const char* path,std::string& error);
    RoomView view() const { return bytes_.empty()?RoomView{}:RoomView(bytes_.data(),bytes_.size()); }
    bool empty() const { return bytes_.empty(); }
    size_t byte_size() const { return bytes_.size(); }
private:
    static bool validate(const uint8_t* bytes,size_t size,std::string& error);
    std::vector<uint8_t> bytes_;
};
}
