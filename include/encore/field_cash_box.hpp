#pragma once
#include "encore/movement.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace encore::upstream {
struct FieldCashTexture {std::string source,path;uint32_t width=0,height=0,bytes=0;bool flavor=false;std::array<uint8_t,32>sha{};};
struct FieldCashStyle {uint32_t texture=0;std::array<float,4>content{},patch{},expand{},region{};};
struct FieldCashNode {std::string path,text;uint32_t parent=UINT32_MAX,kind=0,label_role=0,style=UINT32_MAX,texture=UINT32_MAX,hflags=0,vflags=0,align=0,valign=0,alignment=0,stretch_mode=0;bool amount=false,expand=false;std::array<float,4>rect{};Vec2 minimum{};float separation=0;};
struct FieldCashKey {float time=0,ease=0,value=0;};
struct FieldCashClip {float length=0;std::vector<FieldCashKey>keys;};
struct FieldCashBoxPolicy {std::string source;uint32_t mode=0,layer=0;bool timer_connected=false;float timer_seconds=0;std::vector<FieldCashStyle>styles;std::vector<FieldCashNode>nodes;std::array<FieldCashClip,3>clips;};
class FieldCashBoxData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}
 const std::array<uint8_t,20>&source_pin()const{return pin_;}const std::string&script()const{return script_;}const std::string&font()const{return font_;}const std::string&phone_card_name()const{return units_item_;}uint32_t units_scope_flags()const{return units_scope_flags_;}Vec2 viewport()const{return viewport_;}bool pixel_snap()const{return pixel_snap_;}
 const auto&source_colors()const{return colors_;}double threshold()const{return threshold_;}const auto&textures()const{return textures_;}const FieldCashBoxPolicy&box(bool units)const{return boxes_[units?1:0];}
 const std::string&symbol(bool right,bool chinese)const{return symbols_[right?1:0][chinese?1:0];}
 bool source_hash(const std::string&,std::array<uint8_t,32>&)const;
private:
 bool valid_=false,pixel_snap_=false;std::array<uint8_t,20>pin_{};std::string script_,font_,units_item_;uint32_t units_scope_flags_=0;Vec2 viewport_{};double threshold_=0;std::array<uint32_t,8>colors_{};std::array<std::array<std::string,2>,2>symbols_{};std::vector<FieldCashTexture>textures_;std::array<FieldCashBoxPolicy,2>boxes_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
struct FieldCashBoxHost {
 std::function<bool(const FieldCashBoxData&,std::string&)>bind;
 // Read actual source globals: cash or SUM PhoneCard.doses in party+key
 // inventory source order. Storage is excluded by Inventory's source query.
 std::function<bool(bool phone_units,int64_t&,std::string&)>read_amount;
 // Both measure and renderer bind the same admitted EBMain source face.
 std::function<bool(const std::string&font,const std::string&text,Vec2&,std::string&)>measure;
};
struct FieldCashBoxState {bool open=false,timer=false,collapsed=false,animating=false;uint32_t clip=2;float time=0,timer_left=0,y=0;int64_t amount=0;std::string text;};
struct FieldCashDraw {uint32_t kind=0,index=0;std::array<float,4>rect{};std::string text;};
class FieldCashBoxRuntime {
public:
 bool initialize(const FieldCashBoxData&,FieldCashBoxHost,bool chinese,std::string&);
 // Stable persistent identity 1/2 belongs to the two binary policy slots;
 // identity is retained by each existing phone coroutine independently.
 bool open(bool phone_units,uint64_t&,std::string&);bool update(uint64_t,std::string&);bool close(uint64_t,std::string&);bool open_and_close(bool,std::string&);
 bool idle_frame(float,bool source_ui_processes,std::string&);bool set_language(bool,std::string&);
 bool draw_commands(bool phone_units,uint32_t viewport_width,uint32_t viewport_height,std::vector<FieldCashDraw>&,std::string&)const;
 const FieldCashBoxState&state(bool units)const{return states_[units?1:0];}const FieldCashBoxData*data()const{return data_;}
private:
 const FieldCashBoxData*data_=nullptr;FieldCashBoxHost host_;std::array<FieldCashBoxState,2>states_{};bool chinese_=false,poisoned_=false;std::string error_;
 bool fail(std::string&,const char*);bool effect(bool,std::string&);float sample(const FieldCashClip&,float)const;
};
}
