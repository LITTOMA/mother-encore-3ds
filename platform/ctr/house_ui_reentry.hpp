#pragma once
#include "encore/fresh_house.hpp"
#include "encore/house_reentry.hpp"
#include "podunk_dialogue_host.hpp"
namespace encore::ctr {
class HouseUiContinuation;
class HouseUiReentryBorrowOwner;
// All pointers borrow fixed-address live owners. Keep both House owners and
// both source Trees alive until every borrower has committed successfully.
struct HouseUiReentryInput {
  upstream::FieldGlobalRegistry *registry = nullptr;
  const upstream::HouseReentryData *source = nullptr;
  const upstream::FieldNodeTreeData *source_tree = nullptr;
  const upstream::FieldDoorData *doors = nullptr;
  const upstream::FieldDoorRuntime *door_runtime = nullptr;
  const upstream::FreshHouseState *old_house = nullptr;
  upstream::FreshHouseState *next_house = nullptr;
  upstream::HouseView house_data{};
  std::shared_ptr<upstream::FieldNodeTreeRuntime> old_tree, next_tree;
  upstream::FieldObjectId old_root = 0, next_root = 0;
  upstream::FieldIdentity old_identity{}, next_identity{};
  upstream::SourceRandom *random = nullptr;
  std::vector<uint32_t> *uid_ledger = nullptr;
  HouseUiReentryBorrowOwner *borrowers = nullptr;
};
// A concrete composition must inspect ALL printer/House borrowers, including
// completion/locale/glyph/value callbacks, programme/dialogue/native services,
// factory instances, waits, deferred/input callbacks and captured House
// pointers. An uninspected owner or pending unsupported callback sets complete
// false; a function object's existence is not an ownership receipt.
struct HouseUiReentryBorrowState {
  const upstream::FieldGlobalRegistry *registry = nullptr;
  const upstream::HousePresentation *printer = nullptr;
  const PodunkDialogueRootScript *dialogue_script = nullptr;
  const upstream::SourceRandom *random = nullptr;
  const std::vector<uint32_t> *uid_ledger = nullptr;
  std::vector<upstream::FieldObjectId> dialogue_objects;
  size_t pending_ui_callbacks = 0, pending_native_callbacks = 0;
  bool complete = false;
};
class HouseUiReentryBorrowOwner {
public:
  virtual ~HouseUiReentryBorrowOwner() = default;
  // Read-only admission, including the exact same UI/script and destination
  // source identities. rebound selects which real printer must be observed.
  virtual bool observe(const HouseUiReentryInput &, const HouseUiContinuation &,
                       bool rebound, HouseUiReentryBorrowState &,
                       std::string &) const = 0;
  // Change borrowed pointers/callback receivers only. Preserve actual objects,
  // source fields, waits, signal connections, clocks, RNG and UID ledger.
  // Do not cold construct, replay Ready/LOAD, drain callbacks or reset caches.
  virtual bool rebind(const HouseUiReentryInput &, HouseUiContinuation &,
                      std::string &) = 0;
};
class HouseUiReentryTicket {
public:
  bool valid() const { return owner_ != nullptr; }
private:
  friend class HouseUiContinuation;
  const HouseUiContinuation *owner_ = nullptr;
  HouseUiReentryInput input_;
  HouseUiReentryBorrowState borrowers_;
  uint64_t random_state_ = 0, random_draws_ = 0;
  std::vector<uint32_t> uids_;
  upstream::FieldObjectId ui_ = 0, canvas_ = 0;
  uint32_t story_generation_ = 0;
  size_t outcome_cursor_ = 0;
  bool dialogue_event_ = false, story_event_ = false;
};
} // namespace encore::ctr
