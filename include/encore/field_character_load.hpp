#pragma once
#include "encore/field_character_load_schema.hpp"
#include "encore/field_global_data.hpp"
#include "encore/global_yaml_caches.hpp"
namespace encore::upstream {
struct FieldCharacterSavedItem {
  std::string name;
  bool equipped = false, has_uid = false;
  uint32_t doses = 0, uid = 0;
};
struct FieldCharacterEnemySkill {
  std::string skill;
  int64_t weight = 0, cooldown = 0;
};
class FieldCharacterEnemySkillReference : public FieldGlobalNativeReference {
public:
  virtual bool read_skill(FieldCharacterEnemySkill &,
                          int64_t &remaining_cooldown, std::string &) const = 0;
};
struct FieldCharacterLoadRow {
  uint32_t id = 0, role = 0;
  std::string name, display_name, nickname;
  int64_t level = 0, exp = 0, hp = 0, pp = 0;
  bool untargetable = false;
  std::vector<int64_t> permanent, npc_stats;
  std::vector<std::vector<int64_t>> targets;
  std::vector<std::string> skills;
  std::vector<std::pair<std::string, double>> affinities;
  std::vector<std::pair<std::string, int64_t>> permanent_fields;
  std::vector<FieldCharacterSavedItem> items;
  std::vector<FieldCharacterEnemySkill> npc_skills;
};
class FieldCharacterLoadData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  const auto &identity() const { return identity_; }
  const auto &ir_sha256() const { return ir_; }
  const auto &rows() const { return rows_; }
  const auto &stats() const { return stats_; }
  const auto &skills_order() const { return skills_; }
  const auto &slots() const { return slots_; }
  const auto &exp_floors() const { return floors_; }
  const auto &hp() const { return hp_; }
  const auto &pp() const { return pp_; }
  const auto &npc_skill_getter() const { return npc_skill_getter_; }
  const auto &source_bindings() const { return bindings_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{};
  std::vector<FieldCharacterLoadRow> rows_;
  std::vector<std::string> stats_, skills_, slots_;
  std::string hp_, pp_, npc_skill_getter_;
  std::vector<int64_t> floors_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  FieldCharacterLoadBindings bindings_;
};
struct FieldCharacterOwnedReference {
  FieldGlobalRegistry *registry = nullptr;
  FieldObjectId object = 0;
  std::shared_ptr<void> actual_owner;
  std::shared_ptr<const FieldCharacterEnemySkillReference> enemy_skill_owner{};
};
enum class FieldCharacterLoadWrite : uint32_t {
  None,
  Inventory,
  Name,
  Exp,
  Level,
  Stat,
  Hp,
  Pp,
  Status,
  Nickname,
  LearnedSkills,
  PermanentBoosts,
  Affinities,
  SortSkills,
  Untargetable,
  NpcSkillsReset,
  NpcSkillAppend
};
struct FieldCharacterLoadState {
  FieldObjectId object = 0;
  uint32_t declaration = 0, role = 0;
  std::string name, nickname;
  int64_t level = 0, exp = 0, hp = 0, pp = 0;
  std::vector<int64_t> stats, permanent;
  std::vector<std::string> skills;
  std::vector<std::pair<std::string, double>> affinities;
  std::vector<std::pair<std::string, int64_t>> permanent_fields;
  // Actual source arrays; cold Status capability requires empty status.
  std::vector<FieldObjectId> status;
  FieldCharacterOwnedReference inventory;
  std::vector<FieldGlobalDataItemReference> items;
  std::vector<FieldCharacterOwnedReference> enemy_skills;
  bool untargetable = false;
  // Exactly one source assignment per publication. Collection assignment
  // replaces a source root; sorting mutates the existing Array root.
  FieldCharacterLoadWrite write = FieldCharacterLoadWrite::None;
  size_t stat_index = 0;
};
struct FieldCharacterLoadHost {
  // Lookup the existing globaldata dictionary member in original source order.
  std::function<bool(uint32_t, FieldCharacterLoadState &, std::string &)> read;
  // Publish fields on this same actual Object. Called before synchronous
  // signals; must not run RNG, constructors, setters or a second stat
  // implementation.
  std::function<bool(const FieldCharacterLoadState &, std::string &)> publish;
  std::function<bool(uint32_t, FieldCharacterOwnedReference &, std::string &)>
      new_inventory;
  // Caller already evaluated UID fallback; all four source Item arguments
  // exist.
  std::function<bool(FieldObjectId, const FieldOwnedItem &,
                     FieldGlobalDataItemReference &, std::string &)>
      new_item;
  std::function<bool(const FieldCharacterEnemySkill &,
                     FieldCharacterOwnedReference &, std::string &)>
      new_enemy_skill;
  FieldGlobalDataStatSignal stat_changed;
};
class FieldCharacterLoadRuntime {
public:
  bool initialize(const FieldCharacterLoadData &, const FieldGlobalDataData &,
                  const FieldGlobalDataRuntime &, FieldGlobalRegistry &,
                  GlobalYamlCachesRuntime &, GlobalItemCache &, SourceRandom &,
                  std::vector<uint32_t> &, LoadRngClockProvider,
                  FieldCharacterLoadHost, std::string &);
  // Invoke only at global._load_dict_to_game's actual characters cursor, after
  // actual global scalar assignments and KEY/STORAGE reconstruction complete.
  // Not a replacement for global Ready, party/UI/flags suffix or a general
  // LOAD.
  bool load_cold_default(std::string &);
  bool characters_complete() const {
    return complete_ && !poisoned_ && data_ && owner_ &&
           owner_->character_load_bound_to(*data_);
  }
  bool poisoned() const { return poisoned_; }

private:
  const FieldCharacterLoadData *data_ = nullptr;
  const FieldGlobalDataRuntime *owner_ = nullptr;
  FieldGlobalRegistry *registry_ = nullptr;
  GlobalYamlCachesRuntime *caches_ = nullptr;
  GlobalItemCache *items_ = nullptr;
  SourceRandom *random_ = nullptr;
  std::vector<uint32_t> *ledger_ = nullptr;
  LoadRngClockProvider clock_;
  FieldCharacterLoadHost host_;
  std::vector<FieldCharacterLoadState> cold_;
  bool complete_ = false, poisoned_ = false;
  bool publish(FieldCharacterLoadState &, std::string &);
  bool refresh(FieldCharacterLoadState &, std::string &);
  bool set_stat(FieldCharacterLoadState &, size_t, int64_t, std::string &);
  bool set_current(FieldCharacterLoadState &, bool, int64_t, std::string &);
  bool maximum(FieldCharacterLoadState &, bool, int64_t &, std::string &);
};
} // namespace encore::upstream
