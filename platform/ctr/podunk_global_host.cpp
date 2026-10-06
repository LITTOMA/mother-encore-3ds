#include "podunk_global_host.hpp"
#include "podunk_global_ready.hpp"
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
} // namespace
class PodunkGlobalHost::GlobalObject final
    : public FieldGlobalExternalObject,
      public GlobalLoadGlobalOwner,
      public PodunkExternalNodeLifecycle {
public:
  explicit GlobalObject(PodunkGlobalHost &h) : h_(h) {}
  FieldGlobalExternalBinding binding() const override { return h_.binding_; }
  bool state(FieldGlobalExternalState &s, std::string &e) const override {
    return h_.state(s, e);
  }
  bool deferred(const FieldDeferredMessage &m, std::string &e) override {
    return h_.deferred(m, e);
  }
  bool persist_append(FieldObjectId id, std::string &e) override {
    return h_.core_.append_array(FieldGlobalMemberRole::Persistent, id, e);
  }
  bool assign_stable_canvas(FieldObjectId, std::string &e) override {
    return fail(e, "global source has no stable Canvas member");
  }
  const FieldGlobalExternalObject &external() const override { return *this; }
  bool admit_cold_load_cursor(const GlobalLoadData &,
                              const FieldGlobalDataRuntime &,
                              std::string &e) const override {
    return fail(
        e, "global Ready prefix "
           "_set_localized_default_inputs/_load_settings/_init_player pending");
  }
  bool party_array(std::string_view s,
                   std::shared_ptr<const GlobalLoadObjectArray> &a,
                   std::string &e) const override {
    return h_.core_.party_array(s, a, e);
  }
  bool clear_party(std::string_view s, std::string &e) override {
    return h_.core_.clear_party(s, e);
  }
  bool append_party(std::string_view s, FieldObjectId id,
                    std::string &e) override {
    return h_.core_.append_party(s, id, e);
  }
  bool stage_parent(FieldObjectId p, std::string &e) override {
    FieldGlobalExternalState s;
    if (!h_.state(s, e) || p != h_.registry_->root() || h_.parent_ || s.inside)
      return fail(e, "global original native parent assignment rejected");
    h_.parent_ = p;
    return true;
  }
  bool native_notification(FieldTreePhase p, std::string &e) override {
    if (p != FieldTreePhase::Parented && p != FieldTreePhase::Unparented &&
        p != FieldTreePhase::ChildMoved && p != FieldTreePhase::PathChanged)
      return fail(e, "global external native notification unmapped");
    const auto *s = h_.tree_->state(h_.binding_.object);
    if (!s)
      return fail(e, "global actual root missing");
    FieldNodeBinding b;
    if (!h_.bind(*h_.tree_, s->object, *h_.tree_->descriptor(s->object), b, e))
      return false;
    if (!h_.native_->phase(*h_.tree_, s->object, b, p, e))
      return false;
    if (p == FieldTreePhase::Unparented)
      h_.parent_ = 0;
    return true;
  }
  bool enter(FieldObjectId p, std::string &e) override {
    return p == h_.parent_ && p == h_.registry_->root()
               ? h_.tree_->enter_branch_only(e)
               : fail(e, "global actual staged root parent differs");
  }
  bool ready(std::string &e) override {
    if(h_.continuation_)return fail(e,"global cold Ready cannot replay over House continuation");
    return h_.tree_->ready_entered_branch(e);
  }
  bool continuation_native_ready() const override {
    return h_.continuation_ready_&&h_.continuation_&&h_.continuation_characters_&&
        h_.continuation_->binds_source_owners(*h_.continuation_characters_,h_.core_,*h_.registry_);
  }
  bool adopt_continuation_ready(std::string &e) override {
    FieldGlobalExternalState s;
    if(!h_.continuation_||!h_.continuation_characters_||h_.continuation_ready_||
        !h_.state(s,e)||!s.inside||s.ready||s.parent!=h_.registry_->root()||
        h_.root_->external_parent(h_.binding_.object)!=s.parent||
        !h_.continuation_->binds_source_owners(*h_.continuation_characters_,h_.core_,*h_.registry_))
      return fail(e,"global continuation native boundary lacks actual completed import");
    const auto *d=h_.tree_->descriptor(h_.binding_.object);FieldNodeBinding b;
    if(!d||!h_.bind(*h_.tree_,h_.binding_.object,*d,b,e)||
        !h_.native_->adopt_continuation_ready(*h_.tree_,h_.binding_.object,b,
           *h_.continuation_,*h_.continuation_characters_,h_.core_,e))return false;
    h_.continuation_ready_=true;e.clear();return true;
  }
  bool exit(std::string &e) override {
    if(!h_.tree_->exit(e))return false;
    h_.continuation_ready_=false;return true;
  }

private:
  PodunkGlobalHost &h_;
};
bool PodunkGlobalHost::initialize(
    const FieldGlobalRegistryData &rd, FieldGlobalRegistry &r,
    std::shared_ptr<const FieldGlobalConstructorData> d,
    std::shared_ptr<const GlobalChildReadyData> children, PodunkNativeRoot &root,
    PodunkGlobalNativeOwner &native, GlobalChildAudio *audio, std::string &e) {
  if (data_ || !rd.valid() || !d || !d->valid() || !children ||
      !children->valid() || children->constructor_ir_sha256() != d->ir_sha256() ||
      rd.identity().upstream_commit != d->identity().upstream_commit)
    return fail(e, "global actual constructor source/Registry rejected");
  auto a =
      std::find_if(rd.autoloads().begin(), rd.autoloads().end(),
                   [&](const auto &x) { return x.id == rd.global_autoload(); });
  if (a == rd.autoloads().end() || a->script != d->owner_source() ||
      a->path != d->scene_source() ||
      a->script_sha != d->identity().source_sha256)
    return fail(e, "global actual autoload descriptor missing");
  if (!children_.initialize(std::move(children), d, r, audio, e))
    return false;
  registry_data_ = &rd;
  registry_ = &r;
  data_ = std::move(d);
  root_ = &root;
  native_ = &native;
  return timers_.initialize(
      data_->timer_data(),
      [](FieldObjectId, std::string &err) {
        return fail(err,
                    "global _on_Playtimer_timeout source clock owner pending");
      },
      [this](FieldObjectId id) {
        if (tree_ && tree_->state(id))
          return tree_.get();
        return static_cast<FieldNodeTreeRuntime *>(nullptr);
      },
      e);
}
FieldNodeTreeHost
PodunkGlobalHost::tree_host(std::shared_ptr<FieldNodeTreeRuntime> tree,
                            bool preallocated) {
  FieldNodeTreeHost h;
  auto *actual_tree = tree.get();
  h.object_domain = registry_->kernel();
  h.allocate_object = [this, preallocated](FieldObjectId &id, std::string &e) {
    if (preallocated && !first_allocation_) {
      first_allocation_ = true;
      id = binding_.object;
      return registry_->allocation_pending(id);
    }
    return registry_->allocate_object(id, e);
  };
  h.allocate_fast_name = [this](uint64_t &id, std::string &e) {
    return registry_->allocate_fast_name(id, e);
  };
  h.native_allocated =
      [this, weak = std::weak_ptr<FieldNodeTreeRuntime>(tree)](FieldObjectId id, const FieldNodeDescriptor &,
                   const FieldIdentity &, std::string &e) {
        auto actual = weak.lock();
        if (!actual) return fail(e, "global native allocation Tree expired");
        return registry_->publish_allocated_node(
            std::move(actual), id,
            [this](const FieldDeferredMessage &m, std::string &err) {
              return deferred(m, err);
            }, e);
      };
  h.construct_source =
      [this, actual_tree](FieldObjectId id, const FieldNodeDescriptor &d,
                          const FieldIdentity &i, std::string &e) {
        return construct_source(*actual_tree, id, d, i, e);
      };
  h.bind = [this, actual_tree](FieldObjectId id, const FieldNodeDescriptor &d,
                               FieldNodeBinding &b, std::string &e) {
    return bind(*actual_tree, id, d, b, e);
  };
  h.dispatch = [this, actual_tree](FieldObjectId id, const FieldNodeBinding &b,
                                   FieldTreePhase p, std::string &e) {
    return phase(*actual_tree, id, b, p, e);
  };
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
    return native_->input_registration(id, k, active, e);
  };
  h.external_pause_process = [this](FieldObjectId id) {
    return id == registry_->root();
  };
  h.external_path = [this](FieldObjectId id, std::string_view path,
                           FieldObjectId &out, std::string &e) {
    return registry_->resolve_path(id, path, out, e);
  };
  h.release = [this](FieldObjectId id, const FieldNodeBinding &b,
                     std::string &e) {
    if (!native_->release(id, b, e))
      return false;
    constructed_.erase(id);
    if (children_.owns(id) && !children_.release(id, e))
      return false;
    return true;
  };
  return h;
}
bool PodunkGlobalHost::construct(
    FieldObjectId id, const FieldGlobalExternalSpec &s,
    std::unique_ptr<FieldGlobalExternalObject> &out, std::string &e) {
  if (!data_ || construction_failed_ || object_ ||
      !registry_->allocation_pending(id) ||
      s.stable_id != registry_data_->global_autoload() ||
      !data_->bind_registry(s, e))
    return fail(e, "global constructor actual source object rejected");
  binding_ = {id, s, 0x454e0055, 1};
  auto object = std::make_unique<GlobalObject>(*this);
  tree_ = std::make_shared<FieldNodeTreeRuntime>();
  transition_ = std::make_shared<FieldNodeTreeRuntime>();
  if (!tree_->initialize_recipe(data_->recipe(), tree_host(tree_, true), e) ||
      tree_->root() != id || !tree_->set_name(id, s.name, e) ||
      !registry_->publish_branch(
          tree_, id,
          [this](const FieldDeferredMessage &m, std::string &err) {
            return deferred(m, err);
          },
          e) ||
      !root_->register_external_child(*object, *object, e)) {
    construction_failed_ = true;
    return false;
  }
  object_ = object.get();
  out = std::move(object);
  return true;
}
bool PodunkGlobalHost::construct_source(FieldNodeTreeRuntime &tree,
                                        FieldObjectId id,
                                        const FieldNodeDescriptor &d,
                                        const FieldIdentity &i,
                                        std::string &e) {
  const bool global = &tree == tree_.get();
  const auto &expected =
      global ? data_->recipe().identity() : data_->transition_identity();
  const auto *record =
      global ? data_->recipe().record(d.id) : &data_->transition_node();
  const auto *s = tree.state(id);
  if (!record || !s || !same(i, expected) || record->id != d.id ||
      record->script_sha != d.script_sha || record->script != d.script ||
      record->native_class != d.native_class || s->object != id ||
      !s->name.empty() || s->parent || s->owner || s->inside ||
      s->ready_notified || s->pause != d.pause || constructed_.count(id))
    return fail(
        e, "global source attachment native allocation/property order differs");
  if (!native_->construct(tree, id, d, e))
    return false;
  if (global && id == binding_.object) {
    if (!transition_->initialize_source_node(
            data_->transition_identity(), data_->transition_node(),
            tree_host(transition_, false), e) ||
        !registry_->publish_branch(
            transition_, transition_->root(),
            [this](const FieldDeferredMessage &m, std::string &err) {
              return deferred(m, err);
            },
            e) ||
        !core_.initialize(*data_, *registry_, id, transition_->root(), e))
      return false;
  } else if (!d.script.empty() && global) {
    auto f = std::find_if(
        data_->child_fields().begin(), data_->child_fields().end(),
        [&](const auto &x) { return x.id == d.id && x.script == d.script; });
    if (f == data_->child_fields().end())
      return fail(e, "global source child script constructor unknown");
    if (!children_.construct(tree, id, d, e))
      return false;
  } else if (!global &&
             (d.id != data_->transition_node().id || d.script_methods))
    return fail(e, "global source new Node script constructor unknown");
  constructed_[id] = true;
  return true;
}
bool PodunkGlobalHost::bind(FieldNodeTreeRuntime &tree, FieldObjectId id,
                            const FieldNodeDescriptor &d, FieldNodeBinding &b,
                            std::string &e) {
  if (!constructed_.count(id) || !tree.state(id) ||
      !registry_->object_exists(id))
    return fail(e, "global lifecycle actual source fields missing");
  FieldIdentity i;
  if (!tree.object_identity(id, i))
    return fail(e, "global native lifecycle identity absent");
  b = {i, d.id, d.class_index, 0x454e0055, 1, d.script_sha, d.native_class};
  if (d.native_class == "Timer")
    return timers_.attach(tree, id, b, e);
  return true;
}
bool PodunkGlobalHost::phase(FieldNodeTreeRuntime &tree, FieldObjectId id,
                             const FieldNodeBinding &b, FieldTreePhase p,
                             std::string &e) {
  const auto *d = tree.descriptor(id);
  if (!d || !constructed_.count(id) || b.stable_id != d->id ||
      b.script_sha != d->script_sha)
    return fail(e, "global actual native/script lifecycle binding differs");
  if (p == FieldTreePhase::EnterScript || p == FieldTreePhase::ExitScript) {
    uint32_t mask = p == FieldTreePhase::EnterScript ? 2u : 4u;
    if (d->script_methods & mask)
      return fail(e, "global source enter/exit callback pending");
    return true;
  }
  if (p == FieldTreePhase::ReadyScript) {
    if(continuation_)return fail(e,"global source Ready remains pending during House continuation");
    if (d->script_methods & 1u) {
      if (id == binding_.object) {
        if (!ready_ || !ready_->binds(core_, *registry_))
          return fail(e, "global Ready actual source caller missing");
        return ready_->source_ready(tree, id, b, e);
      }
      return children_.script_phase(tree, id, b, p, e);
    }
    return true;
  }
  if (children_.owns(id) &&
      (p == FieldTreePhase::Idle || p == FieldTreePhase::Input))
    return children_.script_phase(tree, id, b, p, e);
  if (p == FieldTreePhase::Idle || p == FieldTreePhase::Physics ||
      p == FieldTreePhase::Input || p == FieldTreePhase::UnhandledInput ||
      p == FieldTreePhase::UnhandledKeyInput)
    return fail(e, "global source process/input execution owner pending");
  if (!native_->phase(tree, id, b, p, e))
    return false;
  if (d->native_class == "Timer" && p == FieldTreePhase::ReadyNative)
    return timers_.ready(id, e);
  return true;
}
bool PodunkGlobalHost::state(FieldGlobalExternalState &s,
                             std::string &e) const {
  if (!tree_ || construction_failed_ || !binding_.object)
    return fail(e, "global owning source not constructed");
  const auto *n = tree_->state(binding_.object);
  if (!n || !n->alive)
    return fail(e, "global native ObjectDB root expired");
  FieldGlobalExternalState v;
  v.name = n->name;
  v.parent = parent_;
  v.children = n->children;
  v.inside = n->inside;
  v.ready = n->ready_notified;
  if (!core_.object(FieldGlobalMemberRole::CurrentScene, v.current_scene, e))
    return false;
  s = std::move(v);
  return true;
}
bool PodunkGlobalHost::deferred(const FieldDeferredMessage &m, std::string &e) {
  if (m.object != binding_.object)
    return fail(e, "global/SceneTransition deferred source opcode pending");
  return fail(e, "global deferred source method pending");
}
GlobalLoadGlobalOwner *PodunkGlobalHost::owner() const { return object_; }
bool PodunkGlobalHost::bind_characters(const FieldGlobalDataRuntime &c,
                                       std::string &e) {
  return core_.bind_characters(c, e);
}
bool PodunkGlobalHost::bind_continuation(const HouseGlobalBridgeRuntime &bridge,
    const FieldGlobalDataRuntime &characters,std::string &e) {
  const auto *node=tree_?tree_->state(binding_.object):nullptr;
  if(continuation_||ready_||!object_||construction_failed_||!node||node->inside||
      node->parent||parent_||node->ready_notified||
      registry_->external_object(binding_.object)!=object_||
      !bridge.binds_source_owners(characters,core_,*registry_))
    return fail(e,"global continuation requires completed same source/session owners");
  continuation_=&bridge;continuation_characters_=&characters;e.clear();return true;
}
bool PodunkGlobalHost::bind_ready(PodunkGlobalReady &ready, std::string &e) {
  const auto *node = tree_ ? tree_->state(binding_.object) : nullptr;
  if (ready_ || continuation_ || !object_ || construction_failed_ || !node || node->inside ||
      node->ready_notified || !ready.binds(core_, *registry_))
    return fail(e, "global Ready source caller requires the same cold owner");
  ready_ = &ready;
  e.clear();
  return true;
}
} // namespace encore::ctr
