// Manual only. This source is compile-checked without running the cases.
#include "encore/field_goods.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
using namespace encore::upstream;
static std::vector<uint8_t> read(const char *p) {
  std::ifstream f(p, std::ios::binary);
  assert(f);
  return {(std::istreambuf_iterator<char>(f)), {}};
}
static uint32_t crc(const std::vector<uint8_t> &b) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < b.size(); ++i) {
    c ^= i >= 16 && i < 20 ? 0 : b[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
  }
  return ~c;
}
static void patch(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  for (unsigned n = 0; n < 4; ++n)
    b[at + n] = uint8_t(v >> (8 * n));
}
int main(int argc, char **argv) {
  assert(argc == 4);
  std::string e;
  auto raw = read(argv[1]);
  FieldGoodsData d;
  FieldInventoryData inv;
  FieldItemDefinitions defs;
  assert(d.load(raw.data(), raw.size(), e) && inv.load_file(argv[2], e) &&
         defs.load_file(argv[3], e) && d.bind_inventory(inv, e));
  for (size_t n = 0; n < raw.size(); ++n) {
    FieldGoodsData bad;
    assert(!bad.load(raw.data(), n, e));
  }
  for (auto off : {8u, 20u, 24u, 28u}) {
    auto b = raw;
    patch(b, off, 99);
    patch(b, 16, crc(b));
    FieldGoodsData bad;
    assert(!bad.load(b.data(), b.size(), e));
  }
  auto forged = raw;
  forged[96] ^= 1;
  patch(forged, 16, crc(forged));
  FieldGoodsData other;
  assert(other.load(forged.data(), forged.size(), e) &&
         !other.bind_inventory(inv, e));
  FieldInventoryState state;
  state.level = inv.initial_level();
  state.items.party_order = {inv.role(0)->id};
  for (const auto &o : inv.owners())
    state.items.inventories.push_back({o.id, o.role, {}});
  FieldInventoryRuntime inventory;
  assert(inventory.initialize(inv, defs, state, e));
  SourceRandom rng(7);
  std::vector<uint32_t> ledger;
  uint64_t ticks = 0;
  auto clock = [&](LoadRngClockSample &s, std::string &) {
    s = {1700000000, ++ticks};
    return true;
  };
  assert(inventory.source_initial(rng, ledger, clock, e));
  FieldItemDefinitionsRuntime items;
  assert(items.initialize(defs, rng, ledger, clock,
                          inventory.definitions_host(), e));
  FieldGoodsMenu menu;
  assert(!menu.initialize(d, inventory, items, {}, e));
  FieldGoodsHost host;
  host.bind = [&](const auto &g, const auto &i, std::string &e) {
    return g.bind_inventory(i, e);
  };
  host.format = [&](const FieldGoodsTextContext &c, std::string &out,
                    std::string &) {
    auto *t = d.text(c.key);
    if (!t)
      return false;
    out = t->en;
    return true;
  };
  host.sound = [&](const std::string &s, std::string &) {
    return std::any_of(d.textures().begin(), d.textures().end(),
                       [](const auto &t) { return t.bytes > 0; }) &&
           !s.empty();
  };
  host.description = [](bool, std::string &) { return true; };
  host.key_name = [](const std::string &, std::string &out, std::string &) {
    out = "L";
    return true;
  };
  assert(menu.initialize(d, inventory, items, host, e) &&
         menu.open("Ninten", true, false, e));
  assert(!menu.input(FieldGoodsInput(99), e) && !menu.idle(-1, e));
  auto current = inventory.state();
  auto holder = std::find_if(
      current.items.inventories.begin(), current.items.inventories.end(),
      [&](const auto &i) { return i.owner == inv.role(0)->id; });
  const auto *fries = defs.definition("Fries");
  assert(fries);
  holder->items.push_back({fries->id, 0, fries->doses, false});
  assert(inventory.initialize(inv, defs, current, e));
  assert(menu.input(FieldGoodsInput::Right, e) && menu.selection() == 1);
  assert(menu.input(FieldGoodsInput::Accept, e) &&
         menu.phase() == FieldGoodsPhase::Actions);
  assert(menu.input(FieldGoodsInput::Accept, e) &&
         menu.phase() == FieldGoodsPhase::Targets);
  auto revision = inventory.state().revision;
  assert(menu.input(FieldGoodsInput::Cancel, e) &&
         menu.phase() == FieldGoodsPhase::Actions &&
         inventory.state().revision == revision);
  assert(menu.input(FieldGoodsInput::Accept, e) &&
         menu.phase() == FieldGoodsPhase::Targets);
  assert(menu.input(FieldGoodsInput::Accept, e) &&
         menu.phase() == FieldGoodsPhase::Items);
  assert(menu.idle(0, e) && menu.phase() == FieldGoodsPhase::Message);
  assert(menu.input(FieldGoodsInput::Cancel, e) &&
         menu.phase() == FieldGoodsPhase::Items);
  assert(menu.idle(1, e));
  assert(menu.input(FieldGoodsInput::Left, e) && menu.selection() == 0);
  assert(menu.input(FieldGoodsInput::Accept, e) &&
         menu.phase() == FieldGoodsPhase::Actions);
  assert(menu.actions().front().kind == FieldGoodsActionKind::Unequip);
  bool show = false;
  std::array<int64_t, 7> before{}, projected{};
  assert(menu.equipment_preview(show, before, projected, e) && show &&
         before != projected);
  assert(menu.input(FieldGoodsInput::Accept, e) &&
         menu.phase() == FieldGoodsPhase::Items);
  assert(menu.input(FieldGoodsInput::OwnerNext, e) &&
         menu.owner() == inv.role(1)->id);
  assert(menu.input(FieldGoodsInput::Accept, e));
  assert(menu.actions().front().kind == FieldGoodsActionKind::Use);
  assert(menu.input(FieldGoodsInput::Cancel, e) &&
         menu.phase() == FieldGoodsPhase::Items);
  assert(menu.input(FieldGoodsInput::Cancel, e) &&
         menu.phase() == FieldGoodsPhase::Closing);
  assert(menu.idle(d.clip("Close")->length, e) && !menu.visible());
  // Source InventoryUI.open preserves current selection. Prior move cooldown,
  // message-close and stat-close clocks belong to the previous open epoch.
  auto reopened = inventory.state();
  for (auto &owner : reopened.items.inventories)
    if (owner.role == 0)
      owner.items.push_back({fries->id, 1, fries->doses, false});
  assert(inventory.initialize(inv, defs, reopened, e));
  assert(menu.open("Ninten", true, false, e));
  assert(menu.input(FieldGoodsInput::Right, e) && menu.selection() == 1);
  assert(menu.input(FieldGoodsInput::Cancel, e));
  assert(menu.idle(d.clip("Close")->length, e) && !menu.visible());
  assert(menu.open("Ninten", true, false, e) && menu.selection() == 1);
  assert(!menu.message_visual());
  assert(menu.input(FieldGoodsInput::Left, e) && menu.selection() == 0);
}
