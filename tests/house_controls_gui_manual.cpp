// Explicit manual source cases, not registered or executed by this slice.
// The caller must supply the actual entered House branch and unique Viewport.
#include "../platform/ctr/house_return_gui_native.hpp"
#include <limits>
namespace encore::manual {
bool house_controls_gui_manual(ctr::HouseReturnGuiNative&gui,
 ctr::PodunkNativeRoot&root,upstream::FieldObjectId entered_control,
 std::string&e){
 std::string error;
 if(!root.house_gui_registered(entered_control,error)){
  e="House GUI manual positive needs the real entered registered Control";return false;
 }
 auto rejected=[&](bool accepted,const char*name){
  if(accepted||error.empty()||!root.house_gui_registered(entered_control,e)){
   e=std::string("House GUI manual rejection/preservation failed: ")+name;return false;
  }error.clear();return true;
 };
 if(!rejected(gui.enter_control(entered_control,error),"duplicate native Enter"))return false;
 if(!rejected(gui.exit_control(entered_control,error),"unregister before native Exit"))return false;
 if(!rejected(root.clear_house_gui(gui,error),"clear unique Root borrower before Exit"))return false;
 ctr::HouseGuiPick pick;pick.object=entered_control;pick.source=0x12345678;
 if(!rejected(root.house_gui_pick({std::numeric_limits<float>::quiet_NaN(),0},pick,error),"nonfinite viewport coordinate")||pick.object!=entered_control||pick.source!=0x12345678){e="House GUI manual failed picker replaced caller output";return false;}
 ctr::HouseGuiInputResult result;result.handled=true;result.had_focus=true;
 if(!rejected(root.house_gui_pointer(0,result,error),"missing real native InputEvent owner")||!result.handled||!result.had_focus){e="House GUI manual failed pointer changed caller result";return false;}
 uint32_t arity=0x12345678;
 if(!rejected(gui.declaration(entered_control,"unknown_native_signal",arity,error),"unknown native Control signal")||arity!=0x12345678){e="House GUI manual failed signal changed caller arity";return false;}
 // The source72 no-focus keyboard/action result is scoped to House. It must
 // neither consume the event nor claim that independent global UI ran.
 upstream::PlayerInputEvent event;ctr::HouseGuiInputResult actual;
 if(!root.house_gui_action(event,false,actual,e)||actual.handled||actual.had_focus||!actual.house_scope||actual.global_scope_complete){e="House GUI manual no-focus source action differed";return false;}
 e.clear();return true;
}
}
