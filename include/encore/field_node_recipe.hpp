#pragma once
#include "encore/field_node_tree.hpp"
namespace encore::upstream {
// Original native structural recipe. No class/script Ready is approved here.
struct FieldNodeRecipeRecord:FieldNodeDescriptor {};
struct FieldRecipeCanvasLayer {
 uint32_t id=0,world_2d_binding=0;
 int32_t layer=0;
 bool visible=true,follow_viewport=false,custom_viewport=false;
 float follow_scale=1,rotation=0;
 FieldTransform transform{};
 Vec2 offset{},scale{};
};
struct FieldRecipeControl {
 uint32_t id=0,mouse=0,focus=0;
 bool clip=false;
 std::array<int32_t,2>grow{};
 std::array<uint32_t,2>size_flags{};
 std::array<float,4>anchors{},margins{};
 Vec2 position{},size{},scale{},pivot{},min_size{};
 float rotation=0,stretch=0;
};
class FieldNodeRecipeData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);
 bool load_file(const char*,const FieldIdentity&,std::string&);
 bool valid()const{return valid_;}
 bool scene_admitted()const{return false;}
 FieldIdentity identity()const{return identity_;}
 const std::string&source_scene()const{return scene_;}
 const std::array<uint8_t,32>&ir_sha256()const{return ir_;}
 const std::vector<FieldNodeRecipeRecord>&records()const{return records_;}
 const FieldNodeRecipeRecord*record(uint32_t)const;
 const std::vector<std::string>&classes()const{return classes_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
 const FieldRecipeCanvasLayer*canvas_layer(uint32_t)const;
 const FieldRecipeControl*control(uint32_t)const;
private:
 bool valid_=false;
 FieldIdentity identity_{};
 std::array<uint8_t,32>ir_{};
 std::string scene_;
 std::vector<std::string>classes_;
 std::vector<FieldNodeRecipeRecord>records_;
 std::map<uint32_t,size_t>index_;
 std::map<std::string,std::array<uint8_t,32>>sources_;
 std::map<uint32_t,FieldRecipeCanvasLayer>layers_;
 std::map<uint32_t,FieldRecipeControl>controls_;
};
}
