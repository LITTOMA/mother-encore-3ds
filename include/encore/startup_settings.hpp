#pragma once
#include "encore/session_save.hpp"
#include <array>
#include <string>
#include <vector>
namespace encore::upstream {
struct SettingsRect {float x=0,y=0,w=0,h=0;};
struct SettingsLabel {std::string text;SettingsRect rect;};
struct SettingsRow {std::string text;SettingsRect label,value;};
struct SettingsPanel {SettingsRect box;std::vector<SettingsLabel>labels;};
struct SettingsResource {std::string path;uint32_t width=0,height=0,columns=0,rows=0;};
struct SettingsConfirmationField {SettingsRect box,inside,label,icon;uint32_t resource=0;};
// Checked source data shared by startup UI and palette consumers. No game option
// values, labels, palette colors or original layout constants live in C++.
class StartupSettingsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}
 SessionSettings defaults()const;bool supports(const SessionSettings&)const;
 int speed_index(double)const;int flavor_index(const std::string&)const;int prompt_index(const std::string&)const;
 std::vector<double>speeds;std::vector<std::string>flavors,prompts,speed_labels,flavor_labels,prompt_labels;
 std::array<uint32_t,3>default_indices{};bool description=false;uint32_t text_color=0;std::array<uint32_t,4>patch{};
 SettingsRect settings_box,confirmation_settings_box,confirmation_box;std::array<float,2>confirmation_row_offset{};
 std::vector<SettingsRow>rows;std::vector<SettingsPanel>panels;std::vector<SettingsResource>resources;
 uint32_t box_resource=0,card_resource=0,inside_resource=0;std::vector<SettingsConfirmationField>confirmation_fields;SettingsLabel certainty;std::vector<SettingsLabel>confirmation_choices;
 double palette_threshold=0;std::array<uint32_t,8>source_palette{};std::vector<std::array<uint32_t,8>>palettes;std::vector<std::string>skin_paths;
private:bool valid_=false;
};
}
