#include "field_dialogue_audio_player.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
namespace encore::ctr {
namespace {
bool reject(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
bool FieldDialogueAudioPlayer::initialize(AudioPlayer &owner, BusGain bus,
                                          std::string &e) {
  if (owner_ || !owner.available() || !owner.checked_bank() ||
      owner.asset_root().empty() || !bus)
    return reject(
        e,
        "Dialogue native audio requires the live checked NDSP owner/bus graph");
  owner_ = &owner;
  bank_ = owner.checked_bank();
  bus_ = std::move(bus);
  leases_ = {};
  e.clear();
  return true;
}
bool FieldDialogueAudioPlayer::live(std::string &e) const {
  if (owner_ && owner_->available())
    owner_->device().pump();
  return owner_ && owner_->available() && owner_->checked_bank() == bank_ &&
                 bank_
             ? true
             : reject(e, "Dialogue native DSP/PCM owner is no longer live");
}
FieldDialogueAudioPlayer::Voice *
FieldDialogueAudioPlayer::voice(upstream::FieldObjectId id, std::string &e) {
  if (!live(e))
    return nullptr;
  auto i = voices_.find(id);
  if (i == voices_.end()) {
    reject(e, "Dialogue native audio actual voice missing");
    return nullptr;
  }
  return i->second.get();
}
bool FieldDialogueAudioPlayer::prepare(
    const upstream::AudioAsset *asset,
    std::unique_ptr<upstream::AudioPcmStream> &out, std::string &e) {
  if (!live(e))
    return false;
  if (!asset) {
    out.reset();
    return true;
  }
  upstream::AudioAsset checked;
  if (!bank_->find(asset->stable_id, checked) ||
      checked.source_path != asset->source_path ||
      checked.source_sha256 != asset->source_sha256 ||
      checked.pcm_path != asset->pcm_path ||
      checked.pcm_bytes != asset->pcm_bytes ||
      checked.pcm_crc != asset->pcm_crc ||
      checked.channels != asset->channels ||
      checked.sample_rate != asset->sample_rate ||
      checked.frames != asset->frames ||
      checked.loop_start != asset->loop_start ||
      checked.flags != asset->flags || checked.gain_db != asset->gain_db)
    return reject(e,
                  "Dialogue native stream metadata differs from checked bank");
  auto candidate = std::unique_ptr<upstream::AudioPcmStream>(
      new (std::nothrow) upstream::AudioPcmStream);
  if (!candidate)
    return reject(e, "Dialogue PCM cursor allocation failed");
  auto path = owner_->asset_root() + std::string(asset->pcm_path);
  if (!candidate->open(*asset, path.c_str(), e))
    return false;
  out = std::move(candidate);
  return true;
}
bool FieldDialogueAudioPlayer::create(upstream::FieldObjectId id,
                                      const upstream::AudioAsset *asset,
                                      std::string &e) {
  if (!live(e) || !id || voices_.count(id) || voices_.size() >= 192)
    return reject(e, "Dialogue native source voice allocation rejected");
  auto v = std::unique_ptr<Voice>(new (std::nothrow) Voice);
  if (!v)
    return reject(e, "Dialogue source voice allocation failed");
  if (!prepare(asset, v->stream, e))
    return false;
  if (asset)
    v->asset = *asset;
  v->samples = static_cast<int16_t *>(
      linearAlloc(buffer_count * buffer_frames * 2 * sizeof(int16_t)));
  if (!v->samples)
    return reject(e, "Dialogue native linear wave allocation failed");
  std::memset(v->samples, 0,
              buffer_count * buffer_frames * 2 * sizeof(int16_t));
  voices_.emplace(id, std::move(v));
  return true;
}
bool FieldDialogueAudioPlayer::prepare_assign(upstream::FieldObjectId id,
                                              const upstream::AudioAsset *asset,
                                              std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  if (asset && (queued(*v) || v->tail_frames) && v->asset.stable_id &&
      (asset->channels != v->asset.channels ||
       asset->sample_rate != v->asset.sample_rate))
    return reject(e, "Dialogue queued native stream format change awaits its "
                     "real hardware boundary");
  std::unique_ptr<upstream::AudioPcmStream> candidate;
  if (!prepare(asset, candidate, e))
    return false;
  v->candidate = std::move(candidate);
  v->candidate_asset = asset ? *asset : upstream::AudioAsset{};
  v->candidate_ready = true;
  return true;
}
bool FieldDialogueAudioPlayer::assign(upstream::FieldObjectId id,
                                      const upstream::AudioAsset *asset,
                                      std::string &e) {
  auto *v = voice(id, e);
  if (!v || !v->candidate_ready ||
      (asset ? asset->stable_id : 0) != v->candidate_asset.stable_id)
    return reject(e, "Dialogue stream assignment has no checked pre-instance");
  v->stream = std::move(v->candidate);
  if (asset)
    v->asset = *asset;
  else if (!queued(*v) && !v->tail_frames)
    v->asset = {};
  v->candidate_ready = false;
  v->candidate_asset = {};
  v->decoder_playing = false;
  v->position = 0;
  return true;
}
bool FieldDialogueAudioPlayer::reserve(upstream::FieldObjectId id,
                                       std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  if (v->channel >= 0)
    return true;
  for (size_t i = 0; i < leases_.size(); ++i)
    if (!leases_[i]) {
      int channel = -1;
      if (!owner_->device().lease(this, id, channel, e))
        return false;
      leases_[i] = id;
      v->channel = channel;
      owner_->device().reset(v->channel);
      float volumes[12]{};
      volumes[0] = volumes[1] = 1;
      owner_->device().mix(v->channel, volumes);
      owner_->device().interp(v->channel, NDSP_INTERP_POLYPHASE);
      return true;
    }
  return reject(e, "Dialogue hardware capacity exceeded: both independent "
                   "native channels are occupied");
}
bool FieldDialogueAudioPlayer::registered(upstream::FieldObjectId id,
                                          bool registered_, std::string &e) {
  auto *v = voice(id, e);
  if (!v || v->registered == registered_)
    return reject(e, "Dialogue actual AudioServer registration order rejected");
  v->registered = registered_;
  return true;
}
bool FieldDialogueAudioPlayer::mix_due(upstream::FieldObjectId id, bool &due,
                                       std::string &e) {
  auto *v = voice(id, e);
  if (!v || !v->registered)
    return reject(e,
                  "Dialogue mixer outside registered native source callback");
  due = false;
  if (v->channel < 0) {
    due = true;
    return true;
  }
  for (const auto &w : v->waves)
    if (w.status != NDSP_WBUF_QUEUED && w.status != NDSP_WBUF_PLAYING) {
      due = true;
      break;
    }
  if (due && (!v->decoder_playing || v->paused || !v->stream))
    return pump(*v, e);
  return true;
}
void FieldDialogueAudioPlayer::add_tail(Voice &v, int16_t *out,
                                        uint32_t frames) {
  uint32_t count = std::min(v.tail_frames, frames);
  for (size_t i = 0; i < size_t(count) * v.asset.channels; ++i) {
    int sum = int(out[i]) + v.tail[i];
    out[i] = int16_t(std::clamp(sum, -32768, 32767));
  }
  v.tail.erase(v.tail.begin(),
               v.tail.begin() + size_t(count) * v.asset.channels);
  v.tail_frames -= count;
}
bool FieldDialogueAudioPlayer::observe(
    upstream::FieldObjectId id, upstream::FieldDialogueAudioObservation &out,
    std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  out.actual_voice = v->samples != nullptr;
  out.stream_checked = !v->stream || v->stream->is_open();
  out.mix_registered = v->registered;
  out.playback_playing = v->decoder_playing;
  out.position =
      v->asset.sample_rate ? double(v->position) / v->asset.sample_rate : 0;
  return true;
}
bool FieldDialogueAudioPlayer::start(upstream::FieldObjectId id, double seconds,
                                     float pitch, std::string &e) {
  auto *v = voice(id, e);
  if (!v || !v->stream || !std::isfinite(seconds) || seconds < 0 ||
      !std::isfinite(pitch) || pitch <= 0 || pitch > 4)
    return reject(e, "Dialogue actual native playback start rejected");
  double frame = seconds * v->asset.sample_rate;
  if (frame >= v->asset.frames)
    frame = 0;
  if (frame < 0 || frame > UINT32_MAX)
    return reject(e, "Dialogue native seek conversion outside checked bounds");
  auto position = uint32_t(frame);
  if (!reserve(id, e))
    return false;
  if (!v->stream->seek(position))
    return reject(e, "Dialogue native PCM seek failed");
  v->position = position;
  v->decoder_playing = true;
  v->pitch = pitch;
  v->paused = false;
  owner_->device().paused(v->channel, false);
  owner_->device().rate(v->channel, float(v->asset.sample_rate) * pitch);
  owner_->device().format(v->channel, v->asset.channels == 2 ? NDSP_FORMAT_STEREO_PCM16
                                                      : NDSP_FORMAT_MONO_PCM16);
  return true;
}
bool FieldDialogueAudioPlayer::stop(upstream::FieldObjectId id,
                                    std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  v->decoder_playing = false;
  return true;
}
bool FieldDialogueAudioPlayer::pause(upstream::FieldObjectId id, bool paused_,
                                     std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  if (!paused_ && v->decoder_playing && !reserve(id, e))
    return false;
  v->paused = paused_;
  if (v->channel >= 0)
    owner_->device().paused(v->channel, false);
  // Paused fade is invoked by the actual native mixer, and belongs to this
  // audio block. The outer frame pump never submits pending source fades.
  return paused_ ? pump(*v, e) : true;
}
bool FieldDialogueAudioPlayer::queued(const Voice &v) const {
  if (owner_ && owner_->available())
    owner_->device().pump();
  for (const auto &w : v.waves)
    if (w.status == NDSP_WBUF_QUEUED || w.status == NDSP_WBUF_PLAYING)
      return true;
  return false;
}
void FieldDialogueAudioPlayer::advance_position(Voice &v, uint32_t frames) {
  if (!v.asset.loops()) {
    v.position = uint32_t(
        std::min(uint64_t(v.asset.frames), uint64_t(v.position) + frames));
    return;
  }
  uint64_t total = uint64_t(v.position) + frames;
  if (total <= v.asset.frames) {
    v.position = uint32_t(total);
    return;
  }
  uint32_t period = v.asset.frames - v.asset.loop_start;
  uint32_t remainder = uint32_t((total - v.asset.frames) % period);
  v.position = remainder ? v.asset.loop_start + remainder : v.asset.frames;
}
void FieldDialogueAudioPlayer::gain(int16_t *samples, uint32_t frames,
                                    uint16_t channels, float from, float to,
                                    bool muted) {
  float a = muted ? 0 : upstream::audio_linear_gain(from),
        b = muted ? 0 : upstream::audio_linear_gain(to);
  float step = frames ? (b - a) / frames : 0;
  for (uint32_t i = 0; i < frames; ++i) {
    for (uint16_t c = 0; c < channels; ++c) {
      float sample = samples[size_t(i) * channels + c] * a;
      sample = std::clamp(sample, -32768.0f, 32767.0f);
      samples[size_t(i) * channels + c] = int16_t(std::lrint(sample));
    }
    a += step;
  }
}
bool FieldDialogueAudioPlayer::fade(upstream::FieldObjectId id, uint32_t frames,
                                    float silence, bool replacement,
                                    std::string &e) {
  auto *v = voice(id, e);
  if (!v || !frames || frames > 4096 || !std::isfinite(silence))
    return reject(e, "Dialogue native fade request rejected");
  if (!v->stream)
    return true;
  std::vector<int16_t> tail(size_t(frames) * v->asset.channels);
  uint32_t got = 0;
  if (v->decoder_playing) {
    if (!v->stream->read(tail.data(), frames, got, e))
      return false;
    advance_position(*v, got);
  }
  gain(tail.data(), frames, v->asset.channels,
       v->volume + v->asset.gain_db + v->bus_db,
       silence + v->asset.gain_db + v->bus_db, v->bus_muted);
  // set_stream replaces its single fade buffer; stop/seek add into the
  // beginning of the same next hardware block.
  if (replacement) {
    v->tail = std::move(tail);
    v->tail_frames = frames;
  } else {
    auto count = std::max(v->tail_frames, frames);
    v->tail.resize(size_t(count) * v->asset.channels, 0);
    for (size_t i = 0; i < tail.size(); ++i) {
      int sum = int(v->tail[i]) + tail[i];
      v->tail[i] = int16_t(std::clamp(sum, -32768, 32767));
    }
    v->tail_frames = count;
  }
  return true;
}
bool FieldDialogueAudioPlayer::queue(Voice &v, ndspWaveBuf &wave, size_t slot,
                                     uint32_t frames, std::string &e) {
  wave = {};
  wave.data_pcm16 = v.samples + slot * buffer_frames * 2;
  wave.nsamples = frames;
  wave.looping = false;
  auto result = owner_->device().flush(wave.data_pcm16,
                                   frames * v.asset.channels * sizeof(int16_t));
  if (R_FAILED(result))
    return reject(e, "Dialogue native DSP cache flush failed");
  if (R_FAILED(owner_->device().add(v.channel, &wave)))
    return reject(e, "Dialogue native audio device wave queue failed");
  return true;
}
bool FieldDialogueAudioPlayer::pump(Voice &v, std::string &e) {
  owner_->device().pump();
  if (v.channel < 0)
    return true;
  for (size_t i = 0; i < v.waves.size(); ++i) {
    auto &w = v.waves[i];
    if (w.status == NDSP_WBUF_QUEUED || w.status == NDSP_WBUF_PLAYING)
      continue;
    if (v.tail_frames) {
      auto count = std::min<uint32_t>(v.tail_frames, buffer_frames);
      std::copy_n(v.tail.data(), size_t(count) * v.asset.channels,
                  v.samples + i * buffer_frames * 2);
      if (!queue(v, w, i, count, e))
        return false;
      v.tail.erase(v.tail.begin(),
                   v.tail.begin() + size_t(count) * v.asset.channels);
      v.tail_frames -= count;
    }
  }
  return true;
}
bool FieldDialogueAudioPlayer::mix(upstream::FieldObjectId id, float from,
                                   float to, float pitch, std::string_view bus,
                                   std::string &e) {
  auto *v = voice(id, e);
  if (!v || !std::isfinite(from) || !std::isfinite(to) ||
      !std::isfinite(pitch) || pitch <= 0 || pitch > 4)
    return reject(e, "Dialogue native mix request rejected");
  float bus_db = 0;
  bool muted = false;
  if (!bus_(bus, bus_db, muted, e))
    return false;
  if (!std::isfinite(bus_db) || bus_db < -120 || bus_db > 24)
    return reject(e, "Dialogue actual bus gain outside checked bounds");
  v->bus_db = bus_db;
  v->bus_muted = muted;
  v->pitch = pitch;
  if (!v->decoder_playing || v->paused || !v->stream)
    return pump(*v, e);
  if (!reserve(id, e))
    return false;
  owner_->device().rate(v->channel, float(v->asset.sample_rate) * pitch);
  float previous = from;
  for (size_t i = 0; i < v->waves.size(); ++i) {
    auto &w = v->waves[i];
    if (w.status == NDSP_WBUF_QUEUED || w.status == NDSP_WBUF_PLAYING)
      continue;
    if (!v->decoder_playing)
      break;
    auto *samples = v->samples + i * buffer_frames * 2;
    std::memset(samples, 0, buffer_frames * 2 * sizeof(int16_t));
    uint32_t got = 0;
    if (!v->stream->read(samples, buffer_frames, got, e))
      return false;
    advance_position(*v, got);
    if (got < buffer_frames && !v->asset.loops())
      v->decoder_playing = false;
    gain(samples, buffer_frames, v->asset.channels,
         previous + v->asset.gain_db + bus_db, to + v->asset.gain_db + bus_db,
         muted);
    previous = to;
    add_tail(*v, samples, buffer_frames);
    if (!queue(*v, w, i, buffer_frames, e))
      return false;
    break;
  }
  v->volume = to;
  return true;
}
void FieldDialogueAudioPlayer::give_back(upstream::FieldObjectId id, Voice &v) {
  if (v.channel < 0)
    return;
  owner_->device().clear(v.channel);
  std::string error;
  owner_->device().release(this, id, v.channel, error);
  for (auto &lease : leases_)
    if (lease == id)
      lease = 0;
  v.channel = -1;
  v.waves = {};
}
bool FieldDialogueAudioPlayer::pump_streams(std::string &e) {
  if (!live(e))
    return false;
  for (auto &entry : voices_) {
    auto &v = *entry.second;
    if (v.channel >= 0 && !queued(v) && !v.tail_frames &&
        (!v.decoder_playing || v.paused))
      give_back(entry.first, v);
  }
  return true;
}
bool FieldDialogueAudioPlayer::release(upstream::FieldObjectId id,
                                       std::string &e) {
  auto *v = voice(id, e);
  if (!v || v->registered)
    return reject(
        e, "Dialogue native voice release before actual AudioServer removal");
  give_back(id, *v);
  if (v->samples)
    linearFree(v->samples);
  v->samples = nullptr;
  voices_.erase(id);
  return true;
}
void FieldDialogueAudioPlayer::shutdown() {
  for (auto &entry : voices_) {
    auto &v = *entry.second;
    if (owner_ && owner_->available())
      give_back(entry.first, v);
    if (v.samples)
      linearFree(v.samples);
    v.samples = nullptr;
  }
  voices_.clear();
  leases_ = {};
  owner_ = nullptr;
  bank_ = nullptr;
  bus_ = {};
}
upstream::FieldDialogueAudioBackend FieldDialogueAudioPlayer::backend() {
  upstream::FieldDialogueAudioBackend b;
  b.bank = owner_ && owner_->available() ? bank_ : nullptr;
  b.create = [this](auto id, const auto *a, std::string &e) {
    return create(id, a, e);
  };
  b.prepare_assign = [this](auto id, const auto *a, std::string &e) {
    return prepare_assign(id, a, e);
  };
  b.assign = [this](auto id, const auto *a, std::string &e) {
    return assign(id, a, e);
  };
  b.register_mix = [this](auto id, bool on, std::string &e) {
    return registered(id, on, e);
  };
  b.mix_due = [this](auto id, bool &due, std::string &e) {
    return mix_due(id, due, e);
  };
  b.observe = [this](auto id, auto &o, std::string &e) {
    return observe(id, o, e);
  };
  b.fade_stop = [this](auto id, uint32_t n, float db, bool replacement,
                       std::string &e) {
    return fade(id, n, db, replacement, e);
  };
  b.start = [this](auto id, double pos, float pitch, std::string &e) {
    return start(id, pos, pitch, e);
  };
  b.stop = [this](auto id, std::string &e) { return stop(id, e); };
  b.pause = [this](auto id, bool on, std::string &e) {
    return pause(id, on, e);
  };
  b.mix = [this](auto id, float from, float to, float pitch,
                 std::string_view bus,
                 std::string &e) { return mix(id, from, to, pitch, bus, e); };
  b.release = [this](auto id, std::string &e) { return release(id, e); };
  b.preflight_play = [this](auto id, std::string &e) { return reserve(id, e); };
  return b;
}
} // namespace encore::ctr
