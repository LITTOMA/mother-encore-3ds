#include "podunk_house_return.hpp"
#include "podunk_global_host.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool export_owned(const PodunkHouseReturnInput &in, SessionSnapshot &out,
                  PodunkInventorySnapshot &inventory, std::string &e) {
  auto &c = *in.continuation;
  auto &core = c.characters()->runtime();
  const auto *binding = c.bridge_data();
  const auto *characters = c.character_data();
  if (!binding || !characters || !binding->valid() ||
      !in.programme->binds(c, *in.inventory, *in.session) ||
      !c.bridge()->core().binds_source_owners(core, c.global()->core(),
                                              *c.registry()) ||
      !in.programme->export_snapshot(*in.session, out, e) ||
      !in.inventory->snapshot(inventory, e))
    return false;
  if (out.characters.empty() ||
      out.party != std::vector<std::string>{binding->leader()} ||
      out.characters.front().character_id != binding->leader())
    return fail(e, "House return requires the same admitted singleton party");
  FieldObjectId actual = 0;
  for (const auto &body : core.objects())
    if (body.declaration == binding->declaration() && body.kind == 1) {
      if (actual)
        return fail(e, "House return Character source is ambiguous");
      actual = body.object;
    }
  std::shared_ptr<const GlobalLoadObjectArray> party, npcs;
  if (!actual || !c.registry()->object_exists(actual) ||
      !c.global()->core().array(FieldGlobalMemberRole::Party, party, e) ||
      !c.global()->core().array(FieldGlobalMemberRole::PartyNpcs, npcs, e) ||
      !party || !npcs || party->values != std::vector<FieldObjectId>{actual} ||
      !npcs->values.empty())
    return fail(e, "House return actual party ObjectDB Array differs");
  const auto &b = characters->source_bindings();
  auto member = [&](const std::string &name, uint32_t kind,
                    FieldGlobalDataMemberState &value) {
    return core.read_constructed_member(actual, name, value, e) &&
           value.kind == kind && value.value && value.value->kind == kind;
  };
  auto integer = [&](const std::string &name, int64_t &n) {
    FieldGlobalDataMemberState value;
    if (!member(name, 2, value))
      return fail(e, "House return Character integer binding differs");
    n = value.value->integer;
    return true;
  };
  auto text = [&](const std::string &name, std::string &v) {
    FieldGlobalDataMemberState value;
    if (!member(name, 4, value))
      return fail(e, "House return Character String binding differs");
    v = value.value->string;
    return true;
  };
  auto &leader = out.characters.front();
  std::string name;
  if (!text(b.name, name) || name != leader.character_id ||
      !text(b.nickname, leader.nickname) || !integer(b.level, leader.level) ||
      !integer(b.exp, leader.experience) || !integer(b.hp, leader.hp) ||
      !integer(b.pp, leader.pp) || leader.level != inventory.state.level ||
      leader.hp != inventory.state.hp || leader.pp != inventory.state.pp)
    return fail(e, "House return Character and live inventory vitals diverged");
  FieldGlobalDataMemberState skills, permanent, affinities, status;
  if (!member(b.learned_skills, 5, skills) ||
      !member(b.permanent_boosts, 6, permanent) ||
      !member(b.affinities, 6, affinities) || !member(b.status, 5, status))
    return fail(e, "House return Character collection binding differs");
  leader.learned_skills.clear();
  for (const auto &v : skills.value->array) {
    if (!v || v->kind != 4)
      return fail(e, "House return learned skill is not a String");
    leader.learned_skills.push_back(v->string);
  }
  leader.permanent_boosts.clear();
  for (const auto &v : permanent.value->dictionary) {
    if (!v.second || v.second->kind != 2)
      return fail(e, "House return permanent boost is not an integer");
    leader.permanent_boosts.push_back({v.first, v.second->integer});
  }
  leader.affinity_multipliers.clear();
  for (const auto &v : affinities.value->dictionary) {
    if (!v.second || (v.second->kind != 2 && v.second->kind != 3))
      return fail(e, "House return affinity is not a source number");
    leader.affinity_multipliers.push_back(
        {v.first,
         v.second->kind == 2 ? double(v.second->integer) : v.second->real});
  }
  if (!status.value->array.empty() || !status.node_array)
    return fail(e, "House return status requires the actual source Node Array");
  leader.status.clear();
  for (auto id : status.node_array->values) {
    HouseStatusActualData observed;
    std::map<std::string, std::shared_ptr<GlobalYamlValue>> fields;
    if (!c.bridge()->status_data(actual, id, observed, e) ||
        !c.bridge()->status_fields(id, fields, e))
      return false;
    auto turns = fields.find(binding->status().turns_field);
    auto policy = std::find_if(
        binding->status().policies.begin(), binding->status().policies.end(),
        [&](const auto &p) { return p.id == observed.ailment; });
    if (turns == fields.end() || !turns->second || turns->second->kind != 2 ||
        policy == binding->status().policies.end())
      return fail(e, "House return Status.to_dict source fields differ");
    leader.status.push_back(
        {observed.ailment, policy->passive ? turns->second->integer : 0});
  }
  if (leader.status.size() != inventory.state.statuses.size())
    return fail(e,
                "House return actual Status and inventory projection differ");
  for (size_t n = 0; n < leader.status.size(); ++n)
    if (leader.status[n].status_id != inventory.state.statuses[n].id ||
        leader.status[n].passive_healing_turns !=
            inventory.state.statuses[n].passive_turns)
      return fail(e, "House return actual Status identity/turns differ");
  leader.inventory.clear();
  out.key_items.clear();
  out.storage.clear();
  std::set<uint32_t> seen_owners, uids;
  for (const auto &row : inventory.state.items.inventories) {
    const auto *owner = c.inventory_data()->owner(row.owner);
    std::vector<SessionItem> *to = nullptr;
    if (!owner || owner->role != row.role ||
        !seen_owners.insert(row.role).second)
      return fail(e, "House return inventory role/source owner differs");
    if (row.role == 0)
      to = &leader.inventory;
    else if (row.role == 1)
      to = &out.key_items;
    else if (row.role == 2)
      to = &out.storage;
    else
      return fail(e, "House return unknown inventory role");
    for (const auto &item : row.items) {
      const auto *d = c.item_definitions()->definition(item.definition);
      if (!d || !uids.insert(item.uid).second ||
          std::find(c.uid_ledger()->begin(), c.uid_ledger()->end(), item.uid) ==
              c.uid_ledger()->end())
        return fail(e, "House return Item UID/source definition is not owned");
      to->push_back({d->item_name, item.equipped, item.doses, item.uid});
    }
  }
  if (seen_owners != std::set<uint32_t>{0, 1, 2})
    return fail(e, "House return is missing an actual inventory owner");
  // Gameplay currency is owned by the live inventory consumer. Other scalar
  // globals are read by their reviewed SAVE/LOAD member mapping, not names.
  out.cash = inventory.state.cash;
  for (const auto &a : binding->assignments()) {
    if (a.role < 6 || a.role > 17 || a.role == 13)
      continue;
    FieldGlobalDataMemberState value;
    if (!core.read_global_member(a.member, value, e) || !value.value)
      return fail(e, "House return actual global source member unavailable");
    auto v = value.value;
    if (a.role == 6 || a.role == 7 || a.role == 9 || a.role == 10) {
      if (v->kind != 4)
        return fail(e, "House return source String global differs");
      if (a.role == 6)
        out.favorite_food = v->string;
      if (a.role == 7)
        out.player_name = v->string;
      if (a.role == 9)
        out.settings.menu_flavor = v->string;
      if (a.role == 10)
        out.settings.button_prompts = v->string;
    } else if (a.role == 8) {
      if (v->kind != 3 || !std::isfinite(v->real))
        return fail(e, "House return text speed differs");
      out.settings.text_speed = v->real;
    } else if (a.role == 11 || a.role == 14) {
      if (v->kind != 2)
        return fail(e, "House return source currency global differs");
      if (a.role == 11)
        out.earned_cash = v->integer;
      else
        out.bank = v->integer;
    } else if (a.role == 12) {
      if (v->kind != 1)
        return fail(e, "House return description source bool differs");
      out.settings.description = v->boolean;
    } else if (a.role == 15 || a.role == 16) {
      if (v->kind != 6)
        return fail(e, "House return source counter Dictionary differs");
      auto &to = a.role == 15 ? out.keys : out.rare_drops;
      to.clear();
      for (const auto &entry : v->dictionary) {
        if (!entry.second || entry.second->kind != 2)
          return fail(e, "House return source counter is not an integer");
        to.push_back({entry.first, entry.second->integer});
      }
    } else if (a.role == 17) {
      if (v->kind != 6)
        return fail(e, "House return encountered source Dictionary differs");
      out.encountered.clear();
      for (const auto &entry : v->dictionary) {
        if (!entry.second || entry.second->kind != 1)
          return fail(e, "House return encountered is not bool");
        out.encountered.push_back({entry.first, entry.second->boolean});
      }
    }
  }
  return validate_session_snapshot(out, e);
}
bool house_projection(const PodunkHouseReturnInput &in,
                      const SessionSnapshot &full, SessionSnapshot &out,
                      std::string &e) {
  auto next = full;
  next.flags.clear();
  next.object_flags.clear();
  next.seen_dialogue_flags.clear();
  next.encountered.clear();
  next.rare_drops.clear();
  // These counters are not consumed by the legacy House world. Its supported
  // counter domain is retained only in the detached view; full retains all.
  next.keys.clear();
  for (const auto &key : in.session_data->defaults().keys) {
    auto actual = std::find_if(full.keys.begin(), full.keys.end(),
                               [&](const auto &v) { return v.id == key.id; });
    if (actual == full.keys.end())
      return fail(e, "House return required counter missing");
    next.keys.push_back(*actual);
  }
  for (uint32_t n = 0; n < in.room.flag_count(); ++n) {
    const auto name = in.room.string(in.room.flag(n).name_string);
    auto actual = std::find_if(full.flags.begin(), full.flags.end(),
                               [&](const auto &v) { return v.id == name; });
    if (actual == full.flags.end())
      return fail(e, "House return required source flag missing");
    next.flags.push_back(*actual);
  }
  std::set<std::string> house_seen, house_encounters;
  for (uint32_t n = 0; n < in.house.count(HouseSection::Npcs); ++n)
    house_seen.insert(std::string(in.house.string(in.house.npc(n).seen_key)));
  for (uint32_t n = 0; n < in.house.count(HouseSection::Overrides); ++n)
    house_seen.insert(
        std::string(in.house.string(in.house.override_dialogue(n).seen_key)));
  for (uint32_t n = 0; n < in.room.battle_count(); ++n)
    house_encounters.insert(
        std::string(in.room.string(in.room.battle(n).enemy_string)));
  for (const auto &v : full.seen_dialogue_flags)
    if (house_seen.count(v.id))
      next.seen_dialogue_flags.push_back(v);
  for (const auto &v : full.encountered)
    if (house_encounters.count(v.id))
      next.encountered.push_back(v);
  out = std::move(next);
  return true;
}
} // namespace

