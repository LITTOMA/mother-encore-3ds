#include "audio_device.hpp"
#include <cmath>
#include <limits>

namespace encore::ctr {
namespace {
Result invalid_argument(int description) {
  return Result(MAKERESULT(RL_USAGE, RS_INVALIDARG, RM_DSP, description));
}
Result invalid_state() {
  return Result(
      MAKERESULT(RL_USAGE, RS_INVALIDSTATE, RM_DSP, RD_NOT_INITIALIZED));
}
bool pcm_format(uint16_t value) {
  const unsigned channels = value & 3u;
  const unsigned encoding = (value >> 2u) & 3u;
  return (channels == 1 || channels == 2) &&
         (encoding == NDSP_ENCODING_PCM8 || encoding == NDSP_ENCODING_PCM16);
}
} // namespace

bool AudioDevice::valid_channel(int channel) {
  return channel >= 0 && channel < channel_count;
}

bool AudioDevice::initialize(std::string &error) {
  if (initialized_) {
    error.clear();
    return true;
  }
  init_result_ = ndspInit();
  hardware_ = R_SUCCEEDED(init_result_);
  initialized_ = true;
  const uint64_t now = svcGetSystemTick();
  for (auto &channel : channels_) {
    channel = Channel{};
    channel.tick = now;
  }
  leases_.fill(Lease{});
  error.clear();
  return true;
}

void AudioDevice::shutdown() {
  if (!initialized_)
    return;
  for (int channel = 0; channel < channel_count; ++channel)
    clear(channel);
  if (hardware_)
    ndspExit();
  leases_.fill(Lease{});
  hardware_ = false;
  initialized_ = false;
}

void AudioDevice::remove_packet(Channel &channel, size_t index) {
  for (size_t next = index + 1; next < channel.count; ++next)
    channel.queue[next - 1] = channel.queue[next];
  channel.queue[--channel.count] = Packet{};
}

void AudioDevice::advance(Channel &channel, uint64_t now) {
  const uint64_t elapsed = now >= channel.tick ? now - channel.tick : 0;
  channel.tick = now;
  if (hardware_) {
    for (size_t index = 0; index < channel.count;) {
      const auto status = channel.queue[index].wave->status;
      if (status == NDSP_WBUF_DONE || status == NDSP_WBUF_FREE)
        remove_packet(channel, index);
      else
        ++index;
    }
    return;
  }
  // Updating tick while empty/paused discards that time. A later append or
  // unpause can never spend elapsed time from before playback was eligible.
  if (!channel.count || channel.is_paused ||
      !std::isfinite(channel.sample_rate) || channel.sample_rate <= 0)
    return;
  double frames =
      double(elapsed) * double(channel.sample_rate) / double(SYSCLOCK_ARM11);
  while (channel.count) {
    Packet &packet = channel.queue[0];
    ndspWaveBuf &wave = *packet.wave;
    wave.status = NDSP_WBUF_PLAYING;
    if (wave.looping) {
      packet.position =
          std::fmod(packet.position + frames, double(wave.nsamples));
      return;
    }
    const double remaining = double(wave.nsamples) - packet.position;
    if (frames < remaining) {
      packet.position += frames;
      return;
    }
    frames -= remaining;
    wave.status = NDSP_WBUF_DONE;
    wave.next = nullptr;
    remove_packet(channel, 0);
    // Only buffers already queued at this tick share its remainder.
    // Any unused remainder is discarded when the queue becomes empty.
  }
}

void AudioDevice::pump() {
  if (!initialized_)
    return;
  const uint64_t now = svcGetSystemTick();
  for (auto &channel : channels_)
    advance(channel, now);
}

uint32_t AudioDevice::dropped_frames() const {
  return audible() ? ndspGetDroppedFrames() : 0;
}

void AudioDevice::master_volume(float volume) {
  if (audible())
    ndspSetMasterVol(volume);
}

void AudioDevice::clear(int id) {
  if (!initialized_ || !valid_channel(id))
    return;
  Channel &channel = channels_[id];
  if (hardware_)
    ndspChnWaveBufClear(id);
  for (size_t index = 0; index < channel.count; ++index) {
    channel.queue[index].wave->status = NDSP_WBUF_DONE;
    channel.queue[index].wave->next = nullptr;
  }
  channel.queue.fill(Packet{});
  channel.count = 0;
  channel.tick = svcGetSystemTick();
}

void AudioDevice::reset(int id) {
  if (!initialized_ || !valid_channel(id))
    return;
  clear(id);
  if (hardware_)
    ndspChnReset(id);
  channels_[id] = Channel{};
  channels_[id].tick = svcGetSystemTick();
}

void AudioDevice::interp(int id, ndspInterpType interpolation) {
  if (audible() && valid_channel(id))
    ndspChnSetInterp(id, interpolation);
}

void AudioDevice::rate(int id, float sample_rate) {
  if (!initialized_ || !valid_channel(id))
    return;
  advance(channels_[id], svcGetSystemTick());
  channels_[id].sample_rate = sample_rate;
  if (hardware_)
    ndspChnSetRate(id, sample_rate);
}

void AudioDevice::format(int id, uint16_t sample_format) {
  if (!initialized_ || !valid_channel(id))
    return;
  channels_[id].sample_format = sample_format;
  if (hardware_)
    ndspChnSetFormat(id, sample_format);
}

void AudioDevice::mix(int id, const float *volumes) {
  if (audible() && valid_channel(id) && volumes)
    ndspChnSetMix(id, const_cast<float *>(volumes));
}

void AudioDevice::paused(int id, bool value) {
  if (!initialized_ || !valid_channel(id))
    return;
  advance(channels_[id], svcGetSystemTick());
  channels_[id].is_paused = value;
  if (hardware_)
    ndspChnSetPaused(id, value);
}

Result AudioDevice::flush(const void *samples, size_t bytes) {
  if (!initialized_)
    return invalid_state();
  if (!samples || !bytes)
    return invalid_argument(RD_INVALID_POINTER);
  if (bytes > std::numeric_limits<uint32_t>::max())
    return invalid_argument(RD_TOO_LARGE);
  return hardware_ ? DSP_FlushDataCache(samples, uint32_t(bytes)) : Result(0);
}

Result AudioDevice::add(int id, ndspWaveBuf *wave) {
  if (!initialized_)
    return invalid_state();
  if (!valid_channel(id))
    return invalid_argument(RD_OUT_OF_RANGE);
  // Reap/advance before examining capacity or appending new PCM. This also
  // prevents empty time accumulated before add from consuming that buffer.
  pump();
  if (!wave || !wave->data_vaddr || !wave->nsamples)
    return invalid_argument(RD_INVALID_POINTER);
  if (wave->status != NDSP_WBUF_FREE && wave->status != NDSP_WBUF_DONE)
    return invalid_argument(RD_BUSY);
  Channel &channel = channels_[id];
  if (!std::isfinite(channel.sample_rate) || channel.sample_rate <= 0 ||
      !pcm_format(channel.sample_format))
    return invalid_argument(RD_INVALID_COMBINATION);
  for (const auto &other : channels_)
    for (size_t index = 0; index < other.count; ++index)
      if (other.queue[index].wave == wave)
        return invalid_argument(RD_ALREADY_EXISTS);
  if (channel.count == queue_capacity)
    return Result(MAKERESULT(RL_TEMPORARY, RS_OUTOFRESOURCE, RM_DSP, RD_BUSY));
  channel.queue[channel.count++] = Packet{wave, 0};
  if (hardware_)
    ndspChnWaveBufAdd(id, wave);
  else {
    wave->sequence_id = ++channel.sequence;
    wave->next = nullptr;
    wave->status = NDSP_WBUF_QUEUED;
  }
  return 0;
}

bool AudioDevice::lease(const void *owner, uint64_t identity, int &out,
                        std::string &error) {
  if (!initialized_ || !owner || !identity) {
    error = "Audio device lease requires initialized device and actual owner "
            "identity";
    return false;
  }
  for (size_t index = 0; index < leases_.size(); ++index)
    if (leases_[index].owner == owner && leases_[index].identity == identity) {
      out = int(index) + fixed_channels;
      return true;
    }
  for (size_t index = 0; index < leases_.size(); ++index)
    if (!leases_[index].owner) {
      const int channel = int(index) + fixed_channels;
      reset(channel);
      leases_[index] = Lease{owner, identity};
      out = channel;
      return true;
    }
  error = "Audio device dynamic channel pool exhausted";
  return false;
}

bool AudioDevice::release(const void *owner, uint64_t identity, int channel,
                          std::string &error) {
  if (!initialized_ || !owner || !identity || channel < fixed_channels ||
      channel >= channel_count) {
    error = "Audio device release requires a valid actual dynamic lease";
    return false;
  }
  Lease &value = leases_[size_t(channel - fixed_channels)];
  if (value.owner != owner || value.identity != identity) {
    error = "Audio device release owner identity differs";
    return false;
  }
  clear(channel);
  value = Lease{};
  return true;
}
} // namespace encore::ctr
