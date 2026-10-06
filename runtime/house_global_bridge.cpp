#include "encore/house_global_bridge.hpp"
#include "encore/field_global_flags.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
std::shared_ptr<GlobalYamlValue> value(uint32_t kind) {
  auto p = std::make_shared<GlobalYamlValue>();
  p->kind = kind;
  return p;
}
std::shared_ptr<GlobalYamlValue> text(const std::string &s) {
  auto p = value(4);
  p->string = s;
  return p;
}
std::shared_ptr<GlobalYamlValue> integer(int64_t n) {
  auto p = value(2);
  p->integer = n;
  return p;
}
std::shared_ptr<GlobalYamlValue> boolean(bool b) {
  auto p = value(1);
  p->boolean = b;
  return p;
}
} // namespace
bool HouseGlobalBridgeRuntime::adopt(
    const HouseGlobalBridgeData &data, const NativeSessionData &session,
    RoomView room, HouseView house, RoundView round, ItemView legacy,
    const SessionSnapshot &save, const FieldInventoryData &inventory,
    const FieldItemDefinitions &defs, FieldGlobalDataRuntime &core,
    FieldGlobalConstructorRuntime &global, FieldGlobalRegistry &registry,
    SourceRandom &played, const std::vector<uint32_t> &ledger,
    HouseGlobalBridgeHost host, std::string &e) {
  if (complete_ || !data.valid() || !core.ready_complete() ||
      !core.load_complete() || core.registry_ != &registry || !global.data() ||
      !registry.external_object(global.owner()) ||
      core.character_load_data_ != data.characters() ||
      core.global_load_data_ != data.cold() || !host.adopt_item ||
      !host.new_status || !inventory.valid() || !defs.valid() ||
      !inventory.bind_definitions(defs, e) ||
      data.identity().upstream_commit != inventory.source_pin() ||
      defs.source_pin() != inventory.source_pin())
    return fail(e,
                "House continuation requires actual completed target owners");
  if (!validate_native_session_snapshot(session, room, house, round, legacy,
                                        save, e))
    return false;
  if (save.characters.size() != 1 || save.party.size() != 1 ||
      save.characters[0].character_id != data.leader() ||
      save.party[0] != data.leader())
    return fail(e, "House continuation unsupported live party");
  const uint64_t random_state = played.state(), draws = played.raw_draw_count();
  const auto old_ledger = ledger;
  auto objects = core.objects_;
  auto members = core.members_;
  auto references = core.character_items_;
  auto flags = *core.constructor_flags_;
  auto char_it =
      std::find_if(objects.begin(), objects.end(), [&](const auto &b) {
        return b.kind == 1 && b.role == 0 &&
               b.declaration == data.declaration();
      });
  if (char_it == objects.end() || !registry.object_exists(char_it->object) ||
      !char_it->inventory || !registry.native_reference(char_it->inventory))
    return fail(e, "House continuation Ninten actual body/Inventory absent");
  const auto &source = *data.characters();
  const auto &b = source.source_bindings();
  const auto &character = save.characters.front();
  FieldInventoryState projected;
  projected.level = uint32_t(character.level);
  projected.hp = character.hp;
  projected.pp = character.pp;
  projected.cash = save.cash;
  std::map<uint32_t, FieldObjectId> ownerids;
  std::set<FieldObjectId> replaced;
  for (const auto &o : inventory.owners()) {
    FieldObjectId id = 0;
    uint32_t declaration = 0;
    if (o.role == 0) {
      if (o.source_name != data.leader())
        return fail(e, "House continuation unreviewed Inventory owner");
      id = char_it->inventory;
      declaration = data.declaration();
    } else if (o.role == 1 || o.role == 2) {
      auto d =
          std::find_if(core.data_->declarations().begin(),
                       core.data_->declarations().end(), [&](const auto &x) {
                         return x.kind == 2 && x.name == o.source_name;
                       });
      if (d == core.data_->declarations().end())
        return fail(e,
                    "House continuation source KEY/STORAGE declaration absent");
      declaration = d->id;
      auto body =
          std::find_if(objects.begin(), objects.end(), [&](const auto &x) {
            return x.kind == 2 && x.declaration == declaration;
          });
      if (body == objects.end())
        return fail(e, "House continuation actual KEY/STORAGE body absent");
      id = body->object;
    } else
      return fail(e, "House continuation unknown inventory role");
    if (!registry.native_reference(id) || !ownerids.emplace(o.id, id).second ||
        !replaced.insert(id).second)
      return fail(e, "House continuation Inventory actual owner alias");
    FieldItemInventory row;
    row.owner = o.id;
    row.role = o.role;
    const auto &saved = o.role == 0   ? character.inventory
                        : o.role == 1 ? save.key_items
                                      : save.storage;
    for (const auto &i : saved) {
      const auto *d = defs.definition(i.item_id);
      if (!d || i.doses < 0 || uint64_t(i.doses) > UINT32_MAX)
        return fail(e, "House continuation item source/doses rejected");
      row.items.push_back({d->id, i.uid, uint32_t(i.doses), i.equipped});
    }
    projected.items.inventories.push_back(std::move(row));
    if (!o.role)
      projected.items.party_order.push_back(o.id);
  }
  for (const auto &s : character.status)
    projected.statuses.push_back({s.status_id, s.passive_healing_turns});
  for (const auto &p : character.permanent_boosts) {
    auto at =
        std::find(inventory.stats().begin(), inventory.stats().end(), p.id);
    if (at == inventory.stats().end())
      return fail(e, "House continuation unknown permanent stat");
    projected.permanent[size_t(at - inventory.stats().begin())] = p.value;
  }
  FieldInventoryRuntime checked;
  if (!checked.initialize(inventory, defs, projected, e))
    return false;
  std::set<uint32_t> uids;
  for (const auto &o : projected.items.inventories)
    for (const auto &i : o.items) {
      if (!uids.insert(i.uid).second ||
          std::find(ledger.begin(), ledger.end(), i.uid) == ledger.end())
        return fail(e,
                    "House continuation UID collision/unowned ledger identity");
    }
  for (const auto &r : core.god_items_)
    if (uids.count(r.value.uid))
      return fail(e, "House continuation UID collides with target GodStorage");
  for (const auto &entry : core.character_items_)
    if (!replaced.count(entry.first))
      for (const auto &r : entry.second) {
        FieldOwnedItem live;
        if (!r.source_owner || !r.source_owner->read_item(live, e))
          return false;
        if (uids.count(live.uid))
          return fail(
              e,
              "House continuation UID collides with inactive target character");
      }
  std::vector<FieldGlobalDataItemReference> all_items;
  for (const auto &o : projected.items.inventories) {
    const auto id = ownerids.at(o.owner);
    auto body = std::find_if(objects.begin(), objects.end(),
                             [&](const auto &x) { return x.object == id; });
    auto collection = body->collections.find(b.inventory_items_field);
    if (collection == body->collections.end() || !collection->second ||
        collection->second->kind != 5)
      return fail(e, "House continuation actual Inventory _items root absent");
    auto refs = std::make_shared<FieldGlobalDataReferenceArray>();
    std::vector<FieldGlobalDataItemReference> row;
    for (const auto &i : o.items) {
      FieldGlobalDataItemReference r;
      if (!host.adopt_item(o.owner, i, r, e))
        return false;
      auto actual = registry.native_reference(r.object);
      FieldOwnedItem observed;
      auto bind = actual ? actual->binding() : FieldGlobalExternalBinding{};
      std::array<uint8_t, 32> proof{}, expected{};
      if (!actual || r.registry != &registry || r.owner != o.owner ||
          !r.actual_owner || !r.source_owner ||
          actual.owner_before(r.actual_owner) ||
          r.actual_owner.owner_before(actual) ||
          actual.owner_before(r.source_owner) ||
          r.source_owner.owner_before(actual) || bind.family != 0x454e0060 ||
          bind.capability != 1 || bind.source.script != b.item_script ||
          !actual->checked_source_hash(b.item_script, proof) ||
          !data.source_hash(b.item_script, expected) || proof != expected ||
          !r.source_owner->read_item(observed, e) ||
          observed.definition != i.definition || observed.uid != i.uid ||
          observed.doses != i.doses || observed.equipped != i.equipped)
        return fail(
            e,
            "House continuation migration Item actual body/ownership differs");
      refs->values.push_back(actual);
      row.push_back(r);
      all_items.push_back(r);
    }
    body->collections[b.inventory_items_field] = value(5);
    body->reference_arrays[b.inventory_items_field] = refs;
    body->item_objects.clear();
    for (const auto &r : row)
      body->item_objects.push_back(r.object);
    references[id] = std::move(row);
  }
  auto scalar = [&](const std::string &name, uint32_t kind, int64_t n,
                    const std::string &s) {
    auto f = std::find_if(
        char_it->fields.begin(), char_it->fields.end(),
        [&](const auto &v) { return v.name == name && v.kind == kind; });
    if (f == char_it->fields.end())
      return false;
    f->integer_value = n;
    f->string_value = s;
    return true;
  };
  if (!scalar(b.name, 1, 0, data.leader()) ||
      !scalar(b.level, 2, character.level, {}) ||
      !scalar(b.exp, 2, character.experience, {}) ||
      !scalar(b.hp, 2, character.hp, {}) ||
      !scalar(b.pp, 2, character.pp, {}) ||
      !scalar(b.nickname, 1, 0, character.nickname))
    return fail(e, "House continuation Character actual scalar fields missing");
  auto targets =
      std::find_if(source.rows().begin(), source.rows().end(),
                   [&](const auto &r) { return r.id == data.declaration(); });
  if (targets == source.rows().end() ||
      targets->targets.size() != b.stat_fields.size())
    return fail(e, "House continuation raw stat source targets missing");
  for (size_t i = 0; i < b.stat_fields.size(); ++i) {
    if (character.level < 1 ||
        size_t(character.level) > inventory.levels().size() ||
        !scalar(b.stat_fields[i], 2,
                inventory.levels()[size_t(character.level) - 1][i], {}))
      return fail(e, "House continuation source stat field/level missing");
  }
  auto skills = value(5);
  char_it->learned_skills = character.learned_skills;
  for (const auto &s : character.learned_skills)
    skills->array.push_back(text(s));
  char_it->collections[b.learned_skills] = skills;
  auto permanent = value(6);
  for (const auto &s : character.permanent_boosts)
    permanent->dictionary.emplace_back(s.id, integer(s.value));
  char_it->collections[b.permanent_boosts] = permanent;
  char_it->permanent = projected.permanent;
  auto affinities = value(6);
  char_it->affinities.clear();
  for (const auto &s : character.affinity_multipliers) {
    auto v = value(3);
    v->real = s.value;
    affinities->dictionary.emplace_back(s.id, v);
    char_it->affinities.emplace(s.id, s.value);
  }
  char_it->collections[b.affinities] = affinities;
  auto nodes = std::make_shared<FieldGlobalDataNodeArray>();
  std::vector<HouseGlobalStatusObject> statuses;
  for (const auto &s : character.status) {
    auto p = std::find_if(data.status().policies.begin(),
                          data.status().policies.end(),
                          [&](const auto &v) { return v.id == s.status_id; });
    if (p == data.status().policies.end())
      return fail(e, "House continuation unknown Status identity");
    HouseGlobalStatusObject obj;
    if (!host.new_status(data.status(), *p, s, obj, e))
      return false;
    auto tree = registry.tree_owner(obj.object);
    auto *n = tree ? tree->state(obj.object) : nullptr;
    auto *d = tree ? tree->descriptor(obj.object) : nullptr;
    FieldIdentity actual;
    if (!tree || tree != obj.tree || !n || !d || n->parent || n->inside ||
        n->ready_notified || !n->ready_first || tree->root() != obj.object ||
        !tree->object_identity(obj.object, actual) ||
        actual.upstream_commit != data.identity().upstream_commit ||
        d->script != data.status().script ||
        d->native_class != data.status().native ||
        n->binding.family != 0x454e0060 || n->binding.capability != 1 ||
        obj.ailment != s.status_id || obj.turns != s.passive_healing_turns ||
        obj.times != data.status().times || obj.probability != p->probability)
      return fail(e, "House continuation Status actual detached Node/source "
                     "body rejected");
    nodes->values.push_back(obj.object);
    statuses.push_back(obj);
  }
  char_it->collections[b.status] = value(5);
  char_it->node_arrays[b.status] = nodes;
  for (const auto &a : data.assignments()) {
    auto d = std::find_if(core.constructor_data_->declarations().begin(),
                          core.constructor_data_->declarations().end(),
                          [&](const auto &x) { return x.name == a.member; });
    if (d == core.constructor_data_->declarations().end() ||
        d->kind != a.member_kind || d->adapter != a.adapter)
      return fail(e, "House continuation global declaration mapping differs");
    auto &m =
        members[size_t(d - core.constructor_data_->declarations().begin())];
    auto v = value(d->kind);
    switch (a.role) {
    case 1:
      m.vector = {save.position_x, save.position_y};
      continue;
    case 2:
      v = text(save.scene_id);
      break;
    case 3:
      v = text(save.run_sound);
      break;
    case 4:
      v = text(save.shadow_effect);
      break;
    case 5:
      v = integer(int64_t(std::floor(save.playtime_seconds)));
      break;
    case 6:
      v = text(save.favorite_food);
      break;
    case 7:
      v = text(save.player_name);
      break;
    case 8:
      v = value(3);
      v->real = save.settings.text_speed;
      break;
    case 9:
      v = text(save.settings.menu_flavor);
      break;
    case 10:
      v = text(save.settings.button_prompts);
      break;
    case 11:
      v = integer(save.earned_cash);
      break;
    case 12:
      v = boolean(save.settings.description);
      break;
    case 13:
      v = integer(save.cash);
      break;
    case 14:
      v = integer(save.bank);
      break;
    case 15:
      for (const auto &x : save.keys)
        v->dictionary.emplace_back(x.id, integer(x.value));
      break;
    case 16:
      for (const auto &x : save.rare_drops)
        v->dictionary.emplace_back(x.id, integer(x.value));
      break;
    case 17:
      for (const auto &x : save.encountered)
        v->dictionary.emplace_back(x.id, boolean(x.value));
      break;
    case 18:
    case 19:
      continue;
    default:
      return fail(e, "House continuation unknown assignment opcode");
    }
    if (v->kind != m.kind)
      return fail(e, "House continuation actual global value type differs");
    m.value = std::move(v);
  }
  FieldFlagProjection source_flags;
  for (const auto &f : flags.state().normal)
    source_flags.normal.emplace_back(f.first, false);
  for (const auto &f : save.flags) {
    auto i =
        std::find_if(source_flags.normal.begin(), source_flags.normal.end(),
                     [&](const auto &x) { return x.first == f.id; });
    if (i == source_flags.normal.end())
      return fail(e, "House continuation unknown normal flag");
    i->second = f.value;
  }
  for (const auto &f : save.object_flags)
    source_flags.objects.emplace_back(f.id, f.value);
  for (const auto &f : save.seen_dialogue_flags)
    source_flags.seen.emplace_back(f.id, f.value);
  if (!flags.load_source(source_flags, e))
    return false;
  std::shared_ptr<const GlobalLoadObjectArray> party, npcs;
  if (!global.array(FieldGlobalMemberRole::Party, party, e) ||
      !global.array(FieldGlobalMemberRole::PartyNpcs, npcs, e) ||
      party->values != std::vector<FieldObjectId>{char_it->object} ||
      !npcs->values.empty())
    return fail(e, "House continuation target actual party Arrays differ");
  if (played.state() != random_state || played.raw_draw_count() != draws ||
      ledger != old_ledger)
    return fail(e, "House continuation factory advanced played RNG/UID ledger");
  // Everything above was staged. Keep existing actual Character/Inventory IDs;
  // do not modify any constructor, cold LOAD or Ready completion flags.
  core.objects_.swap(objects);
  core.members_.swap(members);
  core.character_items_.swap(references);
  *core.constructor_flags_ = std::move(flags);
  inventory_ = std::move(projected);
  items_ = std::move(all_items);
  statuses_ = std::move(statuses);
  owners_ = std::move(ownerids);
  playtime_remainder_ =
      save.playtime_seconds - std::floor(save.playtime_seconds);
  owner_ = &core;
  registry_ = &registry;
  complete_ = true;
  e.clear();
  return true;
}
bool HouseGlobalBridgeRuntime::actual_inventory_owner(uint32_t id,
                                                      FieldObjectId &out,
                                                      std::string &e) const {
  auto i = owners_.find(id);
  if (!complete_ || !owner_ || !registry_ || i == owners_.end() ||
      !registry_->native_reference(i->second))
    return fail(e, "House continuation actual Inventory unavailable");
  out = i->second;
  e.clear();
  return true;
}
} // namespace encore::upstream
