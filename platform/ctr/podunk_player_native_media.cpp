#include "podunk_player_native_media.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace encore::ctr {
using namespace upstream;
namespace {
using Value = std::shared_ptr<const GlobalYamlValue>;
PodunkPlayerNativeMedia *actual_media_lease = nullptr;
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
Value get(const Value &v, std::string_view k) {
  return v && v->kind == 6 ? v->get(k) : nullptr;
}
bool text(const Value &v, std::string &s) {
  if (!v || v->kind != 4)
    return false;
  s = v->string;
  return true;
}
bool boolean(const Value &v, bool &b) {
  if (!v || v->kind != 1)
    return false;
  b = v->boolean;
  return true;
}
bool number(const Value &v, std::string_view kind, double &n) {
  std::string t, s;
  if (!text(get(v, "type"), t) || t != kind || !text(get(v, "value"), s) ||
      s.empty())
    return false;
  char *end = nullptr;
  n = std::strtod(s.c_str(), &end);
  return end == s.c_str() + s.size() && std::isfinite(n);
}
bool ref(const Value &v, uint32_t &id) {
  std::string t;
  auto n = get(v, "id");
  if (!text(get(v, "type"), t) || t != "ResourceReference" || !n ||
      n->kind != 2 || n->integer < 0 || uint64_t(n->integer) > UINT32_MAX)
    return false;
  id = uint32_t(n->integer);
  return true;
}
bool vector(const Value &v, Vec2 &out) {
  std::string t;
  double x = 0, y = 0;
  if (!text(get(v, "type"), t) || t != "Vector2" ||
      !number(get(v, "x"), "real", x) || !number(get(v, "y"), "real", y) ||
      !std::isfinite(float(x)) || !std::isfinite(float(y)))
    return false;
  out = {float(x), float(y)};
  return true;
}
Value dictionary(const Value &v) {
  std::string t;
  auto pairs = get(v, "pairs");
  if (!text(get(v, "type"), t) || t != "Dictionary" || !pairs ||
      pairs->kind != 5)
    return nullptr;
  auto out = std::make_shared<GlobalYamlValue>();
  out->kind = 6;
  for (const auto &p : pairs->array) {
    if (!p || p->kind != 5 || p->array.size() != 2 || p->array[0]->kind != 4 ||
        out->get(p->array[0]->string))
      return nullptr;
    out->dictionary.emplace_back(p->array[0]->string, p->array[1]);
  }
  return out;
}
Value array(const Value &v) {
  std::string t;
  auto a = get(v, "value");
  return text(get(v, "type"), t) && t == "Array" && a && a->kind == 5 ? a
                                                                      : nullptr;
}
Vec2 transform(const FieldTransform &t, Vec2 v) {
  return {t[0].x * v.x + t[1].x * v.y + t[2].x,
          t[0].y * v.x + t[1].y * v.y + t[2].y};
}
// Godot 3.6.2 AudioStreamPlayer::_mix_internal uses 128-frame stop/seek
// ramps; its constructor uses a 512-frame stream-replacement tail. These are
// native engine behavior, not game audio tuning.
constexpr uint32_t stop_frames = 128, replacement_frames = 512;
constexpr float silence_db = -80;
} // namespace
PodunkPlayerNativeMedia::~PodunkPlayerNativeMedia() {
  // The owning bundle must call shutdown before its AudioServer/Registry dies.
  // Do not emit signals or unregister source callbacks from a destructor.
  for (auto &v : voices_)
    free_voice(v.second);
  if (actual_media_lease == this)
    actual_media_lease = nullptr;
}
bool PodunkPlayerNativeMedia::prepare(
    const PlayerInitializationData &d, PodunkPlayerResources &resources,
    FieldNodeTreeRuntime &tree, FieldGlobalRegistry &registry,
    AudioPlayer &audio, PodunkPlayerMediaHost host,
    const std::vector<uint32_t> &sprites, const std::vector<uint32_t> &voices,
    std::string &e) {
  if (data_ || actual_media_lease || !d.valid() ||
      resources.registry() != &registry || !registry.root() ||
      registry.poisoned() || tree.object_domain() != registry.kernel() ||
      !audio.available() || !host.add_audio_callback ||
      !host.remove_audio_callback || !host.bus_gain ||
      !host.connect_bus_layout || !host.disconnect_bus_layout || !host.emit ||
      !host.connect || !host.disconnect || voices.size() > leases_.size())
    return fail(e, "Player native media requires same live "
                   "Tree/Resource/DSP/signal owners");
  std::vector<uint32_t> seen;
  for (unsigned kind = 0; kind < 2; ++kind)
    for (auto id : kind ? voices : sprites) {
      const auto *r = d.recipe().record(id);
      if (!r || !r->script.empty() || r->native_generated ||
          r->native_class != (kind ? "AudioStreamPlayer" : "AnimatedSprite") ||
          std::find(seen.begin(), seen.end(), id) != seen.end())
        return fail(e, "Player media audited ownership roster rejected");
      seen.push_back(id);
    }
  actual_media_lease = this;
  data_ = &d;
  ir_ = d.ir_sha256();
  resources_ = &resources;
  tree_ = &tree;
  registry_ = &registry;
  audio_ = &audio;
  host_ = std::move(host);
  sprites_claimed_ = sprites;
  audio_claimed_ = voices;
  e.clear();
  return true;
}
bool PodunkPlayerNativeMedia::live(std::string &e) const {
  if (audio_ && audio_->available())
    audio_->device().pump();
  return data_ && data_->valid() && data_->ir_sha256() == ir_ && resources_ &&
                 resources_->registry() == registry_ && registry_ &&
                 !registry_->poisoned() && tree_ &&
                 tree_->object_domain() == registry_->kernel() && audio_ &&
                 audio_->available()
             ? true
             : fail(e, "Player native media source/domain/DSP owner expired");
}
bool PodunkPlayerNativeMedia::rebind_tree(FieldNodeTreeRuntime &next,
                                          std::string &e) {
  if (!live(e) || next.object_domain() != registry_->kernel())
    return fail(e, "Player media transfer requires same actual ObjectDB");
  auto checked = [&](FieldObjectId id) {
    auto owner = registry_->tree_owner(id);
    auto *d = next.descriptor(id);
    FieldIdentity identity;
    return owner && owner.get() == &next && d && next.state(id) &&
           next.object_identity(id, identity) &&
           identity.scene_id == data_->identity().scene_id &&
           identity.source_sha256 == data_->identity().source_sha256 &&
           identity.upstream_commit == data_->identity().upstream_commit &&
           std::find((d->native_class == "AnimatedSprite" ? sprites_claimed_
                                                          : audio_claimed_)
                         .begin(),
                     (d->native_class == "AnimatedSprite" ? sprites_claimed_
                                                          : audio_claimed_)
                         .end(),
                     d->id) != (d->native_class == "AnimatedSprite"
                                    ? sprites_claimed_
                                    : audio_claimed_)
                                   .end();
  };
  for (const auto &s : sprites_)
    if (!checked(s.first))
      return fail(e, "Player Sprite not actually transferred to target Tree");
  for (const auto &v : voices_)
    if (!checked(v.first))
      return fail(
          e,
          "Player AudioStreamPlayer not actually transferred to target Tree");
  tree_ = &next;
  return true;
}
bool PodunkPlayerNativeMedia::node(FieldObjectId id, std::string &e) const {
  if (!live(e))
    return false;
  auto *n = tree_->state(id);
  auto *d = tree_->descriptor(id);
  FieldIdentity identity;
  if (!id || !registry_->object_exists(id) || !n || !n->alive || n->queued ||
      !d || !tree_->object_identity(id, identity) ||
      identity.scene_id != data_->identity().scene_id ||
      identity.source_sha256 != data_->identity().source_sha256 ||
      identity.upstream_commit != data_->identity().upstream_commit ||
      !d->script.empty())
    return fail(e,
                "Player media actual source Node lifetime/identity rejected");
  return true;
}
bool PodunkPlayerNativeMedia::owns(FieldObjectId id) const {
  return sprites_.count(id) || voices_.count(id);
}
PodunkPlayerNativeMedia::Sprite *
PodunkPlayerNativeMedia::sprite(FieldObjectId id, std::string &e) {
  if (!node(id, e))
    return nullptr;
  auto i = sprites_.find(id);
  if (i == sprites_.end()) {
    fail(e, "Player native AnimatedSprite owner absent");
    return nullptr;
  }
  return &i->second;
}
PodunkPlayerNativeMedia::Voice *PodunkPlayerNativeMedia::voice(FieldObjectId id,
                                                               std::string &e) {
  if (!node(id, e))
    return nullptr;
  auto i = voices_.find(id);
  if (i == voices_.end()) {
    fail(e, "Player native AudioStreamPlayer owner absent");
    return nullptr;
  }
  return &i->second;
}
bool PodunkPlayerNativeMedia::construct(FieldObjectId id, std::string &e) {
  if (!node(id, e) || owns(id))
    return fail(e, "Player media duplicate/source construction rejected");
  auto *n = tree_->state(id);
  auto *d = tree_->descriptor(id);
  if (n->inside || n->parent || !n->name.empty())
    return fail(
        e, "Player media requires actual pre-name native allocation cursor");
  auto nodes = get(data_->native_source(), "nodes");
  Value props;
  if (!nodes || nodes->kind != 5)
    return fail(e, "Player media native snapshot missing");
  for (const auto &v : nodes->array) {
    std::string path, cls;
    if (text(get(v, "path"), path) && path == d->path) {
      if (!text(get(v, "class"), cls) || cls != d->native_class)
        return fail(e, "Player media actual native class mismatch");
      props = get(v, "properties");
      break;
    }
  }
  if (!props || props->kind != 6 || !get(props, "script") ||
      get(props, "script")->kind != 0)
    return fail(e, "Player media native property closure differs");
  if (std::find(sprites_claimed_.begin(), sprites_claimed_.end(), d->id) !=
      sprites_claimed_.end()) {
    Sprite s;
    s.state.object = id;
    uint32_t frames = 0;
    double scale = 0;
    if (!ref(get(props, "frames"), frames) ||
        !text(get(props, "animation"), s.state.animation) ||
        !number(get(props, "speed_scale"), "real", scale) || scale < 0 ||
        !std::isfinite(float(scale)) ||
        !boolean(get(props, "playing"), s.state.playing) ||
        !boolean(get(props, "visible"), s.state.visible) ||
        !boolean(get(props, "centered"), s.centered) ||
        !boolean(get(props, "flip_h"), s.flip_h) ||
        !boolean(get(props, "flip_v"), s.flip_v) ||
        !vector(get(props, "offset"), s.state.offset) ||
        (!get(props, "material") || get(props, "material")->kind != 0))
      return fail(e, "Player native AnimatedSprite property type unsupported");
    s.speed = float(scale);
    auto all = get(data_->native_source(), "resources");
    Value animations;
    if (!all || all->kind != 5)
      return fail(e, "Player SpriteFrames native closure missing");
    for (const auto &r : all->array) {
      auto rid = get(r, "id");
      std::string cls;
      if (rid && rid->kind == 2 && rid->integer == frames) {
        if (!text(get(r, "class"), cls) || cls != "SpriteFrames")
          return fail(e, "Player animation frames class rejected");
        animations = array(get(get(r, "properties"), "animations"));
        break;
      }
    }
    if (!animations || animations->array.empty())
      return fail(e, "Player native SpriteFrames animations missing");
    for (const auto &v : animations->array) {
      auto dict = dictionary(v);
      Clip c;
      double speed = 0;
      auto f = array(get(dict, "frames"));
      if (!dict || !text(get(dict, "name"), c.name) ||
          !number(get(dict, "speed"), "real", speed) || speed < 0 ||
          !std::isfinite(float(speed)) || !boolean(get(dict, "loop"), c.loop) ||
          !f || f->array.empty())
        return fail(e, "Player native SpriteFrames typed animation rejected");
      c.speed = float(speed);
      for (const auto &x : f->array) {
        uint32_t source = 0;
        FieldObjectId actual = 0;
        C2D_Image image{};
        if (!ref(x, source) ||
            !resources_->construct_resource(source, 0, actual, e) ||
            !resources_->texture(actual, image, e))
          return false;
        c.frames.push_back(actual);
      }
      for (const auto &old : s.clips)
        if (old.name == c.name)
          return fail(e, "Player SpriteFrames duplicate native animation");
      s.clips.push_back(std::move(c));
    }
    const auto *c = clip(s);
    if (!c)
      return fail(e, "Player native initial animation absent");
    if (auto frame = get(props, "frame")) {
      double f = 0;
      if (!number(frame, "int64", f) || f < 0 || f > UINT32_MAX)
        return fail(e, "Player native initial frame rejected");
      s.state.frame = std::min(uint32_t(f), uint32_t(c->frames.size() - 1));
    }
    s.timeout = s.state.playing && c->speed * s.speed > 0
                    ? 1 / (c->speed * s.speed)
                    : 0;
    sprites_.emplace(id, std::move(s));
    if (sprites_.at(id).state.playing && !internal(id, true, e)) {
      sprites_.erase(id);
      return false;
    }
    e.clear();
    return true;
  }
  if (std::find(audio_claimed_.begin(), audio_claimed_.end(), d->id) !=
      audio_claimed_.end()) {
    Voice v;
    v.state.object = id;
    double volume = 0, pitch = 0, mix = 0;
    if (!number(get(props, "volume_db"), "real", volume) ||
        !number(get(props, "pitch_scale"), "real", pitch) || pitch <= 0 ||
        pitch > 4 || volume < -120 || volume > 24 ||
        !boolean(get(props, "autoplay"), v.autoplay) ||
        !boolean(get(props, "stream_paused"), v.state.paused) ||
        !text(get(props, "bus"), v.state.bus) ||
        !number(get(props, "mix_target"), "int64", mix) || mix != 0)
      return fail(e, "Player native audio properties unsupported");
    float bus = 0;
    bool muted = false;
    if (!host_.bus_gain(v.state.bus, bus, muted, e) || !std::isfinite(bus))
      return fail(e, "Player actual source audio bus missing");
    v.state.volume = v.state.mix_volume = float(volume);
    v.state.pitch = float(pitch);
    v.samples = static_cast<int16_t *>(
        linearAlloc(buffers * block_frames * 2 * sizeof(int16_t)));
    if (!v.samples)
      return fail(e, "Player native NDSP wave allocation failed");
    uint32_t stream_id = 0;
    auto source = get(props, "stream");
    if (!source) {
      linearFree(v.samples);
      return fail(e, "Player native stream property missing");
    }
    if (source->kind != 0 && !ref(source, stream_id)) {
      linearFree(v.samples);
      return fail(e, "Player native stream type rejected");
    }
    v.callback = {id, registry_, this, [](void *owner, upstream::FieldObjectId object, std::string &error) {
      return static_cast<PodunkPlayerNativeMedia *>(owner)->audio_mix(object, error);
    }};
    voices_.emplace(id, std::move(v));
    if (!audio_stream(id, stream_id, e)) {
      free_voice(voices_.at(id));
      voices_.erase(id);
      return false;
    }
    if (!host_.connect_bus_layout(
            id,
            [this, id] {
              std::string e;
              auto *v = voice(id, e);
              if (!v)
                return false;
              float db = 0;
              bool muted = false;
              return host_.bus_gain(v->state.bus, db, muted, e) &&
                     std::isfinite(db);
            },
            e)) {
      free_voice(voices_.at(id));
      voices_.erase(id);
      return false;
    }
    return true;
  }
  return fail(e,
              "Player native media actual source is outside ownership roster");
}
bool PodunkPlayerNativeMedia::internal(FieldObjectId id, bool enable,
                                       std::string &e) {
  return enable ? tree_->add_group(id, "idle_process_internal", e)
                : tree_->remove_group(id, "idle_process_internal", e);
}
const PodunkPlayerNativeMedia::Clip *
PodunkPlayerNativeMedia::clip(const Sprite &s) const {
  for (const auto &c : s.clips)
    if (c.name == s.state.animation)
      return &c;
  return nullptr;
}
bool PodunkPlayerNativeMedia::animated_state(FieldObjectId id,
                                             PodunkPlayerAnimatedState &out,
                                             std::string &e) const {
  if (!node(id, e))
    return false;
  auto i = sprites_.find(id);
  if (i == sprites_.end())
    return fail(e, "Player AnimatedSprite actual owner missing");
  out = i->second.state;
  out.visible = tree_->visible_in_tree(id);
  e.clear();
  return true;
}
bool PodunkPlayerNativeMedia::set_frame(FieldObjectId id, uint32_t frame,
                                        std::string &e) {
  auto *s = sprite(id, e);
  if (!s)
    return false;
  auto *c = clip(*s);
  if (!c)
    return fail(e, "Player AnimatedSprite source clip absent");
  frame = std::min(frame, uint32_t(c->frames.size() - 1));
  if (s->state.frame == frame)
    return true;
  s->state.frame = frame;
  if (s->state.playing) {
    s->timeout = c->speed * s->speed > 0 ? 1 / (c->speed * s->speed) : 0;
    s->over = false;
  }
  return host_.emit(id, "frame_changed", e);
}
bool PodunkPlayerNativeMedia::set_playing(FieldObjectId id, bool playing,
                                          std::string &e) {
  auto *s = sprite(id, e);
  if (!s)
    return false;
  if (s->state.playing == playing)
    return true;
  if (!internal(id, playing, e))
    return false;
  s->state.playing = playing;
  if (playing) {
    auto *c = clip(*s);
    s->timeout = c && c->speed * s->speed > 0 ? 1 / (c->speed * s->speed) : 0;
    s->over = false;
  }
  return true;
}
bool PodunkPlayerNativeMedia::play(FieldObjectId id, std::string_view name,
                                   bool backwards, std::string &e) {
  auto *s = sprite(id, e);
  if (!s)
    return false;
  const Clip *c = nullptr;
  for (const auto &x : s->clips)
    if (x.name == name)
      c = &x;
  if (!c)
    return fail(e, "Player AnimatedSprite unknown original animation");
  s->backwards = backwards;
  if (s->state.animation != name) {
    s->state.animation = std::string(name);
    if (s->state.playing) {
      s->timeout = c->speed * s->speed > 0 ? 1 / (c->speed * s->speed) : 0;
      s->over = false;
    }
    if (!set_frame(id, 0, e))
      return false;
  }
  if (backwards && s->state.frame == 0 &&
      !set_frame(id, uint32_t(c->frames.size() - 1), e))
    return false;
  s->over = false;
  return set_playing(id, true, e);
}
bool PodunkPlayerNativeMedia::set_offset(FieldObjectId id, Vec2 value,
                                         std::string &e) {
  auto *s = sprite(id, e);
  if (!s || !std::isfinite(value.x) || !std::isfinite(value.y))
    return fail(e, "Player native offset rejected");
  s->state.offset = value;
  return true;
}
bool PodunkPlayerNativeMedia::set_visible(FieldObjectId id, bool visible,
                                          std::string &e) {
  auto *s = sprite(id, e);
  if (!s || !tree_->set_visible(id, visible, e))
    return false;
  s->state.visible = visible;
  return true;
}
bool PodunkPlayerNativeMedia::connect(FieldObjectId id, std::string_view signal,
                                      FieldObjectId target,
                                      std::string_view method, uint32_t flags,
                                      std::string &e) {
  if (!node(id, e) || !registry_->object_exists(target) || method.empty() ||
      (flags & ~5u) ||
      ((sprites_.count(id) &&
        (signal != "frame_changed" && signal != "animation_finished")) ||
       (voices_.count(id) && signal != "finished")) ||
      !owns(id))
    return fail(e, "Player media source signal connection rejected");
  return host_.connect(id, signal, target, method, flags, e);
}
bool PodunkPlayerNativeMedia::disconnect(FieldObjectId id,
                                         std::string_view signal,
                                         FieldObjectId target,
                                         std::string_view method,
                                         std::string &e) {
  if (!node(id, e) || !owns(id))
    return false;
  return host_.disconnect(id, signal, target, method, e);
}
bool PodunkPlayerNativeMedia::animated_process(Sprite &s, float delta,
                                               bool paused, bool pending,
                                               std::string &e) {
  if (!std::isfinite(delta) || delta < 0)
    return fail(e, "Player native actual idle delta rejected");
  if (!s.entered || !tree_->can_process(s.state.object, paused) ||
      !s.state.playing || !pending)
    return true;
  float remaining = delta;
  while (remaining > 0) {
    auto *c = clip(s);
    if (!c)
      return fail(e, "Player native frame clip disappeared");
    if (c->speed * s.speed == 0)
      return true;
    if (s.timeout <= 0) {
      s.timeout = 1 / (c->speed * s.speed);
      uint32_t end = uint32_t(c->frames.size() - 1);
      if ((!s.backwards && s.state.frame >= end) ||
          (s.backwards && s.state.frame == 0)) {
        s.state.frame = s.backwards ? (c->loop ? end : 0) : (c->loop ? 0 : end);
        if (c->loop || !s.over) {
          if (!c->loop)
            s.over = true;
          if (!host_.emit(s.state.object, "animation_finished", e))
            return false;
        }
      } else if (s.backwards)
        --s.state.frame;
      else
        ++s.state.frame;
      if (!host_.emit(s.state.object, "frame_changed", e))
        return false;
    }
    float amount = std::min(s.timeout, remaining);
    if (!(amount > 0))
      return fail(e, "Player native frame clock cannot advance");
    remaining -= amount;
    s.timeout -= amount;
  }
  return true;
}
bool PodunkPlayerNativeMedia::admit(FieldObjectId id, std::string_view member,
                                    bool method, std::string &e) const {
  if (!node(id, e))
    return false;
  if (sprites_.count(id)) {
    if ((!method && (member == "frame" || member == "playing" ||
                     member == "offset" || member == "visible")) ||
        (method && (member == "play" || member == "stop")))
      return true;
  }
  if (voices_.count(id)) {
    if ((!method &&
         (member == "stream" || member == "playing" || member == "volume_db" ||
          member == "pitch_scale" || member == "stream_paused")) ||
        (method && (member == "play" || member == "stop" || member == "seek")))
      return true;
  }
  return fail(e, "Player native media member/method outside real owner");
}