bool prepare_podunk_house_return(const PodunkHouseReturnInput &in,
                                 PreparedPodunkHouseReturn &output,
                                 std::string &e) {
  if (!in.continuation || !in.continuation->initialized() || !in.programme ||
      !in.inventory || !in.session || !in.reentry || !in.doors ||
      !in.session_data || !in.restore_data || !in.room.valid() ||
      !in.house.valid() || !in.round.valid() || !in.legacy_items.valid() ||
      !in.continuation->random() || !in.continuation->uid_ledger() ||
      !in.continuation->characters() || !in.continuation->global() ||
      !in.continuation->bridge())
    return fail(e, "House return requires actual same-session owners");
  PreparedPodunkHouseReturn next;
  next.random_state = in.continuation->random()->state();
  next.random_draws = in.continuation->random()->raw_draw_count();
  next.uid_ledger = *in.continuation->uid_ledger();
  PodunkInventorySnapshot inventory;
  if (!export_owned(in, next.session, inventory, e))
    return false;
  next.inventory_revision = inventory.state.revision;
  next.inventory_owner = in.inventory;
  next.continuation_owner = in.continuation;
  next.source_door = in.door;
  // Reentry source binding is checked by the independently loaded resource.
  if (!in.reentry->valid() ||
      !in.reentry->matches(*in.doors, in.room, in.house, e) ||
      in.reentry->door_id() != in.door)
    return fail(e, "House return source Door binding differs");
  const auto target = in.reentry->position(),
             direction = in.reentry->direction();
  next.session.scene_id = in.reentry->target_scene();
  next.session.scene_label =
      std::string(in.room.string(in.room.scene().display_name_string));
  next.session.source_version =
      std::string(in.room.string(in.room.scene().version_string));
  next.session.position_x = target.x;
  next.session.position_y = target.y;
  next.session.direction_x = direction.x;
  next.session.direction_y = direction.y;
  SessionSnapshot projection;
  if (!house_projection(in, next.session, projection, e) ||
      !prepare_session_restore(*in.session_data, in.room, in.house, in.round,
                               in.legacy_items, in.font, projection,
                               next.restore, e) ||
      !prepare_fresh_house(next.restore, *in.restore_data, in.room, in.house,
                           in.font, in.phone, *in.continuation->random(),
                           in.viewport, next.house, e, in.adapters))
    return false;
  if (in.continuation->random()->state() != next.random_state ||
      in.continuation->random()->raw_draw_count() != next.random_draws ||
      *in.continuation->uid_ledger() != next.uid_ledger)
    return fail(e,
                "House return preparation unexpectedly changed live entropy");
  next.valid_ = true;
  output = std::move(next);
  e.clear();
  return true;
}
bool validate_podunk_house_return_commit(
    const PreparedPodunkHouseReturn &candidate, PodunkHouseContinuation &c,
    const PodunkInventoryHost &inventory, std::string &e) {
  PodunkInventorySnapshot actual;
  if (!candidate.valid() || !candidate.house || !candidate.restore.valid() ||
      candidate.continuation_owner != &c ||
      candidate.inventory_owner != &inventory || !c.initialized() ||
      !c.random() || !c.uid_ledger() ||
      c.random()->state() != candidate.random_state ||
      c.random()->raw_draw_count() != candidate.random_draws ||
      *c.uid_ledger() != candidate.uid_ledger ||
      !inventory.snapshot(actual, e) ||
      actual.state.revision != candidate.inventory_revision)
    return fail(e,
                "House return candidate changed before the source Door commit");
  e.clear();
  return true;
}
} // namespace encore::ctr
