#include "encore/podunk_player_character.hpp"
#include <algorithm>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const std::string &reason) {
  e = reason;
  return false;
}
} // namespace
bool PodunkPlayerCharacter::initialize(const FieldGlobalDataRuntime &core,
                                       const FieldCharacterLoadData &characters,
                                       const PlayerReadyData &ready,
                                       const FieldGlobalRegistry &registry,
                                       std::string &e) {
  if (core_ || !characters.valid() || !ready.valid() || registry.poisoned() ||
      core.registry() != &registry || !core.constructor_complete() ||
      !core.character_load_bound_to(characters) || !core.data() ||
      characters.identity().upstream_commit !=
          ready.identity().upstream_commit ||
      characters.identity().upstream_commit !=
          core.data()->identity().upstream_commit)
    return fail(e,
                "Player Character actual owner/resource binding unavailable");
  const auto &b = characters.source_bindings();
  if (b.name.empty() || b.status.empty() || b.character_script.empty() ||
      b.member_script.empty() || b.npc_script.empty() ||
      ready.binding(PlayerReadyBinding::SweatEffect).empty())
    return fail(e, "Player Character checked source fields unavailable");
  // 005a covers the PartyObject/Player callers and animation mapping. Invoked
  // Character fields/methods are covered by 0050, cross-bound to actual 0053.
  for (const auto *source :
       {&b.character_script, &b.member_script, &b.npc_script}) {
    std::array<uint8_t, 32> expected{}, actual{};
    if (!characters.source_hash(*source, expected) ||
        !core.constructor_source_hash(*source, actual) || actual != expected)
      return fail(e,
                  "Player Character actual source hash mismatch: " + *source);
  }
  core_ = &core;
  characters_ = &characters;
  ready_ = &ready;
  registry_ = &registry;
  characters_ir_ = characters.ir_sha256();
  ready_ir_ = ready.ir_sha256();
  e.clear();
  return true;
}
bool PodunkPlayerCharacter::live(FieldObjectId id, FieldGlobalDataObject &out,
                                 std::string &e) const {
  if (!core_ || !characters_ || !ready_ || !registry_ ||
      registry_->poisoned() || core_->registry() != registry_ ||
      !characters_->valid() || !ready_->valid() ||
      characters_->ir_sha256() != characters_ir_ ||
      ready_->ir_sha256() != ready_ir_ ||
      !core_->character_load_bound_to(*characters_) ||
      !registry_->object_exists(id) || !core_->constructed_body_alive(id) ||
      !core_->read_constructed_object(id, out, e))
    return fail(e, "Player Character actual ObjectDB body unavailable: " +
                       std::to_string(id));
  auto declared =
      std::find_if(core_->data()->declarations().begin(),
                   core_->data()->declarations().end(),
                   [&](const auto &d) { return d.id == out.declaration; });
  if (declared == core_->data()->declarations().end() || out.kind != 1 ||
      declared->kind != out.kind || declared->role != out.role ||
      (out.role != 0 && out.role != 1) ||
      declared->script != (out.role == 0
                               ? characters_->source_bindings().member_script
                               : characters_->source_bindings().npc_script))
    return fail(e,
                "Player Character actual source class/declaration rejected: " +
                    std::to_string(id));
  e.clear();
  return true;
}
bool PodunkPlayerCharacter::empty_status(FieldObjectId id,
                                         std::string &e) const {
  FieldGlobalDataObject actual;
  if (!live(id, actual, e))
    return false;
  FieldGlobalDataMemberState status;
  const auto &field = characters_->source_bindings().status;
  if (!core_->read_constructed_member(id, field, status, e))
    return false;
  if (status.kind != 5 || !status.value || status.value->kind != 5)
    return fail(e,
                "Player Character source Status Array unavailable: Character=" +
                    std::to_string(id) + " field=" + field);
  // Check all actual representations. A saved projection cannot substitute for
  // the owned source Array, nor may any populated Node handles be discarded.
  if (!status.value->array.empty() || !status.references.empty() ||
      (status.reference_array && !status.reference_array->values.empty()) ||
      (status.node_array && !status.node_array->values.empty()))
    return fail(e, "Player Character nonempty Status Array requires actual "
                   "Status Node owner: Character=" +
                       std::to_string(id) + " field=" + field);
  e.clear();
  return true;
}
bool PodunkPlayerCharacter::character_effect(FieldObjectId id,
                                             std::string_view effect, bool &out,
                                             std::string &e) const {
  if (!ready_ || effect != ready_->binding(PlayerReadyBinding::SweatEffect))
    return fail(e,
                "Player Character effect query outside checked Ready caller");
  if (!empty_status(id, e))
    return false;
  // The source boolean OR starts false. Empty actual _status means no get_data
  // call, cache mutation, signal or random draw; this is its real identity
  // value.
  out = false;
  e.clear();
  return true;
}
bool PodunkPlayerCharacter::get_sprite(FieldObjectId id, std::string &out,
                                       std::string &e) const {
  FieldGlobalDataObject actual;
  if (!live(id, actual, e))
    return false;
  FieldGlobalDataMemberState name;
  if (!core_->read_constructed_member(id, characters_->source_bindings().name,
                                      name, e))
    return false;
  if (name.kind != 4 || !name.value || name.value->kind != 4)
    return fail(e, "Player Character source sprite name is not String: " +
                       std::to_string(id));
  // get_sprite returns _name in original case, unlike get_name(). Neither
  // PartyMember nor PartyNPC overrides it; no status suffix or fallback here.
  out = name.value->string;
  e.clear();
  return true;
}
bool PodunkPlayerCharacter::is_incapacitated(FieldObjectId id, bool &out,
                                             std::string &e) const {
  if (!empty_status(id, e))
    return false;
  out = false;
  e.clear();
  return true;
}
void PodunkPlayerCharacter::bind(PlayerReadyHost &host) const {
  host.character_effect = [this](FieldObjectId id, std::string_view effect,
                                 bool &out, std::string &e) {
    return character_effect(id, effect, out, e);
  };
  host.character_sprite = [this](FieldObjectId id, std::string &out,
                                 std::string &e) {
    return get_sprite(id, out, e);
  };
  host.character_incapacitated = [this](FieldObjectId id, bool &out,
                                        std::string &e) {
    return is_incapacitated(id, out, e);
  };
}
} // namespace encore::upstream
