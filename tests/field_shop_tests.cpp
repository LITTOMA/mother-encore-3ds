// Manual-only cases. Parent supplies its actual checked 19-definition bundle;
// this file is compile-checked, never automatically run by development CI.
#include "encore/field_shop.hpp"
#include "encore/field_vending_machine.hpp"
#include <cassert>
using namespace encore::upstream;
void manual_field_shop_cases(const FieldShopData &data,
                             const FieldItemDefinitions &defs,
                             SourceRandom &random, uint32_t source_member) {
  std::string error;
  FieldShopSnapshot live;
  live.cash = 100000;
  live.items.inventories.push_back({source_member, 0, {}});
  live.items.party_order = {source_member};
  live.natural_order = {source_member};
  std::vector<uint32_t> ledger;
  std::string closed;
  FieldShopHost host;
  host.bind = [&](const auto &d, const auto &i, std::string &) {
    return &d == &data && &i == &defs && d.source_pin() == i.source_pin();
  };
  host.read = [&](auto &out, std::string &) {
    out = live;
    return true;
  };
  host.commit = [&](const auto &before, const auto &after, const auto &,
                    std::string &) {
    if (before.revision != live.revision)
      return false;
    live = after;
    ++live.revision;
    return true;
  };
  host.sound = [&](const auto &path, std::string &) {
    for (uint32_t k = 0; k < 5; ++k)
      if (path == data.sound(k))
        return true;
    return false;
  };
  host.close = [&](const auto &s, std::string &) {
    closed = s;
    return true;
  };
  LoadRngClockProvider clock = [](auto &s, std::string &) {
    s = {1700000000, 2000000};
    return true;
  };
  FieldShopRuntime menu;
  assert(menu.initialize(data, defs, random, ledger, clock, host, error));
  assert(!menu.open("unsupported-source-shop", error));
  assert(menu.open(data.name(), error));
  assert(ledger.size() == data.offers().size());
  assert(menu.cancel(error) && closed.empty());
  assert(menu.open(data.name(), error));
  assert(menu.select(error));
  const auto cash = live.cash;
  assert(menu.select(error) && menu.phase() == FieldShopPhase::ConfirmBuy);
  assert(menu.answer(true, error));
  assert(live.items.inventories.front().items.size() == 1);
  const auto bought = live.items.inventories.front().items.front();
  assert(bought.definition == data.offers().front());
  assert(live.cash == cash - data.policy(bought.definition)->cost);
  assert(menu.cancel(error));
  assert(menu.move(1, false, error));
  assert(menu.select(error));
  assert(menu.select(error) && menu.phase() == FieldShopPhase::ConfirmSell);
  const auto sale_cash = live.cash;
  assert(menu.answer(true, error));
  assert(live.items.inventories.front().items.empty());
  assert(live.cash == sale_cash + data.policy(bought.definition)->value);
  assert(menu.cancel(error));
  assert(menu.cancel(error));
  assert(closed == data.policy(data.offers().front())->name);
  live.cash = 0;
  assert(menu.open(data.name(), error));
  assert(menu.select(error));
  assert(menu.select(error) && menu.phase() == FieldShopPhase::Insufficient);
  assert(menu.answer(true, error) && menu.phase() == FieldShopPhase::Sell);
  assert(menu.cancel(error));
  assert(menu.cancel(error));
  for (uint32_t i = 0; i < defs.capacity(0); ++i)
    live.items.inventories.front().items.push_back(
        {data.offers().front(), 10000000 + i,
         data.policy(data.offers().front())->doses, false});
  assert(menu.open(data.name(), error));
  assert(menu.select(error));
  assert(menu.select(error) && menu.phase() == FieldShopPhase::Buy &&
         menu.warning());
  assert(menu.idle(data.warning_seconds(), error) && !menu.warning());
  assert(!menu.idle(-1, error));
  assert(menu.cancel(error));
  assert(menu.cancel(error));
  auto bad = host;
  bad.commit = {};
  FieldShopRuntime rejected;
  assert(!rejected.initialize(data, defs, random, ledger, clock, bad, error));
}
void manual_field_shop_decoder_cases(const std::vector<uint8_t> &shop,
                                     const std::vector<uint8_t> &vending) {
  auto recalc = [](std::vector<uint8_t> &b) {
    uint32_t c = ~0u;
    for (size_t i = 0; i < b.size(); ++i) {
      c ^= i >= 16 && i < 20 ? 0 : b[i];
      for (unsigned j = 0; j < 8; ++j)
        c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
    }
    c = ~c;
    for (unsigned j = 0; j < 4; ++j)
      b[16 + j] = uint8_t(c >> (j * 8));
  };
  std::string e;
  FieldShopData s;
  FieldVendingData v;
  assert(s.load(shop.data(), shop.size(), e));
  assert(v.load(vending.data(), vending.size(), e));
  assert(!s.load(nullptr, shop.size(), e));
  assert(!v.load(vending.data(), 63, e));
  for (size_t field : {size_t(8), size_t(20), size_t(24)}) {
    auto b = shop;
    b[field] = 99;
    recalc(b);
    assert(!s.load(b.data(), b.size(), e));
    auto d = vending;
    d[field] = 99;
    recalc(d);
    assert(!v.load(d.data(), d.size(), e));
  }
  auto bad = shop;
  bad.at(32) ^= 1;
  assert(!s.load(bad.data(), bad.size(), e));
  auto trailing = vending;
  trailing.push_back(0);
  assert(!v.load(trailing.data(), trailing.size(), e));
}
