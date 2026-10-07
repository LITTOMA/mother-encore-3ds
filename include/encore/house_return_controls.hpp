#pragma once
#include "encore/field_canvas_art.hpp"
namespace encore::upstream {
// A closed native Control property schema, never script evaluation.
enum class HouseControlValue:uint32_t { Nil,Bool,Integer,Real,Text,Path,Vector,Color,Resource,EditorMetadata };
struct HouseControlProperty {
 HouseControlValue type=HouseControlValue::Nil;std::string canonical,text;
 std::array<float,4>numbers{};int32_t integer=0;bool boolean=false;
};
struct HouseControlRecord {
 uint32_t id=0,parent=0,owner=0,flags=0;
 std::string node,native_class,owner_script,font_source;
 std::array<uint8_t,32>owner_sha{},properties_sha{},font_sha{};
 uint32_t draw_kind=0,texture=0,pose_owner=0;
 std::string texture_source;std::array<uint8_t,32>texture_sha{};
 std::map<std::string,HouseControlProperty>properties;
 const HouseControlProperty*property(std::string_view)const;
};
class HouseReturnControlsData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,const FieldNodeTreeData&,
           const FieldCanvasArtData&,std::string&);
 bool load_file(const char*,const FieldIdentity&,const FieldNodeTreeData&,
                const FieldCanvasArtData&,std::string&);
 bool matches(const FieldNodeTreeData&,const FieldCanvasArtData&,std::string&)const;
 bool valid()const{return valid_;}FieldIdentity identity()const{return identity_;}
 const std::string&source_scene()const{return scene_;}
 const auto&records()const{return records_;}const auto&ir_sha256()const{return ir_;}
 const auto&engine_sources()const{return engine_;}
 const HouseControlRecord*record(uint32_t)const;
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;FieldIdentity identity_{};std::string scene_;
 std::array<uint8_t,32>ir_{},canvas_ir_{},tree_ir_{};
 std::vector<HouseControlRecord>records_;
 std::map<std::string,std::array<uint8_t,32>>sources_;
 std::map<std::string,std::array<uint8_t,32>>engine_;
};
}
