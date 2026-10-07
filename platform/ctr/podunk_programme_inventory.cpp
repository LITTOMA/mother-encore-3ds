#include "podunk_programme_inventory.hpp"
#include "podunk_global_data_host.hpp"
#include "podunk_global_host.hpp"
#include <algorithm>
namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string &e, const char *reason) {
  e = reason;
  return false;
}
} // namespace
bool PodunkProgrammeInventory::owners(std::string &e) const {
  auto *c = input_.continuation;
  if (!c || !c->initialized() || !c->characters() || !c->global() ||
      !c->bridge() || !c->inventory_data() || !c->item_definitions() ||
      !c->registry() || c->registry()->poisoned() ||
      !c->bridge()->core().binds_source_owners(
          c->characters()->runtime(), c->global()->core(), *c->registry()))
    return reject(e,
                  "Programme Inventory is not the actual House continuation");
  e.clear();
  return true;
}
bool PodunkProgrammeInventory::format(const FieldGoodsTextContext &context,
                                      std::string &out, std::string &e) const {
  if (!owners(e) || !input_.goods || !input_.locale ||
      !input_.locale->catalog())
    return false;
  const auto *text = input_.goods->text(context.key);
  if (!text)
    return reject(e, "Goods formatting key not present in source data");
  const auto language = input_.locale->code();
  if (language != "en" && language != "zh_Hans_CN")
    return reject(e, "Goods source formatting locale not supported");
  auto candidate = language == "en" ? text->en : text->zh;
  auto replace = [&](std::string_view marker, const std::string &value) {
    for (size_t at = 0; (at = candidate.find(marker, at)) != candidate.npos;
         at += value.size())
      candidate.replace(at, marker.size(), value);
  };
  if (candidate.find("{target}") != candidate.npos) {
    auto *c = input_.continuation;
    auto *data = c->inventory_data();
    if (context.target != data->role(0)->id)
      return reject(e, "Goods formatting requires actual singleton Character");
    std::string nickname;
    if (!c->characters()->runtime().character_nickname(
            c->characters()->runtime().data()->first_character(), nickname, e))
      return false;
    replace("{target}", nickname);
  }
  replace("{value}", std::to_string(context.value));
  if (candidate.find("{stat}") != candidate.npos) {
    const auto &stats = input_.continuation->inventory_data()->stats();
    const auto at = std::find(stats.begin(), stats.end(), context.stat);
    if (at == stats.end())
      return reject(e, "Goods stat formatting is not source-bound");
    auto order = std::find(input_.goods->stat_order().begin(),
                           input_.goods->stat_order().end(),
                           uint32_t(at - stats.begin()));
    if (order == input_.goods->stat_order().end())
      return reject(e, "Goods source stat label missing");
    const auto *label = input_.goods->text(input_.goods->stat_label(
        uint32_t(order - input_.goods->stat_order().begin())));
    if (!label)
      return reject(e, "Goods source stat translation missing");
    replace("{stat}", language == "en" ? label->en : label->zh);
  }
  // Article/pronoun/FavFood and native Goods key-label presentation are not
  // granted by source-state activation. Their eventual UI owner must supply
  // the audited TextTools adapter before the corresponding branch is used.
  if (candidate.find_first_of("{}[]") != candidate.npos)
    return reject(e,
                  "Goods dynamic TextTools branch requires actual presenter");
  out = std::move(candidate);
  e.clear();
  return true;
}
bool PodunkProgrammeInventory::prepare(PodunkProgrammeInventoryInput input,
                                       std::string &e) {
  if (attempted_)
    return reject(e, "Programme Inventory construction already attempted");
  attempted_ = true;
  input_ = std::move(input);
  if (!owners(e) || !input_.goods || !input_.goods->valid() ||
      !input_.audio_bank || !input_.audio_bank->count() || !input_.locale ||
      !input_.locale->catalog() || !input_.locale->catalog()->valid() ||
      !input_.clock)
    return reject(
        e,
        "Programme Inventory requires real source/locale/audio/clock owners");
  auto &c = *input_.continuation;
  auto *characters = c.characters();
  if (!c.random() || !c.uid_ledger() || !c.audio() ||
      !input_.goods->bind_inventory(*c.inventory_data(), e))
    return false;
  PodunkInventorySnapshot snapshot;
  if (!c.inventory_snapshot(snapshot, e))
    return false;
  PodunkInventoryOps ops;
  ops.owners = [this](const FieldInventoryData &data, const auto &objects,
                      const FieldInventoryState &state, std::string &error) {
    return owners(error) && input_.continuation->characters()->owners(
                                data, objects, state, error);
  };
  ops.owner_exists = [this](FieldObjectId object) {
    auto *registry = input_.continuation->registry();
    return registry && registry->object_exists(object) &&
           bool(registry->native_reference(object));
  };
  ops.format = [this](const auto &context, auto &out, auto &error) {
    return format(context, out, error);
  };
  ops.key_name = [](const auto &, auto &, std::string &error) {
    return reject(error, "Goods native key-label presenter not attached");
  };
  ops.description = [](bool, std::string &error) {
    return reject(error,
                  "Goods description setter requires actual UI source binding");
  };
  if (!inventory_.prepare(
          *c.inventory_data(), *c.item_definitions(), *input_.goods, snapshot,
          *c.random(), *c.uid_ledger(), input_.clock, *c.registry(), *c.audio(),
          *input_.audio_bank, PodunkInventoryAudioMode::Required,
          std::move(ops), e) ||
      !inventory_.activate(e) ||
      !characters->bind_live_inventory(inventory_, e))
    return false;
  ready_ = true;
  e.clear();
  return true;
}
bool PodunkProgrammeInventory::snapshot(PodunkInventorySnapshot &out,
                                        std::string &e) const {
  if (!ready_ || !owners(e))
    return reject(e, "Programme Inventory owner is not active");
  return inventory_.snapshot(out, e);
}
} // namespace encore::ctr
