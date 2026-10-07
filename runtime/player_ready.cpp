#include "encore/player_tree_rebind.hpp"
#include "encore/player_ready.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool substitute(const std::string &pattern, const std::string &value,
                std::string &out, std::string &e) {
  auto pos = pattern.find("%s");
  if (pos == pattern.npos || pattern.find("%s", pos + 2) != pattern.npos)
    return fail(e, "Player source format capability rejected");
  out = pattern.substr(0, pos) + value + pattern.substr(pos + 2);
  return true;
}
} // namespace
bool PlayerReadyRuntime::live(std::string &e) const {
  auto s = body_ && tree_ ? tree_->state(body_->object()) : nullptr;
  return data_ && !poisoned_ && body_->constructed() && s && s->alive &&
                 s->inside && s->bound
             ? true
             : fail(e, "Player Ready actual body/tree not live");
}
bool PlayerReadyRuntime::initialize(const PlayerReadyData &d,
                                    PlayerInitializationBody &b,
                                    FieldNodeTreeRuntime &t,
                                    FieldGlobalConstructorRuntime &g,
                                    const FieldGlobalDataRuntime &c,
                                    PlayerAnimationGraph &a, PlayerReadyHost h,
                                    std::string &e) {
  if (data_ || !d.valid() || !b.constructed() || !b.data() || b.tree() != &t ||
      !g.data() || !c.constructor_complete() || a.data() != &d ||
      d.identity().upstream_commit != b.data()->identity().upstream_commit ||
      d.identity().source_sha256 != b.data()->identity().source_sha256 ||
      !h.resolve_resource || !h.connected || !h.connect ||
      !h.character_effect || !h.character_sprite ||
      !h.character_incapacitated || !h.animation_active || !h.sprite_playing ||
      !h.sprite_frame || !h.resource_exists || !h.texture_path ||
      !h.load_texture || !h.texture_height || !h.sprite_offset_y ||
      !h.shadow_animation || !h.add_sfx)
    return fail(e, "Player Ready source/actual native services pending");
  data_ = &d;
  body_ = &b;
  tree_ = &t;
  global_ = &g;
  characters_ = &c;
  animation_ = &a;
  host_ = std::move(h);
  return true;
}
bool PlayerReadyRuntime::node(PlayerReadyBinding role, FieldObjectId &out,
                              std::string &e) const {
  if (!live(e) ||
      !tree_->get_node(body_->object(), data_->binding(role), out, e))
    return false;
  if (role == PlayerReadyBinding::SpritePath ||
      role == PlayerReadyBinding::SpecialPath ||
      role == PlayerReadyBinding::CameraPath ||
      role == PlayerReadyBinding::TreePath ||
      role == PlayerReadyBinding::AnimationPath) {
    bool found = false;
    for (const auto &row : body_->data()->onready_source()->array) {
      auto path = row->get("path"), kind = row->get("kind"),
           name = row->get("name");
      if (!path || !kind || !name || kind->kind != 2 || kind->integer != 1 ||
          path->kind != 4 || name->kind != 4 ||
          path->string != data_->binding(role))
        continue;
      PlayerInitializationMember member;
      if (!body_->member(name->string, member, e) || member.object != out)
        return fail(e,
                    "Player source onready member not assigned at this cursor");
      found = true;
      break;
    }
    if (!found)
      return fail(e, "Player source onready declaration not found");
  }
  auto s = tree_->state(out);
  return s && s->alive && s->inside && s->bound
             ? true
             : fail(e, "Player Ready actual native child owner pending");
}
bool PlayerReadyRuntime::assign(PlayerReadyBinding role, uint32_t kind,
                                bool boolean, Vec2 vector, FieldObjectId object,
                                std::string &e) {
  PlayerInitializationMember v;
  v.kind = kind;
  v.object = object;
  if (kind == 7)
    v.vector = {vector.x, vector.y};
  else if (kind != 8) {
    auto scalar = std::make_shared<GlobalYamlValue>();
    scalar->kind = kind;
    scalar->boolean = boolean;
    v.value = std::move(scalar);
  }
  return body_->assign_member(data_->binding(role), v, e);
}
bool PlayerReadyRuntime::onready(std::string_view source, std::string &e) {
  auto rows = body_->data()->onready_source();
  if (!rows || rows->kind != 5)
    return fail(e, "Player onready schema unavailable");
  for (const auto &row : rows->array) {
    auto script = row->get("source"), name = row->get("name"),
         kind = row->get("kind");
    if (!script || script->kind != 4 || !name || name->kind != 4 || !kind ||
        kind->kind != 2)
      return fail(e, "Player onready source schema rejected");
    if (script->string != source)
      continue;
    FieldObjectId object = 0;
    if (kind->integer == 1) {
      auto path = row->get("path");
      if (!path || path->kind != 4 ||
          !tree_->get_node(body_->object(), path->string, object, e))
        return false;
    } else if (kind->integer == 2 || kind->integer == 3) {
      if (!host_.resolve_resource(*row, object, e))
        return false;
    } else
      return fail(e, "Player unknown onready expression");
    if (!body_->bind_onready(name->string, object, e))
      return false;
  }
  return true;
}
bool PlayerReadyRuntime::refresh_status(std::string &e) {
  if (!live(e))
    return false;
  PlayerInitializationMember member;
  if (!body_->member(data_->binding(PlayerReadyBinding::PartyMember), member,
                     e) ||
      !characters_->constructed_body_alive(member.object) ||
      !assign(PlayerReadyBinding::Steps, 2, false, {}, 0, e))
    return false;
  bool sweat = false;
  FieldObjectId sprite = 0;
  if (!host_.character_effect(member.object,
                              data_->binding(PlayerReadyBinding::SweatEffect),
                              sweat, e) ||
      !node(PlayerReadyBinding::SweatPath, sprite, e) ||
      !host_.sprite_playing(sprite, sweat, e))
    return false;
  return sweat || host_.sprite_frame(sprite, 0, e);
}
bool PlayerReadyRuntime::spritesheet(std::string &e) {
  PlayerInitializationMember member, costume;
  if (!body_->member(data_->binding(PlayerReadyBinding::PartyMember), member,
                     e) ||
      !body_->member(data_->binding(PlayerReadyBinding::Costume), costume, e) ||
      !costume.value || costume.value->kind != 4)
    return false;
  std::string sprite_name, normal, special, snow;
  if (!host_.character_sprite(member.object, sprite_name, e) ||
      !substitute(data_->binding(PlayerReadyBinding::NormalFormat), sprite_name,
                  normal, e) ||
      !substitute(data_->binding(PlayerReadyBinding::SpecialFormat),
                  sprite_name, special, e) ||
      !substitute(data_->binding(PlayerReadyBinding::SnowFormat), sprite_name,
                  snow, e))
    return false;
  FieldObjectId sprite = 0;
  if (!node(PlayerReadyBinding::SpritePath, sprite, e))
    return false;
  bool normal_exists = false;
  if (!host_.resource_exists(normal, normal_exists, e))
    return false;
  if (normal_exists) {
    std::string current;
    if (!host_.texture_path(sprite, current, e))
      return false;
    if (current != normal) {
      FieldObjectId special_sprite = 0;
      bool special_exists = false;
      if (!host_.resource_exists(special, special_exists, e))
        return false;
      if (special_exists &&
          (!node(PlayerReadyBinding::SpecialPath, special_sprite, e) ||
           !host_.load_texture(special_sprite, special, e)))
        return false;
      bool snow_exists = false;
      if (costume.value->string ==
              data_->binding(PlayerReadyBinding::SnowCostume) &&
          !host_.resource_exists(snow, snow_exists, e))
        return false;
      if (!host_.load_texture(sprite, snow_exists ? snow : normal, e))
        return false;
      uint32_t height = 0;
      if (!host_.texture_height(sprite, height, e))
        return false;
      // Source texture.get_height() is int, hence integer division by the
      // source literal, before unary minus and offset addition.
      float offset = -float(height / data_->height_divisor()) +
                     float(data_->height_offset());
      if (!host_.sprite_offset_y(sprite, offset, e))
        return false;
      return true;
    }
  }
  {
    // The source executes ResourceLoader.exists a second time in elif.
    if (!host_.resource_exists(normal, normal_exists, e))
      return false;
    if (!normal_exists &&
        !host_.load_texture(
            sprite, data_->binding(PlayerReadyBinding::FallbackTexture), e))
      return false;
  }
  return true;
}
bool PlayerReadyRuntime::update_party_member(std::string &e) {
  if (!live(e))
    return false;
  std::shared_ptr<const GlobalLoadObjectArray> party;
  if (!global_->array(FieldGlobalMemberRole::Party, party, e) || !party ||
      party->values.empty())
    return fail(e, "Player source party[0] unavailable");
  if (!assign(PlayerReadyBinding::PartyMember, 8, false, {},
              party->values.front(), e))
    return false;
  bool connected = false;
  if (!host_.connected(
          party->values.front(),
          data_->binding(PlayerReadyBinding::StatusSignal), body_->object(),
          data_->binding(PlayerReadyBinding::RefreshMethod), connected, e))
    return false;
  if (!connected &&
      !host_.connect(party->values.front(),
                     data_->binding(PlayerReadyBinding::StatusSignal),
                     body_->object(),
                     data_->binding(PlayerReadyBinding::RefreshMethod), e))
    return false;
  return refresh_status(e) && spritesheet(e);
}
bool PlayerReadyRuntime::blend_position(Vec2 value, std::string &e) {
  if (!live(e) || !std::isfinite(value.x) || !std::isfinite(value.y))
    return false;
  if (value.x == 0 && value.y == 0)
    return true;
  for (const auto &p : data_->blend_parameters())
    if (!animation_->set_blend(p, value, e))
      return false;
  return true;
}
bool PlayerReadyRuntime::set_anim_state(std::string_view request,
                                        std::string &e) {
  if (!live(e))
    return false;
  PlayerInitializationMember climbing, member;
  if (!body_->member(data_->binding(PlayerReadyBinding::Climbing), climbing,
                     e) ||
      !climbing.value || climbing.value->kind != 1 ||
      !body_->member(data_->binding(PlayerReadyBinding::PartyMember), member,
                     e))
    return false;
  if (climbing.value->boolean)
    return true;
  bool incapacitated = false;
  if (!host_.character_incapacitated(member.object, incapacitated, e))
    return false;
  std::string state(request);
  if (incapacitated) {
    for (const auto &rule : data_->incapacitated_rules())
      if (rule.first == state) {
        state = rule.second;
        break;
      }
    state = data_->fainted_prefix() + state;
  }
  return animation_->travel(state, e);
}
bool PlayerReadyRuntime::ready(FieldTreePhase phase,
                               const FieldNodeBinding &binding,
                               std::string &e) {
  if (!live(e) || started_ || phase != FieldTreePhase::ReadyScript)
    return fail(e, "Player actual first ReadyScript cursor rejected");
  auto s = tree_->state(body_->object());
  auto desc = tree_->descriptor(body_->object());
  if (!s || !desc || !s->ready_notified || s->ready_first ||
      binding.family != 0x454e005a || binding.capability != 1 ||
      binding.family != s->binding.family ||
      binding.capability != s->binding.capability ||
      binding.native_class != desc->native_class ||
      binding.class_index != desc->class_index ||
      binding.stable_id != desc->id || binding.script_sha != desc->script_sha ||
      binding.identity.scene_id != data_->identity().scene_id ||
      binding.identity.source_sha256 != data_->identity().source_sha256 ||
      binding.identity.upstream_commit != data_->identity().upstream_commit)
    return fail(e, "Player actual ReadyScript owner/source rejected");
  started_ = true;
  auto abort = [&]() {
    poisoned_ = true;
    return false;
  };
  // call_multilevel_reversed: base onready declarations and base _ready first,
  // then derived onready declarations and derived _ready. Virtual dispatch of
  // update_party_member already uses the derived method during base Ready.
  auto base = body_->data()->onready_source()->array.front()->get("source");
  if (!base || base->kind != 4 || !onready(base->string, e) ||
      !host_.connect(global_->owner(),
                     data_->binding(PlayerReadyBinding::PartySignal),
                     body_->object(),
                     data_->binding(PlayerReadyBinding::UpdateMethod), e) ||
      !update_party_member(e) || !onready(body_->data()->player_source(), e))
    return abort();
  FieldObjectId camera = 0;
  if (!node(PlayerReadyBinding::CameraPath, camera, e) ||
      !global_->set_object(FieldGlobalMemberRole::CurrentCamera, camera, e))
    return abort();
  std::shared_ptr<const FieldGlobalPartySpaceArray> space;
  if (!global_->party_space(space, e) || !space)
    return abort();
  const auto count = space->values.size();
  const Vec2 position = s->local[2];
  for (size_t i = 0; i < count; ++i) {
    FieldGlobalPartySpaceValue previous;
    if (!global_->push_front_party_space(position, e) ||
        !global_->pop_back_party_space(previous, e))
      return abort();
  }
  FieldObjectId animation_node = 0;
  if (!node(PlayerReadyBinding::TreePath, animation_node, e) ||
      !host_.animation_active(animation_node, true, e) ||
      !animation_->active() ||
      !assign(PlayerReadyBinding::TapRun, 1, false, {}, 0, e) ||
      !assign(PlayerReadyBinding::Crouch, 1, false, {}, 0, e) ||
      !assign(PlayerReadyBinding::Direction, 7, false, data_->direction(), 0,
              e) ||
      !blend_position(data_->direction(), e) ||
      !host_.shadow_animation(
          body_->object(), data_->binding(PlayerReadyBinding::ShadowAnimation),
          e) ||
      !update_party_member(e))
    return abort();
  PlayerInitializationMember run;
  if (!body_->member(data_->binding(PlayerReadyBinding::RunSound), run, e) ||
      !run.value || run.value->kind != 4)
    return abort();
  std::string audio;
  if (!substitute(data_->binding(PlayerReadyBinding::FootstepsFormat),
                  run.value->string, audio, e) ||
      !host_.add_sfx(audio, data_->binding(PlayerReadyBinding::FootstepsVoice),
                     e) ||
      !assign(PlayerReadyBinding::LastStep, 7, false, position, 0, e))
    return abort();
  complete_ = true;
  return true;
}
bool PlayerReadyRuntime::rebind_tree(FieldNodeTreeRuntime &next, std::string &e) {
  if (!body_complete() || !body_ || !body_->constructed() ||
      body_->tree() != &next || !body_->registry() || !body_->data() ||
      !player_rebind_node(*body_->data(), *body_->registry(), next,
                          body_->object(), body_->data()->recipe().identity().scene_id, e))
    return fail(e, "Player Ready rebind requires preserved completed body");
  tree_ = &next;
  e.clear();
  return true;
}
} // namespace encore::upstream
