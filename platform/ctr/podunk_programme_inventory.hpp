#pragma once
#include "encore/field_goods.hpp"
#include "podunk_house_continuation.hpp"
namespace encore::ctr {
struct PodunkProgrammeInventoryInput {
  PodunkHouseContinuation *continuation = nullptr;
  const upstream::FieldGoodsData *goods = nullptr;
  const upstream::AudioBank *audio_bank = nullptr;
  const upstream::LocaleSelection *locale = nullptr;
  upstream::LoadRngClockProvider clock;
};
// The same source Inventory/Item objects are handed to the programme, door,
// payphone and future Goods presenter. Activating ownership does not open UI.
// Destroy this owner before its borrowed continuation/Registry.
class PodunkProgrammeInventory {
public:
  bool prepare(PodunkProgrammeInventoryInput, std::string &);
  PodunkInventoryHost *host() { return ready_ ? &inventory_ : nullptr; }
  const PodunkInventoryHost *host() const {
    return ready_ ? &inventory_ : nullptr;
  }
  bool snapshot(PodunkInventorySnapshot &, std::string &) const;

private:
  PodunkProgrammeInventoryInput input_;
  PodunkInventoryHost inventory_;
  bool attempted_ = false, ready_ = false;
  bool owners(std::string &) const;
  bool format(const upstream::FieldGoodsTextContext &, std::string &,
              std::string &) const;
};
} // namespace encore::ctr
