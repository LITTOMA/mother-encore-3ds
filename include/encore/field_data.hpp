#pragma once
#include "encore/movement.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
struct FieldIdentity {
    uint32_t scene_id=0;
    std::array<uint8_t,20> upstream_commit{};
    std::array<uint8_t,32> source_sha256{};
};
struct FieldGrassProfile {
    uint32_t stable_id=0,texture_first=0,texture_count=0,collision_layer=0,collision_mask=0;
    std::array<uint16_t,4> frames{}; // Idle, Right, Middle, Left source bindings.
    Vec2 sprite_offset{},collision_offset{},collision_extents{};
    float idle_delay=0,squash=0,blend_divisor=0;
    double enter_tween=0,exit_tween=0;
    std::array<Vec2,3> blend_points{}; // Original declaration order; equal-distance ties select the first.
    std::array<uint16_t,3> blend_frames{};
    uint32_t flags=0;
};
struct FieldGrass {
    uint32_t stable_id=0,node_string=0,name_string=0,ready_ordinal=0,seed=0,profile_index=0,grass_types=0,flags=0;
    Vec2 position{},visibility_origin{},visibility_size{};
};
struct FieldTexture {
    uint32_t stable_id=0,source_string=0,path_string=0;
    uint16_t width=0,height=0,columns=0,rows=0;
    uint32_t flags=0;
    std::array<uint8_t,32> source_sha256{},output_sha256{};
};
struct FieldPending {
    uint32_t stable_id=0,node_string=0,script_string=0,ready_ordinal=0,adapter_kind=0,flags=0;
    std::array<uint8_t,32> source_sha256{};
};
// Independent immutable source-backed lifecycle resource. Failed reloads keep
// the old byte owner and its views. Source identity must come from a checked
// catalog, rather than accepting any nonzero hash from an arbitrary file.
class FieldData {
public:
    bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);
    bool load_file(const char*,const FieldIdentity&,std::string&);
    bool valid() const { return !bytes_.empty(); }
    // Capability 1 admits the grass adapter only. Even a zero-pending file
    // cannot promote itself to complete scene compatibility.
    bool scene_admitted() const { return false; }
    FieldIdentity identity() const;
    uint32_t profile_count() const;
    uint32_t grass_count() const;
    uint32_t texture_count() const;
    uint32_t pending_count() const;
    std::string_view string(uint32_t) const;
    FieldGrassProfile profile(uint32_t) const;
    FieldGrass grass(uint32_t) const;
    FieldTexture texture(uint32_t) const;
    FieldPending pending(uint32_t) const;
private:
    uint32_t count(uint32_t) const;
    const uint8_t* record(uint32_t,uint32_t) const;
    std::vector<uint8_t> bytes_;
};
}
