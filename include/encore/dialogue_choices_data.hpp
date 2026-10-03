#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
struct DialogueChoiceRect {float x=0,y=0,w=0,h=0;};
struct DialogueChoiceResource {std::string path;uint32_t width=0,height=0,columns=0,rows=0;};
struct DialogueChoiceKey {double time=0;uint32_t frame=0;};
struct DialogueChoiceOption {std::string translation_key,text;uint32_t target_pc=0;DialogueChoiceRect rect;};
struct DialogueChoiceGroup {
 std::string id,program_identity,source_label;
 uint32_t program_command_count=0,initial_selection=0,cancel_target_pc=0;
 std::vector<DialogueChoiceOption>options;
};
enum class DialogueChoiceSound:uint32_t {Move,Accept,Cancel};
class DialogueChoicesData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}
 bool validate_program(uint32_t group,std::string_view identity,uint32_t command_count,std::string&)const;
 uint32_t columns()const{return columns_;}uint32_t child_count()const{return children_;}
 uint32_t trailing_blank_lines()const{return trailing_blank_lines_;}
 const DialogueChoiceRect&grid()const{return grid_;}
 const DialogueChoiceRect&minimum()const{return minimum_;}
 const DialogueChoiceRect&arrow_geometry()const{return arrow_geometry_;}
 double arrow_move_seconds()const{return move_;}double arrow_loop_seconds()const{return loop_;}
 float font_height()const{return font_height_;}uint32_t text_color()const{return color_;}
 const std::string&sound(DialogueChoiceSound x)const{return sounds_.at(size_t(x));}
 const DialogueChoiceResource&arrow_resource()const{return arrow_;}
 const std::string&font_path()const{return font_path_;}
 const std::vector<DialogueChoiceKey>&arrow_keys()const{return keys_;}
 const std::vector<DialogueChoiceGroup>&groups()const{return groups_;}
private:
 bool valid_=false;uint32_t columns_=0,children_=0,color_=0,trailing_blank_lines_=0;
 DialogueChoiceRect grid_,minimum_,arrow_geometry_;
 double move_=0,loop_=0;float font_height_=0;
 std::vector<std::string>sounds_;DialogueChoiceResource arrow_;std::string font_path_;
 std::vector<DialogueChoiceKey>keys_;std::vector<DialogueChoiceGroup>groups_;
};
}
