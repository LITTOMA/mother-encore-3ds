#include "podunk_player_scene_services.hpp"
#include "house_return_player_scene_owner.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const std::string &message) {
  e = message;
  return false;
}
bool result(bool success, const std::string &original, std::string &e) {
  if (!success)
    e = original;
  return success;
}
} // namespace
bool PodunkPlayerSceneServices::live(std::string &e) const {
  if (!input_.continuation || !input_.continuation->initialized() ||
      !input_.sources || !input_.player || !input_.tree ||
      (!input_.house&&(!input_.consumers||!input_.scripts))||
      (input_.house&&(input_.consumers||input_.scripts||input_.scene_data))||
      !input_.continuation->registry() ||
      input_.continuation->registry()->poisoned())
    return fail(e,
                "Player scene services lost their actual continuation owner");
  if(input_.house&&(input_.house->tree()!=input_.tree||
      input_.house->registry()!=input_.continuation->registry()||
      input_.house->player()!=input_.player||!input_.house->borrowed(e)))
    return fail(e,"Player House route no longer borrows the same fixed Tree/Registry/Player owner");
  if ((input_.preloads &&
       input_.preloads->registry() != input_.continuation->registry()) ||
      (input_.named_sfx &&
       input_.named_sfx->registry() != input_.continuation->registry()) ||
      (input_.player->body().data() &&
       input_.player->body().data() != input_.sources->initialization.get()))
    return fail(e, "Player service source Resource/body owner changed");
  e.clear();
  return true;
}
bool PodunkPlayerSceneServices::prepare(PodunkPlayerSceneInput in,
                                        std::string &e) {
  if (input_.continuation || !in.continuation ||
      !in.continuation->initialized() || !in.sources ||
      !in.sources->initialization || !in.sources->ready ||
      !in.sources->motion || !in.sources->effects || !in.player || !in.tree ||
      (!in.house&&(!in.consumers||!in.scripts))||
      (in.house&&(in.consumers||in.scripts||in.scene_data))||!in.controls||!in.input||
      !in.continuation->global() || !in.continuation->characters() ||
      !in.continuation->character_data() || !in.continuation->signals() ||
      in.continuation->signals()->registry() != in.continuation->registry())
    return fail(e, "Player scene service source/native composition incomplete");
  if(in.house&&(in.house->tree()!=in.tree||in.house->registry()!=in.continuation->registry()||
      in.house->player()!=in.player||!in.house->borrowed(e)))
    return fail(e,"Player House route is not its actual fixed source composition");
  if ((in.preloads && in.preloads->registry() != in.continuation->registry()) ||
      (in.named_sfx && in.named_sfx->registry() != in.continuation->registry()))
    return fail(
        e, "Player preload/named SFX Resource belongs to another Registry");
  auto &native = in.native;
  if ((native.registry && native.registry != in.continuation->registry()) ||
      (native.global && native.global != &in.continuation->global()->core()) ||
      (native.characters &&
       native.characters != &in.continuation->characters()->runtime()) ||
      (native.random && native.random != in.continuation->random()) ||
      (native.audio && native.audio != in.continuation->audio()))
    return fail(
        e,
        "Player service cannot mix another Registry/Character/RNG/audio owner");
  input_ = std::move(in);
  services_ = input_.native;
  services_.registry = input_.continuation->registry();
  services_.global = &input_.continuation->global()->core();
  services_.characters = &input_.continuation->characters()->runtime();
  services_.character_data = input_.continuation->character_data();
  services_.statuses = input_.continuation->status_runtime();
  services_.random = input_.continuation->random();
  services_.audio = input_.continuation->audio();
  if (input_.preloads) {
    services_.ready.resolve_resource = [this](const auto &row, auto &id,
                                              auto &error) {
      return live(error) && input_.preloads->resolve(row, id, error);
    };
  }
  if (input_.named_sfx) {
    services_.ready.add_sfx = [this](auto source, auto name, auto &error) {
      FieldObjectId id;
      return live(error) && input_.named_sfx->add_sfx(source, name, id, error);
    };
    services_.motion.audio_resource = [this](auto source, auto, auto &exists,
                                             auto &error) {
      FieldObjectId stream;
      if (!live(error) || !input_.named_sfx->resource(source, stream, error))
        return false;
      exists = stream != 0;
      return true;
    };
    services_.motion.audio_voice = [this](auto source, auto name, auto &out,
                                          auto &error) {
      FieldObjectId id;
      if (!live(error) || !input_.named_sfx->get_sfx(name, id, error))
        return false;
      PlayerAudioVoice next;
      if (!id) {
        out = next;
        return true;
      }
      PodunkSceneAudioState state;
      FieldObjectId stream, expected = 0;
      if (!input_.named_sfx->state(id, state, stream, error) ||
          (!source.empty() &&
           !input_.named_sfx->resource(source, expected, error)))
        return false;
      next.exists = true;
      next.playing = state.playing;
      next.same_stream = expected && expected == stream;
      out = next;
      return true;
    };
    services_.motion.audio_play = [this](auto source, auto name, auto &error) {
      FieldObjectId expected, id;
      if (!live(error) ||
          !input_.named_sfx->resource(source, expected, error) ||
          !input_.named_sfx->get_sfx(name, id, error))
        return false;
      if (id) {
        PodunkSceneAudioState state;
        FieldObjectId stream;
        if (!input_.named_sfx->state(id, state, stream, error))
          return false;
        if (stream == expected)
          return input_.named_sfx->play(id, error);
      }
      return input_.named_sfx->play_sfx(source, name, id, error);
    };
    services_.motion.audio_stop = [this](auto name, auto &error) {
      FieldObjectId id;
      if (!live(error) || !input_.named_sfx->get_sfx(name, id, error))
        return false;
      return !id || input_.named_sfx->stop(id, error);
    };
  }
  auto *bus = input_.continuation->signals();
  services_.ready.connected = [this, bus](auto a, auto s, auto b, auto m,
                                          auto &v, auto &err) {
    return live(err) && bus->connected(a, s, b, m, v, err);
  };
  services_.ready.connect = [this, bus](auto a, auto s, auto b, auto m,
                                        auto &err) {
    return live(err) && bus->connect(a, s, b, m, 0, {}, err);
  };
  auto &motion = services_.motion;
  motion.controls = [this](auto &v, auto &err) {
    if (!live(err) || !input_.controls(v, err))
      return false;
    return std::isfinite(v.x) && std::isfinite(v.y)
               ? true
               : fail(err, "Player actual input vector nonfinite");
  };
  motion.input = [this](auto a, auto q, auto &v, auto &err) {
    return live(err) && input_.input(a, q, v, err);
  };
  motion.emit = [this, bus](auto id, auto name, const auto &args, auto &err) {
    return live(err) && bus->emit(id, name, args, err);
  };
  motion.collider_connected = [this, bus](auto collider, auto player,
                                          auto method, auto &v, auto &err) {
    return live(err) && bus->connected(collider,
                                       input_.sources->motion->text(
                                           PlayerMotionText::TreeExitedSignal),
                                       player, method, v, err);
  };
  motion.collider_connection = [this, bus](auto collider, auto player,
                                           auto method, bool add, auto &err) {
    if (!live(err) || player != input_.player->body().object() ||
        method !=
            input_.sources->motion->text(PlayerMotionText::ColliderMethod))
      return fail(err, "Player cached collider source receiver mismatch");
    const auto &signal =
        input_.sources->motion->text(PlayerMotionText::TreeExitedSignal);
    return add ? bus->connect(collider, signal, player, method, 0,
                              {std::monostate{}}, err)
               : bus->disconnect(collider, signal, player, method, err);
  };
  motion.is_climbing = [this](auto id, auto &v, auto &err) {
    return climbing(id, v, err);
  };
  motion.character_name = [this](auto id, auto &v, auto &err) {
    return character_name(id, v, err);
  };
  motion.has_skill = [this](auto id, auto skill, auto &v, auto &err) {
    return has_field_skill(id, skill, v, err);
  };
  motion.button_skills = [this](auto id, auto &v, auto &err) {
    return button_skills(id, v, err);
  };
  motion.damage_effects = [this](auto id, auto name, auto &v, auto &err) {
    return damage_effects(id, name, v, err);
  };
  motion.collider_info = [this](auto id, auto &v, auto &err) {
    return collider_info(id, v, err);
  };
  motion.interact = [this](auto id, auto &err) {
    return interact(id, false, err);
  };
  motion.telepathy = [this](auto id, auto &err) {
    return interact(id, true, err);
  };
  motion.player_turn = [this](auto id, auto &err) {
    return turn_player(id, false, err);
  };
  motion.party_turn = [this](auto id, auto &err) {
    return turn_player(id, true, err);
  };
  motion.press_prompt = [this](auto id, auto &err) {
    return press_prompt(id, err);
  };
  motion.timer_paused = [this](auto id, bool paused, auto &e) {
    return live(e) && input_.player->timers().set_paused(id, paused, e);
  };
  motion.media_playing = [this](auto id, bool playing, auto &e) {
    return live(e) && input_.player->media().audio_playing(id, playing, e);
  };
  motion.media_paused = [this](auto id, bool paused, auto &e) {
    return live(e) && input_.player->media().audio_paused(id, paused, e);
  };
  motion.collision_mask = [this](auto bit, bool enabled, auto &e) {
    return live(e) &&
           input_.player->kinematic().set_collision_mask(bit, enabled, e);
  };
  motion.current_scene_area = [this](auto &area, auto &e) {
    return current_scene_area(area, e);
  };
  motion.party_call = [this](auto method, const auto &args, auto &e) {
    return party_call(method, args, e);
  };
  motion.dust = [this](auto &err) {
    if (!live(err))
      return false;
    for (const auto &c : input_.sources->effects->creators())
      if (c.kind == 2) {
        FieldObjectId actual = 0;
        if (!input_.tree->get_node(input_.player->body().object(), c.path,
                                   actual, err))
          return false;
        return input_.player->effects().core().create_dust(actual, err);
      }
    return fail(err,
                "Player source DustCreator not in checked source resource");
  };
  e.clear();
  return true;
}
bool PodunkPlayerSceneServices::rebind_house(HouseReturnPlayerSceneOwner&owner,
    FieldNodeTreeRuntime&old,FieldNodeTreeRuntime&next,
    const std::vector<FieldObjectId>&nodes,std::string&e){
  if(!live(e)||input_.house||input_.tree!=&old||&old==&next||
      !input_.consumers||!input_.scripts||owner.tree()!=&next||
      owner.player()!=input_.player||owner.registry()!=services_.registry||
      !owner.borrowed(e)||!input_.player->ready_complete()||
      input_.player->tree()!=&next||input_.player->body().tree()!=&next||
      old.object_domain()!=services_.registry->kernel()||
      next.object_domain()!=services_.registry->kernel()||nodes.empty()||
      nodes.front()!=input_.player->body().object())
    return fail(e,"Player service House rebind lacks the same actual transferred source owners");
  bool area=false;
  if(!owner.current_scene_area(area,e)||!area)return false;
  for(auto id:nodes){const auto*n=next.state(id);
    if(!input_.player->owns(id)||old.state(id)||!n||!n->alive||!n->bound||
      n->inside||n->ready_first||services_.registry->tree_owner(id).get()!=&next)
      return fail(e,"Player service rebind precedes the actual complete detached subtree transfer");
  }
  // Existing services_/motion/Ready closures capture this same address. No
  // callback, Ready resource, Random, voice or party owner is reconstructed.
  input_.tree=&next;input_.consumers=nullptr;input_.scripts=nullptr;
  input_.scene_data=nullptr;input_.house=&owner;e.clear();return true;
}
bool PodunkPlayerSceneServices::singleton_party(std::string &e) const {
  std::shared_ptr<const GlobalLoadObjectArray> objects;
  if (!live(e) ||
      !services_.global->array(FieldGlobalMemberRole::PartyObjects, objects,
                               e) ||
      !objects || objects->values.size() != 1 ||
      objects->values.front() != input_.player->body().object())
    return fail(e, "Player party_call requires actual source follower owners");
  return true;
}
bool PodunkPlayerSceneServices::current_scene_area(bool &area,
                                                   std::string &e) const {
  if(input_.house)return live(e)&&input_.house->current_scene_area(area,e);
  FieldObjectId scene;
  FieldIdentity actual;
  if (!live(e) || !input_.scene_data || !input_.scene_data->valid() ||
      !services_.global->object(FieldGlobalMemberRole::CurrentScene, scene, e))
    return fail(e, "Player currentScene actual source binding unavailable");
  auto tree = input_.continuation->registry()->tree_owner(scene);
  auto descriptor = tree ? tree->descriptor(scene) : nullptr;
  if (tree.get() != input_.tree || scene != tree->root() || !descriptor ||
      !tree->object_identity(scene, actual) ||
      actual.upstream_commit != input_.scene_data->identity().upstream_commit ||
      actual.source_sha256 != input_.scene_data->identity().source_sha256)
    return fail(e, "Player currentScene source owner differs");
  area = descriptor->id == input_.scene_data->area().id;
  if (!area)
    return fail(
        e,
        "Player currentScene script class is not in reviewed AreaRoom domain");
  return true;
}
bool PodunkPlayerSceneServices::party_call(
    std::string_view method, const std::vector<FieldDeferredValue> &args,
    std::string &e) {
  if (!singleton_party(e))
    return false;
  const auto &data = *input_.sources->motion;
  const auto &p = data.lifecycle();
  FieldObjectId player = input_.player->body().object(), flash, misc;
  auto child = [&](PlayerMotionNode role, FieldObjectId &out) {
    return input_.tree->get_node(player, data.node(role), out, e);
  };
  if (method == p.pause_flash || method == p.resume_flash) {
    if (!args.empty() || !child(PlayerMotionNode::FlashAnimation, flash))
      return fail(e, "Party Flash source arguments rejected");
    std::string assigned;
    bool playing;
    float position, length;
    if (!input_.player->animations().playback_snapshot(flash, assigned, playing,
                                                       position, length, e))
      return false;
    auto pausable = [&](std::string_view v) {
      return std::find(p.pausable_flash.begin(), p.pausable_flash.end(), v) !=
             p.pausable_flash.end();
    };
    if (method == p.pause_flash)
      return playing && pausable(assigned)
                 ? input_.player->animations().stop(flash, false, e)
                 : input_.player->animations().play(
                       flash, data.text(PlayerMotionText::ResetFlash), e);
    if (pausable(assigned) && position > 0 && position < length)
      return input_.player->animations().play(flash, assigned, e);
    return true;
  }
  if (method == p.pause_timers || method == p.resume_timers) {
    return args.empty() && child(PlayerMotionNode::MiscTimer, misc) &&
           input_.player->timers().set_paused(misc, method == p.pause_timers,
                                              e);
  }
  if (method == data.text(PlayerMotionText::AfterimageStopMethod)) {
    if (!args.empty())
      return fail(e, "Party AfterImage stop arguments rejected");
    for (const auto &c : input_.sources->effects->creators())
      if (c.kind == 1) {
        FieldObjectId actual;
        if (!input_.tree->get_node(player, c.path, actual, e))
          return false;
        return input_.player->effects().core().stop_creating(actual, e);
      }
    return fail(e, "Party AfterImage actual creator unavailable");
  }
  if (method == data.text(PlayerMotionText::CollisionsMethod)) {
    if (args.size() != 1 || !std::get_if<bool>(&args.front()))
      return fail(e, "Party collision source arguments rejected");
    return collisions(std::get<bool>(args.front()), e);
  }
  if (input_.native.motion.party_call)
    return input_.native.motion.party_call(method, args, e);
  return fail(e, "Party source method has no actual owner");
}
bool PodunkPlayerSceneServices::pause(bool running, bool idle, bool emit,
                                      std::string &e) {
  return live(e) && singleton_party(e) &&
         input_.player->motion().pause(running, idle, emit, e);
}
bool PodunkPlayerSceneServices::unpause(bool emit, std::string &e) {
  return live(e) && singleton_party(e) &&
         input_.player->motion().unpause(emit, e);
}
bool PodunkPlayerSceneServices::collisions(bool enabled, std::string &e) {
  return live(e) && input_.player->motion().collisions(enabled, e);
}
bool PodunkPlayerSceneServices::direction_and_input(Vec2 direction,
                                                    std::string &e) {
  return live(e) && input_.player->motion().direction_and_input(direction, e);
}
bool PodunkPlayerSceneServices::update_party_member(std::string &e) {
  return live(e) && input_.player->motion().update_party_member(e);
}
bool PodunkPlayerSceneServices::exit_camera(std::string &e) {
  return live(e) && input_.player->motion().exit_camera(e);
}
bool PodunkPlayerSceneServices::respawn(std::string &e) {
  bool area;
  if (!current_scene_area(area, e) || !area || !input_.player->ready_complete())
    return fail(e, "Respawn requires actual source currentScene/Player Ready");
  std::string source_path;
  if(input_.house){if(!input_.house->respawn_path(source_path,e))return false;}
  else {
    if(!input_.scene_data||!input_.scene_data->valid())return fail(e,"Respawn source scene resource is absent");
    source_path=std::string(input_.scene_data->source_scene());
    if(source_path.rfind("res://",0)!=0)source_path="res://"+source_path;
  }
  const auto &fields = input_.sources->initialization->policy().respawn_fields;
  if (fields.size() != 4)
    return fail(e, "Respawn source assignment schema rejected");
  auto state = input_.tree->state(input_.player->body().object());
  FieldObjectId shadow;
  if (!state || !(shadow = input_.tree->source_object(
                      input_.sources->visual->shadow().id)) != 0)
    return false;
  auto native = input_.player->visuals().native(shadow);
  PlayerVisualNativeState snapshot;
  if (!native || !native->state(snapshot, e) || !snapshot.constructed ||
      !snapshot.native_ready)
    return fail(e, "Respawn actual Shadow unavailable");
  PlayerInitializationMember run;
  if (!input_.player->body().member(
          input_.sources->motion->field(PlayerMotionField::RunSound), run, e) ||
      run.kind != 4 || !run.value)
    return fail(e, "Respawn actual run_sound unavailable");
  auto &owner = input_.continuation->characters()->runtime();
  if (!owner.write_source_vector(fields[0], state->local[2], e))
    return false;
  GlobalYamlValue value;
  value.kind = 4;
  value.string = source_path;
  if (!owner.write_global_scalar(fields[1], value, e))
    return false;
  value.string = run.value->string;
  if (!owner.write_global_scalar(fields[2], value, e))
    return false;
  value.string = snapshot.animation;
  return owner.write_global_scalar(fields[3], value, e);
}
PodunkPlayerServices PodunkPlayerSceneServices::services() const {
  return services_;
}
bool PodunkPlayerSceneServices::turn_player(FieldObjectId target, bool party,
                                            std::string &e) {
  if(input_.house)return live(e)&&input_.house->turn_player(target,party,e);
  FieldSceneScriptAdmission admission;
  if (!source(target, admission, e))
    return false;
  auto cores = input_.consumers->runtime_instances();
  bool x = false, y = false;
  const FieldNpcDescriptor *npc = nullptr;
  if (cores.npc_data)
    for (const auto &n : cores.npc_data->npcs())
      if (n.id == admission.id)
        npc = &n;
  if (npc) {
    x = npc->has(FieldNpcFlag::PlayerTurnX);
    y = npc->has(FieldNpcFlag::PlayerTurnY);
  } else if (auto interact = cores.interact_data
                                 ? cores.interact_data->record(admission.id)
                                 : nullptr) {
    // These are the existing checked InteractDialog schema axis bits.
    x = bool(interact->flags & 2u);
    y = bool(interact->flags & 4u);
  } else
    return fail(
        e, "Source target player_turn property has no typed field consumer");
  if (party) {
    std::shared_ptr<const GlobalLoadObjectArray> objects;
    if (!services_.global->array(FieldGlobalMemberRole::PartyObjects, objects,
                                 e) ||
        !objects || objects->values.size() != 1 ||
        objects->values.front() != input_.player->body().object())
      return fail(e, "Party turn requires actual follower source owners");
  }
  // Original target.get(player_turn) false skips every directional mutation.
  if (!x && !y)
    return true;
  FieldTransform from, to;
  auto owner = services_.registry->tree_owner(target);
  if (!owner || !owner->world_transform(target, to, e) ||
      !input_.tree->world_transform(input_.player->body().object(), from, e))
    return false;
  return input_.player->motion().turn_to(
      {to[2].x - from[2].x, to[2].y - from[2].y}, x, y, e);
}
bool PodunkPlayerSceneServices::climbing(FieldObjectId id, bool &out,
                                         std::string &e) const {
  if (!live(e) || id != input_.player->body().object() ||
      input_.continuation->registry()->tree_owner(id).get() != input_.tree)
    return fail(e, "PartyObject is_climbing actual follower consumer missing");
  PlayerInitializationMember value;
  if (!input_.player->body().member(
          input_.sources->motion->field(PlayerMotionField::Climbing), value, e))
    return false;
  if (value.kind != 1 || !value.value || value.value->kind != 1)
    return fail(e, "Player actual climbing field type differs");
  out = value.value->boolean;
  e.clear();
  return true;
}
bool PodunkPlayerSceneServices::character_name(FieldObjectId id,
                                               std::string &out,
                                               std::string &e) const {
  if (!live(e))
    return false;
  FieldGlobalDataMemberState value;
  if (!services_.characters->read_constructed_member(
          id, services_.character_data->source_bindings().name, value, e))
    return false;
  if (value.kind != 4 || !value.value || value.value->kind != 4)
    return fail(e, "Character get_name actual source String missing");
  out = value.value->string;
  // Original get_name uses to_lower, while Ready get_sprite preserves case.
  // The admitted character identifiers are ASCII; Unicode is not approximated.
  for (char &c : out) {
    if (static_cast<unsigned char>(c) >= 128)
      return fail(e, "Character non-ASCII lowercase source consumer pending");
    c = char(std::tolower(static_cast<unsigned char>(c)));
  }
  e.clear();
  return true;
}
bool PodunkPlayerSceneServices::source(FieldObjectId id,
                                       FieldSceneScriptAdmission &out,
                                       std::string &e) const {
  if (!live(e) || input_.house || !input_.scripts->admission(id, out))
    return fail(
        e, "Player interaction target has no actual source Ready consumer");
  return true;
}
bool PodunkPlayerSceneServices::collider_info(FieldObjectId id,
                                              PlayerColliderInfo &out,
                                              std::string &e) const {
  if (!live(e))
    return false;
  if(input_.house)return input_.house->collider_info(id,out,e);
  auto owner = services_.registry->tree_owner(id);
  const auto *s = owner ? owner->state(id) : nullptr;
  const auto *d = owner ? owner->descriptor(id) : nullptr;
  if (!s || !d || !s->alive || !s->inside)
    return fail(e, "Player interaction collider is not a live source Node");
  PlayerColliderInfo next;
  next.name = s->name;
  next.parent = s->parent;
  next.area = d->native_class == "Area2D";
  if (!d->script.empty()) {
    FieldSceneScriptAdmission a;
    if (!source(id, a, e))
      return false;
    const auto cores = input_.consumers->runtime_instances();
    if (cores.npc_data &&
        std::any_of(cores.npc_data->npcs().begin(),
                    cores.npc_data->npcs().end(),
                    [&](const auto &n) { return n.id == a.id; })) {
      next.interact = true;
      next.telepathy = true;
      next.has_dialog = true;
      next.has_thoughts = true;
      if (!cores.npc || !cores.npc->has_dialog(a.id, false, next.dialog) ||
          !cores.npc->has_dialog(a.id, true, next.has_thoughts))
        return fail(e, cores.npc ? cores.npc->error()
                                 : "Actual NPC consumer missing");
      const auto n = std::find_if(cores.npc_data->npcs().begin(),
                                  cores.npc_data->npcs().end(),
                                  [&](const auto &v) { return v.id == a.id; });
      next.no_problem_thoughts = n->has(FieldNpcFlag::NoProblemThoughts);
    } else if (cores.interact_data && cores.interact_data->record(a.id)) {
      next.interact = true;
      next.telepathy = true;
      next.has_thoughts = true;
      if (!cores.interact ||
          !cores.interact->has_thoughts(a.id, next.has_thoughts))
        return fail(e, cores.interact ? cores.interact->error()
                                      : "Actual Interact consumer missing");
    } else if ((cores.present_data && cores.present_data->binding(a.id)) ||
               (cores.dropped_data && cores.dropped_data->binding(a.id)) ||
               (cores.openable_data && cores.openable_data->record(a.id)) ||
               (cores.payphone_data && cores.payphone_data->record(a.id)) ||
               (cores.vending_data &&
                cores.vending_data->descriptor().id == a.id)) {
      // Only checked source rows whose existing typed cores expose interact.
      next.interact = true;
    } else
      return fail(
          e, "Player collider source interaction methods not implemented: " +
                 d->path);
  }
  out = std::move(next);
  e.clear();
  return true;
}
bool PodunkPlayerSceneServices::interact(FieldObjectId id, bool thoughts,
                                         std::string &e) {
  if(input_.house)return live(e)&&input_.house->interact(id,thoughts,e);
  FieldSceneScriptAdmission a;
  if (!source(id, a, e))
    return false;
  auto c = input_.consumers->runtime_instances();
  if (c.npc_data &&
      std::any_of(c.npc_data->npcs().begin(), c.npc_data->npcs().end(),
                  [&](const auto &n) { return n.id == a.id; }))
    return result(
        c.npc && (thoughts ? c.npc->telepathy(a.id) : c.npc->interact(a.id)),
        c.npc ? c.npc->error() : "NPC owner missing", e);
  if (c.interact_data && c.interact_data->record(a.id))
    return result(c.interact && (thoughts ? c.interact->telepathy(a.id)
                                          : c.interact->interact(a.id)),
                  c.interact ? c.interact->error() : "Interact owner missing",
                  e);
  if (thoughts)
    return fail(e, "Source target has no admitted telepathy method");
  if (c.present && c.present_data && c.present_data->binding(a.id))
    return c.present->interact(a.id, e);
  if (c.dropped && c.dropped_data && c.dropped_data->binding(a.id))
    return c.dropped->interact(a.id, e);
  if (c.openable && c.openable_data && c.openable_data->record(a.id))
    return result(c.openable->interact(a.id), c.openable->error(), e);
  if (c.payphone && c.payphone_data && c.payphone_data->record(a.id))
    return result(c.payphone->interact(a.id), c.payphone->error(), e);
  if (c.vending && c.vending_data && c.vending_data->descriptor().id == a.id) {
    const auto *p = input_.tree->descriptor(input_.player->body().object());
    return p ? c.vending->interact(p->id, e)
             : fail(e, "Vending actual Player source descriptor absent");
  }
  return fail(e, "Actual source target interact method consumer missing");
}
bool PodunkPlayerSceneServices::press_prompt(FieldObjectId id, std::string &e) {
  if(input_.house)return live(e)&&input_.house->press_prompt(id,e);
  FieldSceneScriptAdmission a;
  if (!source(id, a, e))
    return false;
  auto c = input_.consumers->runtime_instances();
  if (!c.prompt || !c.prompt->instance(a.id))
    return fail(e, "Actual source ButtonPrompt consumer missing");
  return result(c.prompt->press(a.id), c.prompt->error(), e);
}
bool PodunkPlayerSceneServices::damage_effects(
    FieldObjectId id, std::string_view name,
    std::vector<PlayerDamageEffect> &out, std::string &e) const {
  if (!live(e) ||
      name != input_.sources->motion->text(PlayerMotionText::DamageEffect))
    return fail(e, "Player damage effect query outside checked source caller");
  FieldGlobalDataMemberState status;
  if (!services_.characters->read_constructed_member(
          id, services_.character_data->source_bindings().status, status, e))
    return false;
  if (status.kind != 5 || !status.value || status.value->kind != 5 ||
      !status.value->array.empty() || !status.references.empty() ||
      (status.reference_array && !status.reference_array->values.empty()))
    return fail(e, "Player Status Array needs actual Node handles");
  std::vector<PlayerDamageEffect> next;
  if (status.node_array)
    for (auto actual : status.node_array->values) {
      HouseStatusActualData value;
      if (!input_.continuation->bridge()->status_data(id, actual, value, e) ||
          !value.data || value.data->kind != 6)
        return false;
      const auto *data = input_.continuation->status_effects();
      if (!data)
        return fail(e, "Player actual Status effects source missing");
      auto cases = value.data->get(data->effects_key());
      if (!cases)
        continue;
      if (cases->kind != 6)
        return fail(e, "Player Status effects source Dictionary rejected");
      for (const auto &v : cases->dictionary) {
        if (v.first != data->any_case())
          return fail(e, "Player damage effects case requires typed Character "
                         "type consumer");
        if (!v.second || v.second->kind != 6)
          return fail(e, "Player Status case is not Dictionary");
        if (auto effect = v.second->get(name)) {
          if (!input_.sources->motion->business_bindings() || effect->kind != 6)
            return fail(
                e, "Player overworld damage source Dictionary binding missing");
          PlayerDamageEffect row;
          auto integer = [&](PlayerMotionBusiness key,
                             PlayerMotionNumber fallback, int64_t &number) {
            auto value = effect->get(input_.sources->motion->business(key));
            if (value && value->kind != 2)
              return fail(e, "Player damage source parameter is not int");
            number = value ? value->integer
                           : int64_t(input_.sources->motion->number(fallback));
            return true;
          };
          if (!integer(PlayerMotionBusiness::DamageSteps,
                       PlayerMotionNumber::DamageStepsDefault, row.steps) ||
              !integer(PlayerMotionBusiness::DamageValue,
                       PlayerMotionNumber::DamageValueDefault, row.value) ||
              !integer(PlayerMotionBusiness::DamageVariation,
                       PlayerMotionNumber::DamageVarianceDefault,
                       row.variation))
            return false;
          next.push_back(row);
        }
      }
    }
  out = std::move(next);
  e.clear();
  return true;
}
bool PodunkPlayerSceneServices::battle_skill(FieldObjectId id,
                                             std::string_view wanted, bool &out,
                                             std::string &e) const {
  if (!live(e) || !input_.sources->motion->business_bindings())
    return fail(e, "Player field-skill source business bindings missing");
  const auto &b = input_.sources->motion;
  FieldGlobalDataObject character;
  if (!services_.characters->read_constructed_object(id, character, e) ||
      character.kind != 1 || character.role != 0)
    return fail(e, "Player has_skill requires actual PartyMember body");
  std::string name;
  if (!character_name(id, name, e))
    return false;
  std::vector<std::string> skills = character.learned_skills;
  PodunkInventorySnapshot snapshot;
  if (!input_.continuation->inventory_snapshot(snapshot, e))
    return false;
  std::vector<FieldObjectId> inventories{character.inventory};
  for (const auto &v : services_.characters->objects())
    if (v.kind == 2 && v.role == 1)
      inventories.push_back(v.object);
  if (inventories.size() != 2 || !inventories.front())
    return fail(e, "Source skill NORMAL/KEY inventory owners unavailable");
  for (auto actual : inventories) {
    FieldGlobalDataObject inv;
    if (!input_.continuation->characters()->read_object(actual, inv, e) ||
        inv.kind != 2)
      return false;
    for (auto item : inv.item_objects) {
      auto found =
          std::find_if(snapshot.items.begin(), snapshot.items.end(),
                       [&](const auto &v) { return v && v->object == item; });
      FieldOwnedItem owned;
      if (found == snapshot.items.end() ||
          !PodunkInventoryHost::check_source_item(*found,
                                                  *services_.registry) ||
          !(*found)->read_item(owned, e))
        return fail(e,
                    "PartyMember item skill lacks its source Item Reference");
      const auto *definition =
          input_.continuation->global_item_definitions()->definition(
              owned.definition);
      if (!definition)
        return fail(e, "PartyMember item skill definition not in source cache");
      bool equip = false;
      if (!owned.equipped) {
        // Item.is_equippable calls _has_function, which performs get_item_data
        // even when this item will not subsequently supply a skill.
        const auto *cached =
            input_.continuation->characters()->items_cache().get_item_data(
                owned.definition);
        if (!cached || cached != definition)
          return fail(e, "Item.is_equippable actual cache identity differs");
        for (const auto &action : cached->actions)
          if (action.function == FieldItemFunction::Equip)
            equip = true;
      }
      if (owned.equipped || !equip) {
        const auto *cached =
            input_.continuation->characters()->items_cache().get_item_data(
                owned.definition);
        if (!cached || cached != definition)
          return fail(e, "Item.get_data actual cache identity differs");
        const FieldItemPendingValue *enabled = nullptr;
        for (const auto &value : cached->pending_metadata)
          if (value.key == b->business(PlayerMotionBusiness::EnableSkill))
            enabled = &value;
        if (enabled && enabled->kind != 1)
          return fail(e, "Source item enable_skill is not String");
        if (!enabled || enabled->text.empty())
          continue;
        bool allowed = std::find(cached->can_use.begin(), cached->can_use.end(),
                                 name) != cached->can_use.end();
        if (allowed)
          skills.push_back(enabled->text);
      }
    }
  }
  out = std::find(skills.begin(), skills.end(), wanted) != skills.end();
  e.clear();
  return true;
}
bool PodunkPlayerSceneServices::has_field_skill(FieldObjectId id,
                                                std::string_view key, bool &out,
                                                std::string &e) const {
  if (!live(e) || !input_.sources->motion->business_bindings())
    return fail(e, "Player field-skill source bindings unavailable");
  auto d = input_.sources->motion;
  std::shared_ptr<GlobalYamlValue> skill;
  if (!input_.continuation->characters()->call_cache_getter(
          d->business(PlayerMotionBusiness::GetFieldSkill), {std::string(key)},
          skill, e) ||
      !skill || skill->kind != 6 || skill->dictionary.empty())
    return fail(e, "Player field skill outside actual source cache");
  bool result = true;
  if (auto usable = skill->get(d->business(PlayerMotionBusiness::Usable))) {
    if (usable->kind != 6)
      return fail(e, "Field skill usable source Dictionary expected");
    std::string name;
    if (!character_name(id, name, e))
      return false;
    auto allowed = usable->get(name);
    if (!allowed || !allowed->truthy())
      result = false;
  }
  if (auto flag = skill->get(d->business(PlayerMotionBusiness::Flag))) {
    if (flag->kind != 4)
      return fail(e, "Field skill source flag is not String");
    bool present = false, value = false;
    if (!input_.continuation->characters()->flags().read(false, flag->string,
                                                         present, value, e))
      return false;
    if (!present || !value)
      result = false;
  }
  if (auto battle =
          skill->get(d->business(PlayerMotionBusiness::BattleSkill))) {
    if (battle->kind != 4)
      return fail(e, "Field skill source battle_skill is not String");
    bool learned = false;
    if (!battle_skill(id, battle->string, learned, e))
      return false;
    if (!learned)
      result = false;
  }
  out = result;
  e.clear();
  return true;
}
bool PodunkPlayerSceneServices::button_skills(FieldObjectId id,
                                              std::vector<std::string> &out,
                                              std::string &e) const {
  if (!live(e) || !input_.sources->motion->business_bindings())
    return fail(e, "Player skill-button source bindings unavailable");
  auto d = input_.sources->motion;
  std::shared_ptr<GlobalYamlValue> keys;
  if (!input_.continuation->characters()->call_cache_getter(
          d->business(PlayerMotionBusiness::GetAllFieldSkills), {}, keys, e) ||
      !keys || keys->kind != 5)
    return false;
  std::vector<std::string> next;
  for (const auto &key : keys->array) {
    if (!key || key->kind != 4)
      return fail(e, "Source field skill cache key is not String");
    bool allowed = false;
    if (!has_field_skill(id, key->string, allowed, e))
      return false;
    if (!allowed)
      continue;
    std::shared_ptr<GlobalYamlValue> skill;
    if (!input_.continuation->characters()->call_cache_getter(
            d->business(PlayerMotionBusiness::GetFieldSkill), {key->string},
            skill, e) ||
        !skill || skill->kind != 6)
      return false;
    auto button = skill->get(d->business(PlayerMotionBusiness::SkillButton));
    if (button && button->truthy())
      next.push_back(key->string);
  }
  std::sort(next.begin(), next.end());
  out = std::move(next);
  e.clear();
  return true;
}
} // namespace encore::ctr
