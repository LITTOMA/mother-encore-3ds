#pragma once
#include "audio_player.hpp"
#include "podunk_audio_callback.hpp"
#include "podunk_player_resources.hpp"
namespace encore::ctr {
struct PodunkPlayerMediaHost {
  // Bind to the actual AudioServer callback list and shared ObjectDB signals.
  std::function<bool(upstream::FieldObjectId, PodunkNativeAudioCallback,
                     std::string &)>
      add_audio_callback;
  std::function<bool(upstream::FieldObjectId, std::string &)>
      remove_audio_callback;
  std::function<bool(std::string_view, float &, bool &, std::string &)>
      bus_gain;
  // Actual source AudioServer connection; callbacks validate the same bus
  // graph.
  std::function<bool(upstream::FieldObjectId, std::function<bool()>,
                     std::string &)>
      connect_bus_layout;
  std::function<bool(upstream::FieldObjectId, std::string &)>
      disconnect_bus_layout;
  std::function<bool(upstream::FieldObjectId, std::string_view, std::string &)>
      emit;
  std::function<bool(upstream::FieldObjectId, std::string_view,
                     upstream::FieldObjectId, std::string_view, uint32_t,
                     std::string &)>
      connect;
  std::function<bool(upstream::FieldObjectId, std::string_view,
                     upstream::FieldObjectId, std::string_view, std::string &)>
      disconnect;
};
struct PodunkPlayerAnimatedState {
  upstream::FieldObjectId object = 0;
  uint32_t frame = 0;
  bool playing = false, visible = false;
  upstream::Vec2 offset{};
  std::string animation;
};
struct PodunkPlayerAudioState {
  upstream::FieldObjectId object = 0, stream = 0;
  uint32_t source_stream = 0;
  bool entered = false, ready = false, active = false, set_stop = false,
       stop_priority = false, paused = false, paused_fade = false,
       internal = false;
  double pending_seek = -1;
  float volume = 0, mix_volume = 0, pitch = 1;
  std::string bus;
};
// One live Player media owner leases at most two shared audio channels. It borrows the
// existing initialized audio device and real source Resource payloads. It never
// initializes DSP, owns another animation/player clock or creates a Node.
class PodunkPlayerNativeMedia {
public:
  PodunkPlayerNativeMedia() = default;
  ~PodunkPlayerNativeMedia();
  PodunkPlayerNativeMedia(const PodunkPlayerNativeMedia &) = delete;
  PodunkPlayerNativeMedia &operator=(const PodunkPlayerNativeMedia &) = delete;
  bool prepare(const upstream::PlayerInitializationData &,
               PodunkPlayerResources &, upstream::FieldNodeTreeRuntime &,
               upstream::FieldGlobalRegistry &, AudioPlayer &,
               PodunkPlayerMediaHost,
               const std::vector<uint32_t> &claimed_sprites,
               const std::vector<uint32_t> &claimed_audio, std::string &);
  // After actual persistent-subtree transfer, retain the same frames, voices
  // and ObjectIDs. No construction, Ready or clock advancement is performed.
  bool rebind_tree(upstream::FieldNodeTreeRuntime &, std::string &);
  bool construct(upstream::FieldObjectId, std::string &);
  bool owns(upstream::FieldObjectId) const;
  bool admit(upstream::FieldObjectId, std::string_view member, bool method,
             std::string &) const;
  bool phase(upstream::FieldObjectId, upstream::FieldTreePhase,
             float actual_delta, bool tree_paused, bool update_pending,
             std::string &);
  bool animated_state(upstream::FieldObjectId, PodunkPlayerAnimatedState &,
                      std::string &) const;
  bool set_frame(upstream::FieldObjectId, uint32_t, std::string &);
  bool set_playing(upstream::FieldObjectId, bool, std::string &);
  bool play(upstream::FieldObjectId, std::string_view animation, bool backwards,
            std::string &);
  bool set_offset(upstream::FieldObjectId, upstream::Vec2, std::string &);
  bool set_visible(upstream::FieldObjectId, bool, std::string &);
  bool connect(upstream::FieldObjectId, std::string_view,
               upstream::FieldObjectId, std::string_view method, uint32_t flags,
               std::string &);
  bool disconnect(upstream::FieldObjectId, std::string_view,
                  upstream::FieldObjectId, std::string_view method,
                  std::string &);
  bool draw(upstream::FieldObjectId, const upstream::FieldTransform &viewport,
            bool pixel_snap, std::string &) const;
  bool audio_state(upstream::FieldObjectId, PodunkPlayerAudioState &,
                   std::string &) const;
  bool audio_stream(upstream::FieldObjectId, uint32_t source_resource,
                    std::string &);
  bool audio_playing(upstream::FieldObjectId, bool, std::string &);
  bool audio_play(upstream::FieldObjectId, double from, std::string &);
  bool audio_seek(upstream::FieldObjectId, double, std::string &);
  bool audio_paused(upstream::FieldObjectId, bool, std::string &);
  bool audio_volume(upstream::FieldObjectId, float, std::string &);
  bool audio_pitch(upstream::FieldObjectId, float, std::string &);
  bool audio_bus(upstream::FieldObjectId, std::string_view, std::string &);
  bool audio_position(upstream::FieldObjectId, double &, std::string &) const;
  bool audio_mix(upstream::FieldObjectId, std::string &);
  bool tree_pause(bool paused, std::string &);
  bool stream(uint32_t source_resource, upstream::FieldObjectId &,
              std::string &);
  bool release(upstream::FieldObjectId, std::string &);
  bool shutdown(std::string &);

private:
  struct Clip {
    std::string name;
    float speed = 0;
    bool loop = false;
    std::vector<upstream::FieldObjectId> frames;
  };
  struct Sprite {
    PodunkPlayerAnimatedState state;
    std::vector<Clip> clips;
    float speed = 1, timeout = 0;
    bool centered = false, flip_h = false, flip_v = false, over = false,
         backwards = false, entered = false, ready = false;
  };
  static constexpr size_t buffers = 3, block_frames = 1024;
  struct Voice {
    PodunkAudioVoiceCallback callback;
    PodunkPlayerAudioState state;
    upstream::PlayerResourceAudio binding;
    const std::vector<uint8_t> *pcm = nullptr;
    upstream::AudioFrameCursor cursor;
    std::array<ndspWaveBuf, buffers> waves{};
    int16_t *samples = nullptr;
    std::vector<int16_t> tail;
    uint32_t tail_frames = 0;
    int channel = -1;
    bool decoder_playing = false, autoplay = false;
  };
  bool live(std::string &) const;
  bool node(upstream::FieldObjectId, std::string &) const;
  bool internal(upstream::FieldObjectId, bool, std::string &);
  bool animated_process(Sprite &, float, bool, bool, std::string &);
  const Clip *clip(const Sprite &) const;
  Sprite *sprite(upstream::FieldObjectId, std::string &);
  Voice *voice(upstream::FieldObjectId, std::string &);
  bool reserve(Voice &, std::string &);
  bool read(Voice &, int16_t *, uint32_t, uint32_t &, std::string &);
  bool fade(Voice &, uint32_t, bool replacement, std::string &);
  bool queue(Voice &, size_t, const int16_t *, uint32_t, std::string &);
  bool queued(const Voice &) const;
  void free_voice(Voice &);
  const upstream::PlayerInitializationData *data_ = nullptr;
  std::array<uint8_t, 32> ir_{};
  PodunkPlayerResources *resources_ = nullptr;
  upstream::FieldNodeTreeRuntime *tree_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  AudioPlayer *audio_ = nullptr;
  PodunkPlayerMediaHost host_;
  std::vector<uint32_t> sprites_claimed_, audio_claimed_;
  std::map<upstream::FieldObjectId, Sprite> sprites_;
  std::map<upstream::FieldObjectId, Voice> voices_;
  std::array<upstream::FieldObjectId, 2> leases_{};
};
} // namespace encore::ctr
