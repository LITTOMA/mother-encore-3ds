#pragma once
#include "encore/continue_menu_data.hpp"
#include "encore/localization.hpp"
#include <string_view>
namespace encore::upstream {
struct TitleTextureRemap {
 std::string locale,base_path,path;uint32_t width=0,height=0;
};
// Immutable, external texture bindings. Source language uses the unchanged
// Continue pack. Other locales must supply every normal/selected title option.
class TitleLocaleData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return !fallback_.empty();}
 std::string_view fallback()const{return fallback_;}
 const std::vector<TitleTextureRemap>&remaps()const{return remaps_;}
 bool resolve(const ContinueMenuData&,const LocaleSelection&,
              std::vector<SaveMenuResource>&resources,
              std::vector<SaveMenuRect>&option_rects,std::string&error)const;
private:
 std::string fallback_;std::vector<TitleTextureRemap>remaps_;
};
}
