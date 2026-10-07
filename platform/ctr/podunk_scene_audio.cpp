#include "podunk_scene_audio.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
namespace encore::ctr {
using namespace upstream;
namespace {
inline uint32_t rotate(uint32_t v, unsigned n) {
  return (v >> n) | (v << (32 - n));
}
// Integrity machinery only. The digest binds converted tex3ds bytes to the
// checked resource; all sprite pixels, grids and transforms come from data.
inline std::array<uint8_t, 32> sha256(const uint8_t *data, size_t size) {
  static constexpr uint32_t constants[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const size_t blocks = (size + 9 + 63) / 64;
  for (size_t block = 0; block < blocks; ++block) {
    uint8_t raw[64]{};
    for (size_t j = 0; j < 64; ++j) {
      const size_t at = block * 64 + j;
      if (at < size)
        raw[j] = data[at];
      else if (at == size)
        raw[j] = 0x80;
    }
    if (block + 1 == blocks) {
      const uint64_t bits = uint64_t(size) * 8;
      for (unsigned j = 0; j < 8; ++j)
        raw[63 - j] = uint8_t(bits >> (j * 8));
    }
    uint32_t w[64];
    for (unsigned j = 0; j < 16; ++j)
      w[j] = uint32_t(raw[j * 4]) << 24 | uint32_t(raw[j * 4 + 1]) << 16 |
             uint32_t(raw[j * 4 + 2]) << 8 | raw[j * 4 + 3];
    for (unsigned j = 16; j < 64; ++j) {
      const auto a = w[j - 15], b = w[j - 2];
      w[j] = w[j - 16] + (rotate(a, 7) ^ rotate(a, 18) ^ (a >> 3)) + w[j - 7] +
             (rotate(b, 17) ^ rotate(b, 19) ^ (b >> 10));
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5],
             g = h[6], v = h[7];
    for (unsigned j = 0; j < 64; ++j) {
      const auto t1 = v + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) +
                      ((e & f) ^ (~e & g)) + constants[j] + w[j],
                 t2 = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) +
                      ((a & b) ^ (a & c) ^ (b & c));
      v = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += v;
  }
  std::array<uint8_t, 32> out{};
  for (unsigned i = 0; i < 8; ++i)
    for (unsigned j = 0; j < 4; ++j)
      out[i * 4 + j] = uint8_t(h[i] >> (24 - j * 8));
  return out;
}

PodunkSceneAudio *channel_owner = nullptr;
bool fail(std::string &e, const char *m) {
  e = m;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
bool number(const FieldDeferredValue &v, double &out) {
  if (auto *p = std::get_if<double>(&v))
    out = *p;
  else if (auto *p = std::get_if<int64_t>(&v))
    out = double(*p);
  else
    return false;
  return std::isfinite(out);
}
Vec2 transform(const FieldTransform &t, Vec2 p) {
  return {t[0].x * p.x + t[1].x * p.y + t[2].x,
          t[0].y * p.x + t[1].y * p.y + t[2].y};
}
int16_t sample(float x) {
  return int16_t(std::lrint(std::clamp(x, -32768.f, 32767.f)));
}
} // namespace
PodunkSceneAudio::~PodunkSceneAudio() {
  std::string e;
  shutdown(e);
}
bool PodunkSceneAudio::live(std::string &e) const {
  return ((data_ && data_->valid() && data_->ir_sha() == ir_ && channel_owner == this) ||
                  (named_ && named_->valid() && named_->ir_sha256() == ir_)) && tree_ &&
                 registry_ && audio_ && audio_->available() &&
                 registry_->data()
             ? true
             : fail(e, "Scene audio actual DSP/source owner unavailable");
}
bool PodunkSceneAudio::prepare(const FieldSceneAudioData &d,
                               FieldNodeTreeRuntime &t, FieldGlobalRegistry &r,
                               AudioPlayer &a, PodunkSceneAudioHost h,
                               std::string &e) {
  if (data_ || named_ || !d.valid() || !a.available() ||
      (channel_owner && channel_owner != this) ||
      !h.server.add_audio_callback || !h.server.remove_audio_callback ||
      !h.server.bus_gain || !h.server.connect_bus_layout ||
      !h.server.disconnect_bus_layout || !h.server.emit)
    return fail(e, "Scene audio checked sources/server ownership absent");
  std::string path = a.asset_root() + d.scene_bank();
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f || f.tellg() < 64 || f.tellg() > 65536)
    return fail(e, "Scene audio bank unavailable or oversized");
  std::vector<uint8_t> b(static_cast<size_t>(f.tellg()));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), b.size()) ||
      sha256(b.data(), b.size()) != d.bank_sha())
    return fail(e, "Scene audio bank source fingerprint");
  auto bank = std::make_shared<AudioBank>();
  if (!bank->load(b.data(), b.size(), e))
    return false;
  // Check all metadata first, then publish this additional immutable bank.
  for (const auto &s : d.streams()) {
    AudioAsset v;
    if (!(s.bank == 2
              ? bank->find(s.asset_id, v)
              : (a.checked_bank() && a.checked_bank()->find(s.asset_id, v))) ||
        v.source_sha256 != s.source_sha || v.source_path != "res://" + s.source)
      return fail(e, "Scene audio explicit bank/asset/source binding");
  }
  uint32_t present = 0;
  for (uint32_t i = 0; i < bank->count(); ++i) {
    AudioAsset actual, expected = bank->asset(i);
    std::string absent;
    if (!a.checked_asset(expected.stable_id, actual, absent))
      continue;
    if (actual.source_sha256 != expected.source_sha256 ||
        actual.source_path != expected.source_path ||
        actual.pcm_path != expected.pcm_path ||
        actual.sample_rate != expected.sample_rate ||
        actual.channels != expected.channels ||
        actual.flags != expected.flags || actual.frames != expected.frames ||
        actual.loop_start != expected.loop_start ||
        actual.pcm_bytes != expected.pcm_bytes ||
        actual.pcm_crc != expected.pcm_crc ||
        actual.gain_db != expected.gain_db)
      return fail(e, "Scene audio existing bank metadata differs");
    ++present;
  }
  if (present && present != bank->count())
    return fail(e, "Scene audio partial bank identity overlap");
  if (!present && !a.include_bank(bank, e))
    return false;
  data_ = &d;
  ir_ = d.ir_sha();
  tree_ = &t;
  registry_ = &r;
  audio_ = &a;
  host_ = std::move(h);
  bank_ = std::move(bank);
  channel_owner = this;
  tree_paused_ = false;
  return true;
}
bool PodunkSceneAudio::prepare_named(const PlayerNamedSfxData &d,
                                     FieldNodeTreeRuntime &t,
                                     FieldGlobalRegistry &r, AudioPlayer &a,
                                     PodunkPlayerMediaHost h, std::string &e) {
  if (data_ || named_ || !d.valid() || !a.available() ||
      t.object_domain() != r.kernel() || !h.add_audio_callback ||
      !h.remove_audio_callback || !h.bus_gain || !h.connect_bus_layout ||
      !h.disconnect_bus_layout || !h.emit)
    return fail(e,
                "Named SFX checked actual AudioServer/source ownership absent");
  std::ifstream f(a.asset_root() + d.bank_path(),
                  std::ios::binary | std::ios::ate);
  if (!f || f.tellg() < 64 || f.tellg() > 65536)
    return fail(e, "Named SFX bank missing/oversized");
  std::vector<uint8_t> b(static_cast<size_t>(f.tellg()));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), b.size()) ||
      sha256(b.data(), b.size()) != d.bank_sha256())
    return fail(e, "Named SFX bank fingerprint mismatch");
  auto bank = std::make_shared<AudioBank>();
  if (!bank->load(b.data(), b.size(), e))
    return false;
  for (auto &v : d.streams()) {
    AudioAsset asset;
    if (!bank->find(v.audio.asset_id, asset) &&
        !a.checked_asset(v.audio.asset_id, asset, e))
      return false;
    if (asset.source_path != "res://" + v.audio.source ||
        asset.source_sha256 != v.audio.source_sha)
      return fail(e, "Named SFX actual stream bank/source mismatch");
  }
  uint32_t present = 0;
  for (uint32_t i = 0; i < bank->count(); ++i) {
    AudioAsset asset;
    std::string absent;
    auto expected = bank->asset(i);
    if (!a.checked_asset(expected.stable_id, asset, absent))
      continue;
    if (asset.source_path != expected.source_path ||
        asset.source_sha256 != expected.source_sha256 ||
        asset.pcm_path != expected.pcm_path ||
        asset.pcm_crc != expected.pcm_crc || asset.frames != expected.frames ||
        asset.flags != expected.flags ||
        asset.sample_rate != expected.sample_rate ||
        asset.channels != expected.channels ||
        asset.loop_start != expected.loop_start ||
        asset.pcm_bytes != expected.pcm_bytes ||
        asset.gain_db != expected.gain_db)
      return fail(e, "Named SFX existing converted asset changed");
    ++present;
  }
  if (present && present != bank->count())
    return fail(e, "Named SFX partial bank overlap");
  if (!present && !a.include_bank(bank, e))
    return false;
  named_ = &d;
  ir_ = d.ir_sha256();
  tree_ = &t;
  registry_ = &r;
  audio_ = &a;
  host_.server = std::move(h);
  bank_ = std::move(bank);
  return true;
}
const FieldSceneAudioStream *
PodunkSceneAudio::stream_binding(uint32_t id) const {
  if (data_)
    return data_->stream(id);
  auto *v = named_ ? named_->stream(id) : nullptr;
  return v ? &v->audio : nullptr;
}
bool PodunkSceneAudio::construct_named(FieldObjectId id,
                                       const FieldNodeDescriptor &n,
                                       const FieldIdentity &identity,
                                       std::string &e) {
  if (!named_ || !live(e) || !owns(n) ||
      !same(identity, named_->voice_identity()) || owns(id) ||
      registry_->tree_owner(id).get() != tree_ || !registry_->object_exists(id))
    return fail(e, "Named SFX actual .new native object/source differs");
  return construct_voice(id, named_->prototype(), e);
}
bool PodunkSceneAudio::owns(const FieldNodeDescriptor &n) const {
  if(named_)return n.id==named_->voice_descriptor().id && n.native_class==named_->voice_descriptor().native_class && n.script.empty() && n.class_index==named_->voice_descriptor().class_index;
  auto *s = data_ ? data_->node(n.id) : nullptr;
  return s && n.path == s->path && n.script.empty();
}
bool PodunkSceneAudio::owns(FieldObjectId id) const {
  return voices_.count(id) != 0;
}
PodunkSceneAudio::Voice *PodunkSceneAudio::voice(FieldObjectId id,
                                                 std::string &e) {
  if (!live(e))
    return nullptr;
  auto i = voices_.find(id);
  auto *s = tree_->state(id);
  if (i == voices_.end() || !s || !s->alive ||
      registry_->tree_owner(id).get() != tree_) {
    fail(e, "Scene audio same live ObjectDB/Tree node absent");
    return nullptr;
  }
  return &i->second;
}
bool PodunkSceneAudio::asset(uint32_t source, AudioAsset &out,
                             std::string &e) const {
  auto *s = stream_binding(source);
  if (!s)
    return fail(e, "Scene audio unknown source stream");
  if (!audio_->checked_asset(s->asset_id, out, e) ||
      out.source_sha256 != s->source_sha ||
      out.source_path != "res://" + s->source)
    return fail(e, "Scene audio actual immutable bank asset changed");
  return true;
}
bool PodunkSceneAudio::construct(FieldObjectId id, const FieldNodeDescriptor &n,
                                 const FieldIdentity &identity,
                                 std::string &e) {
  if (!live(e) || !owns(n) || !same(identity, data_->identity()) || owns(id) ||
      !registry_->object_exists(id) || registry_->tree_owner(id).get() != tree_)
    return fail(e, "Scene audio native allocated/source constructor mismatch");
  return construct_voice(id,*data_->node(n.id),e);
}
bool PodunkSceneAudio::construct_voice(FieldObjectId id,const FieldSceneAudioNode&source,std::string&e){
  const auto *s=&source;
  Voice v;
  v.source = s;
  v.state.object = id;
  v.state.source_stream = s->stream;
  v.state.volume_db = s->volume_db;
  v.state.pitch = s->pitch;
  v.state.bus = s->bus;
  v.state.paused = s->paused;
  v.mix_volume = s->volume_db;
  if (s->stream && !asset(s->stream, v.asset, e))
    return false;
  float db = 0;
  bool mute = false;
  if (!host_.server.bus_gain(s->bus, db, mute, e) || !std::isfinite(db))
    return false;
  v.callback = {id, registry_, this, [](void *owner, upstream::FieldObjectId object, std::string &error) {
    return static_cast<PodunkSceneAudio *>(owner)->mix(object, error);
  }};
  voices_.emplace(id, std::move(v));
  if (!host_.server.connect_bus_layout(
          id,
          [this, id] {
            std::string e;
            auto *v = voice(id, e);
            float db = 0;
            bool mute = false;
            return v && host_.server.bus_gain(v->state.bus, db, mute, e) &&
                   std::isfinite(db);
          },
          e)) {
    voices_.erase(id);
    return false;
  }
  return true;
}
bool PodunkSceneAudio::bind(FieldObjectId id, FieldNodeBinding &b,
                            std::string &e) {
  auto *v = voice(id, e);
  auto *n = tree_->descriptor(id);
  if (!v || !n || n->id != v->source->id || !owns(*n))
    return fail(e, "Scene audio actual node binding");
  b.identity = data_?data_->identity():named_->voice_identity();
  b.stable_id = n->id;
  b.class_index = n->class_index;
  b.script_sha = n->script_sha;
  b.native_class =
      v->source->kind == 1 ? "AudioStreamPlayer" : "AudioStreamPlayer2D";
  b.family = data_?0x454e0067:0x454e0073;
  b.capability = 1;
  return true;
}
bool PodunkSceneAudio::internal(Voice &v, bool enabled, std::string &e) {
  const char *group = v.source->kind == 1 ? "idle_process_internal"
                                          : "physics_process_internal";
  if (enabled == v.internal)
    return true;
  if (!(enabled ? tree_->add_group(v.state.object, group, e)
                : tree_->remove_group(v.state.object, group, e)))
    return false;
  v.internal = enabled;
  return true;
}
bool PodunkSceneAudio::queued(const Voice &v) const {
  for (const auto &w : v.waves)
    if (w.status == NDSP_WBUF_QUEUED || w.status == NDSP_WBUF_PLAYING)
      return true;
  return false;
}
void PodunkSceneAudio::release_channel(Voice &v) {
  if (v.channel >= 0 && audio_ && audio_->available())
    ndspChnWaveBufClear(v.channel);
  if (v.channel >= 0 && audio_ && audio_->available()) {
    std::string error;
    audio_->release_native_channel(v.state.object, this, v.channel, error);
  }
  for (auto &x : leases_)
    if (x == v.state.object)
      x = 0;
  v.channel = -1;
  v.waves = {};
  if (v.samples)
    linearFree(v.samples);
  v.samples = nullptr;
}
void PodunkSceneAudio::free_voice(Voice &v) {
  release_channel(v);
  v.pcm.reset();
  v.tail.clear();
}
bool PodunkSceneAudio::reserve(Voice &v, std::string &e) {
  if (v.channel >= 0)
    return true;
  size_t i = 0;
  for (; i < leases_.size(); ++i)
    if (!leases_[i])
      break;
  if (i == leases_.size())
    return fail(e, "Scene audio six NDSP voices occupied; playback rejected");
  auto *p = static_cast<int16_t *>(
      linearAlloc(buffer_count * block_frames * 2 * sizeof(int16_t)));
  if (!p)
    return fail(e, "Scene audio actual DSP allocation failed");
  int channel = -1;
  if (!audio_->lease_native_channel(v.state.object, this, channel, e)) {
    linearFree(p);
    return false;
  }
  v.samples = p;
  leases_[i] = v.state.object;
  v.channel = channel;
  ndspChnReset(v.channel);
  ndspChnSetFormat(v.channel, NDSP_FORMAT_STEREO_PCM16);
  ndspChnSetInterp(v.channel, NDSP_INTERP_POLYPHASE);
  float gains[12]{};
  gains[0] = gains[1] = 1;
  ndspChnSetMix(v.channel, gains);
  return true;
}
bool PodunkSceneAudio::read(Voice &v, int16_t *out, uint32_t n, uint32_t &got,
                            std::string &e) {
  got = 0;
  if (!v.pcm || !v.decoder)
    return true;
  std::vector<int16_t> raw(size_t(n) * v.asset.channels);
  if (!v.pcm->read(raw.data(), n, got, e))
    return false;
  for (uint32_t i = 0; i < got; ++i)
    for (unsigned c = 0; c < 2; ++c)
      out[size_t(i) * 2 + c] =
          raw[size_t(i) * v.asset.channels + (v.asset.channels == 1 ? 0 : c)];
  if (got < n && !v.asset.loops())
    v.decoder = false;
  return true;
}
bool PodunkSceneAudio::fade(Voice &v, uint32_t n, std::string &e) {
  if (!v.decoder)
    return true;
  std::vector<int16_t> b(size_t(n) * 2, 0);
  uint32_t got = 0;
  if (!read(v, b.data(), n, got, e))
    return false;
  float db = 0;
  bool mute = false;
  if (!host_.server.bus_gain(v.state.bus, db, mute, e))
    return false;
  float start = mute ? 0 : audio_linear_gain(v.mix_volume + db),
        end = mute ? 0 : audio_linear_gain(-80 + db);
  for (uint32_t i = 0; i < n; ++i)
    for (unsigned c = 0; c < 2; ++c)
      b[size_t(i) * 2 + c] =
          sample(b[size_t(i) * 2 + c] * (start + (end - start) * float(i) / n));
  if (v.tail.size() < b.size())
    v.tail.resize(b.size());
  for (size_t i = 0; i < b.size(); ++i)
    v.tail[i] = sample(float(v.tail[i]) + b[i]);
  v.mix_volume = -80;
  return true;
}
bool PodunkSceneAudio::set_stream(FieldObjectId id, uint32_t source,
                                  std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  AudioAsset a;
  if (source && !asset(source, a, e))
    return false;
  if (v->source->kind == 1 && v->active && !v->state.paused &&
      !fade(*v, 512, e))
    return false;
  v->pcm.reset();
  v->asset = a;
  v->state.source_stream = source;
  v->active = v->decoder = false;
  v->pending_seek = v->setplay = -1;
  v->set_stop = false;
  return true;
}
bool PodunkSceneAudio::play(FieldObjectId id, double from, std::string &e) {
  auto *v = voice(id, e);
  if (!v || !std::isfinite(from) || from < 0 || from > double(UINT32_MAX))
    return fail(e, "Scene audio play position rejected");
  if (!v->state.source_stream)
    return true;
  const auto *binding = stream_binding(v->state.source_stream);
  if (!binding || (binding->native_class == "AudioStreamSample" && from != 0))
    return fail(
        e, "Scene sample nonzero start needs reviewed native seek semantics");
  // Independent real cursors allow the same checked sound on multiple nodes.
  if (!reserve(*v, e))
    return false;
  if (!v->pcm) {
    auto pcm = std::make_unique<AudioPcmStream>();
    std::string path = audio_->asset_root() + std::string(v->asset.pcm_path);
    if (!pcm->open(v->asset, path.c_str(), e)) {
      release_channel(*v);
      return false;
    }
    v->pcm = std::move(pcm);
  }
  if (!internal(*v, true, e))
    return false;
  if (v->source->kind == 2) {
    if (!v->active && v->setplay < 0)
      v->previous.clear();
    v->setplay = from;
    v->output_ready = false;
  } else {
    v->pending_seek = from;
    v->stop_priority = false;
    v->active = true;
  }
  return true;
}
bool PodunkSceneAudio::stop(FieldObjectId id, std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  if (v->source->kind == 2) {
    v->active = v->decoder = false;
    v->setplay = -1;
    v->tail.clear();
    release_channel(*v);
    return internal(*v, false, e);
  }
  if (v->state.source_stream && v->active) {
    v->set_stop = v->stop_priority = true;
  }
  return true;
}
bool PodunkSceneAudio::seek(FieldObjectId id, double from, std::string &e) {
  auto *v = voice(id, e);
  if (!v || !std::isfinite(from) || from < 0 || from > double(UINT32_MAX))
    return fail(e, "Scene audio seek position rejected");
  if (v->state.source_stream) {
    const auto *binding = stream_binding(v->state.source_stream);
    if (!binding || (binding->native_class == "AudioStreamSample" && from != 0))
      return fail(e,
                  "Scene sample nonzero seek needs reviewed native semantics");
    v->pending_seek = from;
  }
  return true;
}
bool PodunkSceneAudio::set_paused(FieldObjectId id, bool p, std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  if (v->state.paused != p) {
    v->state.paused = p;
    v->fade_pause = p;
    v->fade_in = !p;
    if (!p && v->channel >= 0)
      ndspChnSetPaused(v->channel, false);
  }
  return true;
}
bool PodunkSceneAudio::set_volume(FieldObjectId id, float x, std::string &e) {
  auto *v = voice(id, e);
  if (!v || !std::isfinite(x) || (x < -120 || x > 24))
    return fail(e, "Scene audio volume rejected");
  v->state.volume_db = x;
  return true;
}
bool PodunkSceneAudio::set_pitch(FieldObjectId id, float x, std::string &e) {
  auto *v = voice(id, e);
  if (!v || !std::isfinite(x) || x <= 0 || x > 64)
    return fail(e, "Scene audio pitch rejected");
  v->state.pitch = x;
  return true;
}
bool PodunkSceneAudio::set_bus(FieldObjectId id, std::string_view s,
                               std::string &e) {
  auto *v = voice(id, e);
  float db = 0;
  bool mute = false;
  if (!v || s.empty() || !host_.server.bus_gain(s, db, mute, e) ||
      !std::isfinite(db))
    return fail(e, "Scene audio unknown bus");
  v->state.bus = s;
  return true;
}
bool PodunkSceneAudio::spatial(Voice &v, std::string &e) {
  if (!host_.listeners || !host_.area_bus)
    return fail(e, "Scene positional audio actual World2D listeners/Area bus "
                   "owner pending");
  std::vector<PodunkSceneAudioListener> listeners;
  if (!host_.listeners(v.state.object, listeners, e))
    return false;
  auto *s = tree_->state(v.state.object);
  if (!s)
    return false;
  Vec2 pos = s->world[2];
  std::string bus = v.state.bus;
  if (!host_.area_bus(pos, v.source->area_mask, bus, e))
    return false;
  std::vector<Output> outputs;
  for (const auto &l : listeners) {
    if (!l.viewport || !registry_->object_exists(l.viewport) ||
        !std::isfinite(l.screen_size.x) || !std::isfinite(l.screen_size.y) ||
        l.screen_size.x <= 0 || l.screen_size.y <= 0)
      return fail(e, "Scene positional audio actual viewport invalid");
    if (!l.enabled)
      continue;
    Vec2 at = l.listener_position, relative{pos.x - at.x, pos.y - at.y};
    if (!l.explicit_listener) {
      float det = l.canvas[0].x * l.canvas[1].y - l.canvas[1].x * l.canvas[0].y;
      if (!std::isfinite(det) || det == 0)
        return fail(e, "Scene listener canvas singular");
      Vec2 center{l.screen_size.x * .5f - l.canvas[2].x,
                  l.screen_size.y * .5f - l.canvas[2].y};
      at = {(center.x * l.canvas[1].y - center.y * l.canvas[1].x) / det,
            (-center.x * l.canvas[0].y + center.y * l.canvas[0].x) / det};
      auto p = transform(l.canvas, pos);
      relative = {p.x - l.screen_size.x * .5f, p.y - l.screen_size.y * .5f};
    }
    float distance = std::hypot(pos.x - at.x, pos.y - at.y);
    if (!std::isfinite(distance))
      return fail(e, "Scene listener position invalid");
    if (distance > v.source->max_distance)
      continue;
    float gain =
        std::pow(1 - distance / v.source->max_distance, v.source->attenuation) *
        audio_linear_gain(v.state.volume_db);
    float pan = std::clamp(relative.x / l.screen_size.x, -1.f, 1.f) *
                v.source->panning * data_->global_panning() * .5f;
    pan = std::clamp(pan + .5f, 0.f, 1.f);
    outputs.push_back({l.viewport, (1 - pan) * gain, pan * gain, bus});
    if (outputs.size() > 8)
      return fail(e, "Scene positional audio viewport capacity unsupported");
  }
  v.outputs = std::move(outputs);
  v.output_ready = true;
  return true;
}
bool PodunkSceneAudio::mix(FieldObjectId id, std::string &e) {
  auto *v = voice(id, e);
  if (!v || !v->state.inside)
    return fail(e, "Scene audio mix outside actual AudioServer");
  if (v->channel < 0)
    return true;
  size_t slot = 0;
  for (; slot < buffer_count; ++slot)
    if (v->waves[slot].status != NDSP_WBUF_QUEUED &&
        v->waves[slot].status != NDSP_WBUF_PLAYING)
      break;
  if (slot == buffer_count)
    return true;
  if (v->state.source_stream && v->active &&
      (!v->state.paused || v->fade_pause)) {
    if (v->source->kind == 1 && v->set_stop && !v->state.paused) {
      if (!fade(*v, 128, e))
        return false;
      v->decoder = false;
      v->set_stop = false;
    }
    if ((v->source->kind == 2 || !v->state.paused) && v->pending_seek >= 0 &&
        !v->stop_priority) {
      if (v->source->kind == 1 && v->decoder && !fade(*v, 128, e))
        return false;
      double frame = v->pending_seek * v->asset.sample_rate;
      if (!std::isfinite(frame) || frame > UINT32_MAX)
        return fail(e, "Scene PCM seek conversion rejected");
      uint32_t at = frame >= v->asset.frames ? 0 : uint32_t(frame);
      if (!v->pcm || !v->pcm->seek(at))
        return fail(e, "Scene actual PCM seek failed");
      v->decoder = true;
      v->pending_seek = -1;
      if (v->source->kind == 1)
        v->mix_volume = v->state.volume_db;
    }
    v->stop_priority = false;
  }
  std::vector<int16_t> b(block_frames * 2, 0);
  uint32_t got = 0;
  if (v->active && v->decoder && (!v->state.paused || v->fade_pause)) {
    uint32_t amount = v->fade_pause ? 128 : block_frames;
    if (v->source->kind == 1 && v->state.paused) {
      if (!fade(*v, amount, e))
        return false;
      v->fade_pause = false;
    } else {
      if (!read(*v, b.data(), amount, got, e))
        return false;
      if (v->source->kind == 1) {
        float db = 0;
        bool mute = false;
        if (!host_.server.bus_gain(v->state.bus, db, mute, e) ||
            !std::isfinite(db) || db > 24)
          return fail(e, "Scene audio bus gain invalid");
        float from = mute ? 0 : audio_linear_gain(v->mix_volume + db),
              to = mute ? 0 : audio_linear_gain(v->state.volume_db + db);
        for (uint32_t i = 0; i < got; ++i)
          for (unsigned c = 0; c < 2; ++c)
            b[size_t(i) * 2 + c] =
                sample(b[size_t(i) * 2 + c] *
                       (from + (to - from) * float(i) / amount));
        v->mix_volume = v->state.volume_db;
      } else {
        std::vector<int16_t> out(block_frames * 2, 0);
        for (const auto &current : v->outputs) {
          float db = 0;
          bool mute = false;
          if (!host_.server.bus_gain(current.bus, db, mute, e) ||
              !std::isfinite(db) || db > 24)
            return fail(e, "Scene positional audio bus gain invalid");
          auto old = std::find_if(
              v->previous.begin(), v->previous.end(),
              [&](const auto &o) { return o.viewport == current.viewport; });
          float scale = mute ? 0 : audio_linear_gain(db);
          float from[2]{v->fade_in ? 0
                                   : (old == v->previous.end() ? current.left
                                                               : old->left),
                        v->fade_in ? 0
                                   : (old == v->previous.end() ? current.right
                                                               : old->right)};
          float to[2]{v->state.paused ? 0 : current.left,
                      v->state.paused ? 0 : current.right};
          for (uint32_t i = 0; i < got; ++i)
            for (unsigned c = 0; c < 2; ++c)
              out[size_t(i) * 2 + c] =
                  sample(out[size_t(i) * 2 + c] +
                         b[size_t(i) * 2 + c] *
                             (from[c] + (to[c] - from[c]) * float(i) / amount) *
                             scale);
        }
        b = std::move(out);
        v->previous = v->outputs;
        v->output_ready = false;
        v->fade_pause = v->fade_in = false;
      }
    }
  }
  uint32_t tail =
      std::min<uint32_t>(block_frames, uint32_t(v->tail.size() / 2));
  for (size_t i = 0; i < size_t(tail) * 2; ++i)
    b[i] = sample(float(b[i]) + v->tail[i]);
  if (tail)
    v->tail.erase(v->tail.begin(), v->tail.begin() + size_t(tail) * 2);
  uint32_t count = std::max(got, tail);
  if (count) {
    auto *target = v->samples + slot * block_frames * 2;
    std::copy_n(b.data(), size_t(count) * 2, target);
    auto &w = v->waves[slot];
    w = {};
    w.data_pcm16 = target;
    w.nsamples = count;
    if (R_FAILED(DSP_FlushDataCache(target, count * 2 * sizeof(int16_t))))
      return fail(e, "Scene DSP cache flush failed");
    ndspChnSetRate(v->channel, float(v->asset.sample_rate) * v->state.pitch);
    ndspChnWaveBufAdd(v->channel, &w);
  }
  if (v->source->kind == 2 && !v->decoder)
    v->active = false;
  if (v->state.paused && !queued(*v) && v->tail.empty())
    ndspChnSetPaused(v->channel, true);
  if (!v->decoder && !queued(*v) && v->tail.empty() && v->pending_seek < 0 &&
      v->setplay < 0)
    release_channel(*v);
  return true;
}
bool PodunkSceneAudio::tree_pause(bool paused, std::string &e) {
  if (!live(e))
    return false;
  if (paused == tree_paused_)
    return true;
  for (auto &entry : voices_) {
    if (paused && !tree_->can_process(entry.first, true)) {
      if (!set_paused(entry.first, true, e))
        return false;
    } else if (!paused && !set_paused(entry.first, false, e))
      return false;
  }
  tree_paused_ = paused;
  return true;
}
bool PodunkSceneAudio::phase(FieldObjectId id, FieldTreePhase p, float,
                             bool paused, bool, std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (v->state.inside || !host_.server.add_audio_callback(
                               id,
                               {v->source->kind == 2 ? podunk_audio_stream_player_2d_mix : podunk_audio_stream_player_mix, &v->callback},
                               e))
      return fail(e, "Scene AudioServer native Enter rejected");
    v->state.inside = true;
    if (v->source->autoplay)
      return play(id, 0, e);
    break;
  case FieldTreePhase::ReadyNative:
    if (!v->state.inside || v->state.ready)
      return fail(e, "Scene audio native Ready sequence rejected");
    v->state.ready = true;
    break;
  case FieldTreePhase::ExitNative:
    if (!v->state.inside || !host_.server.remove_audio_callback(id, e))
      return fail(e, "Scene AudioServer native Exit rejected");
    v->state.inside = false;
    break;
  case FieldTreePhase::IdleInternal:
  case FieldTreePhase::PhysicsInternal:
    if (!v->internal || !tree_->can_process(id, paused))
      break;
    if ((p == FieldTreePhase::IdleInternal) != (v->source->kind == 1))
      return fail(e, "Scene audio wrong native processing clock");
    if (v->source->kind == 2) {
      if (!v->output_ready && !spatial(*v, e))
        return false;
      if (v->setplay >= 0) {
        v->pending_seek = v->setplay;
        v->setplay = -1;
        v->active = true;
      }
    }
    if (!v->active || (v->pending_seek < 0 && !v->decoder)) {
      v->active = false;
      if (!internal(*v, false, e))
        return false;
      return host_.server.emit(id, "finished", e);
    }
    break;
  default:
    break;
  }
  return true;
}
bool PodunkSceneAudio::state(FieldObjectId id, PodunkSceneAudioState &out,
                             std::string &e) const {
  if (!live(e))
    return false;
  auto i = voices_.find(id);
  if (i == voices_.end())
    return fail(e, "Scene audio state node absent");
  const auto &v = i->second;
  out = v.state;
  out.playing = v.source->kind == 1 ? v.active && !v.set_stop
                                    : v.active || v.setplay >= 0;
  out.position = v.pending_seek >= 0 ? v.pending_seek
                 : v.pcm ? double(v.pcm->position()) / v.asset.sample_rate
                         : 0;
  return true;
}
bool PodunkSceneAudio::signal_declaration(FieldObjectId id, std::string_view s,
                                          uint32_t &arity,
                                          std::string &e) const {
  if (!owns(id) || s != "finished")
    return fail(e, "Scene audio undeclared signal");
  arity = 0;
  return true;
}
bool PodunkSceneAudio::deferred(const FieldDeferredMessage &m, std::string &e) {
  if (!owns(m.object))
    return fail(e, "Scene audio method wrong owner");
  const auto &args = m.args;
  double x = 0;
  if (m.kind == FieldDeferredKind::Call) {
    if (m.member == "play" &&
        (args.empty() || (args.size() == 1 && number(args[0], x))))
      return play(m.object, x, e);
    if (m.member == "stop" && args.empty())
      return stop(m.object, e);
    if (m.member == "seek" && args.size() == 1 && number(args[0], x))
      return seek(m.object, x, e);
  }
  if (m.kind == FieldDeferredKind::Set && args.size() == 1) {
    const auto &a = args[0];
    if (m.member == "stream") {
      if (std::holds_alternative<std::monostate>(a))
        return set_stream(m.object, 0, e);
      return fail(e, "Scene source Resource set requires typed "
                     "set_stream(sourceID), never fake ObjectID");
    }
    if (m.member == "playing") {
      if (auto *p = std::get_if<bool>(&a))
        return *p ? play(m.object, 0, e) : stop(m.object, e);
    }
    if (m.member == "stream_paused") {
      if (auto *p = std::get_if<bool>(&a))
        return set_paused(m.object, *p, e);
    }
    if (m.member == "volume_db" && number(a, x))
      return set_volume(m.object, float(x), e);
    if (m.member == "pitch_scale" && number(a, x))
      return set_pitch(m.object, float(x), e);
    if (m.member == "bus") {
      if (auto *p = std::get_if<std::string>(&a))
        return set_bus(m.object, *p, e);
    }
  }
  return fail(e, "Scene audio unsupported native method/property");
}
bool PodunkSceneAudio::release(FieldObjectId id, std::string &e) {
  auto i = voices_.find(id);
  if (i == voices_.end())
    return fail(e, "Scene audio release absent owner");
  auto &v = i->second;
  if (v.state.inside && !host_.server.remove_audio_callback(id, e))
    return false;
  if (!host_.server.disconnect_bus_layout(id, e))
    return false;
  free_voice(v);
  voices_.erase(i);
  return true;
}
bool PodunkSceneAudio::shutdown(std::string &e) {
  bool ok = true;
  while (!voices_.empty()) {
    auto id = voices_.begin()->first;
    if (!release(id, e)) {
      free_voice(voices_.begin()->second);
      voices_.erase(voices_.begin());
      ok = false;
    }
  }
  if (channel_owner == this)
    channel_owner = nullptr;
  data_ = nullptr;
  named_ = nullptr;
  tree_ = nullptr;
  registry_ = nullptr;
  audio_ = nullptr;
  bank_.reset();
  host_ = {};
  return ok;
}
} // namespace encore::ctr
