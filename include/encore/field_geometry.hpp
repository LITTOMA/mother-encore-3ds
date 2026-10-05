#pragma once
#include "encore/field_data.hpp"

namespace encore::upstream {
struct FieldGeometryTransform { Vec2 x{},y{},origin{}; };
// Shape parameters are source-native, never polygon approximations of circles.
enum class FieldGeometryKind : uint32_t {
    Empty,Rectangle,Circle,Capsule,Convex,Concave,Segment,Ray,Line,
    SourceSolidPolygon,SourceSegmentPolygon
};
struct FieldGeometryNode {
    uint32_t stable_id=0,path=0,parent=0,order=0,ready=0,flags=0,class_name=0,script=0;
    std::array<uint8_t,32> script_sha256{};
    FieldGeometryTransform local{},world{};
};
struct FieldGeometryOwner {
    // kind: Static 1, Kinematic 2, Rigid 3, Area 4.
    // flags: pickable 1, monitoring 2, monitorable 4, audio override 8,
    // gravity point 16, sync_to_physics 32. These flags grant no script capability.
    uint32_t node=0,kind=0,layer=0,mask=0,flags=0,shape_first=0,shape_count=0,audio_bus=0,space_override=0,platform_leave=0;
    float safe_margin=0,priority=0,gravity=0,gravity_distance_scale=0;
    Vec2 gravity_vector{};
    float linear_damp=0,angular_damp=0;
    Vec2 constant_linear_velocity{};
    float constant_angular_velocity=0;
};
struct FieldGeometryShape {
    // disabled bit 1 and one_way bit 2, regardless of Canvas visibility.
    uint32_t node=0,owner=0,kind=0,flags=0,geometry=0,part_first=0,part_count=0;
    float margin=0,owner_margin=0;
    FieldGeometryTransform world{},owner_transform{},cached_before_enter_tree{};
};
struct FieldGeometryPrimitive {
    FieldGeometryKind kind=FieldGeometryKind::Empty;
    uint32_t point_first=0,point_count=0,resource=0;
    // Rect extents [0,1], Circle radius [0], Capsule radius/height [0,1],
    // Ray length/slips [0,1], Line normal/d [0,1,2], solver bias [5].
    std::array<float,6> parameters{};
};
// Immutable source geometry only. SceneHost must bind every scripted ancestor,
// deferred deletion and live transform before using an owner in gameplay.
class FieldGeometryView {
public:
    bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);
    bool load_file(const char*,const FieldIdentity&,std::string&);
    bool valid() const {return !bytes_.empty();}
    bool scene_admitted() const {return false;}
    FieldIdentity identity() const;
    std::string_view source_scene() const;
    uint32_t node_count() const;
    uint32_t owner_count() const;
    uint32_t shape_count() const;
    uint32_t geometry_count() const;
    std::string_view string(uint32_t) const;
    FieldGeometryNode node(uint32_t) const;
    FieldGeometryOwner owner(uint32_t) const;
    FieldGeometryShape shape(uint32_t) const;
    FieldGeometryPrimitive geometry(uint32_t) const;
    Vec2 point(uint32_t) const;
    // Source ordered owner indices. This does not resolve pending scripts or
    // pretend disabled/deleted owners can be activated by static metadata.
    bool owners_for_layer(uint32_t,size_t,std::vector<uint32_t>&,std::string&) const;
private:
    bool admit(std::vector<uint8_t>&&,const FieldIdentity&,std::string&);
    uint32_t count(uint32_t) const;
    const uint8_t* record(uint32_t,uint32_t) const;
    std::vector<uint8_t> bytes_;
};
}
