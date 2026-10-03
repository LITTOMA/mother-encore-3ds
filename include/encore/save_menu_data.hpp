#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace encore::upstream {
// Schema IDs describe mechanisms; source tuning and presentation live in ENCSMENU.
enum class SaveMenuLayout : uint32_t {Reference,Body,Card,CardPatch,CursorPatch,Name,Level,Title,Time,NoData,NoDataLabel,FileNumber,DividerVertical,DividerHorizontal,Confirm,ConfirmPatch,ConfirmText,Choices,ArrowOffset,IconFirst,IconRest,ChoiceYes,ChoiceNo,TimeFormat,Count};
enum class SaveMenuText : uint32_t {NoData,Level,Time,TooMuchTime,Overwrite,Yes,No,TimeSeparator,TimeSpace,Count};
enum class SaveMenuSound : uint32_t {Move,Accept,Back,Count};
struct SaveMenuRect {float x=0,y=0,w=0,h=0;};
struct SaveMenuResource {std::string path;uint32_t width=0,height=0,columns=0,rows=0;};
struct SaveMenuGlyph {uint32_t codepoint=0,u=0,v=0,width=0,height=0;float advance=0,offset_x=0,offset_y=0;};
struct SaveMenuFlavor {std::string id;uint32_t resource=0,confirm_resource=0,divider_color=0,background_color=0;};
struct SaveMenuIcon {std::string id;uint32_t resource=0;};
struct SaveMenuCursorKey {double time=0;SaveMenuRect margins;};
struct SaveMenuArrowKey {double time=0;uint32_t frame=0;};
class SaveMenuData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}uint32_t slot_count()const{return slots_;}
 double activation_seconds()const{return activation_;}double scroll_seconds()const{return scroll_;}
 double cursor_seconds()const{return cursor_loop_;}double arrow_seconds()const{return arrow_loop_;}
 double arrow_move_seconds()const{return arrow_move_;}float slot_spacing()const{return spacing_;}
 float bottle_spacing()const{return bottle_spacing_;}
 float bottle_height()const{return bottle_height_;}float ebmain_height()const{return ebmain_height_;}
 uint32_t font_resource()const{return font_;}uint32_t outline_resource()const{return outline_;}
 uint32_t cursor_resource()const{return cursor_;}uint32_t arrow_resource()const{return arrow_;}
 uint32_t text_color()const{return text_color_;}uint32_t time_color()const{return time_color_;}uint32_t outline_color()const{return outline_color_;}
 const SaveMenuRect&layout(SaveMenuLayout x)const{return layouts_.at(size_t(x));}
 const std::string&text(SaveMenuText x)const{return texts_.at(size_t(x));}
 const std::string&sound(SaveMenuSound x)const{return sounds_.at(size_t(x));}
 const std::vector<SaveMenuResource>&resources()const{return resources_;}
 const std::vector<SaveMenuGlyph>&glyphs()const{return glyphs_;}
 const std::vector<SaveMenuFlavor>&flavors()const{return flavors_;}
 const std::vector<SaveMenuIcon>&icons()const{return icons_;}
 const std::vector<SaveMenuCursorKey>&cursor_keys()const{return cursor_keys_;}
 const std::vector<SaveMenuArrowKey>&arrow_keys()const{return arrow_keys_;}
 int flavor_index(const std::string&)const;int icon_index(const std::string&)const;
 bool supports_text(const std::string&,bool bottle)const;
private:
 bool valid_=false;uint32_t slots_=0,font_=0,outline_=0,cursor_=0,arrow_=0,text_color_=0,time_color_=0,outline_color_=0;
 double activation_=0,scroll_=0,cursor_loop_=0,arrow_loop_=0,arrow_move_=0;
 float spacing_=0,bottle_spacing_=0,bottle_height_=0,ebmain_height_=0;
 std::vector<SaveMenuRect>layouts_;std::vector<std::string>texts_,sounds_;
 std::vector<SaveMenuResource>resources_;std::vector<SaveMenuGlyph>glyphs_;std::vector<uint32_t>ebmain_codepoints_;
 std::vector<SaveMenuFlavor>flavors_;std::vector<SaveMenuIcon>icons_;
 std::vector<SaveMenuCursorKey>cursor_keys_;std::vector<SaveMenuArrowKey>arrow_keys_;
};
}
