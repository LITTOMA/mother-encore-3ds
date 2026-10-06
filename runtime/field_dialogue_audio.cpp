#include "encore/field_dialogue_audio.hpp"
#include <cmath>
namespace encore::upstream {
namespace {
bool reject(std::string &e, const char *s) {
  e = s;
  return false;
}
bool position(double s) { return std::isfinite(s) && s >= 0 && s <= 86400; }
} // namespace
const FieldDialogueAudioState *
FieldDialogueAudioRuntime::state(FieldObjectId id) const {
  auto i = states_.find(id);
  return i == states_.end() ? nullptr : &i->second;
}
FieldDialogueAudioState *FieldDialogueAudioRuntime::get(FieldObjectId id,
                                                        std::string &e) {
  auto i = states_.find(id);
  auto *s = tree_ ? tree_->state(id) : nullptr;
  if (i == states_.end() || !s || !s->alive || s->source != i->second.source) {
    reject(e, "Dialogue audio actual ObjectID lifetime rejected");
    return nullptr;
  }
  return &i->second;
}
bool FieldDialogueAudioRuntime::initialize(const FieldDialogueAudioData &d,
                                           const FieldNodeRecipeData &r,
                                           FieldNodeTreeRuntime &t,
                                           SourceRandom &rng,
                                           FieldDialogueAudioHost h,
                                           std::string &e) {
  auto &b = h.backend;
  if (data_ || !d.valid() || !r.valid() || d.recipe_sha() != r.ir_sha256() ||
      d.identity().scene_id != r.identity().scene_id ||
      d.identity().source_sha256 != r.identity().source_sha256 ||
      d.identity().upstream_commit != r.identity().upstream_commit || !b.bank ||
      !d.verify_bank(*b.bank, e) || !b.create || !b.assign ||
      !b.prepare_assign || !b.register_mix || !b.mix_due || !b.observe ||
      !b.fade_stop || !b.start || !b.pause || !b.mix || !b.stop || !b.release ||
      !b.preflight_play || !h.add_audio_callback || !h.remove_audio_callback ||
      !h.internal_process || !h.emit)
    return reject(e,
                  "Dialogue audio real PCM/NDSP/AudioServer owners incomplete");
  for (const auto &n : d.nodes()) {
    auto *p = r.record(n.id);
    if (!p || p->parent != n.parent || p->ready != n.ready ||
        p->path != n.path || p->native_class != "AudioStreamPlayer" ||
        !p->script.empty() || p->pause != n.pause || p->priority != n.priority)
      return reject(e, "Dialogue audio original native recipe differs");
  }
  data_ = &d;
  recipe_ = &r;
  tree_ = &t;
  random_ = &rng;
  host_ = std::move(h);
  return true;
}
bool FieldDialogueAudioRuntime::attach(FieldObjectId root, std::string &e) {
  if (!data_ || !root || factories_.count(root) || factories_.size() >= 64)
    return reject(e, "Dialogue audio factory ownership rejected");
  auto *root_state = tree_->state(root);
  auto *desc = tree_->descriptor(root);
  FieldIdentity identity{};
  if (!root_state || !root_state->alive || root_state->inside || !desc ||
      desc->id != recipe_->identity().scene_id ||
      !tree_->object_identity(root, identity) ||
      identity.scene_id != data_->identity().scene_id ||
      identity.source_sha256 != data_->identity().source_sha256 ||
      identity.upstream_commit != data_->identity().upstream_commit)
    return reject(e, "Dialogue audio real detached factory required");
  std::vector<FieldDialogueAudioState> pending;
  for (const auto &n : data_->nodes()) {
    FieldObjectId id = 0;
    if (!tree_->get_node(root, n.path, id, e))
      return false;
    auto *p = tree_->descriptor(id);
    if (!p || p->id != n.id || p->native_class != "AudioStreamPlayer" ||
        states_.count(id))
      return reject(e, "Dialogue audio real source object differs");
    FieldDialogueAudioState s;
    s.object = id;
    s.source = n.id;
    s.stream = n.stream;
    s.volume = n.volume;
    s.mix_volume = n.volume;
    s.pitch = n.pitch;
    s.paused = n.paused;
    s.bus = n.bus;
    pending.push_back(s);
  }
  std::vector<FieldObjectId> created;
  for (const auto &s : pending) {
    AudioAsset asset;
    if (!host_.backend.bank->find(s.stream, asset) ||
        !host_.backend.create(s.object, &asset, e)) {
      for (auto id : created) {
        std::string ignored;
        host_.backend.release(id, ignored);
      }
      return false;
    }
    created.push_back(s.object);
  }
  for (auto &s : pending)
    states_.emplace(s.object, s);
  factories_.emplace(root, std::move(created));
  return true;
}
bool FieldDialogueAudioRuntime::observation(FieldDialogueAudioState &s,
                                            FieldDialogueAudioObservation &o,
                                            std::string &e) {
  if (!host_.backend.observe(s.object, o, e))
    return false;
  if (!o.actual_voice || !o.stream_checked ||
      (s.entered && !o.mix_registered) || !std::isfinite(o.position) ||
      o.position < 0)
    return reject(
        e, "Dialogue audio actual checked voice/callback receipt missing");
  return true;
}
bool FieldDialogueAudioRuntime::enter_native(FieldObjectId id, std::string &e) {
  auto *s = get(id, e);
  if (!s || s->entered)
    return reject(e, "Dialogue audio native EnterTree rejected");
  if (!host_.backend.register_mix(id, true, e))
    return false;
  if (!host_.add_audio_callback(
          id,
          [this, id] {
            std::string e;
            return audio_mix(id, e);
          },
          e)) {
    std::string ignored;
    host_.backend.register_mix(id, false, ignored);
    return false;
  }
  s->entered = true;
  if (data_->node(s->source)->autoplay)
    return play(id, 0, e);
  return true;
}
bool FieldDialogueAudioRuntime::ready_native(FieldObjectId id, std::string &e) {
  auto *s = get(id, e);
  if (!s || !s->entered || s->ready)
    return reject(e, "Dialogue audio native Ready rejected");
  FieldDialogueAudioObservation observation_;
  if (!observation(*s, observation_, e))
    return false;
  s->ready = true;
  return true;
}
bool FieldDialogueAudioRuntime::exit_native(FieldObjectId id, std::string &e) {
  auto *s = get(id, e);
  if (!s || !s->entered)
    return reject(e, "Dialogue audio native ExitTree rejected");
  if (!host_.remove_audio_callback(id, e) ||
      !host_.backend.register_mix(id, false, e))
    return false;
  s->entered = false;
  s->ready = false;
  return true;
}
bool FieldDialogueAudioRuntime::release(FieldObjectId root, std::string &e) {
  auto i = factories_.find(root);
  if (i == factories_.end())
    return reject(e, "Dialogue audio unknown factory release");
  for (auto id : i->second) {
    auto s = states_.find(id);
    if (s == states_.end() || s->second.entered)
      return reject(e, "Dialogue audio release before actual ExitTree");
    if (!host_.backend.release(id, e))
      return false;
    states_.erase(s);
  }
  factories_.erase(i);
  return true;
}
bool FieldDialogueAudioRuntime::internal(FieldDialogueAudioState &s,
                                         bool enable, std::string &e) {
  if (s.internal == enable)
    return true;
  if (!host_.internal_process(s.object, enable, e))
    return false;
  s.internal = enable;
  return true;
}
bool FieldDialogueAudioRuntime::set_stream(FieldObjectId id, uint32_t asset_id,
                                           std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  AudioAsset asset;
  const AudioAsset *target = nullptr;
  if (asset_id) {
    auto *binding = data_->asset(asset_id);
    if (!binding || !host_.backend.bank->find(asset_id, asset) ||
        asset.source_path != binding->source ||
        asset.source_sha256 != binding->sha)
      return reject(e,
                    "Dialogue audio unsupported source filename/PCM binding");
    target = &asset;
  }
  // Backend assignment pre-instances/validates the candidate before fading or
  // replacing the live source. Its old source tail is retained independently.
  if (!host_.backend.prepare_assign(id, target, e))
    return false;
  if (s->active && s->stream && !s->paused &&
      !host_.backend.fade_stop(id, data_->fade_replace_frames(),
                               data_->silence_db(), true, e))
    return false;
  if (!host_.backend.assign(id, target, e))
    return false;
  if (s->stream) {
    s->active = false;
    s->pending_seek = -1;
    s->set_stop = false;
  }
  s->stream = asset_id;
  return true;
}
bool FieldDialogueAudioRuntime::phrase_sound(FieldObjectId id,
                                             std::string_view name,
                                             std::string &e) {
  if (!get(id, e))
    return false;
  if (name.empty())
    return set_stream(id, 0, e);
  std::string source = name.substr(0, 6) == "res://"
                           ? std::string(name)
                           : data_->text_prefix() + std::string(name);
  source += data_->extension();
  auto *a = data_->asset(source);
  if (!a)
    return reject(e, "Dialogue phrase sound has no checked original filename");
  return set_stream(id, a->id, e);
}
bool FieldDialogueAudioRuntime::play(FieldObjectId id, double from,
                                     std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  if (!position(from))
    return reject(e, "Dialogue audio source play position rejected");
  if (!s->stream)
    return true;
  if (!host_.backend.preflight_play(id, e))
    return false;
  if (!internal(*s, true, e))
    return false;
  s->pending_seek = from;
  s->stop_priority = false;
  s->active = true;
  return true;
}
bool FieldDialogueAudioRuntime::seek(FieldObjectId id, double to,
                                     std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  if (!position(to))
    return reject(e, "Dialogue audio source seek rejected");
  if (s->stream)
    s->pending_seek = to;
  return true;
}
bool FieldDialogueAudioRuntime::stop(FieldObjectId id, std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  if (s->stream && s->active) {
    s->set_stop = true;
    s->stop_priority = true;
  }
  return true;
}
bool FieldDialogueAudioRuntime::set_volume(FieldObjectId id, float volume,
                                           std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  if (!std::isfinite(volume) || volume < -120 || volume > 24)
    return reject(e, "Dialogue audio source volume outside checked bounds");
  s->volume = volume;
  return true;
}
bool FieldDialogueAudioRuntime::set_pitch(FieldObjectId id, float pitch,
                                          std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  if (!std::isfinite(pitch) || pitch <= 0 || pitch > 4)
    return reject(e, "Dialogue audio source pitch outside checked bounds");
  s->pitch = pitch;
  return true;
}
bool FieldDialogueAudioRuntime::set_bus(FieldObjectId id, std::string_view bus,
                                        std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  if (bus != data_->node(s->source)->bus)
    return reject(e, "Dialogue audio unknown bus graph rejected");
  s->bus = bus;
  return true;
}
bool FieldDialogueAudioRuntime::set_paused(FieldObjectId id, bool paused,
                                           std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  if (paused != s->paused) {
    if (!paused && !host_.backend.pause(id, false, e))
      return false;
    s->paused = paused;
    s->paused_fade = paused;
  }
  return true;
}
bool FieldDialogueAudioRuntime::tree_pause(FieldObjectId id, bool paused,
                                           bool can_process, std::string &e) {
  if (paused && !can_process)
    return set_paused(id, true, e);
  if (!paused)
    return set_paused(id, false, e);
  return get(id, e) != nullptr;
}
bool FieldDialogueAudioRuntime::character_sound(FieldObjectId id, bool delay,
                                                std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  if (!s->stream || delay)
    return true;
  if (!host_.backend.preflight_play(id, e))
    return false;
  auto range = data_->pitch_range();
  float pitch = float(random_->rand_range(range[0], range[1]));
  return set_pitch(id, pitch, e) && play(id, 0, e);
}
bool FieldDialogueAudioRuntime::audio_mix(FieldObjectId id, std::string &e) {
  auto *s = get(id, e);
  if (!s || !s->entered)
    return reject(e, "Dialogue audio mix outside actual callback owner");
  bool due = false;
  if (!host_.backend.mix_due(id, due, e))
    return false;
  if (!due)
    return true;
  FieldDialogueAudioObservation o;
  if (!observation(*s, o, e))
    return false;
  if (!s->stream || !s->active || (s->paused && !s->paused_fade))
    return true;
  if (s->paused) {
    if (s->paused_fade && o.playback_playing) {
      if (!host_.backend.fade_stop(id, data_->fade_stop_frames(),
                                   data_->silence_db(), false, e) ||
          !host_.backend.pause(id, true, e))
        return false;
      s->paused_fade = false;
    }
    return true;
  }
  if (s->set_stop) {
    if (!host_.backend.fade_stop(id, data_->fade_stop_frames(),
                                 data_->silence_db(), false, e) ||
        !host_.backend.stop(id, e))
      return false;
    s->set_stop = false;
  }
  if (s->pending_seek >= 0 && !s->stop_priority) {
    if (o.playback_playing &&
        !host_.backend.fade_stop(id, data_->fade_stop_frames(),
                                 data_->silence_db(), false, e))
      return false;
    if (!host_.backend.start(id, s->pending_seek, s->pitch, e))
      return false;
    s->pending_seek = -1;
    s->mix_volume = s->volume;
  }
  s->stop_priority = false;
  if (!host_.backend.mix(id, s->mix_volume, s->volume, s->pitch, s->bus, e))
    return false;
  s->mix_volume = s->volume;
  return true;
}
bool FieldDialogueAudioRuntime::internal_process(FieldObjectId id,
                                                 std::string &e) {
  auto *s = get(id, e);
  if (!s || !s->entered || !s->internal)
    return reject(e, "Dialogue audio unknown native internal process");
  FieldDialogueAudioObservation o;
  if (!observation(*s, o, e))
    return false;
  if (!s->active || (s->pending_seek < 0 && !o.playback_playing)) {
    s->active = false;
    if (!internal(*s, false, e))
      return false;
    return host_.emit(id, data_->finished_signal(), e);
  }
  return true;
}
bool FieldDialogueAudioRuntime::is_playing(FieldObjectId id, bool &out,
                                           std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  out = s->stream && s->active && !s->set_stop;
  return true;
}
bool FieldDialogueAudioRuntime::playback_position(FieldObjectId id, double &out,
                                                  std::string &e) {
  auto *s = get(id, e);
  if (!s)
    return false;
  if (!s->stream) {
    out = 0;
    return true;
  }
  if (s->pending_seek >= 0) {
    out = s->pending_seek;
    return true;
  }
  FieldDialogueAudioObservation o;
  if (!observation(*s, o, e))
    return false;
  out = o.position;
  return true;
}
} // namespace encore::upstream
