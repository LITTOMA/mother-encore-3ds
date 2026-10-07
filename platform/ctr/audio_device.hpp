#pragma once
#include <3ds.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace encore::ctr {
// One output device for all checked PCM consumers. A missing DSP component
// selects a wall-clock sink; it never approves missing or invalid audio assets.
class AudioDevice {
public:
  static constexpr int channel_count = 24;
  static constexpr int fixed_channels = 6;
  static constexpr size_t queue_capacity = 3;

  AudioDevice() = default;
  ~AudioDevice() { shutdown(); }
  AudioDevice(const AudioDevice &) = delete;
  AudioDevice &operator=(const AudioDevice &) = delete;
  bool initialize(std::string &);
  void shutdown();
  bool available() const { return initialized_; }
  bool audible() const { return initialized_ && hardware_; }
  Result init_result() const { return init_result_; }
  void pump();
  uint32_t dropped_frames() const;
  void master_volume(float);
  void reset(int);
  void clear(int);
  void interp(int, ndspInterpType);
  void rate(int, float);
  void format(int, uint16_t);
  void mix(int, const float *);
  void paused(int, bool);
  Result flush(const void *, size_t);
  Result add(int, ndspWaveBuf *);
  bool lease(const void *owner, uint64_t identity, int &channel, std::string &);
  bool release(const void *owner, uint64_t identity, int channel,
               std::string &);

private:
  struct Packet {
    ndspWaveBuf *wave = nullptr;
    double position = 0;
  };
  struct Channel {
    std::array<Packet, queue_capacity> queue{};
    size_t count = 0;
    uint64_t tick = 0;
    float sample_rate = float(NDSP_SAMPLE_RATE);
    uint16_t sample_format = NDSP_FORMAT_MONO_PCM16;
    uint16_t sequence = 0;
    bool is_paused = false;
  };
  struct Lease {
    const void *owner = nullptr;
    uint64_t identity = 0;
  };
  static bool valid_channel(int);
  void advance(Channel &, uint64_t);
  void remove_packet(Channel &, size_t);
  std::array<Channel, channel_count> channels_{};
  std::array<Lease, channel_count - fixed_channels> leases_{};
  Result init_result_ = -1;
  bool initialized_ = false;
  bool hardware_ = false;
};
} // namespace encore::ctr
