#pragma once
#include "encore/field_payphone.hpp"
#include "encore/field_shop.hpp"
#include <array>
#include <map>
namespace encore::upstream {
struct FieldInventoryOwner {
  uint32_t id = 0, role = 0;
  std::string source_name;
};
struct FieldInventoryAction {
  FieldItemFunction function{};
  std::string name, fail;
};
struct FieldInventoryPolicy {
  uint32_t definition = 0;
  std::string source;
  std::array<uint8_t, 32> source_sha{};
  bool consume_allowed = false, use_allowed = false;
  std::array<uint32_t, 7> boost_order{};
  std::vector<FieldInventoryAction> actions;
};
struct FieldInventoryStatusPolicy {
  std::string id, source, heal, fail;
  int32_t priority = 0;
  bool persistent = false, passive = false, exclusive = false,
       unconscious = false;
  struct Block {
    uint32_t selector = 0;
    std::string message;
  };
  std::vector<Block> blocks;
};
struct FieldInventoryText {
  std::string key, en, zh;
};
struct FieldInventoryLayout {
  std::array<float, 4> rect{}, color{};
};
struct FieldInventoryInitial {
  uint32_t owner = 0;
  FieldOwnedItem item{};
};
struct FieldInventoryGlyph {
  std::string path;
  uint32_t bytes = 0, width = 0, height = 0;
  std::array<float, 2> position{};
  std::array<uint8_t, 32> sha{};
};
class FieldInventoryData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  bool bind_definitions(const FieldItemDefinitions &, std::string &) const;
  const auto &source_pin() const { return pin_; }
  const auto &identity() const { return identity_; }
  const auto &owners() const { return owners_; }
  const auto &slots() const { return slots_; }
  const auto &stats() const { return stats_; }
  const auto &levels() const { return levels_; }
  const auto &initial() const { return initial_; }
  const auto &statuses() const { return statuses_; }
  uint32_t description_rows() const { return description_rows_; }
  const auto &sort_weights() const { return weights_; }
  int32_t sort_slot_step() const { return slot_step_; }
  const auto &sort_scores() const { return scores_; }
  const auto &equipped_glyph() const { return glyph_; }
  uint32_t initial_level() const { return initial_level_; }
  int64_t initial_hp() const { return initial_hp_; }
  int64_t initial_pp() const { return initial_pp_; }
  int64_t initial_cash() const { return initial_cash_; }
  uint32_t refresh_hp() const { return refresh_hp_; }
  double message_delay() const { return delay_; }
  const FieldInventoryOwner *owner(uint32_t) const;
  const FieldInventoryOwner *role(uint32_t) const;
  const FieldInventoryPolicy *policy(uint32_t) const;
  const FieldInventoryStatusPolicy *status(const std::string &) const;
  const FieldInventoryText *text(const std::string &) const;
  const FieldInventoryLayout *layout(const std::string &) const;
  const std::array<float, 4> *parameter(const std::string &) const;
  const std::string &sound(uint32_t n) const { return sounds_.at(n); }
  const std::string &feedback(uint32_t n) const { return feedback_.at(n); }
  const std::string &action_label(uint32_t n) const { return labels_.at(n); }

private:
  bool valid_ = false;
  std::array<uint8_t, 20> pin_{};
  std::array<uint8_t, 32> identity_{};
  std::vector<FieldInventoryOwner> owners_;
  std::array<std::string, 4> slots_{};
  std::array<std::string, 7> stats_{};
  std::vector<std::array<int32_t, 7>> levels_;
  std::vector<FieldInventoryInitial> initial_;
  uint32_t initial_level_ = 0, refresh_hp_ = 0;
  int64_t initial_hp_ = 0, initial_pp_ = 0, initial_cash_ = 0;
  double delay_ = 0;
  std::vector<FieldInventoryPolicy> policies_;
  std::vector<FieldInventoryStatusPolicy> statuses_;
  std::vector<FieldInventoryText> texts_;
  std::array<std::string, 3> sounds_{};
  std::array<std::string, 6> feedback_{};
  std::array<std::string, 4> labels_{};
  std::map<std::string, FieldInventoryLayout> layouts_;
  std::map<std::string, std::array<float, 4>> parameters_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  uint32_t description_rows_ = 0;
  std::array<int32_t, 7> weights_{};
  int32_t slot_step_ = 0;
  std::array<int32_t, 11> scores_{};
  FieldInventoryGlyph glyph_;
};
struct FieldInventoryStatus {
  std::string id;
  int64_t passive_turns = 0;
};
// This state owns only the admitted inventory/vitals/cash projection. It has no
// flags, scene, party/autoload roster, learned skills or generated RNG ledger.
struct FieldInventoryState {
  FieldItemSnapshot items;
  uint32_t level = 0;
  int64_t hp = 0, pp = 0, cash = 0;
  std::array<int64_t, 7> permanent{};
  std::vector<FieldInventoryStatus> statuses;
  uint64_t revision = 0;
};
enum class FieldInventoryEventKind : uint32_t { Message = 1, Sound, UseItem };
struct FieldInventoryEvent {
  FieldInventoryEventKind kind{};
  std::string key, stat;
  int64_t value = 0;
  FieldOwnedItem item{};
  uint32_t target = 0;
  // TargetCharaSelect emits stat/heal immediately, then its HP/PP coroutine.
  double after_seconds = 0;
  bool update = false;
};
struct FieldInventoryResult {
  bool performed = false, removed = false;
  uint32_t doses = 0;
  std::vector<FieldInventoryEvent> events;
};
enum class FieldGoodsActionKind : uint32_t {
  Consume = 1,
  Use,
  Equip,
  Unequip,
  Sort,
  Drop
};
struct FieldGoodsAction {
  FieldGoodsActionKind kind{};
  uint32_t source_action = 0;
  std::string label;
};
struct FieldGoodsRow {
  FieldOwnedItem item;
  std::string name_key;
  bool equipped = false;
};
class FieldInventoryRuntime {
public:
  bool initialize(const FieldInventoryData &, const FieldItemDefinitions &,
                  const FieldInventoryState &, std::string &);
  bool validate(const FieldInventoryState &, std::string &) const;
  const auto &state() const { return state_; }
  const FieldInventoryData *data() const { return data_; }
  const FieldItemDefinitions *definitions() const { return defs_; }
  FieldItemDefinitionsHost definitions_host();
  FieldShopHost
  shop_host(const FieldShopData &,
            std::function<bool(const std::string &, std::string &)> sound,
            std::function<bool(const std::string &, std::string &)> close);
  // Replaces only money/card callbacks after concrete source cross-binding;
  // actual node/flags/audio/dialogue/amount-box consumers remain mandatory.
  bool bind_payphone_inventory(const FieldPayphoneData &, FieldPayphoneHost &,
                               std::string &);
  // Caller already accepted the rest of its source scene/save. These operations
  // never alter those domains or certify unimplemented source behaviours.
  bool source_initial(SourceRandom &, std::vector<uint32_t> &,
                      LoadRngClockProvider, std::string &);
  bool equip(uint32_t uid, bool equipped, FieldInventoryResult &,
             std::string &);
  bool consume(uint32_t uid, uint32_t source_action, uint32_t target,
               FieldInventoryResult &, std::string &);
  bool use(uint32_t uid, uint32_t source_action, FieldInventoryResult &,
           std::string &);
  bool switch_items(uint32_t owner, uint32_t first, uint32_t second,
                    std::string &);
  bool sort_auto(uint32_t owner, bool chinese, std::string &);
  bool rows(uint32_t owner, std::vector<FieldGoodsRow> &, std::string &) const;
  bool actions(uint32_t uid, std::vector<FieldGoodsAction> &,
               std::string &) const;
  bool effective_stats(std::array<int64_t, 7> &, std::string &) const;
  bool encode_save(std::vector<uint8_t> &, std::string &) const;
  bool decode_save(const uint8_t *, size_t, FieldInventoryState &,
                   std::string &) const;
  // Scope-only relative order key/store/Ninten; not a replacement for full
  // global.LOAD (inactive character allocations belong to its source owner).
  bool restore(const uint8_t *, size_t, SourceRandom &, std::vector<uint32_t> &,
               LoadRngClockProvider, std::string &);
  bool has_context() const { return has_context_; }
  const FieldOwnedItem &context() const { return context_; }

private:
  const FieldInventoryData *data_ = nullptr;
  const FieldItemDefinitions *defs_ = nullptr;
  FieldInventoryState state_;
  bool has_context_ = false;
  FieldOwnedItem context_{};
  bool stats(const FieldInventoryState &, std::array<int64_t, 7> &,
             std::string &) const;
  bool commit_definitions(const FieldItemSnapshot &, const FieldItemSnapshot &,
                          const FieldItemResult &, std::string &);
  bool commit_shop(const FieldShopData &, const FieldShopSnapshot &,
                   const FieldShopSnapshot &, const FieldShopResult &,
                   std::string &);
};
} // namespace encore::upstream
