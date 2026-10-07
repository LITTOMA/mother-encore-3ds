#include "podunk_named_sfx.hpp"
#include <algorithm>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
std::string binding_text(const PlayerNamedSfxData &d, const char *key) {
  auto v = d.constructor()->get(key);
  return v && v->kind == 4 ? v->string : std::string{};
}
bool native_name(std::string_view s) {
  return !s.empty() && s.size() <= 65536 &&
         s.find_first_of("/\\:@\n\r") == s.npos && s != "." && s != ".." &&
         s.find('\0') == s.npos;
}
} // namespace
class PodunkNamedSfx::StreamResource final : public FieldGlobalSourceResource {
public:
  StreamResource(FieldGlobalExternalBinding b, const PlayerNamedSfxStream &s)
      : binding_(std::move(b)), source_(s) {}
  FieldGlobalExternalBinding binding() const override { return binding_; }
  const char *resource_class() const override {
    return source_.audio.native_class.c_str();
  }
  bool state(FieldGlobalExternalState &s, std::string &e) const override {
    s = {};
    s.name = binding_.source.name;
    e.clear();
    return true;
  }
  bool deferred(const FieldDeferredMessage &, std::string &e) override {
    return fail(e, "Named SFX immutable native stream method unsupported");
  }
  bool persist_append(FieldObjectId, std::string &e) override {
    return fail(e, "AudioStream is not a persistent Node");
  }
  bool assign_stable_canvas(FieldObjectId, std::string &e) override {
    return fail(e, "AudioStream has no stable Canvas member");
  }
  const PlayerNamedSfxStream &source() const { return source_; }

private:
  FieldGlobalExternalBinding binding_;
  PlayerNamedSfxStream source_;
};
class PodunkNamedSfx::ManagerObject final : public FieldGlobalExternalObject,
                                            public PodunkExternalNodeLifecycle {
public:
  explicit ManagerObject(PodunkNamedSfx &h) : h_(h) {}
  FieldGlobalExternalBinding binding() const override { return h_.binding_; }
  bool state(FieldGlobalExternalState &s, std::string &e) const override {
    return h_.manager_state(s, e);
  }
  bool deferred(const FieldDeferredMessage &m, std::string &e) override {
    return h_.deferred(m, e);
  }
  bool persist_append(FieldObjectId, std::string &e) override {
    return fail(e, "audManager has no source persist_append");
  }
  bool assign_stable_canvas(FieldObjectId, std::string &e) override {
    return fail(e, "audManager has no stable Canvas property");
  }
  bool stage_parent(FieldObjectId p, std::string &e) override {
    if (h_.parent_ || p != h_.registry_->root() ||
        h_.tree_->state(h_.object())->inside)
      return fail(e, "audManager actual native parent assignment rejected");
    h_.parent_ = p;
    return true;
  }
  bool native_notification(FieldTreePhase p, std::string &e) override {
    if (p != FieldTreePhase::Parented && p != FieldTreePhase::Unparented &&
        p != FieldTreePhase::ChildMoved && p != FieldTreePhase::PathChanged)
      return fail(e, "audManager native notification unsupported");
    if (p == FieldTreePhase::Unparented)
      h_.parent_ = 0;
    return true;
  }
  bool enter(FieldObjectId p, std::string &e) override {
    return p == h_.parent_ && p == h_.root_->external_parent(h_.object())
               ? h_.tree_->enter_branch_only(e)
               : fail(e, "audManager staged actual root parent differs");
  }
  bool ready(std::string &e) override {
    return h_.tree_->ready_entered_branch(e);
  }
  bool exit(std::string &e) override { return h_.tree_->exit(e); }

private:
  PodunkNamedSfx &h_;
};
bool PodunkNamedSfx::prepare(std::shared_ptr<const PlayerNamedSfxData> d,
                             FieldGlobalRegistry &r, PodunkNativeRoot &root,
                             FieldObjectSignals &s, PodunkAudioServer &server,
                             AudioPlayer &a, MusicRegionService &music,
                             std::string &e) {
  if (data_ || !d || !d->valid() || !r.data() || s.registry() != &r ||
      server.registry() != &r || !server.alive() || !a.available() ||
      d->identity().upstream_commit != r.data()->identity().upstream_commit)
    return fail(e, "Named SFX source/actualRegistry/AudioServer/DSP rejected");
  data_ = std::move(d);
  registry_ = &r;
  root_ = &root;
  signals_ = &s;
  server_ = &server;
  audio_ = &a;
  music_ = &music;
  tree_ = std::make_shared<FieldNodeTreeRuntime>();
  e.clear();
  return true;
}
FieldNodeTreeHost PodunkNamedSfx::tree_host() {
  FieldNodeTreeHost h;
  h.object_domain = registry_->kernel();
  h.allocate_object = [this](FieldObjectId &id, std::string &e) {
    if (!first_) {
      first_ = true;
      id = object();
      return registry_->allocation_pending(id);
    }
    return registry_->allocate_object(id, e);
  };
  h.allocate_fast_name = [this](uint64_t &id, std::string &e) {
    return registry_->allocate_fast_name(id, e);
  };
  h.native_allocated = [this](FieldObjectId id, const FieldNodeDescriptor &,
                              const FieldIdentity &, std::string &e) {
    return registry_->publish_allocated_node(
        tree_, id,
        [this](const FieldDeferredMessage &m, std::string &e) {
          return deferred(m, e);
        },
        e);
  };
  h.construct_source = [this](FieldObjectId id, const FieldNodeDescriptor &n,
                              const FieldIdentity &i, std::string &e) {
    return construct_source(id, n, i, e);
  };
  h.bind = [this](FieldObjectId id, const FieldNodeDescriptor &n,
                  FieldNodeBinding &b,
                  std::string &e) { return bind(id, n, b, e); };
  h.dispatch = [this](FieldObjectId id, const FieldNodeBinding &b,
                      FieldTreePhase p,
                      std::string &e) { return phase(id, b, p, e); };
  h.deferred = [this](const FieldDeferredMessage &m, std::string &e) {
    return deferred(m, e);
  };
  h.enqueue_global = [this](FieldDeferredMessage m, std::string &e) {
    return registry_->enqueue(std::move(m), e);
  };
  h.flush_global = [this](std::string &e) {
    return registry_->flush_messages(e);
  };
  h.object_exists = [this](FieldObjectId id) {
    return registry_->object_exists(id);
  };
  h.input_registration = [this](FieldObjectId id, uint32_t k, bool active,
                                std::string &e) {
    return root_->input_registration(id, k, active, e);
  };
  h.external_pause_process = [this](FieldObjectId) { return false; };
  h.external_path = [this](FieldObjectId id, std::string_view p,
                           FieldObjectId &out, std::string &e) {
    return registry_->resolve_path(id, p, out, e);
  };
  h.release = [this](FieldObjectId id, const FieldNodeBinding &,
                     std::string &e) {
    if (voices_.owns(id) && !voices_.release(id, e))
      return false;
    if (!signals_->release(id, e))
      return false;
    constructed_.erase(id);
    native_entered_.erase(id);
    native_ready_.erase(id);
    voice_streams_.erase(id);
    music_nodes_.erase(id);
    return true;
  };
  return h;
}
bool PodunkNamedSfx::construct(FieldObjectId id,
                               const FieldGlobalExternalSpec &s,
                               std::unique_ptr<FieldGlobalExternalObject> &out,
                               std::string &e) {
  if (!data_ || object() || failed_ || !registry_->allocation_pending(id) ||
      s.role != 3 || s.source != data_->recipe().source_scene() ||
      s.source_sha != data_->identity().source_sha256)
    return fail(e, "Named SFX original autoload constructor rejected");
  const auto *n = data_->recipe().record(data_->identity().scene_id);
  if (!n || n->script != s.script || n->script_sha != s.script_sha ||
      n->native_class != s.native_class)
    return fail(e, "Named SFX exact source script/native binding rejected");
  binding_ = {id, s, 0x454e0073, 1};
  auto owner = std::make_unique<ManagerObject>(*this);
  if (!tree_->initialize_recipe(data_->recipe(), tree_host(), e) ||
      tree_->root() != id || !tree_->set_name(id, s.name, e) ||
      !registry_->publish_branch(
          tree_, id,
          [this](const FieldDeferredMessage &m, std::string &e) {
            return deferred(m, e);
          },
          e) ||
      !voices_.prepare_named(*data_, *tree_, *registry_, *audio_,
                             server_->media_host(), e) ||
      !root_->register_external_child(*owner, *owner, e)) {
    failed_ = true;
    return false;
  }
  if (!tree_->get_node(id, binding_text(*data_, "sfx_parent"), sfx_parent_,
                       e) ||
      !tree_->get_node(id, binding_text(*data_, "music_parent"), music_parent_,
                       e))
    return false;
  out = std::move(owner);
  e.clear();
  return true;
}
bool PodunkNamedSfx::construct_source(FieldObjectId id,
                                      const FieldNodeDescriptor &n,
                                      const FieldIdentity &i, std::string &e) {
  const auto *s = tree_->state(id);
  if (!s || s->inside || s->parent || !s->name.empty() ||
      constructed_.count(id) || registry_->tree_owner(id) != tree_)
    return fail(e, "Named SFX actual native construction order rejected");
  if (n.id == data_->voice_descriptor().id) {
    if (!same(i, data_->voice_identity()) ||
        n.native_class != data_->voice_descriptor().native_class ||
        !n.script.empty() ||
        (constructing_music_ ? false : !voices_.construct_named(id, n, i, e)))
      return false;
    if (constructing_music_)
      music_nodes_.emplace(id, *constructing_music_);
    else
      voice_streams_[id] = 0;
  } else {
    const auto *r = data_->recipe().record(n.id);
    if (!r || !same(i, data_->identity()) ||
        r->native_class != n.native_class || r->script != n.script ||
        r->script_sha != n.script_sha)
      return fail(e, "Named SFX source recipe constructor differs");
    if (n.native_class == "Control" && !data_->recipe().control(n.id))
      return fail(e, "Named SFX source Control fields absent");
    if (id == object()) {
      for (auto &v : data_->effects()->array) {
        auto name = v->get("name"), source = v->get("source");
        FieldObjectId resource = 0;
        if (!this->resource(source->string, resource, e))
          return false;
        sound_effects_.emplace_back(name->string, resource);
      }
      music_changers_.clear();
      overworld_battle_music_ = false;
      tween_ = 0;
    }
  }
  constructed_.insert(id);
  return true;
}
bool PodunkNamedSfx::bind(FieldObjectId id, const FieldNodeDescriptor &n,
                          FieldNodeBinding &b, std::string &e) {
  if (!constructed_.count(id) || registry_->tree_owner(id) != tree_)
    return fail(e, "Named SFX actual owned Node absent");
  if (voices_.owns(id))
    return voices_.bind(id, b, e);
  if (music_nodes_.count(id)) {
    b = {};
    b.identity = data_->voice_identity();
    b.stable_id = n.id;
    b.class_index = n.class_index;
    b.family = 0x454e0073;
    b.capability = 1;
    b.native_class = n.native_class;
    return true;
  }
  b = {};
  b.identity = data_->identity();
  b.stable_id = n.id;
  b.class_index = n.class_index;
  b.family = 0x454e0073;
  b.capability = 1;
  b.script_sha = n.script_sha;
  b.native_class = n.native_class;
  return true;
}
bool PodunkNamedSfx::phase(FieldObjectId id, const FieldNodeBinding &b,
                           FieldTreePhase p, std::string &e) {
  if (!constructed_.count(id) || b.family != 0x454e0073 ||
      registry_->tree_owner(id) != tree_)
    return fail(e, "Named SFX actual lifecycle owner differs");
  if ((p == FieldTreePhase::NodeAdded || p == FieldTreePhase::NodeRemoved ||
       p == FieldTreePhase::ChildEntered || p == FieldTreePhase::ChildExiting ||
       p == FieldTreePhase::ReadyNative || p == FieldTreePhase::ReadyScript) &&
      !root_->node_notification(id, p, e))
    return false;
  if (voices_.owns(id) && !voices_.phase(id, p, 0, false, false, e))
    return false;
  if (p == FieldTreePhase::EnterNative) {
    if (!native_entered_.insert(id).second)
      return fail(e, "Named SFX repeated native Enter");
  } else if (p == FieldTreePhase::ExitNative) {
    if (!native_entered_.erase(id))
      return fail(e, "Named SFX native Exit without Enter");
  } else if (p == FieldTreePhase::ReadyNative) {
    if (!native_entered_.count(id) || !native_ready_.insert(id).second)
      return fail(e, "Named SFX native Ready before actual Enter");
  } else if (p == FieldTreePhase::ReadyScript && id == object()) {
    if (!source_ready(e))
      return false;
  } else if (p == FieldTreePhase::IdleInternal && !voices_.owns(id))
    return fail(e, "Named SFX inactive Tween has no admitted interpolation");
  const char *signal = nullptr;
  if (p == FieldTreePhase::TreeEntered)
    signal = "tree_entered";
  else if (p == FieldTreePhase::ReadySignal)
    signal = "ready";
  else if (p == FieldTreePhase::TreeExiting)
    signal = "tree_exiting";
  else if (p == FieldTreePhase::TreeExited)
    signal = "tree_exited";
  if (signal && !signals_->emit(id, signal, {}, e))
    return false;
  return true;
}
bool PodunkNamedSfx::manager_state(FieldGlobalExternalState &s,
                                   std::string &e) const {
  if (!tree_ || !object())
    return fail(e, "Named SFX actual manager object absent");
  const auto *n = tree_->state(object());
  if (!n)
    return fail(e, "Named SFX manager Node removed");
  s = {};
  s.name = n->name;
  s.parent = parent_;
  s.children = n->children;
  s.inside = n->inside;
  s.ready = script_ready_ && n->ready_notified;
  e.clear();
  return true;
}
bool PodunkNamedSfx::live(std::string &e) const {
  const auto *n = tree_ ? tree_->state(object()) : nullptr;
  return data_ && data_->valid() && !failed_ && registry_ &&
                 !registry_->poisoned() && n && n->inside &&
                 parent_ == root_->external_parent(object())
             ? true
             : fail(e, "Named SFX actual entered manager parent unavailable");
}
bool PodunkNamedSfx::resource(std::string_view path, FieldObjectId &out,
                              std::string &e) {
  if (path.compare(0, 6, "res://") == 0)
    path.remove_prefix(6);
  auto *v = data_ ? data_->stream(path) : nullptr;
  if (!v)
    return fail(e, "Named SFX unknown original stream path");
  auto found = resources_.find(v->audio.id);
  if (found != resources_.end()) {
    if (registry_->source_resource(found->second.first) != found->second.second)
      return fail(e, "Named SFX source stream cache ObjectDB mismatch");
    out = found->second.first;
    return true;
  }
  FieldGlobalExternalSpec s;
  s.role = 4;
  s.stable_id = v->audio.id;
  s.identity = data_->identity();
  s.identity.scene_id = v->audio.id;
  s.identity.source_sha256 = v->audio.source_sha;
  s.source = v->audio.source;
  s.source_sha = v->audio.source_sha;
  s.native_class = v->audio.native_class;
  auto slash = s.source.find_last_of('/');
  s.name = s.source.substr(slash == s.source.npos ? 0 : slash + 1);
  FieldObjectId id = 0;
  if (!registry_->allocate_object(id, e))
    return false;
  auto resource = std::make_unique<StreamResource>(
      FieldGlobalExternalBinding{id, s, 0x454e0073, 1}, *v);
  const auto *owner = resource.get();
  if (!registry_->publish_source_resource(s, id, std::move(resource), e))
    return false;
  resources_[v->audio.id] = {id, owner};
  out = id;
  return true;
}
bool PodunkNamedSfx::create_voice(FieldObjectId parent, std::string_view bus,
                                  std::string_view name, FieldObjectId &out,
                                  std::string &e) {
  if (!live(e) || !tree_->state(parent) || !tree_->state(parent)->inside ||
      (!name.empty() && !native_name(name)))
    return fail(e, "Named SFX actual parent/name rejected");
  FieldObjectId id = 0;
  if (!tree_->instantiate_audio_source(data_->voice_identity(),
                                       data_->voice_descriptor(), id, e) ||
      !voices_.set_bus(id, bus, e))
    return false;
  if (!name.empty() && !tree_->set_name(id, name, e))
    return false;
  if (!tree_->add_child(parent, id, e))
    return false;
  out = id;
  return true;
}
bool PodunkNamedSfx::get_sfx(std::string_view name, FieldObjectId &out,
                             std::string &e) const {
  if (!live(e) || !native_name(name))
    return fail(e, "Named SFX original get_node name rejected");
  const auto *p = tree_->state(sfx_parent_);
  if (!p || !p->inside)
    return fail(e, "Named SFX source Sfx parent absent");
  for (auto id : p->children) {
    const auto *n = tree_->state(id);
    if (n && n->name == name) {
      if (!voices_.owns(id) ||
          tree_->descriptor(id)->native_class != "AudioStreamPlayer")
        return fail(e, "Named SFX name refers to foreign native child");
      out = id;
      return true;
    }
  }
  out = 0;
  e.clear();
  return true;
}
bool PodunkNamedSfx::add_sfx(std::string_view source, std::string_view name,
                             FieldObjectId &out, std::string &e) {
  FieldObjectId stream = 0;
  return resource(source, stream, e) && add_sfx(stream, name, out, e);
}
bool PodunkNamedSfx::add_sfx(FieldObjectId stream, std::string_view name,
                             FieldObjectId &out, std::string &e) {
  if (!get_sfx(name, out, e))
    return false;
  if (!out && !create_voice(sfx_parent_, data_->bus(), name, out, e))
    return false;
  return set_stream(out, stream, e);
}
bool PodunkNamedSfx::set_stream(FieldObjectId id, FieldObjectId resource,
                                std::string &e) {
  if (!live(e) || !voices_.owns(id))
    return fail(e, "Named SFX stream setter wrong actual Node");
  uint32_t source = 0;
  if (resource) {
    for (auto &v : resources_)
      if (v.second.first == resource &&
          registry_->source_resource(resource) == v.second.second)
        source = v.first;
    if (!source)
      return fail(e,
                  "Named SFX stream setter requires actual checked Resource");
  }
  if (!voices_.set_stream(id, source, e))
    return false;
  voice_streams_[id] = resource;
  return true;
}
bool PodunkNamedSfx::play_sfx(std::string_view source, std::string_view name,
                              FieldObjectId &out, std::string &e) {
  return add_sfx(source, name, out, e) && play(out, e);
}
bool PodunkNamedSfx::play(FieldObjectId id, std::string &e) {
  return live(e) && voices_.play(id, 0, e);
}
bool PodunkNamedSfx::stop(FieldObjectId id, std::string &e) {
  return live(e) && voices_.stop(id, e);
}
bool PodunkNamedSfx::state(FieldObjectId id, PodunkSceneAudioState &s,
                           FieldObjectId &stream, std::string &e) const {
  if (!live(e) || !voices_.state(id, s, e))
    return false;
  auto i = voice_streams_.find(id);
  if (i == voice_streams_.end())
    return fail(e, "Named SFX native stream owner absent");
  stream = i->second;
  if (stream && !registry_->source_resource(stream))
    return fail(e, "Named SFX stream Resource was removed");
  return true;
}
bool PodunkNamedSfx::music_state(FieldObjectId id, MusicSourcePlayer &out,
                                 std::string &e) const {
  auto n = music_nodes_.find(id);
  if (n == music_nodes_.end() || registry_->tree_owner(id) != tree_)
    return fail(e, "Named Music native Node has no same backend owner");
  std::vector<MusicSourcePlayer> actual;
  if (!music_->source_players(*audio_, actual, e))
    return false;
  auto found = std::find_if(actual.begin(), actual.end(), [&](const auto &v) {
    return v.kind == n->second.kind &&
           v.player_identity == n->second.player_identity &&
           v.order == n->second.order;
  });
  if (found == actual.end())
    return fail(e, "Named Music original playback instance retired");
  out = *found;
  return true;
}
bool PodunkNamedSfx::synchronize_music_children(std::string &e) {
  if (!live(e) || !music_)
    return fail(e, "Named Music actual manager/service not entered");
  std::vector<MusicSourcePlayer> actual;
  if (!music_->source_players(*audio_, actual, e))
    return false;
  std::vector<FieldObjectId> ordered;
  for (const auto &v : actual) {
    auto found = std::find_if(
        music_nodes_.begin(), music_nodes_.end(), [&](const auto &n) {
          return n.second.kind == v.kind &&
                 n.second.player_identity == v.player_identity;
        });
    FieldObjectId id = 0;
    if (found == music_nodes_.end()) {
      constructing_music_ = &v;
      bool created = tree_->instantiate_audio_source(
          data_->voice_identity(), data_->voice_descriptor(), id, e);
      constructing_music_ = nullptr;
      if (!created || !tree_->add_child(music_parent_, id, e))
        return false;
    } else {
      id = found->first;
      if (found->second.order != v.order)
        return fail(e,
                    "Named Music retained source child creation order changed");
    }
    ordered.push_back(id);
  }
  for (auto it = music_nodes_.begin(); it != music_nodes_.end();) {
    if (std::find(ordered.begin(), ordered.end(), it->first) == ordered.end()) {
      if (!tree_->queue_free(it->first, e))
        return false;
      // Preserve the actual deferred child until the shared deletion boundary.
      ++it;
    } else
      ++it;
  }
  const auto *p = tree_->state(music_parent_);
  if (!p)
    return fail(e, "Named Music AudioPlayers source parent missing");
  for (auto id : p->children)
    if (std::find(ordered.begin(), ordered.end(), id) == ordered.end()) {
      const auto *n = tree_->state(id);
      PodunkSceneAudioState idle;
      const bool retained_idle = voices_.owns(id) && voice_streams_.at(id) == 0 &&
                                 voices_.state(id, idle, e) && !idle.playing;
      if (!n || (!n->queued && !retained_idle))
        return fail(e,
                    "Named Music actual source parent contains unknown child");
    }
  // Actual children retained until deletion must not be silently hidden from
  // source get_child_count or `_add_at_zero`.
  imported_music_ = p->children;
  music_bound_ = true;
  e.clear();
  return true;
}
bool PodunkNamedSfx::source_ready(std::string &e) {
  if (!live(e) || script_ready_)
    return fail(
        e, "audManager Ready awaits actual surviving Music source child graph");
  if (!tree_->get_node(object(), binding_text(*data_, "tween_node"), tween_, e))
    return false;
  if (!synchronize_music_children(e))
    return false;
  const auto *p = tree_->state(music_parent_);
  if (!p || p->children != imported_music_)
    return fail(e, "audManager AudioPlayers source graph changed before Ready");
  if (p->children.empty()) {
    const auto observed = audio_->observe_music();
    if (observed.present)
      return fail(
          e,
          "audManager zero child count contradicts actual bounded Music owner");
    FieldObjectId idle = 0;
    if (!create_voice(music_parent_, data_->music_bus(), {}, idle, e))
      return false;
  }
  script_ready_ = true;
  return true;
}
bool PodunkNamedSfx::signal_declaration(FieldObjectId id,
                                        std::string_view signal,
                                        uint32_t &arity, std::string &e) const {
  if (voices_.owns(id) && signal == "finished")
    return voices_.signal_declaration(id, signal, arity, e);
  if (!constructed_.count(id))
    return fail(e, "Named SFX unknown signal object");
  if (signal == "tree_entered" || signal == "tree_exiting" ||
      signal == "tree_exited" || signal == "ready" || signal == "renamed") {
    arity = 0;
    return true;
  }
  return fail(e, "Named SFX undeclared native signal");
}
bool PodunkNamedSfx::deferred(const FieldDeferredMessage &m, std::string &e) {
  if (voices_.owns(m.object)) {
    if (m.kind == FieldDeferredKind::Set && m.member == "stream" &&
        m.args.size() == 1) {
      if (std::holds_alternative<std::monostate>(m.args[0]))
        return set_stream(m.object, 0, e);
      auto ref = std::get_if<FieldObjectRef>(&m.args[0]);
      return ref ? set_stream(m.object, ref->id, e)
                 : fail(e, "Named SFX native stream Variant rejected");
    }
    return voices_.deferred(m, e);
  }
  return fail(
      e, "audManager unimplemented source method/Tween interpolation rejected");
}
bool PodunkNamedSfx::process(bool physics, bool paused, std::string &e) {
  return live(e) && synchronize_music_children(e) &&
         voices_.tree_pause(paused, e) && tree_->process(physics, paused, e);
}
} // namespace encore::ctr
