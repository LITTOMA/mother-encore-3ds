#include "dungeon_key_hud_renderer.hpp"
#include "save_menu_renderer.hpp"
// Compile both template paths against the actual resident font adapter with
// the real 3DS toolchain. Host checks use explicitly declared font/GPU doubles.
namespace encore::ctr {
bool key_hud_font_compile_contract(DungeonKeyHudRenderer&r,const upstream::DungeonKeyIndicator&h,const SaveMenuRenderer&f,float w,float height,uint64_t epoch,std::string&e){return r.compose(h,f,w,height,e)&&r.draw(epoch,r.frame(),f,e);}
}
