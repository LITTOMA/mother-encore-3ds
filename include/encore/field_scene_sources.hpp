#pragma once
#include "encore/field_scene_host.hpp"
#include "encore/field_canvas_art.hpp"
#include "encore/field_native_timer.hpp"
#include "encore/field_scene_audio.hpp"
#include "encore/field_scene_signal_callbacks.hpp"
#include "encore/field_visibility.hpp"
#include "encore/field_npc_world.hpp"
#include "encore/prompt_native.hpp"
#include "encore/audio_server.hpp"
#include "encore/field_scene_materials.hpp"
#include "encore/scene_clip_native.hpp"
#include "encore/podunk_bundle.hpp"
namespace encore::upstream {
// Stable ownership of the complete scene's actual typed binary sources. Load
// before allocating destination nodes; it neither dispatches Ready nor uses
// RNG. Keep this object alive until all runtime consumers and GPU owners exit.
class FieldSceneSources {
public:
  FieldSceneSources() = default;
  FieldSceneSources(const FieldSceneSources &) = delete;
  FieldSceneSources &operator=(const FieldSceneSources &) = delete;
  bool load(const PodunkBundleData &, const std::string &romfs_root,
            std::string &);
  bool valid() const { return loaded_ && !failed_; }
  // Assign immutable sources only. Existing actual runtime owners and their
  // callbacks must still be initialized and supplied by the scene host.
  bool bind_sources(FieldSceneConsumers &, std::string &) const;
  const FieldData &grass() const { return grass_; }
  const FieldNpcData &npc() const { return npc_; }
  const FieldEnemyData &enemy() const { return enemy_; }
  const FieldTintData &tint() const { return tint_; }
  const FieldSpriteData &sprite() const { return sprite_; }
  const FieldEmoteData &emote() const { return emote_; }
  const FieldDandelionData &dandelion() const { return dandelion_; }
  const FieldDoorData &door() const { return door_; }
  const FieldPromptData &prompt() const { return prompt_; }
  const FieldBushData &bush() const { return bush_; }
  const FieldInteractData &interact() const { return interact_; }
  const FieldPresentData &present() const { return present_; }
  const FieldDroppedData &dropped() const { return dropped_; }
  const FieldSparklesData &sparkles() const { return sparkles_; }
  const FieldOpenableDoorData &openable() const { return openable_; }
  const FieldPayphoneData &payphone() const { return payphone_; }
  const FieldButterflyData &butterfly() const { return butterfly_; }
  const FieldCutsceneAreaData &cutscene() const { return cutscene_; }
  const FieldBirdData &birds() const { return birds_; }
  const FieldCameraAreaData &camera_area() const { return camera_area_; }
  const FieldMusicChangerData &music() const { return music_; }
  const FieldCameraArrowsData &arrows() const { return arrows_; }
  const FieldSceneActionsData &actions() const { return actions_; }
  const FieldSteppingSoundsData &stepping() const { return stepping_; }
  const FieldPlayerTransitionsData &transitions() const { return transitions_; }
  const FieldGameCameraData &camera() const { return camera_; }
  const FieldDoorNpcData &door_npc() const { return door_npc_; }
  const FieldMelodyBackgroundData &melody() const { return melody_; }
  const FieldVendingData &vending() const { return vending_; }
  const FieldMapView &map() const { return map_; }
  const FieldGeometryView &geometry() const { return geometry_; }
  const FieldSceneData &lifecycle() const { return lifecycle_; }
  const FieldNodeTreeData &tree() const { return tree_; }
  const FieldCanvasArtData &canvas() const { return canvas_; }
  const FieldNativeTimerData &timers() const { return timers_; }
  const FieldSceneAudioData &audio() const { return audio_; }
  const FieldSceneSignalCallbacksData &signals() const { return signals_; }
  const FieldVisibilityData &visibility() const { return visibility_; }
  const FieldNpcWorldData &npc_world() const { return npc_world_; }
  const PromptNativeData &prompt_native() const { return prompt_native_; }
  const AudioServerData &audio_server() const { return audio_server_; }
  const FieldSceneMaterialsData &materials() const { return materials_; }
  const SceneClipNativeData &clips() const { return clips_; }
private:
  FieldData grass_;
  FieldNpcData npc_;
  FieldEnemyData enemy_;
  FieldTintData tint_;
  FieldSpriteData sprite_;
  FieldEmoteData emote_;
  FieldDandelionData dandelion_;
  FieldDoorData door_;
  FieldPromptData prompt_;
  FieldBushData bush_;
  FieldInteractData interact_;
  FieldPresentData present_;
  FieldDroppedData dropped_;
  FieldSparklesData sparkles_;
  FieldOpenableDoorData openable_;
  FieldPayphoneData payphone_;
  FieldButterflyData butterfly_;
  FieldCutsceneAreaData cutscene_;
  FieldBirdData birds_;
  FieldCameraAreaData camera_area_;
  FieldMusicChangerData music_;
  FieldCameraArrowsData arrows_;
  FieldSceneActionsData actions_;
  FieldSteppingSoundsData stepping_;
  FieldPlayerTransitionsData transitions_;
  FieldGameCameraData camera_;
  FieldDoorNpcData door_npc_;
  FieldMelodyBackgroundData melody_;
  FieldVendingData vending_;
  FieldMapView map_;
  FieldGeometryView geometry_;
  FieldSceneData lifecycle_;
  FieldNodeTreeData tree_;
  FieldCanvasArtData canvas_;
  FieldNativeTimerData timers_;
  FieldSceneAudioData audio_;
  FieldSceneSignalCallbacksData signals_;
  FieldVisibilityData visibility_;
  FieldNpcWorldData npc_world_;
  PromptNativeData prompt_native_;
  AudioServerData audio_server_;
  FieldSceneMaterialsData materials_;
  SceneClipNativeData clips_;
  bool loaded_ = false, failed_ = false;
};
} // namespace encore::upstream
