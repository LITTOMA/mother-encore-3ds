#pragma once
#include "house_return_controls.hpp"
#include "encore/player_motion.hpp"
namespace encore::ctr {
struct HouseGuiPick {
 upstream::FieldObjectId object=0,root=0;uint32_t source=0;
 upstream::Vec2 local_position{};upstream::FieldTransform inverse{};
 // This is the actual House branch. A global UI owner must merge its own
 // roots before claiming a complete Viewport input result.
 bool house_scope=true,global_scope_complete=false;
};
struct HouseGuiInputResult {
 bool handled=false,had_focus=false,house_scope=true,global_scope_complete=false;
};
// Actual source Control registrations borrow the ONE Viewport. No additional
// SceneTree, physics space, layout state, focusable dummy or event is created.
class HouseReturnGuiNative {
public:
 bool prepare(HouseReturnControlsNative&,upstream::FieldObjectSignals&,
              PodunkNativeRoot&,std::string&);
 bool enter_control(upstream::FieldObjectId,std::string&);
 bool exit_control(upstream::FieldObjectId,std::string&);
 bool live_registration(upstream::FieldObjectId,std::string&)const;
 bool retired_registration(upstream::FieldObjectId,std::string&)const;
 bool pick(upstream::Vec2 actual_viewport_position,HouseGuiPick&,std::string&);
 bool action_input(const upstream::PlayerInputEvent&,bool paused,
                   HouseGuiInputResult&,std::string&);
 // Pointer dispatch cannot fabricate an InputEvent Reference from a hit.
 // Existing PlayerInputEvent has no MouseButton/Motion native fields/owner.
 bool pointer_input(upstream::FieldObjectId actual_event,
                    HouseGuiInputResult&,std::string&);
 bool declaration(upstream::FieldObjectId,std::string_view,uint32_t&,
                  std::string&)const;
 bool can_unbind(std::string&)const;
 const upstream::FieldNodeTreeRuntime*tree()const{return tree_;}
 const upstream::FieldGlobalRegistry*registry()const{return registry_;}
 const upstream::FieldObjectSignals*signals()const{return signals_;}
 const PodunkNativeRoot*native_root()const{return root_;}
 const auto&registrations()const{return registered_;}
private:
 struct Registration {uint32_t source=0;bool root=false,subwindow=false;};
 HouseReturnControlsNative*controls_=nullptr;upstream::FieldNodeTreeRuntime*tree_=nullptr;
 upstream::FieldGlobalRegistry*registry_=nullptr;upstream::FieldObjectSignals*signals_=nullptr;
 PodunkNativeRoot*root_=nullptr;std::map<upstream::FieldObjectId,Registration>registered_;
 std::vector<upstream::FieldObjectId>roots_,subwindows_;
 bool actual(upstream::FieldObjectId,HouseControlNativeState&,std::string&)const;
 bool active(std::string&)const;
 bool ordered(std::vector<upstream::FieldObjectId>&,std::string&)const;
 bool find(upstream::FieldObjectId,upstream::Vec2,HouseGuiPick&,
           std::set<upstream::FieldObjectId>&,std::string&);
};
}
