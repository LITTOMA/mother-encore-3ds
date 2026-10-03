#pragma once
#include "encore/save_menu_data.hpp"
#include <array>
namespace encore::upstream {
enum class ContinueLayout:uint32_t {Reference,Body,Actions,ArrowOffset,SelectedCursor,FadeGeometry,FadeOffset,TitleViewportOffsets,Count};
enum class ContinueSound:uint32_t {Move,Accept,Back,Restricted,Count};
enum class ContinueAnimation:uint32_t {CircleIn,CircleOut,FadeIn,FadeOut,Count};
struct ContinueFadeAnimation {double length=0,key_end=0,from=0,to=0,ease=0;double value(double seconds)const;};
struct ContinueViewportOffset {float x=0,y=0;};
struct ContinueDraw {uint32_t resource=0;SaveMenuRect rect;std::array<ContinueViewportOffset,2>viewport_offsets;};
struct ContinueTitleOption {uint32_t resource=0,selected_resource=0;SaveMenuRect rect;std::array<ContinueViewportOffset,2>viewport_offsets;};
struct ContinueViewport {float width=0;std::array<SaveMenuRect,4>actions;};
class ContinueMenuData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}uint32_t background_color()const{return background_;}
 uint32_t title_background_color()const{return title_background_;}
 double door_in_speed()const{return door_in_;}double door_out_speed()const{return door_out_;}double music_fade_seconds()const{return music_fade_;}double door_music_fade_seconds()const{return door_music_fade_;}double action_repeat_seconds()const{return action_repeat_;}
 const std::string&title_music()const{return title_music_;}
 const std::string&text(uint32_t action)const{return texts_.at(action);}
 const std::string&sound(ContinueSound sound)const{return sounds_.at(size_t(sound));}
 const SaveMenuRect&layout(ContinueLayout key)const{return layouts_.at(size_t(key));}
 const ContinueFadeAnimation&animation(ContinueAnimation key)const{return animations_.at(size_t(key));}
 const std::vector<SaveMenuResource>&resources()const{return resources_;}
 const std::vector<ContinueDraw>&title_layers()const{return layers_;}
 const std::vector<ContinueTitleOption>&title_options()const{return options_;}
 const std::vector<ContinueViewport>&viewports()const{return viewports_;}
 int viewport_index(float width)const;
private:
 bool valid_=false;uint32_t background_=0,title_background_=0;double door_in_=0,door_out_=0,music_fade_=0,door_music_fade_=0,action_repeat_=0;
 std::string title_music_;std::vector<std::string>texts_,sounds_;std::vector<SaveMenuRect>layouts_;
 std::vector<ContinueFadeAnimation>animations_;std::vector<SaveMenuResource>resources_;
 std::vector<ContinueDraw>layers_;std::vector<ContinueTitleOption>options_;std::vector<ContinueViewport>viewports_;
};
}
