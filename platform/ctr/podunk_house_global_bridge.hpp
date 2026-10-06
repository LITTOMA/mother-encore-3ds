#pragma once
#include "encore/house_global_bridge.hpp"
#include "podunk_inventory_host.hpp"
#include "podunk_global_data_host.hpp"
#include "encore/house_status_effects.hpp"
namespace encore::ctr {
// Concrete target-object adoption. Original session gameplay/RNG stays live;
// callers bootstrap the separate target with the same UID exclusion ledger.
class PodunkHouseGlobalBridge {
  struct StatusBody {
    upstream::HouseGlobalStatusObject object;
    std::map<std::string, std::shared_ptr<upstream::GlobalYamlValue>> fields;
  };
  const upstream::HouseGlobalBridgeData *data_ = nullptr;
  const upstream::FieldItemDefinitions *all_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkGlobalDataHost *globaldata_ = nullptr;
  upstream::HouseGlobalBridgeRuntime core_;
  std::vector<std::shared_ptr<PodunkItemObject>> items_;
  std::map<upstream::FieldObjectId, std::shared_ptr<StatusBody>> statuses_;
  bool attempted_ = false;
  static bool fail(std::string &e, const char *s) {
    e = s;
    return false;
  }
  bool construct_status(const upstream::HouseGlobalStatusBinding &binding,
                        const upstream::HouseGlobalStatusPolicy &policy,
                        const upstream::SessionStatus &saved,
                        upstream::HouseGlobalStatusObject &out,
                        std::string &e) {
    using namespace upstream;
    if (!data_ || !registry_ || !globaldata_ || saved.status_id != policy.id ||
        (!policy.passive && saved.passive_healing_turns))
      return fail(e, "House continuation actual Status arguments rejected");
    auto body = std::make_shared<StatusBody>();
    body->object.tree = std::make_shared<FieldNodeTreeRuntime>();
    auto tree = body->object.tree;
    std::weak_ptr<FieldNodeTreeRuntime> weak = tree;
    std::weak_ptr<StatusBody> weak_body = body;
    FieldIdentity identity = data_->identity();
    identity.scene_id = binding.id;
    if (!data_->source_hash(binding.script, identity.source_sha256))
      return fail(e, "House continuation Status script proof absent");
    FieldNodeDescriptor descriptor;
    descriptor.id = binding.id;
    descriptor.path = ".";
    descriptor.index = -1;
    descriptor.script = binding.script;
    descriptor.script_sha = identity.source_sha256;
    descriptor.native_class = binding.native;
    FieldNodeTreeHost host;
    host.object_domain = registry_->kernel();
    host.allocate_object = [this](auto &id, auto &error) {
      return registry_->allocate_object(id, error);
    };
    host.allocate_fast_name = [this](auto &id, auto &error) {
      return registry_->allocate_fast_name(id, error);
    };
    host.object_exists = [this](auto id) {
      return registry_->object_exists(id);
    };
    host.native_allocated = [this, weak](auto id, const auto &, const auto &,
                                         auto &error) {
      auto owner = weak.lock();
      return owner && registry_->publish_allocated_node(
                          owner, id,
                          [](const auto &, auto &err) {
                            return fail(err, "Detached source Status has no "
                                             "deferred gameplay endpoint");
                          },
                          error);
    };
    host.construct_source = [this, weak_body, weak, binding, policy,
                             saved](auto id, const auto &node, const auto &,
                                    auto &error) {
      auto body = weak_body.lock();
      auto owner = weak.lock();
      if (!body || !owner || registry_->tree_owner(id) != owner ||
          node.script != binding.script || node.native_class != binding.native)
        return fail(error,
                    "Status constructor actual native allocation differs");
      body->object.object = id;
      body->object.ailment = saved.status_id;
      body->object.turns = binding.turns;
      body->object.times = binding.times;
      body->object.probability = 0;
      auto literal = [](uint32_t kind, int64_t n, const std::string &s) {
        auto v = std::make_shared<GlobalYamlValue>();
        v->kind = kind;
        v->integer = n;
        v->string = s;
        return v;
      };
      body->fields[binding.ailment] = literal(4, 0, saved.status_id);
      body->fields[binding.turns_field] = literal(2, binding.turns, {});
      body->fields[binding.times_field] = literal(2, binding.times, {});
      body->fields[binding.probability] = literal(2, 0, {});
      // Source Status._init performs BOTH get_data calls on the same cache.
      std::shared_ptr<GlobalYamlValue> first, second;
      if (!globaldata_->call_cache_getter(binding.getter, {saved.status_id},
                                          first, error) ||
          !globaldata_->call_cache_getter(binding.getter, {saved.status_id},
                                          second, error) ||
          !first || !second || first.get() != second.get() || first->kind != 6)
        return fail(error,
                    "Status constructor actual source cache owner rejected");
      auto healing = second->get(binding.healing_key);
      if (!healing || healing->kind != 6)
        return fail(error,
                    "Status constructor actual healing Dictionary missing");
      auto passive = healing->get(binding.passive_key);
      if (passive && passive->kind != 1)
        return fail(error, "Status constructor passive-heal value is not bool");
      if ((passive && passive->boolean) != policy.passive)
        return fail(error, "Status constructor actual passive policy differs");
      // Typed source policy was reviewed against this same cache YAML source.
      if (policy.passive) {
        auto chance = healing->get(binding.probability_key);
        if (chance &&
            (chance->kind != 2 || chance->integer != policy.probability))
          return fail(error,
                      "Status constructor actual passive probability differs");
        body->object.probability = policy.probability;
        body->fields[binding.probability] = literal(2, policy.probability, {});
      }
      body->object.turns = saved.passive_healing_turns;
      body->fields[binding.turns_field] =
          literal(2, saved.passive_healing_turns, {});
      statuses_.emplace(id, body);
      error.clear();
      return true;
    };
    host.bind = [this, identity](auto id, const auto &node, auto &bound,
                                 auto &error) {
      if (!statuses_.count(id) || node.id != identity.scene_id ||
          node.script_sha != identity.source_sha256)
        return fail(error, "Status source body not constructed");
      bound = {identity, node.id,         node.class_index, 0x454e0060,
               1,        node.script_sha, node.native_class};
      error.clear();
      return true;
    };
    host.dispatch = [](auto, const auto &, FieldTreePhase phase, auto &error) {
      if (phase == FieldTreePhase::Deleting ||
          phase == FieldTreePhase::PathChanged) {
        error.clear();
        return true;
      }
      return fail(
          error,
          "Detached Status lifecycle/gameplay is outside adoption scope");
    };
    host.deferred = [](const auto &, auto &error) {
      return fail(error, "Detached Status has no deferred caller");
    };
    host.input_registration = [](auto, uint32_t, bool enabled, auto &error) {
      if (enabled)
        return fail(error, "Detached Status has no source input callback");
      error.clear();
      return true;
    };
    host.external_pause_process = [](auto) { return false; };
    host.release = [this](auto id, const auto &, auto &error) {
      statuses_.erase(id);
      error.clear();
      return true;
    };
    if (!tree->initialize_source_node(identity, descriptor, std::move(host), e))
      return false;
    body->object.tree = tree;
    out = body->object;
    e.clear();
    return true;
  }
  void discard_candidates() {
    auto copy = statuses_;
    for (const auto &v : copy) {
      auto tree = v.second->object.tree;
      std::string e;
      if (tree && tree->state(v.first)) {
        tree->queue_free(v.first, e);
        tree->flush_delete_queue(e);
      }
    }
    statuses_.clear();
    items_.clear();
  }

public:
  bool construct_continuation_autoload(
      uint32_t stable, const upstream::HouseGlobalBridgeData &data,
      const upstream::NativeSessionData &session, upstream::RoomView room,
      upstream::HouseView house, upstream::RoundView round, upstream::ItemView legacy,
      const upstream::SessionSnapshot &save, upstream::FieldGlobalRegistry &registry,
      upstream::SourceRandom &played, const std::vector<uint32_t> &ledger,
      std::string &e) {
    if (attempted_) return fail(e,"House continuation owner adoption already attempted");
    return core_.construct_continuation_autoload(stable,data,session,room,house,
        round,legacy,save,registry,played,ledger,e);
  }
  bool prepare_continuation(const upstream::HouseGlobalBridgeData &data,
                            PodunkGlobalDataHost &characters,
                            upstream::FieldGlobalConstructorRuntime &global,
                            upstream::FieldGlobalRegistry &registry,
                            std::string &e) {
    if (attempted_) return fail(e, "House continuation already attempted");
    return core_.prepare_continuation(data, characters.runtime(), global, registry, e);
  }
  bool adopt(const upstream::HouseGlobalBridgeData &data,
             const upstream::NativeSessionData &session,
             upstream::RoomView room, upstream::HouseView house,
             upstream::RoundView round, upstream::ItemView old_items,
             const upstream::SessionSnapshot &save,
             const upstream::FieldInventoryData &inventory,
             const upstream::FieldItemDefinitions &podunk,
             const upstream::FieldItemDefinitions &all,
             PodunkGlobalDataHost &characters,
             upstream::FieldGlobalConstructorRuntime &global,
             upstream::FieldGlobalRegistry &registry,
             upstream::SourceRandom &played,
             const std::vector<uint32_t> &ledger, std::string &e) {
    if (attempted_ || !all.valid() || !all.global_constructor_scope() ||
        all.source_pin() != podunk.source_pin())
      return fail(
          e, "House continuation owning host repeated/definitions invalid");
    attempted_ = true;
    data_ = &data;
    all_ = &all;
    registry_ = &registry;
    globaldata_ = &characters;
    upstream::HouseGlobalBridgeHost host;
    host.adopt_item = [this](uint32_t owner, const auto &value, auto &out,
                             auto &error) {
      std::shared_ptr<PodunkItemObject> actual;
      if (!PodunkInventoryHost::adopt_existing_session_item(
              *data_, *all_, *registry_, owner, value, actual, error))
        return false;
      out = {registry_, actual->object, owner, value, actual, actual};
      items_.push_back(actual);
      return true;
    };
    host.new_status = [this](const auto &b, const auto &p, const auto &s,
                             auto &out, auto &error) {
      return construct_status(b, p, s, out, error);
    };
    if (!core_.adopt(data, session, room, house, round, old_items, save,
                     inventory, podunk, characters.runtime(), global, registry,
                     played, ledger, std::move(host), e)) {
      discard_candidates();
      return false;
    }
    return true;
  }
  bool snapshot(const upstream::FieldInventoryData &data,
                PodunkInventorySnapshot &out, std::string &e) const {
    if (!core_.complete() || !registry_ ||
        data.source_pin() != data_->identity().upstream_commit)
      return fail(e, "House continuation actual owning snapshot unavailable");
    PodunkInventorySnapshot next;
    next.source_pin = data.source_pin();
    next.inventory_identity = data.identity();
    next.state = core_.inventory_state();
    for (const auto &o : data.owners()) {
      upstream::FieldObjectId id = 0;
      if (!core_.actual_inventory_owner(o.id, id, e))
        return false;
      next.owners.push_back({o.id, id});
    }
    next.items = items_;
    out = std::move(next);
    e.clear();
    return true;
  }
  bool status_fields(
      upstream::FieldObjectId id,
      std::map<std::string, std::shared_ptr<upstream::GlobalYamlValue>> &out,
      std::string &e) const {
    auto i = statuses_.find(id);
    if (!core_.complete() || i == statuses_.end() ||
        registry_->tree_owner(id) != i->second->object.tree ||
        !i->second->object.tree->state(id))
      return fail(e, "House continuation Status actual owner unavailable");
    out = i->second->fields;
    e.clear();
    return true;
  }
  bool status_data(upstream::FieldObjectId character,
                   upstream::FieldObjectId id,
                   upstream::HouseStatusActualData &out,
                   std::string &e) const {
    using namespace upstream;
    auto i = statuses_.find(id);
    if (!core_.complete() || !data_ || !registry_ || !globaldata_ ||
        i == statuses_.end() || registry_->poisoned() ||
        globaldata_->runtime().registry() != registry_ ||
        registry_->tree_owner(id) != i->second->object.tree)
      return fail(e, "Status.get_data actual continuation owner unavailable");
    FieldGlobalDataMemberState array;
    if (!globaldata_->runtime().read_constructed_member(
            character, data_->characters()->source_bindings().status, array, e))
      return false;
    if (!array.node_array ||
        std::count(array.node_array->values.begin(),
                   array.node_array->values.end(), id) != 1)
      return fail(e, "Status.get_data not owned by actual Character Array");
    auto node = i->second->object.tree->state(id);
    const auto &binding = data_->status();
    auto ailment = i->second->fields.find(binding.ailment);
    auto times = i->second->fields.find(binding.times_field);
    if (!node || !node->alive || node->queued || !node->bound ||
        ailment == i->second->fields.end() || !ailment->second ||
        ailment->second->kind != 4 ||
        ailment->second->string != i->second->object.ailment ||
        times == i->second->fields.end() || !times->second ||
        times->second->kind != 2 ||
        times->second->integer != i->second->object.times)
      return fail(e, "Status.get_data actual script fields rejected");
    HouseStatusActualData next;
    next.registry = registry_;
    next.tree = i->second->object.tree;
    next.object = id;
    next.ailment = ailment->second->string;
    next.times = times->second->integer;
    if (!globaldata_->call_cache_getter(binding.getter, {next.ailment},
                                        next.data, e) ||
        !next.data || next.data->kind != 6)
      return fail(e, "Status.get_data actual source Dictionary unavailable");
    out = std::move(next);
    e.clear();
    return true;
  }
  const auto &core() const { return core_; }
};
} // namespace encore::ctr
