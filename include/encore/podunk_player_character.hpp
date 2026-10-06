#pragma once
#include "encore/field_character_load.hpp"
#include "encore/player_ready.hpp"
#include "encore/house_status_effects.hpp"
namespace encore::upstream {
// Same live Character body and Registry. Populated status requires the actual
// source Node/cache consumer; unknown states and effect queries reject.
class PodunkPlayerCharacter {
public:
  bool initialize(const FieldGlobalDataRuntime &,
                  const FieldCharacterLoadData &, const PlayerReadyData &,
                  const FieldGlobalRegistry &, std::string &);
  bool bind_status_effects(const HouseStatusEffectsRuntime &, std::string &);
  bool character_effect(FieldObjectId, std::string_view, bool &,
                        std::string &) const;
  bool get_sprite(FieldObjectId, std::string &, std::string &) const;
  bool is_incapacitated(FieldObjectId, bool &, std::string &) const;
  void bind(PlayerReadyHost &) const;

private:
  bool live(FieldObjectId, FieldGlobalDataObject &, std::string &) const;
  bool empty_status(FieldObjectId, std::string &) const;
  const HouseStatusEffectsRuntime *statuses_ = nullptr;
  const FieldGlobalDataRuntime *core_ = nullptr;
  const FieldCharacterLoadData *characters_ = nullptr;
  const PlayerReadyData *ready_ = nullptr;
  const FieldGlobalRegistry *registry_ = nullptr;
  std::array<uint8_t, 32> characters_ir_{}, ready_ir_{};
};
} // namespace encore::upstream
