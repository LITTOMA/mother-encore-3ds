#pragma once
#include "encore/field_scene_audio.hpp"
#include "podunk_player_native_media.hpp"
#include "podunk_scene_loop.hpp"
namespace encore::ctr {
struct PodunkSceneAudioListener {
  upstream::FieldObjectId viewport = 0;
  bool enabled = false, explicit_listener = false;
  upstream::Vec2 screen_size{}, listener_position{};
  upstream::FieldTransform canvas{};
};
struct PodunkSceneAudioHost {
  PodunkPlayerMediaHost server;
  // Actual World2D viewport/listener list and first physics Area audio bus.
  // No callback is evaluated until an actual positional player processes.
  std::function<bool(upstream::FieldObjectId,
                     std::vector<PodunkSceneAudioListener> &, std::string &)>
      listeners;
  std::function<bool(upstream::Vec2, uint32_t, std::string &, std::string &)>
      area_bus;
};
struct PodunkSceneAudioState {
  upstream::FieldObjectId object = 0;
  uint32_t source_stream = 0;
  float volume_db = 0, pitch = 1;
  std::string bus;
  bool inside = false, ready = false, playing = false, paused = false;
  double position = 0;
};
// Owns actual source AudioStreamPlayer nodes, not audioManager children.
// Six leased NDSP channels are acquired on play and recycled after drain.
// Metadata construction/Ready never opens unused PCM or creates Resource IDs.
class PodunkSceneAudio final : public PodunkSceneNativeMechanism {
public:
  ~PodunkSceneAudio();
  bool prepare(const upstream::FieldSceneAudioData &,
               upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, AudioPlayer &,
               PodunkSceneAudioHost, std::string &);
  bool owns(const upstream::FieldNodeDescriptor &) const override;
  bool owns(upstream::FieldObjectId) const override;
  bool construct(upstream::FieldObjectId, const upstream::FieldNodeDescriptor &,
                 const upstream::FieldIdentity &, std::string &) override;
  bool bind(upstream::FieldObjectId, upstream::FieldNodeBinding &,
            std::string &) override;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase, float, bool,
             bool, std::string &) override;
  bool deferred(const upstream::FieldDeferredMessage &, std::string &) override;
  bool release(upstream::FieldObjectId, std::string &) override;
  bool state(upstream::FieldObjectId, PodunkSceneAudioState &,
             std::string &) const;
  bool set_stream(upstream::FieldObjectId, uint32_t, std::string &);
  bool play(upstream::FieldObjectId, double, std::string &);
  bool stop(upstream::FieldObjectId, std::string &);
  bool seek(upstream::FieldObjectId, double, std::string &);
  bool set_paused(upstream::FieldObjectId, bool, std::string &);
  bool set_volume(upstream::FieldObjectId, float, std::string &);
  bool set_pitch(upstream::FieldObjectId, float, std::string &);
  bool set_bus(upstream::FieldObjectId, std::string_view, std::string &);
  bool mix(upstream::FieldObjectId, std::string &);
  // Actual SceneTree pause/unpause notification boundary, not a frame clock.
  bool tree_pause(bool, std::string &);
  bool signal_declaration(upstream::FieldObjectId, std::string_view, uint32_t &,
                          std::string &) const;
  bool shutdown(std::string &);

private:
  static constexpr uint32_t buffer_count = 3, block_frames = 1024;
  struct Output {
    upstream::FieldObjectId viewport = 0;
    float left = 0, right = 0;
    std::string bus;
  };
  struct Voice {
    const upstream::FieldSceneAudioNode *source = nullptr;
    PodunkSceneAudioState state;
    upstream::AudioAsset asset{};
    std::unique_ptr<upstream::AudioPcmStream> pcm;
    std::array<ndspWaveBuf, buffer_count> waves{};
    int16_t *samples = nullptr;
    int channel = -1;
    bool active = false, decoder = false, internal = false, set_stop = false,
         stop_priority = false;
    bool fade_pause = false, fade_in = false, output_ready = false;
    double pending_seek = -1, setplay = -1;
    float mix_volume = 0;
    std::vector<int16_t> tail;
    std::vector<Output> outputs, previous;
  };
  bool live(std::string &) const;
  Voice *voice(upstream::FieldObjectId, std::string &);
  bool asset(uint32_t, upstream::AudioAsset &, std::string &) const;
  bool internal(Voice &, bool, std::string &);
  bool reserve(Voice &, std::string &);
  bool read(Voice &, int16_t *, uint32_t, uint32_t &, std::string &);
  bool fade(Voice &, uint32_t, std::string &);
  bool spatial(Voice &, std::string &);
  bool queued(const Voice &) const;
  void free_voice(Voice &);
  void release_channel(Voice &);
  const upstream::FieldSceneAudioData *data_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  AudioPlayer *audio_ = nullptr;
  std::array<uint8_t, 32> ir_{};
  PodunkSceneAudioHost host_;
  std::shared_ptr<const upstream::AudioBank> bank_;
  std::map<upstream::FieldObjectId, Voice> voices_;
  std::array<upstream::FieldObjectId, 6> leases_{};
  bool tree_paused_ = false;
};
} // namespace encore::ctr
