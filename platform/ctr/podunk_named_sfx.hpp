#pragma once
#include "encore/player_named_sfx.hpp"
#include "music_region_service.hpp"
#include "podunk_audio_server.hpp"
#include "podunk_native_root.hpp"
#include "podunk_scene_audio.hpp"
namespace encore::ctr {
// Real original audManager root and $Sfx Nodes. This owner executes only the
// audited constructor, minimal Ready and named-SFX methods, never general
// music.
class PodunkNamedSfx {
public:
  bool prepare(std::shared_ptr<const upstream::PlayerNamedSfxData>,
               upstream::FieldGlobalRegistry &, PodunkNativeRoot &,
               upstream::FieldObjectSignals &, PodunkAudioServer &,
               AudioPlayer &, MusicRegionService &, std::string &);
  bool construct(upstream::FieldObjectId,
                 const upstream::FieldGlobalExternalSpec &,
                 std::unique_ptr<upstream::FieldGlobalExternalObject> &,
                 std::string &);
  bool add_sfx(std::string_view source, std::string_view name,
               upstream::FieldObjectId &, std::string &);
  bool add_sfx(upstream::FieldObjectId stream, std::string_view name,
               upstream::FieldObjectId &, std::string &);
  bool get_sfx(std::string_view, upstream::FieldObjectId &,
               std::string &) const;
  bool play_sfx(std::string_view source, std::string_view name,
                upstream::FieldObjectId &, std::string &);
  bool resource(std::string_view source, upstream::FieldObjectId &,
                std::string &);
  bool state(upstream::FieldObjectId, PodunkSceneAudioState &,
             upstream::FieldObjectId &stream, std::string &) const;
  bool play(upstream::FieldObjectId, std::string &);
  bool stop(upstream::FieldObjectId, std::string &);
  bool set_stream(upstream::FieldObjectId, upstream::FieldObjectId,
                  std::string &);
  bool signal_declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                          std::string &) const;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &);
  bool process(bool physics, bool paused, std::string &);
  const upstream::FieldGlobalRegistry *registry() const { return registry_; }
  upstream::FieldObjectId object() const { return binding_.object; }
  upstream::FieldObjectId music_parent() const { return music_parent_; }
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree() const { return tree_; }
  // Materialize same source Music child bodies around existing playback owners,
  // without allocating a second NDSP voice or replaying the music.
  bool synchronize_music_children(std::string &);
  bool music_state(upstream::FieldObjectId, MusicSourcePlayer &,
                   std::string &) const;

private:
  class ManagerObject;
  class StreamResource;
  friend class ManagerObject;
  upstream::FieldNodeTreeHost tree_host();
  bool construct_source(upstream::FieldObjectId,
                        const upstream::FieldNodeDescriptor &,
                        const upstream::FieldIdentity &, std::string &);
  bool bind(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
            upstream::FieldNodeBinding &, std::string &);
  bool phase(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
             upstream::FieldTreePhase, std::string &);
  bool source_ready(std::string &);
  bool create_voice(upstream::FieldObjectId parent, std::string_view bus,
                    std::string_view name, upstream::FieldObjectId &,
                    std::string &);
  bool manager_state(upstream::FieldGlobalExternalState &, std::string &) const;
  bool live(std::string &) const;
  std::shared_ptr<const upstream::PlayerNamedSfxData> data_;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkNativeRoot *root_ = nullptr;
  upstream::FieldObjectSignals *signals_ = nullptr;
  PodunkAudioServer *server_ = nullptr;
  AudioPlayer *audio_ = nullptr;
  MusicRegionService *music_ = nullptr;
  upstream::FieldGlobalExternalBinding binding_{};
  upstream::FieldObjectId parent_ = 0, sfx_parent_ = 0, music_parent_ = 0,
                          tween_ = 0;
  std::shared_ptr<upstream::FieldNodeTreeRuntime> tree_;
  PodunkSceneAudio voices_;
  std::set<upstream::FieldObjectId> constructed_, native_entered_,
      native_ready_;
  std::map<uint32_t, std::pair<upstream::FieldObjectId, const StreamResource *>>
      resources_;
  std::map<upstream::FieldObjectId, upstream::FieldObjectId> voice_streams_;
  std::vector<std::pair<std::string, upstream::FieldObjectId>> sound_effects_;
  std::vector<upstream::FieldObjectId> music_changers_, imported_music_;
  std::map<upstream::FieldObjectId, MusicSourcePlayer> music_nodes_;
  const MusicSourcePlayer *constructing_music_ = nullptr;
  bool first_ = false, failed_ = false, script_ready_ = false,
       music_bound_ = false, overworld_battle_music_ = false;
};
} // namespace encore::ctr
