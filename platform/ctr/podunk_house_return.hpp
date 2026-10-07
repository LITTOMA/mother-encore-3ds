#pragma once
#include "encore/fresh_house.hpp"
#include "encore/house_reentry.hpp"
#include "podunk_programme_state.hpp"

namespace encore::ctr {
struct PodunkHouseReturnInput {
  PodunkHouseContinuation *continuation = nullptr;
  const PodunkProgrammeState *programme = nullptr;
  const PodunkInventoryHost *inventory = nullptr;
  const upstream::SessionSnapshot *session = nullptr;
  const upstream::HouseReentryData *reentry = nullptr;
  const upstream::FieldDoorData *doors = nullptr;
  uint32_t door = 0;
  const upstream::NativeSessionData *session_data = nullptr;
  const upstream::RestoreData *restore_data = nullptr;
  upstream::RoomView room{};
  upstream::HouseView house{};
  upstream::RoundView round{};
  upstream::ItemView legacy_items{};
  upstream::BattleView font{};
  upstream::PhoneView phone{};
  upstream::Vec2 viewport{};
  upstream::FreshHouseAdapters adapters{};
};

// Source objects and entropy remain borrowed from the existing continuation.
// The full session retains outdoor dictionaries. Only the legacy House view
// receives a checked projection into its smaller supported resource domain.
// No source Player is created, LOAD replayed or Ready granted by this bridge.
struct PreparedPodunkHouseReturn {
  upstream::SessionSnapshot session;
  upstream::PreparedSessionRestore restore;
  std::unique_ptr<upstream::FreshHouseState> house;
  uint64_t random_state = 0, random_draws = 0;
  std::vector<uint32_t> uid_ledger;
  uint64_t inventory_revision = 0;
  const PodunkInventoryHost *inventory_owner = nullptr;
  const PodunkHouseContinuation *continuation_owner = nullptr;
  uint32_t source_door = 0;
  bool valid() const { return valid_; }

private:
  bool valid_ = false;
  friend bool prepare_podunk_house_return(const PodunkHouseReturnInput &,
                                          PreparedPodunkHouseReturn &,
                                          std::string &);
};

// Read-only preflight and detached legacy House construction. Output is
// unchanged on failure; the caller commits its actual Door source lifecycle.
bool prepare_podunk_house_return(const PodunkHouseReturnInput &,
                                 PreparedPodunkHouseReturn &, std::string &);
// Recheck borrowed entropy and inventory revision before committing the Door.
// This does not replace the actual scene, reparent the player, or finish Ready.
bool validate_podunk_house_return_commit(const PreparedPodunkHouseReturn &,
                                         PodunkHouseContinuation &,
                                         const PodunkInventoryHost &,
                                         std::string &);
} // namespace encore::ctr
