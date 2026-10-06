#pragma once
#include "encore/field_global_data.hpp"
namespace encore::upstream {
// Execution roles only. Actual member/script names and constructor literals
// are loaded from the independently reviewed character resource.
enum class FieldCharacterLoadStep : uint32_t {
  NormalInventory = 1,
  Name = 2,
  ExpFromLevel = 3,
  Status = 4,
  Nickname = 5,
  LearnedSkills = 6,
  PermanentBoosts = 7,
  Affinities = 8,
  SetHp = 9,
  SetPp = 10,
  SortSkills = 11,
  LevelRaw = 12,
  ExpRaw = 13,
  HpRaw = 14,
  MaxHpRaw = 15,
  PpRaw = 16,
  MaxPpRaw = 17,
  OffenseRaw = 18,
  DefenseRaw = 19,
  SpeedRaw = 20,
  IqRaw = 21,
  GutsRaw = 22,
  Untargetable = 23,
  ResetEnemySkills = 24,
  FilterEnemySkills = 25,
  NewEnemySkill = 26,
  AppendEnemySkill = 27
};
enum class FieldCharacterItemArgument : uint32_t {
  Name = 1,
  Equipped = 2,
  Doses = 3,
  UidFallback = 4,
  SavedUid = 5,
  CastUid = 6,
  AllocateItem = 7,
  AssignName = 8,
  AssignEquipped = 9,
  AssignUid = 10,
  AssignDoses = 11,
  DefaultDoses = 12,
  Append = 13,
  PublishItems = 14
};
struct FieldCharacterLoadBindings {
  std::string name, level, exp, status, hp, pp, nickname, inventory,
      inventory_getter, learned_skills, permanent_boosts, affinities,
      untargetable, npc_skills;
  std::vector<std::string> stat_fields;
  std::string character_script, member_script, npc_script, inventory_script,
      inventory_type_field, inventory_items_field, inventory_getter_method;
  uint32_t normal_inventory_type = 0;
  uint32_t enemy_skill_id = 0, item_constructor_id = 0;
  std::string item_script, item_native, enemy_skill_native, item_name_field,
      item_uid_field, item_equipped_field, item_doses_field;
  std::string enemy_skill_script, enemy_id_field, enemy_weight_field,
      enemy_cooldown_field, enemy_remaining_field;
  std::vector<FieldGlobalDataDefault> character_defaults, member_defaults,
      npc_defaults, inventory_defaults, enemy_skill_defaults, item_defaults;
  std::string npc_nickname_prefix, npc_nickname_suffix;
  std::vector<FieldCharacterLoadStep> member_load_order, character_load_order,
      npc_load_order;
  std::vector<FieldCharacterItemArgument> item_argument_order;
  std::vector<FieldGlobalDataDefault> enemy_constructor_defaults;
};
} // namespace encore::upstream
