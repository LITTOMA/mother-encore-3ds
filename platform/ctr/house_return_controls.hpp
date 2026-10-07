#pragma once
#include "encore/house_return_controls.hpp"
#include "house_return_button_prompt_native.hpp"
#include "podunk_native_root.hpp"
#include "podunk_scene_native.hpp"
namespace encore::ctr {
struct HouseControlNativeState {
 const upstream::HouseControlRecord*source=nullptr;
 upstream::FieldObjectId object=0,parent=0,owner=0;
 upstream::FieldNodeBinding binding{};
 upstream::Vec2 position{},size{};std::string text;
 bool constructed=false,bound=false,entered=false,ready=false;
};
// All72 actual native Controls:69 Prompt leaves borrow the sole layout/text/
// font owner;2 Canvas leaves borrow the same native GPU owner;the remaining
// Control retains its own rect state and separately owned Room Shaker script.
class HouseReturnControlsNative final : public upstream::FieldCanvasControlOwner,
                                        public PodunkSceneCanvasLeaf {
public:
 bool prepare(const upstream::HouseReturnControlsData&,
              const upstream::FieldNodeTreeData&,const upstream::FieldCanvasArtData&,
              upstream::FieldNodeTreeRuntime&,upstream::FieldGlobalRegistry&,
              PodunkNativeRoot&,HouseReturnButtonPromptNative&,PodunkSceneNative&,
              std::string&);
 bool owns(const upstream::FieldNodeDescriptor&)const;
 bool owns(upstream::FieldObjectId)const;
 // For HBox/Label call after the SAME Prompt owner constructed that native
 // leaf. It is validated and borrowed, never constructed a second time.
 bool construct(upstream::FieldObjectId,const upstream::FieldNodeDescriptor&,
                const upstream::FieldIdentity&,std::string&);
 bool bind(upstream::FieldObjectId,const upstream::FieldNodeBinding&,std::string&);
 bool finish_factory(std::string&)const;
 // HBox/Label: observe after PromptNative consumed this same native phase.
 // This does not call its clock or lifecycle twice. Control consumes native
 // phases itself; ReadyScript/EnterScript/ExitScript remain the source owner.
 bool phase(upstream::FieldObjectId,upstream::FieldTreePhase,std::string&);
 bool release_deleted(upstream::FieldObjectId,std::string&);
 bool snapshot(upstream::FieldObjectId,HouseControlNativeState&,std::string&)const;
 // Actual Room Shaker parent CanvasItem anchor rectangle must come from its
 // concrete native owner. A Node2D default rect is not the viewport size.
 bool parent_rect_changed(PodunkSceneNative&actual_parent_owner,
                          upstream::FieldObjectId parent,std::string&);
 const upstream::FieldCanvasArtData*canvas_data()const override{return art_;}
 const upstream::HouseReturnControlsData*control_data()const{return data_;}
 const PodunkNativeRoot*native_root()const{return root_;}
 const upstream::FieldNodeTreeRuntime*canvas_tree()const override{return tree_;}
 const upstream::FieldGlobalRegistry*canvas_registry()const override{return registry_;}
 bool admit_control(const upstream::FieldCanvasControlBoundary&,
                    upstream::FieldObjectId,upstream::FieldObjectId,std::string&)const override;
 bool control_snapshot(const upstream::FieldCanvasControlBoundary&,
                       upstream::FieldObjectId,upstream::FieldObjectId,
                       bool&drawable,std::string&)const override;
 bool owns_drawable(upstream::FieldObjectId)const override;
 bool draw_leaf(const upstream::FieldCanvasOrderSlot&,
                const upstream::FieldTransform&,bool,std::string&)override;
private:
 struct Entry {
  const upstream::HouseControlRecord*source=nullptr;
  upstream::FieldNodeBinding binding{};
  upstream::Vec2 position{},size{},parent_size{};
  bool bound=false,entered=false,ready=false,parent_rect_observed=false;
 };
 const upstream::HouseReturnControlsData*data_=nullptr;
 const upstream::FieldNodeTreeData*source_=nullptr;
 const upstream::FieldCanvasArtData*art_=nullptr;
 upstream::FieldNodeTreeRuntime*tree_=nullptr;
 upstream::FieldGlobalRegistry*registry_=nullptr;
 PodunkNativeRoot*root_=nullptr;HouseReturnButtonPromptNative*prompt_=nullptr;
 PodunkSceneNative*native_=nullptr;
 std::map<upstream::FieldObjectId,Entry>entries_;
 bool actual(upstream::FieldObjectId,const Entry*&,std::string&)const;
 bool prompt_state(upstream::FieldObjectId,HouseButtonPromptControlState&,
                   std::string&)const;
 bool canvas_state(upstream::FieldObjectId,PodunkSceneControlState&,
                   std::string&)const;
 bool resize(upstream::FieldObjectId,Entry&,upstream::Vec2,std::string&);
};
}
