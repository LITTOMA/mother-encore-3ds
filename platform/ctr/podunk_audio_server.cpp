#include "podunk_audio_server.hpp"
#include <algorithm>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *m) {
  e = m;
  return false;
}
} // namespace
bool PodunkAudioServer::alive() const {
  return data_ && data_->valid() && registry_ && signals_ &&
         signals_->registry() == registry_ && audio_ && audio_->available();
}
bool PodunkAudioServer::live(std::string &e) const {
  if (!alive() || !binding_.object ||
      !registry_->object_exists(binding_.object))
    return fail(e, "Shared AudioServer actual owner/backend is not live");
  return true;
}
FieldGlobalExternalBinding PodunkAudioServer::binding() const {
  return binding_;
}
bool PodunkAudioServer::checked_source_hash(std::string_view p,
                                            std::array<uint8_t, 32> &h) const {
  return data_ && data_->source_hash(p, h);
}
bool PodunkAudioServer::create(const AudioServerData &d, FieldGlobalRegistry &r,
                               FieldObjectSignals &s, AudioPlayer &a,
                               PodunkAudioServer *&out, std::string &e) {
  if (!d.valid() || !r.data() ||
      d.identity().upstream_commit != r.data()->identity().upstream_commit ||
      s.registry() != &r || !a.available())
    return fail(e,
                "AudioServer same Registry/signals/initialized NDSP required");
  auto owned = std::make_unique<PodunkAudioServer>();
  owned->data_ = &d;
  owned->registry_ = &r;
  owned->signals_ = &s;
  owned->audio_ = &a;
  if (!owned->body_.initialize(d, e))
    return false;
  FieldObjectId id = 0;
  if (!r.allocate_object(id, e))
    return false;
  auto &b = owned->binding_;
  b.object = id;
  b.family = 0x454e006e;
  b.capability = 1;
  b.source.identity = d.identity();
  b.source.stable_id = d.identity().scene_id;
  b.source.role = 5;
  b.source.native_class = "AudioServer";
  b.source.source = d.engine_source();
  b.source.source_sha = d.identity().source_sha256;
  // This native singleton is a real Object body, not a Node/Reference and it
  // neither grants any source Ready cursor nor initializes the DSP again.
  auto *actual = owned.get();
  auto spec = b.source;
  if (!r.publish_native_object(spec, id, std::move(owned), e)) {
    std::string cleanup;
    r.retire_object(id, cleanup);
    return false;
  }
  out = actual;
  e.clear();
  return true;
}
bool PodunkAudioServer::callback_valid(FieldObjectId id,
                                       PodunkNativeAudioCallback c,
                                       std::string &e) const {
  if (!live(e) || !id || !c.function || !c.userdata ||
      c.userdata->object != id || c.userdata->registry != registry_ ||
      !c.userdata->owner || !c.userdata->mix_owner ||
      !registry_->object_exists(id))
    return fail(e, "AudioServer actual callback body/Registry rejected");
  auto t = registry_->tree_owner(id);
  auto *d = t ? t->descriptor(id) : nullptr;
  if (!d)
    return fail(e, "AudioServer callback lacks actual native Node");
  if ((d->native_class == "AudioStreamPlayer" &&
       c.function != podunk_audio_stream_player_mix) ||
      (d->native_class == "AudioStreamPlayer2D" &&
       c.function != podunk_audio_stream_player_2d_mix) ||
      (d->native_class != "AudioStreamPlayer" &&
       d->native_class != "AudioStreamPlayer2D"))
    return fail(e, "AudioServer callback native class/function mismatch");
  return true;
}
bool PodunkAudioServer::add(FieldObjectId id, PodunkNativeAudioCallback c,
                            std::string &e) {
  if (pumping_)
    return fail(e,
                "AudioServer callback-list mutation during mix is unsupported");
  if (!callback_valid(id, c, e))
    return false;
  Key k{c.function, c.userdata};
  auto old = by_object_.find(id);
  if (old != by_object_.end()) {
    if (old->second.function == k.function &&
        old->second.userdata == k.userdata) {
      e.clear();
      return true;
    }
    return fail(e, "AudioServer same Node changed its native callback body");
  }
  if (!callbacks_.emplace(k, id).second)
    return fail(e, "AudioServer callback userdata aliases another actual Node");
  by_object_.emplace(id, k);
  e.clear();
  return true;
}
bool PodunkAudioServer::remove(FieldObjectId id, std::string &e) {
  if (!live(e) || pumping_)
    return fail(
        e,
        "AudioServer callback removal during mix/without live owner rejected");
  auto i = by_object_.find(id);
  if (i != by_object_.end()) {
    callbacks_.erase(i->second);
    by_object_.erase(i);
  }
  e.clear();
  return true;
}
bool PodunkAudioServer::pump(std::string &e) {
  if (!live(e) || pumping_)
    return fail(e, "Shared AudioServer mix reentry/backend rejected");
  pumping_ = true;
  for (const auto &c : callbacks_) {
    PodunkNativeAudioCallback actual{c.first.function, c.first.userdata};
    if (!callback_valid(c.second, actual, e) ||
        !actual.function(actual.userdata, e)) {
      pumping_ = false;
      return false;
    }
  }
  pumping_ = false;
  e.clear();
  return true;
}
bool PodunkAudioServer::connect_layout(FieldObjectId id,
                                       std::function<bool()> callback,
                                       std::string &e) {
  if (!live(e) || !callback || !registry_->object_exists(id) ||
      layout_.count(id))
    return fail(e, "AudioServer actual native layout observer rejected");
  if (!signals_->connect(binding_.object, data_->layout_signal(), id,
                         data_->layout_method(), 0, {}, e))
    return false;
  layout_.emplace(id, std::move(callback));
  e.clear();
  return true;
}
bool PodunkAudioServer::disconnect_layout(FieldObjectId id, std::string &e) {
  if (!live(e) || !layout_.count(id))
    return fail(e, "AudioServer native layout observer absent");
  if (!signals_->disconnect(binding_.object, data_->layout_signal(), id,
                            data_->layout_method(), e))
    return false;
  layout_.erase(id);
  e.clear();
  return true;
}
bool PodunkAudioServer::signal_declaration(FieldObjectId id,
                                           std::string_view name,
                                           uint32_t &args,
                                           std::string &e) const {
  if (!live(e) || id != binding_.object || name != data_->layout_signal())
    return fail(e, "AudioServer signal declaration/source rejected");
  args = 0;
  e.clear();
  return true;
}
bool PodunkAudioServer::handles_layout(const FieldDeferredMessage &m) const {
  return data_ && m.kind == FieldDeferredKind::Call &&
         m.member == data_->layout_method() && layout_.count(m.object);
}
bool PodunkAudioServer::layout_callback(const FieldDeferredMessage &m,
                                        std::string &e) {
  if (!live(e) || !handles_layout(m) || !m.args.empty() ||
      !registry_->object_exists(m.object))
    return fail(e,
                "AudioServer native layout callback target/arguments rejected");
  if (!layout_.at(m.object)())
    return fail(e, "AudioServer actual voice rejected changed bus graph");
  e.clear();
  return true;
}
bool PodunkAudioServer::set_bus_volume(uint32_t i, float v, std::string &e) {
  return live(e) && body_.set_volume(i, v, e);
}
int PodunkAudioServer::get_bus_index(std::string_view n) const {
  return alive() ? body_.index(n) : -1;
}
PodunkPlayerMediaHost PodunkAudioServer::media_host() {
  PodunkPlayerMediaHost h;
  h.add_audio_callback = [this](auto id, auto cb, auto &e) {
    return add(id, cb, e);
  };
  h.remove_audio_callback = [this](auto id, auto &e) { return remove(id, e); };
  h.bus_gain = [this](auto n, auto &db, auto &muted, auto &e) {
    return live(e) && body_.gain(n, db, muted, e);
  };
  h.connect_bus_layout = [this](auto id, auto cb, auto &e) {
    return connect_layout(id, std::move(cb), e);
  };
  h.disconnect_bus_layout = [this](auto id, auto &e) {
    return disconnect_layout(id, e);
  };
  h.emit = [this](auto id, auto n, auto &e) {
    return live(e) && signals_->emit(id, n, {}, e);
  };
  h.connect = [this](auto id, auto n, auto target, auto method, auto flags,
                     auto &e) {
    return live(e) && signals_->connect(id, n, target, method, flags, {}, e);
  };
  h.disconnect = [this](auto id, auto n, auto target, auto method, auto &e) {
    return live(e) && signals_->disconnect(id, n, target, method, e);
  };
  return h;
}
} // namespace encore::ctr
