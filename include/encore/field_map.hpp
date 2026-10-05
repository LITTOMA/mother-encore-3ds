#pragma once
#include "encore/field_data.hpp"
#include <functional>

namespace encore::upstream {
struct FieldMapRect { Vec2 minimum{},maximum{}; };
struct FieldMapLayer {
    uint32_t stable_id=0,node=0,canvas=0,cell_first=0,cell_count=0,draw_first=0,draw_count=0,polygon_first=0,polygon_count=0,flags=0,layer=0,mask=0;
    Vec2 position{},cell_size{};
};
struct FieldMapCell {
    int32_t x=0,y=0,tile=0,atlas_x=0,atlas_y=0,quadrant_ordinal=0;
    uint32_t flags=0,draw_first=0,draw_count=0,polygon_first=0,polygon_count=0;
};
struct FieldMapDraw {
    uint32_t map=0,cell=0,texture=0,flags=0,order=0,gate=0;
    int32_t z=0;
    // Godot negative rect sizes encode UV flip, anchored at position.
    // Transpose swaps destination width/height, after the signed UV flags.
    Vec2 position{},signed_size{},uv_position{},uv_size{};
    std::array<float,4> color{};
};
struct FieldMapPolygon {
    // kind 1: strict convex; 2: native segment pairs; 3: native collinear
    // ConvexPolygonShape2D, including repeated vertices. No implicit hull,
    // rectangle, thinning, or discarded collision substitutes are allowed.
    uint32_t map=0,cell=0,kind=0,point_first=0,point_count=0,layer=0,order=0,gate=0;
    uint32_t geometry=0,transform=0;
    FieldMapRect bounds{};
};
struct FieldMapLocalGeometry { uint32_t kind=0,point_first=0,point_count=0,bvh_first=0,bvh_count=0,leaf_first=0,leaf_count=0; };
struct FieldMapShapeTransform { Vec2 x{},y{},origin{}; };
struct FieldMapConcaveNode { uint32_t left=0,right=0,segment=0; FieldMapRect bounds{}; };
struct FieldMapTexture {
    uint32_t stable_id=0,source=0,path=0,width=0,height=0,group=0,frame=0,frames=0;
    float fps=0,delay=0;
    std::array<uint8_t,32> source_sha256{},output_sha256{};
};
struct FieldMapCanvas {
    uint32_t stable_id=0,node=0,parent=0,order=0,flags=0,gate=0;
    int32_t z=0;
    Vec2 position{};
};
struct FieldMapGate { uint32_t stable_id=0,node=0,appear=0,disappear=0; bool delete_if_hidden=false; };
enum class FieldMapGateState : uint8_t { Pending,Visible,Hidden,Deleted };
// The event host owns Ready deletion and subsequent flag notifications. A
// missing gate evaluator fails closed rather than treating scripted gates as
// unconditionally visible. Hidden affects drawing; Deleted also removes body.
using FieldMapGateQuery=std::function<FieldMapGateState(uint32_t stable_id)>;
struct FieldMapAnimationState { uint32_t group=0,frame=0; float time=0; bool first_draw=true; };
class FieldMapView {
public:
    bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);
    bool load_file(const char*,const FieldIdentity&,std::string&);
    bool valid() const { return !bytes_.empty(); }
    bool scene_admitted() const { return false; } // Static map capability only.
    FieldIdentity identity() const;
    std::string_view source_scene() const;
    uint32_t map_count() const;
    uint32_t cell_count() const;
    uint32_t draw_count() const;
    uint32_t polygon_count() const;
    uint32_t texture_count() const;
    uint32_t canvas_count() const;
    uint32_t gate_count() const;
    std::string_view string(uint32_t) const;
    FieldMapLayer map(uint32_t) const;
    FieldMapCell cell(uint32_t) const;
    FieldMapDraw draw(uint32_t) const;
    FieldMapPolygon polygon(uint32_t) const;
    Vec2 point(uint32_t) const;
    FieldMapTexture texture(uint32_t) const;
    FieldMapCanvas canvas(uint32_t) const;
    FieldMapGate gate(uint32_t) const;
    FieldMapLocalGeometry local_geometry(uint32_t) const;
    Vec2 local_point(uint32_t) const;
    FieldMapShapeTransform shape_transform(uint32_t) const;
    FieldMapConcaveNode concave_node(uint32_t) const;
    uint32_t concave_leaf(uint32_t) const;
    bool concave_segments(uint32_t polygon,FieldMapRect local_query,size_t capacity,std::vector<uint32_t>& pair_indices,std::string&) const;
    // Return source ordered indices, culled using native quadrant bounds.
    // Capacity exhaustion reports an error; no partial frame/collision list.
    bool collect_draws(FieldMapRect,const FieldMapGateQuery&,size_t,std::vector<uint32_t>&,std::string&) const;
    bool collision_polygons(FieldMapRect,uint32_t layer_mask,const FieldMapGateQuery&,size_t,std::vector<uint32_t>&,std::string&) const;
    Vec2 sort_anchor(uint32_t draw_index) const;
    bool start_animation(uint32_t texture_index,FieldMapAnimationState&,std::string&) const;
    // Native AnimatedTexture::_update_proxy: f32 accumulator, strict >,
    // at most frame_count advances per draw; the first draw adds no delta.
    bool advance_animation(FieldMapAnimationState&,float draw_delta,std::string&) const;
private:
    bool admit(std::vector<uint8_t>&&,const FieldIdentity&,std::string&);
    uint32_t count(uint32_t) const;
    const uint8_t* record(uint32_t,uint32_t) const;
    bool map_active(uint32_t,const FieldMapGateQuery&,bool drawing,bool&,std::string&) const;
    bool spatial_chunks(FieldMapRect,std::vector<uint32_t>&,std::string&) const;
    std::vector<uint8_t> bytes_;
};
}