bool PodunkPlayerNativeMedia::audio_state(FieldObjectId id,
                                          PodunkPlayerAudioState &out,
                                          std::string &e) const {
  if (!node(id, e))
    return false;
  auto i = voices_.find(id);
  if (i == voices_.end())
    return fail(e, "Player actual audio state absent");
  out = i->second.state;
  return true;
}
bool PodunkPlayerNativeMedia::stream(uint32_t source, FieldObjectId &out,
                                     std::string &e) {
  if (!live(e))
    return false;
  if (!source) {
    out = 0;
    return true;
  }
  return resources_->construct_audio(source, out, e);
}
bool PodunkPlayerNativeMedia::queued(const Voice &v) const {
  if (audio_ && audio_->available())
    audio_->device().pump();
  for (const auto &w : v.waves)
    if (w.status == NDSP_WBUF_QUEUED || w.status == NDSP_WBUF_PLAYING)
      return true;
  return false;
}
bool PodunkPlayerNativeMedia::reserve(Voice &v, std::string &e) {
  if (v.channel >= 0)
    return true;
  for (size_t i = 0; i < leases_.size(); ++i)
    if (!leases_[i]) {
      int channel = -1;
      if (!audio_->device().lease(this, v.state.object, channel, e))
        return false;
      leases_[i] = v.state.object;
      v.channel = channel;
      audio_->device().reset(v.channel);
      audio_->device().interp(v.channel, NDSP_INTERP_POLYPHASE);
      audio_->device().format(v.channel, NDSP_FORMAT_STEREO_PCM16);
      float gain[12]{};
      gain[0] = gain[1] = 1;
      audio_->device().mix(v.channel, gain);
      return true;
    }
  return fail(e, "Player native audio two voice leases occupied");
}
bool PodunkPlayerNativeMedia::read(Voice &v, int16_t *out, uint32_t amount,
                                   uint32_t &got, std::string &e) {
  got = 0;
  if (!v.pcm || !v.state.stream)
    return true;
  const std::vector<uint8_t> *actual = nullptr;
  PlayerResourceAudio binding;
  if (!resources_->pcm(v.state.stream, actual, binding, e) || actual != v.pcm ||
      binding.id != v.binding.id || binding.frames != v.binding.frames ||
      binding.channels != v.binding.channels ||
      actual->size() != uint64_t(binding.frames) * binding.channels * 2)
    return fail(e, "Player actual PCM Resource/cursor changed");
  while (got < amount) {
    uint32_t first = 0, n = v.cursor.take(amount - got, first);
    if (!n)
      break;
    for (uint32_t i = 0; i < n; ++i)
      for (uint32_t channel = 0; channel < 2; ++channel) {
        size_t at = (size_t(first + i) * binding.channels +
                     (binding.channels == 1 ? 0 : channel)) *
                    2;
        uint16_t value = uint16_t((*actual)[at]) | uint16_t((*actual)[at + 1])
                                                       << 8;
        int16_t sample;
        std::memcpy(&sample, &value, 2);
        out[size_t(got + i) * 2 + channel] = sample;
      }
    got += n;
  }
  return true;
}
bool PodunkPlayerNativeMedia::queue(Voice &v, size_t slot,
                                    const int16_t *samples, uint32_t count,
                                    std::string &e) {
  if (!count)
    return true;
  if (!reserve(v, e) || count > block_frames)
    return fail(e, "Player native wave queue bounds rejected");
  auto &w = v.waves[slot];
  if (w.status == NDSP_WBUF_QUEUED || w.status == NDSP_WBUF_PLAYING)
    return fail(e, "Player native wave overwrite rejected");
  auto *target = v.samples + slot * block_frames * 2;
  if (samples != target)
    std::copy_n(samples, size_t(count) * 2, target);
  w = {};
  w.data_pcm16 = target;
  w.nsamples = count;
  w.looping = false;
  if (R_FAILED(audio_->device().flush(target, count * 2 * sizeof(int16_t))))
    return fail(e, "Player actual DSP cache flush failed");
  audio_->device().rate(v.channel, float(v.binding.rate) * v.state.pitch);
  if (R_FAILED(audio_->device().add(v.channel, &w)))
    return fail(e, "Player actual audio device wave queue failed");
  return true;
}
bool PodunkPlayerNativeMedia::fade(Voice &v, uint32_t count, bool replacement,
                                   std::string &e) {
  if (!v.pcm || !v.decoder_playing)
    return true;
  float bus = 0;
  bool muted = false;
  if (!host_.bus_gain(v.state.bus, bus, muted, e) || !std::isfinite(bus))
    return false;
  std::vector<int16_t> samples(size_t(count) * 2, 0);
  uint32_t got = 0;
  if (!read(v, samples.data(), count, got, e))
    return false;
  float start = muted ? 0 : audio_linear_gain(v.state.mix_volume + bus),
        target = muted ? 0 : audio_linear_gain(silence_db + bus);
  for (uint32_t i = 0; i < count; ++i) {
    float gain = start + (target - start) * float(i) / count;
    for (unsigned c = 0; c < 2; ++c)
      samples[size_t(i) * 2 + c] = int16_t(std::lrint(std::clamp(
          float(samples[size_t(i) * 2 + c]) * gain, -32768.f, 32767.f)));
  }
  if (replacement) {
    v.tail = std::move(samples);
    v.tail_frames = count;
  } else {
    v.tail.resize(size_t(std::max(v.tail_frames, count)) * 2, 0);
    for (size_t i = 0; i < samples.size(); ++i)
      v.tail[i] =
          int16_t(std::clamp(int(v.tail[i]) + samples[i], -32768, 32767));
    v.tail_frames = std::max(v.tail_frames, count);
  }
  v.state.mix_volume = silence_db;
  return true;
}
bool PodunkPlayerNativeMedia::audio_stream(FieldObjectId id, uint32_t source,
                                           std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  FieldObjectId actual = 0;
  const std::vector<uint8_t> *pcm = nullptr;
  PlayerResourceAudio binding;
  AudioFrameCursor cursor;
  if (source) {
    if (!stream(source, actual, e) ||
        !resources_->pcm(actual, pcm, binding, e) ||
        !cursor.reset(binding.frames, 0, binding.loop))
      return false;
    if (v->binding.rate && v->binding.rate != binding.rate &&
        (queued(*v) || v->tail_frames))
      return fail(
          e, "Player native sample-rate change awaits actual queued boundary");
  }
  // Pre-instance actual candidate before replacing the live cursor, matching
  // native set_stream. Mono and stereo mix into the same real stereo bus.
  if (v->state.active && v->state.stream && !v->state.paused &&
      !fade(*v, replacement_frames, true, e))
    return false;
  if (v->state.stream) {
    v->state.active = false;
    v->state.pending_seek = -1;
    v->state.set_stop = false;
  }
  v->state.source_stream = source;
  v->state.stream = actual;
  v->pcm = pcm;
  v->cursor = cursor;
  v->decoder_playing = false;
  if (source)
    v->binding = binding;
  return true;
}
bool PodunkPlayerNativeMedia::audio_play(FieldObjectId id, double from,
                                         std::string &e) {
  auto *v = voice(id, e);
  if (!v || !std::isfinite(from) || from < 0 || from > 86400)
    return fail(e, "Player native audio play position rejected");
  if (!v->state.stream)
    return true;
  if (!reserve(*v, e))
    return false;
  if (!v->state.internal) {
    if (!internal(id, true, e))
      return false;
    v->state.internal = true;
  }
  v->state.pending_seek = from;
  v->state.stop_priority = false;
  v->state.active = true;
  return true;
}
bool PodunkPlayerNativeMedia::audio_playing(FieldObjectId id, bool playing,
                                            std::string &e) {
  if (playing)
    return audio_play(id, 0, e);
  auto *v = voice(id, e);
  if (!v)
    return false;
  if (v->state.stream && v->state.active) {
    v->state.set_stop = true;
    v->state.stop_priority = true;
  }
  return true;
}
bool PodunkPlayerNativeMedia::audio_seek(FieldObjectId id, double value,
                                         std::string &e) {
  auto *v = voice(id, e);
  if (!v || !std::isfinite(value) || value < 0 || value > 86400)
    return fail(e, "Player native seek outside checked position");
  if (v->state.stream)
    v->state.pending_seek = value;
  return true;
}
bool PodunkPlayerNativeMedia::audio_paused(FieldObjectId id, bool paused,
                                           std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  if (v->state.paused != paused) {
    v->state.paused = paused;
    v->state.paused_fade = paused;
    if (!paused && v->channel >= 0)
      audio_->device().paused(v->channel, false);
  }
  return true;
}
bool PodunkPlayerNativeMedia::audio_volume(FieldObjectId id, float value,
                                           std::string &e) {
  auto *v = voice(id, e);
  if (!v || !std::isfinite(value) || value < -120 || value > 24)
    return fail(e,
                "Player audio finite volume outside supported native bounds");
  v->state.volume = value;
  return true;
}
bool PodunkPlayerNativeMedia::audio_pitch(FieldObjectId id, float value,
                                          std::string &e) {
  auto *v = voice(id, e);
  if (!v || !std::isfinite(value) || value <= 0 || value > 4)
    return fail(e, "Player audio pitch outside checked backend bounds");
  v->state.pitch = value;
  return true;
}
bool PodunkPlayerNativeMedia::audio_bus(FieldObjectId id,
                                        std::string_view value,
                                        std::string &e) {
  auto *v = voice(id, e);
  if (!v)
    return false;
  float db = 0;
  bool muted = false;
  if (!host_.bus_gain(value, db, muted, e) || !std::isfinite(db))
    return fail(e, "Player actual source bus graph rejected");
  v->state.bus = std::string(value);
  return true;
}
bool PodunkPlayerNativeMedia::audio_position(FieldObjectId id, double &out,
                                             std::string &e) const {
  if (!node(id, e))
    return false;
  auto v = voices_.find(id);
  if (v == voices_.end())
    return fail(e, "Player actual audio position owner absent");
  const auto &s = v->second;
  out = !s.state.stream             ? 0
        : s.state.pending_seek >= 0 ? s.state.pending_seek
        : s.binding.rate ? double(s.cursor.position()) / s.binding.rate
                         : 0;
  return true;
}
bool PodunkPlayerNativeMedia::audio_mix(FieldObjectId id, std::string &e) {
  auto *v = voice(id, e);
  if (!v || !v->state.entered)
    return fail(e, "Player audio mix outside actual AudioServer callback");
  size_t slot = 0;
  for (; slot < buffers; ++slot)
    if (v->waves[slot].status != NDSP_WBUF_QUEUED &&
        v->waves[slot].status != NDSP_WBUF_PLAYING)
      break;
  if (slot == buffers)
    return true;
  if (v->state.stream && v->state.active &&
      (!v->state.paused || v->state.paused_fade)) {
    if (v->state.paused) {
      if (v->state.paused_fade && v->decoder_playing) {
        if (!fade(*v, stop_frames, false, e))
          return false;
        v->state.paused_fade = false;
      }
    } else {
      if (v->state.set_stop) {
        if (!fade(*v, stop_frames, false, e))
          return false;
        v->decoder_playing = false;
        v->state.set_stop = false;
      }
      if (v->state.pending_seek >= 0 && !v->state.stop_priority) {
        if (v->decoder_playing && !fade(*v, stop_frames, false, e))
          return false;
        double frame = v->state.pending_seek * v->binding.rate;
        if (!std::isfinite(frame) || frame > UINT32_MAX)
          return fail(e, "Player actual seek frame conversion rejected");
        uint32_t position = frame >= v->binding.frames ? 0 : uint32_t(frame);
        if (!v->cursor.seek(position))
          return fail(e, "Player actual PCM seek failed");
        v->decoder_playing = true;
        v->state.pending_seek = -1;
        v->state.mix_volume = v->state.volume;
      }
      v->state.stop_priority = false;
    }
  }
  uint32_t frames = 0;
  std::vector<int16_t> samples(block_frames * 2, 0);
  if (v->state.active && !v->state.paused && v->decoder_playing && v->pcm) {
    if (!read(*v, samples.data(), block_frames, frames, e))
      return false;
    if (frames < block_frames && !v->binding.loop)
      v->decoder_playing = false;
    float db = 0;
    bool muted = false;
    if (!host_.bus_gain(v->state.bus, db, muted, e) || !std::isfinite(db) ||
        db > 24)
      return fail(e, "Player actual audio bus gain rejected");
    float from = muted ? 0 : audio_linear_gain(v->state.mix_volume + db),
          to = muted ? 0 : audio_linear_gain(v->state.volume + db);
    for (uint32_t i = 0; i < block_frames; ++i) {
      float gain = from + (to - from) * float(i) / block_frames;
      for (unsigned c = 0; c < 2; ++c)
        samples[size_t(i) * 2 + c] = int16_t(std::lrint(std::clamp(
            float(samples[size_t(i) * 2 + c]) * gain, -32768.f, 32767.f)));
    }
    v->state.mix_volume = v->state.volume;
  }
  // Tail has its own actual samples, so replacing/clearing stream cannot erase
  // the native short ramp. It uses the same next AudioServer hardware block.
  uint32_t tail = std::min<uint32_t>(v->tail_frames, block_frames);
  for (size_t i = 0; i < size_t(tail) * 2; ++i)
    samples[i] =
        int16_t(std::clamp(int(samples[i]) + v->tail[i], -32768, 32767));
  if (tail) {
    v->tail.erase(v->tail.begin(), v->tail.begin() + size_t(tail) * 2);
    v->tail_frames -= tail;
  }
  uint32_t count = std::max(frames, tail);
  if (!queue(*v, slot, samples.data(), count, e))
    return false;
  // Pause only once the source short ramp has actually drained. Pausing queued
  // earlier audio here would suppress the ramp rather than produce it.
  if (v->state.paused && !queued(*v) && !v->tail_frames && v->channel >= 0)
    audio_->device().paused(v->channel, true);
  return true;
}
bool PodunkPlayerNativeMedia::tree_pause(bool paused, std::string &e) {
  if (!live(e))
    return false;
  for (auto &v : voices_) {
    if (paused && !tree_->can_process(v.first, true)) {
      if (!audio_paused(v.first, true, e))
        return false;
    } else if (!paused && !audio_paused(v.first, false, e))
      return false;
  }
  return true;
}
bool PodunkPlayerNativeMedia::phase(FieldObjectId id, FieldTreePhase phase,
                                    float delta, bool paused, bool pending,
                                    std::string &e) {
  if (!node(id, e) || !owns(id))
    return fail(e, "Player media lifecycle outside actual owner");
  auto s = sprites_.find(id);
  if (s != sprites_.end()) {
    auto &v = s->second;
    switch (phase) {
    case FieldTreePhase::EnterNative:
      if (v.entered)
        return fail(e, "Player AnimatedSprite duplicate EnterTree");
      v.entered = true;
      break;
    case FieldTreePhase::ReadyNative:
      if (!v.entered || v.ready)
        return fail(e, "Player AnimatedSprite Ready outside source lifecycle");
      v.ready = true;
      break;
    case FieldTreePhase::ExitNative:
      if (!v.entered)
        return fail(e, "Player AnimatedSprite ExitTree without entry");
      v.entered =
          false; // Source native Ready body is first-only across reentry.
      break;
    case FieldTreePhase::IdleInternal:
      return animated_process(v, delta, paused, pending, e);
    default:
      break;
    }
    return true;
  }
  auto &v = voices_.at(id);
  switch (phase) {
  case FieldTreePhase::EnterNative:
    if (v.state.entered || !host_.add_audio_callback(
                               id,
                               {podunk_audio_stream_player_mix, &v.callback},
                               e))
      return fail(e,
                  "Player native AudioServer EnterTree registration rejected");
    v.state.entered = true;
    if (v.autoplay)
      return audio_play(id, 0, e);
    break;
  case FieldTreePhase::ReadyNative:
    if (!v.state.entered || v.state.ready || !v.samples)
      return fail(e, "Player actual native audio Ready rejected");
    v.state.ready = true;
    break;
  case FieldTreePhase::ExitNative:
    if (!v.state.entered || !host_.remove_audio_callback(id, e))
      return fail(e, "Player native AudioServer ExitTree removal rejected");
    v.state.entered = false; // Preserve the actually executed first Ready body.
    break;
  case FieldTreePhase::IdleInternal:
    if (v.state.internal && tree_->can_process(id, paused) &&
        (!v.state.active || (v.state.pending_seek < 0 && !v.decoder_playing))) {
      v.state.active = false;
      if (!internal(id, false, e))
        return false;
      v.state.internal = false;
      return host_.emit(id, "finished", e);
    }
    break;
  default:
    break;
  }
  return true;
}
void PodunkPlayerNativeMedia::free_voice(Voice &v) {
  if (v.channel >= 0 && audio_ && audio_->available())
    audio_->device().clear(v.channel);
  if (v.channel >= 0 && audio_) {
    std::string error;
    audio_->device().release(this, v.state.object, v.channel, error);
  }
  for (auto &id : leases_)
    if (id == v.state.object)
      id = 0;
  v.channel = -1;
  v.waves = {};
  if (v.samples)
    linearFree(v.samples);
  v.samples = nullptr;
}
bool PodunkPlayerNativeMedia::release(FieldObjectId id, std::string &e) {
  if (!live(e))
    return false;
  auto s = sprites_.find(id);
  if (s != sprites_.end()) {
    if (s->second.entered)
      return fail(e, "Player AnimatedSprite release before actual ExitTree");
    sprites_.erase(s);
    return true;
  }
  auto v = voices_.find(id);
  if (v == voices_.end() || v->second.state.entered)
    return fail(e,
                "Player native audio release before actual callback removal");
  if (!host_.disconnect_bus_layout(id, e))
    return false;
  free_voice(v->second);
  voices_.erase(v);
  return true;
}
bool PodunkPlayerNativeMedia::shutdown(std::string &e) {
  if (!data_)
    return true;
  if (!live(e))
    return false;
  for (const auto &s : sprites_)
    if (s.second.entered)
      return fail(e, "Player media shutdown before Sprite ExitTree");
  for (const auto &v : voices_)
    if (v.second.state.entered)
      return fail(e,
                  "Player media shutdown before AudioServer callback removal");
  for (auto &v : voices_) {
    if (!host_.disconnect_bus_layout(v.first, e))
      return false;
    free_voice(v.second);
  }
  sprites_.clear();
  voices_.clear();
  if (actual_media_lease == this)
    actual_media_lease = nullptr;
  data_ = nullptr;
  resources_ = nullptr;
  tree_ = nullptr;
  registry_ = nullptr;
  audio_ = nullptr;
  host_ = {};
  return true;
}
bool PodunkPlayerNativeMedia::draw(FieldObjectId id,
                                   const FieldTransform &viewport, bool snap,
                                   std::string &e) const {
  if (!node(id, e))
    return false;
  auto it = sprites_.find(id);
  if (it == sprites_.end())
    return fail(e, "Player AnimatedSprite actual draw owner absent");
  const auto &s = it->second;
  if (!tree_->visible_in_tree(id))
    return true;
  if (!s.entered)
    return fail(e, "Player AnimatedSprite draw before actual EnterTree");
  auto *c = clip(s);
  if (!c || s.state.frame >= c->frames.size())
    return fail(e, "Player AnimatedSprite GPU frame outside source clip");
  C2D_Image image{};
  if (!resources_->texture(c->frames[s.state.frame], image, e))
    return false;
  if (!image.tex || !image.subtex || Tex3DS_SubTextureRotated(image.subtex))
    return fail(e, "Player animated GPU source image rejected");
  FieldTransform world;
  if (!tree_->world_transform(id, world, e))
    return false;
  Vec2 x = transform(viewport, world[0]), y = transform(viewport, world[1]),
       origin = transform(viewport, world[2]);
  x.x -= viewport[2].x;
  x.y -= viewport[2].y;
  y.x -= viewport[2].x;
  y.y -= viewport[2].y;
  float sx = std::hypot(x.x, x.y), sy = std::hypot(y.x, y.y);
  if (sx == 0 || sy == 0)
    return true;
  if (std::fabs(x.x * y.x + x.y * y.y) > 1e-5f * sx * sy)
    return fail(e, "Player AnimatedSprite native skew needs quad renderer");
  auto sub = *image.subtex;
  bool negative = x.x * y.y - x.y * y.x < 0;
  if (s.flip_h)
    std::swap(sub.left, sub.right);
  if (s.flip_v != negative)
    std::swap(sub.top, sub.bottom);
  Vec2 offset = s.state.offset;
  if (s.centered) {
    offset.x -= sub.width / 2.f;
    offset.y -= sub.height / 2.f;
  }
  if (snap) {
    offset.x = std::floor(offset.x);
    offset.y = std::floor(offset.y);
  }
  Vec2 center{offset.x + sub.width / 2.f, offset.y + sub.height / 2.f};
  origin.x += x.x * center.x + y.x * center.y;
  origin.y += x.y * center.x + y.y * center.y;
  FieldColor color;
  if (!tree_->effective_color(id, color, e))
    return false;
  C2D_ImageTint tint;
  C2D_PlainImageTint(&tint,
                     C2D_Color32f(color[0], color[1], color[2], color[3]), 0);
  image.subtex = &sub;
  C3D_TexSetFilter(image.tex, GPU_NEAREST, GPU_NEAREST);
  if (!C2D_DrawImageAtRotated(image, origin.x, origin.y, 0,
                              std::atan2(x.y, x.x), &tint, sx, sy))
    return fail(e, "Player AnimatedSprite actual GPU submission failed");
  return true;
}
} // namespace encore::ctr
