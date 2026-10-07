#pragma once
#include "encore/house_reentry.hpp"
#include "encore/field_node_tree.hpp"
#include "encore/podunk_bundle.hpp"
#include "encore/field_npc_world.hpp"
#include "encore/field_native_timer.hpp"
#include "encore/field_visibility.hpp"
#include "encore/field_sprite_bridge.hpp"
#include "encore/field_map.hpp"
#include "encore/field_canvas_art.hpp"
#include "encore/field_tint.hpp"
#include "encore/house_return_button_prompt.hpp"
#include "encore/house_return_controls.hpp"
#include "encore/house_return_inspection_programme.hpp"
#include "encore/house_inspection_restore.hpp"
#include "encore/house_return_camera_control.hpp"
#include "encore/field_scene_audio.hpp"

namespace encore::upstream {
// One immutable, cross-bound destination owner. Admission does not allocate
// nodes, execute source lifecycle, alter flags or consume random state.
class HouseReturnSources {
public:
  bool load(const PodunkBundleData &, const std::string &romfs_root,
            const FieldDoorData &, RoomView, HouseView, DrawerProgramView, std::string &);
  bool valid() const { return valid_; }
  const HouseReentryData &reentry() const { return reentry_; }
  const FieldGeometryView &geometry() const { return geometry_; }
  const FieldNodeTreeData &tree() const { return tree_; }
  const FieldNpcData &npcs() const { return npcs_; }
  const FieldNpcWorldData &npc_world() const { return npc_world_; }
  const FieldNativeTimerData &timers() const { return timers_; }
  const FieldVisibilityData &visibility() const { return visibility_; }
  const FieldSpriteData &sprites() const { return sprites_; }
  const FieldMapView &map() const { return map_; }
  const FieldCanvasArtData &canvas() const { return canvas_; }
  const FieldTintData &tint() const { return tint_; }
  const HouseReturnButtonPromptData &button_prompts()const{return button_prompts_;}
  const HouseReturnControlsData &controls()const{return controls_;}
  const FieldInteractData &interact()const{return interact_;}
  const HouseReturnInspectionProgrammes &inspections()const{return inspections_;}
  const HouseInspectionRestoreData &inspection_restore()const{return inspection_restore_;}
  const HouseReturnCameraControlData &camera_control()const{return camera_control_;}
  const FieldSceneAudioData &scene_audio()const{return scene_audio_;}
  static bool admit_interact(const FieldInteractData&,const FieldNodeTreeData&,std::string&);
  // The complete tree owns the TileMaps' authoritative local/world matrices.
  // Only IDs already bound to a loaded reentry certificate can resolve here.
  const FieldNodeDescriptor *tilemap_node(uint32_t source_id) const;

private:
  bool valid_ = false;
  HouseReentryData reentry_;
  FieldGeometryView geometry_;
  FieldNodeTreeData tree_;
  FieldNpcData npcs_;
  FieldNpcWorldData npc_world_;
  FieldNativeTimerData timers_;
  FieldVisibilityData visibility_;
  FieldSpriteData sprites_;
  FieldMapView map_;
  FieldCanvasArtData canvas_;
  FieldTintData tint_;
  HouseReturnButtonPromptData button_prompts_;
  HouseReturnControlsData controls_;
  FieldInteractData interact_;
  HouseReturnInspectionProgrammes inspections_;
  HouseInspectionRestoreData inspection_restore_;
  HouseReturnCameraControlData camera_control_;
  FieldSceneAudioData scene_audio_;
};
} // namespace encore::upstream
