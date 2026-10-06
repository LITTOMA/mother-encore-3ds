#pragma once
#include "audio_player.hpp"
#include "encore/field_dialogue_audio.hpp"
#include <map>
#include <memory>
namespace encore::ctr {
// Independent native AudioStreamPlayer voices. Owns real PCM cursors and wave
// queues; never initializes another DSP service or steals an existing lane.
class FieldDialogueAudioPlayer {
public:
  using BusGain = std::function<bool(std::string_view, float &db, bool &muted,
                                     std::string &)>;
  FieldDialogueAudioPlayer() = default;
  ~FieldDialogueAudioPlayer() { shutdown(); }
  FieldDialogueAudioPlayer(const FieldDialogueAudioPlayer &) = delete;
  FieldDialogueAudioPlayer &
  operator=(const FieldDialogueAudioPlayer &) = delete;
  bool initialize(AudioPlayer &, BusGain, std::string &);
  upstream::FieldDialogueAudioBackend backend();
  // Existing frame/loading pump only reclaims completed hardware leases.
  // Pending source fades are submitted by registered audio callbacks, never
  // by a second game clock. This pump never emits finished.
  bool pump_streams(std::string &);
  void shutdown();

private:
  static constexpr size_t buffer_count = 3, buffer_frames = 1024;
  struct Voice {
    upstream::AudioAsset asset{}, candidate_asset{};
    std::unique_ptr<upstream::AudioPcmStream> stream, candidate;
    std::array<ndspWaveBuf, buffer_count> waves{};
    int16_t *samples = nullptr;
    std::vector<int16_t> tail;
    uint32_t tail_frames = 0, position = 0;
    int channel = -1;
    bool candidate_ready = false, registered = false, decoder_playing = false,
         paused = false;
    float pitch = 1, volume = 0, bus_db = 0;
    bool bus_muted = false;
  };
  AudioPlayer *owner_ = nullptr;
  const upstream::AudioBank *bank_ = nullptr;
  BusGain bus_;
  std::map<upstream::FieldObjectId, std::unique_ptr<Voice>> voices_;
  std::array<upstream::FieldObjectId, 2> leases_{};
  bool live(std::string &) const;
  Voice *voice(upstream::FieldObjectId, std::string &);
  bool prepare(const upstream::AudioAsset *,
               std::unique_ptr<upstream::AudioPcmStream> &, std::string &);
  bool create(upstream::FieldObjectId, const upstream::AudioAsset *,
              std::string &);
  bool prepare_assign(upstream::FieldObjectId, const upstream::AudioAsset *,
                      std::string &);
  bool assign(upstream::FieldObjectId, const upstream::AudioAsset *,
              std::string &);
  bool reserve(upstream::FieldObjectId, std::string &);
  bool registered(upstream::FieldObjectId, bool, std::string &);
  bool mix_due(upstream::FieldObjectId, bool &, std::string &);
  bool observe(upstream::FieldObjectId,
               upstream::FieldDialogueAudioObservation &, std::string &);
  bool start(upstream::FieldObjectId, double, float, std::string &);
  bool stop(upstream::FieldObjectId, std::string &);
  bool pause(upstream::FieldObjectId, bool, std::string &);
  bool fade(upstream::FieldObjectId, uint32_t, float, bool, std::string &);
  bool mix(upstream::FieldObjectId, float, float, float, std::string_view,
           std::string &);
  bool release(upstream::FieldObjectId, std::string &);
  bool pump(Voice &, std::string &);
  bool queue(Voice &, ndspWaveBuf &, size_t, uint32_t, std::string &);
  bool queued(const Voice &) const;
  void add_tail(Voice &, int16_t *, uint32_t);
  void give_back(upstream::FieldObjectId, Voice &);
  void advance_position(Voice &, uint32_t);
  static void gain(int16_t *, uint32_t, uint16_t, float, float, bool);
};
} // namespace encore::ctr
