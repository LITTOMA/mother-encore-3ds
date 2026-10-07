#include "podunk_programme_state.hpp"
#include "podunk_global_data_host.hpp"
#include "podunk_global_host.hpp"
#include <algorithm>
#include <set>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *message) {
  e = message;
  return false;
}
bool same_key(const BasementKeyItem &a, const BasementKeyItem &b) {
  return a.id == b.id && a.doses == b.doses && a.grant == b.grant &&
         a.source == b.source && a.name_key == b.name_key;
}
} // namespace
bool PodunkProgrammeState::owners(std::string &e) const {
  const auto &i = input_;
  if (!i.continuation || !i.continuation->initialized() || !i.inventory ||
      !i.programme || !i.programme->valid() || !i.basement ||
      !i.basement->valid() || !i.tree || !i.scene || !i.npc || !i.session ||
      !i.continuation->registry() || i.continuation->registry()->poisoned() ||
      !i.continuation->characters() || !i.continuation->global() ||
      !i.continuation->bridge() ||
      !i.continuation->bridge()->core().complete() ||
      !i.continuation->bridge()->core().binds_source_owners(
          i.continuation->characters()->runtime(),
          i.continuation->global()->core(), *i.continuation->registry()))
    return fail(e, "Programme state does not own actual House continuation");
  if (i.continuation->characters()->runtime().registry() !=
          i.continuation->registry() ||
      !i.continuation->inventory_data() ||
      !i.continuation->item_definitions() ||
      i.programme->commit() != i.continuation->item_definitions()->source_pin())
    return fail(e, "Programme state source/global owner differs");
  PodunkInventorySnapshot snapshot;
  if (!i.inventory->snapshot(snapshot, e))
    return false;
  for (const auto &owner : snapshot.owners) {
    FieldObjectId actual = 0;
    if (!i.continuation->bridge()->core().actual_inventory_owner(owner.owner,
                                                                 actual, e) ||
        actual != owner.object ||
        !i.continuation->registry()->object_exists(actual))
      return fail(e, "Programme state actual inventory owner differs");
  }
  e.clear();
  return true;
}
bool PodunkProgrammeState::live(std::string &e) const {
  if (!prepared_ || !owners(e))
    return false;
  const auto *root = input_.tree->state(input_.tree->root());
  const auto *player = input_.tree->state(input_.player);
  if (!input_.scene->scene_ready() || !root || !root->alive || !root->inside ||
      !root->ready_notified || root->queued || !player || !player->alive ||
      !player->inside || !player->ready_notified || player->queued ||
      input_.continuation->registry()->current_scene() != input_.tree->root() ||
      input_.continuation->registry()->tree_owner(input_.player).get() !=
          input_.tree)
    return fail(e, "Programme state actual current scene/Player not live");
  e.clear();
  return true;
}
bool PodunkProgrammeState::prepare(PodunkProgrammeStateInput input,
                                   std::string &e) {
  if (prepared_)
    return fail(e, "Programme state already prepared");
  input_ = input;
  if (!owners(e) || !validate_session_snapshot(*input.session, e))
    return false;
  const auto *defs = input.continuation->item_definitions();
  const auto *npc_data = input.npc->data();
  const auto *chars = input.continuation->character_data();
  const auto *all = input.continuation->global_item_definitions();
  if (!npc_data || !chars || !all || !input.inventory->items() ||
      input.inventory->items()->data() != defs ||
      npc_data->source_pin() != input.programme->commit())
    return fail(e, "Programme state typed resources are not the same owners");
  const auto &p = *input.programme;
  const auto *descriptor = static_cast<const FieldNpcDescriptor *>(nullptr);
  for (const auto &n : npc_data->npcs())
    if (n.id == p.npc().id)
      descriptor = &n;
  if (!descriptor || descriptor->node != p.npc().node ||
      descriptor->ready_ordinal != p.npc().ready_ordinal)
    return fail(e, "Programme state source NPC binding differs");
  std::vector<Grant> grants;
  for (uint32_t index = 0; index < p.program_count(); ++index) {
    auto record = p.record(index);
    if (!record)
      return fail(e, "Programme source record missing");
    for (uint32_t n = 0; n < record->table.command_count; ++n) {
      const auto at = record->table.first_command + n;
      const auto command = p.command(at);
      if (command.opcode != uint16_t(DialogueActionKind::GrantKeyItem))
        continue;
      const auto *key = p.key(command.target_index);
      const auto *policy = input.basement->key_item(command.target_index);
      const auto *label = p.source_label(at);
      const auto *binding =
          label ? defs->programme(record->path, *label) : nullptr;
      const auto *d = binding ? defs->definition(binding->definition) : nullptr;
      std::array<uint8_t, 32> a{}, b{};
      if (!key || !policy || !same_key(*key, *policy) || !key->grant ||
          !binding || binding->operation != 4 || binding->scene != p.scene() ||
          binding->node != p.npc().node || !d || !d->keyitem() ||
          d->item_name != key->source || d->name_key != key->name_key ||
          d->doses != key->doses || !p.source_hash(d->source, a) ||
          !defs->source_hash(d->source, b) || a != b)
        return fail(e, "Programme Item.get/AddItem source binding rejected");
      for (const auto &old : grants)
        if (old.key == key->id)
          return fail(e, "Programme key effect has ambiguous source cursor");
      grants.push_back({key->id, d->id, binding->program, binding->label});
    }
  }
  GlobalItemCache *cache = nullptr;
  auto &data = input.continuation->characters()->runtime();
  const auto *yaml = input.continuation->characters()->yaml_cache_data();
  if (!yaml)
    return fail(e, "Programme actual global Items cache absent");
  for (const auto &policy : yaml->policies())
    if (policy.role == 4) {
      FieldGlobalDataMemberState member;
      if (cache || !data.read_global_member(policy.member, member, e) ||
          !member.items)
        return fail(e, "Programme Items cache member binding rejected");
      cache = member.items;
    }
  if (!cache || !input.continuation->random() ||
      !input.continuation->uid_ledger() ||
      !input.inventory->bind_source_item_factory(
          *chars, *all, *cache, *input.continuation->random(),
          *input.continuation->uid_ledger(), e))
    return false;
  grants_ = std::move(grants);
  prepared_ = true;
  e.clear();
  return true;
}
bool PodunkProgrammeState::actual_path(uint64_t object, std::string &path,
                                       std::string &e) const {
  if (!live(e) ||
      object != input_.tree->source_object(input_.programme->npc().id))
    return fail(e, "Programme actor is not actual source NPC");
  const auto *n = input_.tree->state(object);
  const auto *d = input_.tree->descriptor(object);
  FieldIdentity identity;
  std::array<uint8_t, 32> sha{};
  if (!n || !d || !n->alive || !n->inside || !n->ready_notified || n->queued ||
      d->path != input_.programme->npc().node ||
      d->ready != input_.programme->npc().ready_ordinal ||
      !input_.tree->object_identity(object, identity) ||
      identity.upstream_commit != input_.programme->commit() ||
      !input_.programme->source_hash(input_.programme->scene(), sha) ||
      identity.source_sha256 != sha ||
      !input_.programme->source_hash(d->script, sha) || d->script_sha != sha ||
      input_.continuation->registry()->tree_owner(object).get() != input_.tree)
    return fail(e, "Programme source NPC ObjectDB/Ready identity rejected");
  return input_.continuation->registry()->get_path(object, path, e);
}
bool PodunkProgrammeState::actor(uint32_t source, uint64_t &object,
                                 std::string &path, std::string &e) const {
  if (!prepared_ || source != input_.programme->npc().id)
    return fail(e, "Programme unknown source NPC");
  auto id = input_.tree->source_object(source);
  std::string actual;
  if (!actual_path(id, actual, e))
    return false;
  object = id;
  path = std::move(actual);
  e.clear();
  return true;
}
bool PodunkProgrammeState::flag(std::string_view name, bool &value,
                                std::string &e) const {
  if (!owners(e))
    return false;
  bool present = false;
  return input_.continuation->characters()->flags().read(false, name, present,
                                                         value, e);
}
bool PodunkProgrammeState::seen_identity(std::string_view key,
                                         std::string &e) const {
  uint64_t object = 0;
  std::string path;
  if (!actor(input_.programme->npc().id, object, path, e))
    return false;
  for (const auto &row : input_.programme->npc().rows)
    if (key == path + ":" + row.flag + ":" + std::to_string(row.ordinal) + ":" +
                   row.program) {
      e.clear();
      return true;
    }
  return fail(e,
              "Programme seen key is not the actual source get_path identity");
}
bool PodunkProgrammeState::seen(std::string_view key, bool &value,
                                std::string &e) const {
  return seen_identity(key, e) &&
         input_.continuation->characters()->flags().seen(key, value, e);
}
bool PodunkProgrammeState::mark_seen(std::string_view key, std::string &e) {
  // Source marks before UI construction and emits no flags_updated here.
  return seen_identity(key, e) &&
         input_.continuation->characters()->flags().mark_seen(key, e) &&
         writeback(e);
}
const PodunkProgrammeState::Grant *
PodunkProgrammeState::grant(const BasementKeyItem &key) const {
  if (!prepared_)
    return nullptr;
  const auto *policy = input_.basement->key_item(key.id);
  if (!policy || !same_key(key, *policy) || !key.grant)
    return nullptr;
  for (const auto &g : grants_)
    if (g.key == key.id)
      return &g;
  return nullptr;
}
bool PodunkProgrammeState::admit_key(const BasementKeyItem &key,
                                     std::string &e) const {
  if (!owners(e))
    return false;
  const auto *policy = input_.basement->key_item(key.id);
  const auto *defs = input_.continuation->item_definitions();
  const FieldItemDefinition *definition = nullptr;
  for (const auto &d : defs->definitions())
    if (d.item_name == key.source)
      definition = &d;
  if (!policy || !same_key(key, *policy) || !key.grant || !definition ||
      !definition->keyitem() || definition->doses != key.doses ||
      definition->name_key != key.name_key)
    return fail(e, "Programme key effect definition is not source-bound");
  PodunkInventorySnapshot state;
  if (!input_.inventory->snapshot(state, e))
    return false;
  const auto *owner = input_.continuation->inventory_data()->role(1);
  if (!owner)
    return fail(e, "Programme actual KEY inventory missing");
  for (const auto &o : state.owners)
    if (o.owner == owner->id) {
      FieldGlobalDataObject actual;
      if (!input_.continuation->characters()->read_object(o.object, actual,
                                                          e) ||
          actual.kind != 2 || actual.role != 1)
        return fail(e,
                    "Programme KEY inventory is not actual globaldata owner");
      e.clear();
      return true;
    }
  return fail(e, "Programme KEY inventory owner unavailable");
}
bool PodunkProgrammeState::grant_key(const BasementKeyItem &key,
                                     std::string &e) {
  if (!live(e) || !admit_key(key, e))
    return false;
  auto *g = grant(key);
  if (!g)
    return fail(e, "Programme key grant has no admitted source cursor");
  FieldItemResult result;
  if (!input_.inventory->items()->grant_programme(g->programme, g->label,
                                                  result, e))
    return false;
  const auto *owner = input_.continuation->inventory_data()->role(1);
  FieldObjectId object = 0;
  FieldOwnedItem actual;
  if (result.kind != FieldItemResultKind::Owned || result.owner != owner->id ||
      result.item.definition != g->definition ||
      result.item.doses != key.doses ||
      !input_.inventory->item_object(result.item.uid, object, e))
    return fail(e, "Programme actual Item/AddItem transaction differs");
  auto ref = input_.inventory->object(object);
  if (!ref || !ref->read_item(actual, e) || actual.uid != result.item.uid ||
      actual.definition != result.item.definition ||
      actual.doses != result.item.doses)
    return fail(e, "Programme actual Item Reference publication missing");
  return writeback(e);
}
bool PodunkProgrammeState::writeback(std::string &e) {
  if (!input_.session || !owners(e))
    return false;
  SessionSnapshot next;
  if (!export_snapshot(*input_.session, next, e))
    return false;
  *input_.session = std::move(next);
  e.clear();
  return true;
}
bool PodunkProgrammeState::binds(const PodunkHouseContinuation &c,
                                  const PodunkInventoryHost &inventory,
                                  const SessionSnapshot &session) const {
  return prepared_ && input_.continuation == &c &&
         input_.inventory == &inventory && input_.session == &session;
}
bool PodunkProgrammeState::export_snapshot(const SessionSnapshot &base,
                                            SessionSnapshot &out,
                                            std::string &e) const {
  if (!prepared_ || !owners(e))
    return false;
  PodunkInventorySnapshot state;
  if (!input_.inventory->snapshot(state, e))
    return false;
  auto next = base;
  next.key_items.clear();
  const auto *defs = input_.continuation->item_definitions();
  for (const auto &row : state.state.items.inventories)
    if (row.role == 1)
      for (const auto &item : row.items) {
        const auto *d = defs->definition(item.definition);
        FieldObjectId id = 0;
        FieldOwnedItem actual;
        if (!d || !d->keyitem() ||
            !input_.inventory->item_object(item.uid, id, e))
          return false;
        auto ref = input_.inventory->object(id);
        if (!ref || !ref->read_item(actual, e) || actual.uid != item.uid ||
            actual.definition != item.definition ||
            actual.doses != item.doses || actual.equipped != item.equipped)
          return fail(e,
                      "Programme save KEY identity differs from actual Item");
        next.key_items.push_back(
            {d->item_name, item.equipped, item.doses, item.uid});
      }
  const auto &flags = input_.continuation->characters()->flags().state();
  auto copy = [](const FieldFlagDictionary &from,
                 std::vector<SessionFlag> &to) {
    to.clear();
    for (const auto &v : from)
      to.push_back({v.first, v.second});
  };
  copy(flags.normal, next.flags);
  copy(flags.objects, next.object_flags);
  copy(flags.seen, next.seen_dialogue_flags);
  if (!validate_session_snapshot(next, e))
    return false;
  out = std::move(next);
  e.clear();
  return true;
}
bool PodunkProgrammeState::apply(PodunkProgrammeOps &ops, std::string &e) {
  if (!prepared_ || !owners(e))
    return false;
  ops.actual_path = [this](auto object, auto &path, auto &error) {
    return actual_path(object, path, error);
  };
  ops.flag = [this](auto name, bool &value, auto &error) {
    return flag(name, value, error);
  };
  ops.seen = [this](auto key, bool &value, auto &error) {
    return seen(key, value, error);
  };
  ops.mark_seen = [this](auto key, auto &error) {
    return mark_seen(key, error);
  };
  ops.admit_key = [this](const auto &key, auto &error) {
    return admit_key(key, error);
  };
  ops.keys.validate_key_item = ops.admit_key;
  ops.keys.grant_key_item = [this](const auto &key, auto &error) {
    return grant_key(key, error);
  };
  e.clear();
  return true;
}
} // namespace encore::ctr
