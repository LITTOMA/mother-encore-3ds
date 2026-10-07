#include "podunk_mick_session.hpp"
#include "podunk_dialogue_actor_resource.hpp"
#include "podunk_dialogue_business_native.hpp"
#include "podunk_global_data_host.hpp"
#include "podunk_global_host.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.source_sha256 == b.source_sha256 &&
         a.upstream_commit == b.upstream_commit;
}
auto unavailable(const char *s) {
  return [s](auto &&...args) -> bool {
    auto v = std::forward_as_tuple(args...);
    std::get<sizeof...(args) - 1>(v) = s;
    return false;
  };
}
} // namespace
struct PodunkMickSession::State {
  PodunkMickInput in;
  PodunkStableCanvas canvas;
  FieldNodeRecipeData recipe;
  FieldDialogueLifecycleData life;
  FieldDialogueUiData ui;
  FieldDialogueVisualData visual;
  FieldDialogueAudioData audio;
  PodunkDialogueRootData root_data;
  std::shared_ptr<const DialogueActorResourceData> actor_data;
  PodunkDialogueActorResource actor;
  FieldDialogueAudioPlayer audio_player;
  PodunkDialogueHost dialogue;
  PodunkDialogueRootOwner root_script;
  PodunkProgrammeHost programme;
  PodunkDialogueOptionsAdapter options;
  PodunkDialogueSceneNative native;
  PodunkDialogueBusinessNative business;
  PodunkDialogueLifecyclePorts lifecycle;
  std::set<FieldObjectId> dialogue_objects, dialogue_history;
  PodunkDialogueFrame current;
  FieldObjectId notifying = 0, printer_owner = 0;
  std::map<FieldObjectId, PodunkDialogueWaitConnection> wait_connections;
  PlayerInputEvent event;
  Vec2 previous_controls{};
  std::vector<Vec2> pressed_directions, released_directions;
  uint32_t generation = 0;
  bool prepared = false, active = false, input_live = false, handled = false,
       update_pending = false, idle_frame_live = false, deferred_live = false,
       source_scene_retired = false;
  uint32_t source_callback_depth = 0;
  FieldObjectId source_callback_receiver = 0;
  FreshHouseState *return_house=nullptr;
  HousePresentationCallbacks return_callbacks{};
  PodunkMickHouseTalkerOwner *house_talker=nullptr;
  struct SourceCall {
    State &owner;FieldObjectId previous;
    SourceCall(State &s,FieldObjectId receiver=0):owner(s),previous(s.source_callback_receiver){
      ++owner.source_callback_depth;owner.source_callback_receiver=receiver;
    }
    ~SourceCall(){owner.source_callback_receiver=previous;--owner.source_callback_depth;}
    SourceCall(const SourceCall&)=delete;SourceCall&operator=(const SourceCall&)=delete;
  };
  std::string failure;
  static bool text_completed(void *owner){
    auto *self=static_cast<State*>(owner);
    if(!self||!self->printer_owner||!self->dialogue_objects.count(self->printer_owner)||
       !self->in.continuation->registry()->object_exists(self->printer_owner))return false;
    SourceCall call(*self,self->printer_owner);
    return self->root_script.text_completed(self->printer_owner,self->failure);
  }
  template <class Data>
  bool plain(PodunkPackRole role, Data &out, std::string &e) {
    std::vector<uint8_t> bytes;
    return in.bundle->read(role, in.asset_root, bytes, e) &&
           out.load(bytes.data(), bytes.size(), e);
  }
  template <class Data>
  bool identified(PodunkPackRole role, Data &out, std::string &e) {
    const auto *entry = in.bundle->entry(role);
    std::vector<uint8_t> bytes;
    return entry && in.bundle->read(role, in.asset_root, bytes, e) &&
           out.load(bytes.data(), bytes.size(), entry->identity, e);
  }
  bool frame(FieldObjectId id, PodunkDialogueFrame &out, std::string &e) const {
    if (id != notifying || !dialogue_objects.count(id))
      return fail(e, "Mick dialogue frame differs from actual Tree dispatch");
    out = current;
    e.clear();
    return true;
  }
  bool session(std::string &e) const {
    if(source_scene_retired)return fail(e,"Mick programme cannot borrow a fresh House source context");
    auto *r = in.continuation->registry();
    auto tree = r->tree_owner(in.player->body().object());
    auto *n = tree ? tree->state(in.player->body().object()) : nullptr;
    if (!in.scene->scene_ready() || !in.player->ready_complete() || !n ||
        !n->alive || !n->inside || !n->ready_notified ||
        r->current_scene() != tree->root())
      return fail(
          e, "Mick programme does not own actual active outdoor scene/player");
    e.clear();
    return true;
  }
  bool sound(std::string_view source, bool play, std::string &e) {
    AudioAsset asset;
    if (!in.continuation->audio()->source_asset(source, asset, e))
      return false;
    if (!play) {
      e.clear();
      return true;
    }
    return in.continuation->audio()->play(asset.stable_id, AudioLane::Effect,
                                          e);
  }
  bool closed_commands(std::string &e) const {
    bool open = false;
    auto *ui = in.continuation->ui();
    if (!ui->source_is_pause_menu_active(ui->binding().object, open, e))
      return false;
    return !open ||
           fail(e, "Mick interaction requires actual commands close owner");
  }
  bool printer(FieldObjectId id, HousePresentation &p, bool acquire,
               std::string &e) {
    if (&p != in.printer || !dialogue_objects.count(id) ||
        !in.continuation->registry()->object_exists(id))
      return fail(e, "Mick actual shared printer identity differs");
    if (acquire) {
      if (printer_owner && printer_owner != id)
        return fail(e, "Mick dialogue cannot steal active printer");
      printer_owner = id;
    } else {
      if (printer_owner != id)
        return fail(e, "Mick printer release owner differs");
      printer_owner = 0;
    }
    e.clear();
    return true;
  }
  bool wait_connection(FieldObjectId id, PodunkDialogueWaitConnection &out,
                       std::string &e) const {
    auto i = wait_connections.find(id);
    auto *bus = in.continuation->signals();
    bool connected = false;
    if (i == wait_connections.end() ||
        !bus->connected(i->second.emitter, i->second.signal, id,
                        i->second.method, connected, e) ||
        !connected)
      return fail(e, "Mick original WaitTimer connection unavailable");
    out = i->second;
    e.clear();
    return true;
  }
  bool connect_wait(FieldObjectId id, std::string &e) {
    auto tree = in.continuation->registry()->tree_owner(id);
    const auto *ref = life.reference(10);
    FieldObjectId timer = 0;
    if (!tree || !ref || !tree->get_node(id, life.node(10), timer, e) ||
        !tree->descriptor(timer) || tree->descriptor(timer)->id != ref->id ||
        wait_connections.count(id))
      return fail(e, "Mick WaitTimer original node binding differs");
    PodunkDialogueWaitConnection w{timer, id, root_data.wait_signal(),
                                   root_data.wait_method(), 0};
    if (!in.continuation->signals()->connect(timer, w.signal, id, w.method, 0,
                                             {}, e))
      return false;
    wait_connections.emplace(id, std::move(w));
    e.clear();
    return true;
  }
  bool source_input(const PlayerInputEvent &next,std::string &e){
    SourceCall call(*this);
    if(input_live)return fail(e,"Mick nested actual input event rejected");
    Vec2 controls;
    if(!in.controls(controls,e))return false;
    event=next;input_live=true;handled=false;
    const Vec2 directions[]={{-1,0},{1,0},{0,-1},{0,1}};
    auto on=[](Vec2 c,Vec2 d){return (d.x&&c.x*d.x>0)||(d.y&&c.y*d.y>0);};
    for(auto d:directions){
      if(on(controls,d)&&!on(previous_controls,d))pressed_directions.push_back(d);
      if(!on(controls,d)&&on(previous_controls,d))released_directions.push_back(d);
    }
    previous_controls=controls;e.clear();return true;
  }
};
PodunkMickSession::PodunkMickSession() : state_(std::make_unique<State>()) {}
PodunkMickSession::~PodunkMickSession() {
  if (state_ && state_->active && state_->in.printer)
    state_->in.printer->set_text_completion_callback(nullptr, nullptr);
}
bool PodunkMickSession::prepare(PodunkMickInput in, std::string &e) {
  auto &s = *state_;
  if (s.prepared || !in.continuation || !in.bundle || !in.sources ||
      !in.programmes || !in.basement || !in.psi || !in.choice_data ||
      !in.choices || !in.locale || !in.printer || !in.scene || !in.npc ||
      !in.state || !in.player || !in.player_camera || !in.cameras ||
      !in.audio_server || !in.timers || !in.geometry || !in.physics ||
      !in.bars || !in.fade || !in.house_renderer || !in.font ||
      !in.source_font || !in.motion || !in.house.valid() || !in.controls ||
      !in.query_input)
    return fail(e, "Mick source session requires actual continued "
                   "scene/UI/inventory owners");
  s.in = std::move(in);
  if (!s.identified(PodunkPackRole::DialogueNodeRecipe, s.recipe, e) ||
      !s.plain(PodunkPackRole::DialogueLifecycle, s.life, e) ||
      !s.plain(PodunkPackRole::DialogueRootScript, s.root_data, e) ||
      !s.identified(PodunkPackRole::DialogueUi, s.ui, e) ||
      !s.identified(PodunkPackRole::DialogueVisual, s.visual, e) ||
      !s.identified(PodunkPackRole::DialogueAudio, s.audio, e))
    return false;
  auto actor = std::make_shared<DialogueActorResourceData>();
  if (!s.identified(PodunkPackRole::DialogueActorResource, *actor, e) ||
      !s.actor.prepare(actor, *s.in.continuation->registry(), e) ||
      !s.canvas.prepare(*s.in.continuation->registry(),
                        *s.in.continuation->native_root(), e))
    return false;
  s.actor_data = std::move(actor);
  s.prepared = true;
  e.clear();
  return true;
}
bool PodunkMickSession::activate(std::string &e) {
  auto &s = *state_;
  if (!s.prepared || s.active || !s.session(e))
    return fail(e, "Mick activation requires completed real scene transition");
  auto &c = *s.in.continuation;
  auto &r = *c.registry();
  auto &bus = *c.signals();
  auto &global = c.global()->core();
  FieldObjectId canvas = 0;
  if (!c.ui()->bind_source_global(global, e) ||
      !r.create_continuation_stable_canvas(canvas, e) || !s.canvas.owns(canvas))
    return false;
  PodunkDialogueRootEndpoints h;
  h.tree = [&s](auto id) {
    return s.in.continuation->registry()->tree_owner(id).get();
  };
  h.frame = [&s](auto id, auto &frame, auto &error) {
    return s.frame(id, frame, error);
  };
  h.player_position = [&s](Vec2 &out, std::string &error) {
    FieldTransform world;
    auto *r = s.in.continuation->registry();
    auto t = r->tree_owner(s.in.player->body().object());
    if (!t || !t->world_transform(s.in.player->body().object(), world, error))
      return false;
    out = world[2];
    return true;
  };
  h.animation_playing = [&s](auto id, auto &playing, auto &error) {
    return s.native.animation_playing(id, playing, error);
  };
  h.wait_connection = [&s](auto id, auto &out, auto &error) {
    return s.wait_connection(id, out, error);
  };
  h.preload_actor = [&s](auto path, const auto &sha, auto &id, auto &error) {
    return s.actor.preload(path, sha, id, error);
  };
  h.release_actor = [&s](auto id, auto &error) {
    return s.actor.release(id, error);
  };
  h.translate_bullet = [&s](auto key, auto format, auto &out, auto &error) {
    if (key != s.root_data.bullet_key() ||
        format != s.root_data.bullet_format())
      return fail(error, "Mick bullet source format differs");
    const auto translation = s.in.locale->text(key);
    if (translation.status == TranslationStatus::MissingKey)
      return fail(error, "Mick bullet translation absent");
    const auto text = translation.text;
    out = std::string(format);
    auto at = out.find("%s");
    if (at == out.npos || out.find('%', at + 2) != out.npos)
      return fail(error, "Mick bullet format is unsupported");
    out.replace(at, 2, text);
    return true;
  };
  h.input_handled = [&s](auto id, auto &error) {
    if (!s.input_live || id != s.notifying)
      return fail(error, "Mick input handling outside current source event");
    s.handled = true;
    error.clear();
    return true;
  };
  h.printer_ownership = [&s](auto id, auto &p, bool acquire, auto &error) {
    return s.printer(id, p, acquire, error);
  };
  h.prepare_options_labels = [&s](auto id, const auto &choices, auto locale,
                                  auto &error) {
    return s.options.prepare(id, choices, locale, error);
  };
  h.hide_options_labels = [&s](auto id, auto &error) {
    return s.options.hide(id, error);
  };
  h.talker_talking = [&s](auto id, bool talking, auto &error) {
    if(s.source_scene_retired){
      if(!s.return_house||!s.house_talker||s.house_talker->house()!=s.return_house||
         s.in.printer!=&s.return_house->presentation)
        return fail(error,"Returned House talker has no exact native source receiver");
      return s.house_talker->set_talking(id,talking,error);
    }
    auto t = s.in.continuation->registry()->tree_owner(id);
    const auto *d = t ? t->descriptor(id) : nullptr;
    auto *npc = s.in.npc;
    if (!d || !npc || !npc->set_talking(d->id, talking))
      return fail(error, "Mick source talker body unavailable");
    error.clear();
    return true;
  };
  h.name_rect_changed = [&s](auto id, auto &error) {
    return s.native.name_rect_changed(id, error);
  };
  if (!s.root_script.initialize(s.root_data, s.life, s.recipe, *s.in.printer,
                                s.programme, s.dialogue, *s.in.choices,
                                *s.in.timers, s.in.locale, std::move(h), e) ||
      !s.options.initialize(
          s.root_data, s.recipe, s.dialogue, s.root_script, s.programme,
          *s.in.choices,
          [&s](auto id) {
            return s.in.continuation->registry()->tree_owner(id).get();
          },
          e))
    return false;
  PodunkDialogueBusinessInput business;
  business.data = c.ui_continuation_data();
  business.life = &s.life;
  business.scene = &s.in.sources->lifecycle();
  business.registry = &r;
  business.signals = &bus;
  business.global = &global;
  business.global_data = &c.characters()->runtime();
  business.ui = c.ui();
  business.root_script = &s.root_script;
  business.dialogue = &s.dialogue;
  business.player = s.in.player;
  business.bars = s.in.bars;
  business.fade = s.in.fade;
  business.sounds = c.named_sfx();
  if (!s.business.prepare(std::move(business), e) ||
      !s.lifecycle.prepare({c.ui_continuation_data(), &s.life, &s.recipe,
                            s.in.programmes, s.in.motion, &r, &bus, c.ui(),
                            s.in.player, &s.dialogue, &s.root_script,
                            &s.programme, &s.business},
                           e))
    return false;
  PodunkDialogueNativeInput native;
  native.recipe = &s.recipe;
  native.ui = &s.ui;
  native.visual = &s.visual;
  native.audio = &s.audio;
  native.timer_data = &s.in.sources->timers();
  native.registry = &r;
  native.signals = &bus;
  native.timers = s.in.timers;
  native.geometry = s.in.geometry;
  native.physics = s.in.physics;
  native.global = &global;
  native.root = c.native_root();
  native.cameras = s.in.cameras;
  native.player_camera = s.in.player_camera;
  native.audio_server = s.in.audio_server;
  native.sounds = c.named_sfx();
  native.dialogue = &s.dialogue;
  native.printer = s.in.printer;
  native.house_renderer = s.in.house_renderer;
  native.font = s.in.font;
  native.source_font = s.in.source_font;
  native.asset_root = s.in.asset_root;
  native.controls = s.in.controls;
  native.frame = [&s](auto id, auto &out, auto &error) {
    return s.frame(id, out, error);
  };
  native.control_directions = [&s](auto &pressed, auto &released, auto &error) {
    pressed = s.pressed_directions;
    released = s.released_directions;
    error.clear();
    return true;
  };
  native.update_pending = [&s](auto &out, auto &error) {
    out = s.update_pending;
    error.clear();
    return true;
  };
  PodunkDialogueServices services;
  services.root_script = &s.root_script;
  services.lifecycle = s.lifecycle.host();
  if (!s.native.prepare(std::move(native), e) || !s.native.apply(services, e) ||
      !s.audio_player.initialize(
          *c.audio(),
          [&s](auto bus, auto &db, auto &muted, auto &error) {
            return s.in.audio_server->body().gain(bus, db, muted, error);
          },
          e))
    return false;
  services.emit = [&bus](auto id, auto name, const auto &arg, auto &error) {
    std::vector<FieldDeferredValue> args;
    if (!std::holds_alternative<std::monostate>(arg))
      args.push_back(arg);
    return bus.emit(id, name, args, error);
  };
  services.menu_sound = [&s](auto root, const HouseAudioEvent &event,
                             auto &error) {
    if (!s.dialogue_objects.count(root) || !event.voice.empty() ||
        event.pitch != 1)
      return fail(error, "Mick menu sound event/source owner differs");
    const auto *data = s.in.continuation->ui_continuation_data();
    const std::string *source = nullptr, *name = nullptr;
    if (event.kind == HouseAudioKind::MenuOpen) {
      source = &data->dialogue_policy().open_sound_source;
      name = &data->dialogue_policy().open_sound_name;
    } else if (event.kind == HouseAudioKind::MenuClose) {
      source = &data->business_policy().close_sound_source;
      name = &data->business_policy().close_sound_name;
    } else
      return fail(error, "Mick printer event is not a source menu sound");
    FieldObjectId voice = 0;
    return s.in.continuation->named_sfx()->play_sfx(*source, *name, voice,
                                                    error);
  };
  services.input_sound = [&s](auto id, auto &out, const auto &audio,
                              auto &error) {
    auto t = s.in.continuation->registry()->tree_owner(id);
    if (!t)
      return false;
    const auto &path = s.in.continuation->ui_continuation_data()
                           ->dialogue_policy()
                           .input_sound_node;
    if (path.empty() || !t->get_node(id, path, out, error))
      return false;
    const auto *d = t->descriptor(out);
    const auto *n = d ? audio.node(d->id) : nullptr;
    const auto *r = d ? s.recipe.record(d->id) : nullptr;
    if (!d || !n || !r || n->path != path || r->path != path ||
        d->native_class != r->native_class ||
        r->native_class != "AudioStreamPlayer")
      return fail(error, "Mick actual InputSound source binding differs");
    error.clear();
    return true;
  };
  auto *npc = s.in.npc;
  if (!npc ||
      !s.dialogue.initialize(s.life, *s.in.programmes, s.recipe, s.ui, s.visual,
                             s.audio, s.in.sources->timers(), r, *s.in.timers,
                             *npc, s.in.house, *s.in.printer, *c.random(),
                             s.audio_player, std::move(services), e) ||
      !c.ui()->bind_dialogue_sources(s.life, s.recipe, s.root_script,
                                     *s.in.printer, e))
    return false;
  PodunkProgrammeOps ops;
  if (!s.in.state->apply(ops, e))
    return false;
  ops.admit_session_scene = [&s](auto &error) { return s.session(error); };
  ops.admit_audio = [&s](auto p, auto &error) {
    return s.sound(p, false, error);
  };
  ops.play_audio = [&s](auto p, auto &error) {
    return s.sound(p, true, error);
  };
  ops.admit_text = [&s](const auto &text, auto &error) {
    for (uint32_t i = 0; i < s.in.house.count(HouseSection::Dialogues); ++i) {
      auto d = s.in.house.dialogue(i);
      if (d.id == text.id && s.in.house.string(d.source_path) == text.source &&
          d.segment_count) {
        error.clear();
        return true;
      }
    }
    return fail(error,
                "Mick source phrase is absent from shared text resource");
  };
  ops.prepare_text = [&s](const FieldProgrammeText &text, FieldObjectId root,
                          std::string &error) {
    if (s.in.programmes->text(text.id) != &text)
      return fail(error,
                  "Mick phrase cursor is not the original programme text");
    return s.root_script.phrase_begin(root, error);
  };
  ops.presented_text = [&s](const auto &text, auto id, auto &error) {
    return s.root_script.presented_text(id, text, error);
  };
  ops.player_name = [&c](auto &out, auto &error) {
    auto &data = c.characters()->runtime();
    return data.character_nickname(data.data()->first_character(), out, error);
  };
  ops.admit_lifecycle = [&s](const auto &a, const auto &ctx, auto &error) {
    return s.dialogue.admit(a, ctx, error);
  };
  ops.apply_lifecycle = [&s](const auto &a, const auto &ctx, auto &error) {
    return s.dialogue.apply(a, ctx, error);
  };
  ops.open_dialogue = [&s](const auto &data, auto p, const auto &ctx,
                           auto generation, auto &id, auto &error) {
    return s.dialogue.open(data, p, ctx, generation, id, error);
  };
  ops.admit_dialogue_ready = [&s](auto id, auto generation, auto &error) {
    return s.dialogue.admit_ready(id, generation, error);
  };
  ops.close_commands_for_telepathy = [&s](auto &error) {
    return s.closed_commands(error);
  };
  const auto reject = unavailable(
      "Original Player telepathy effect consumer remains unsupported");
  ops.telepathy.player_has_field_skill = reject;
  ops.telepathy.probe = reject;
  ops.telepathy.admit_dialogue = reject;
  ops.telepathy.clear_event_collider = reject;
  ops.telepathy.turn = reject;
  ops.telepathy.effect = reject;
  ops.telepathy.dialogue = reject;
  ops.telepathy.press_prompt = reject;
  auto tree = r.tree_owner(s.in.player->body().object());
  if (!s.programme.prepare(
          *s.in.programmes, *s.in.basement, *s.in.psi, s.in.sources->npc(),
          *npc, s.in.sources->tree(), *tree, *s.in.scene, s.in.house,
          *s.in.printer, *s.in.choice_data, *s.in.choices, std::move(ops), e) ||
      !s.programme.activate(e))
    return false;
  s.in.printer->set_text_completion_callback(State::text_completed,&s);
  s.active = true;
  e.clear();
  return true;
}
void PodunkMickSession::apply_npc(FieldNpcHost &host) {
  auto &s = *state_;
  host.admit_program = [&s](const std::string &path, std::string &e) {
    if (!s.active)
      return fail(e, "Outdoor programme session is not active");
    for (uint32_t i = 0; i < s.in.programmes->program_count(); ++i) {
      auto *r = s.in.programmes->record(i);
      if (r && r->path == path)
        for (const auto &row : s.in.programmes->npc().rows)
          if (row.program == path && row.supported)
            return s.programme.admit_npc(s.in.programmes->npc().id,
                                         row.thoughts, e);
    }
    return fail(
        e, "Outdoor NPC programme is outside implemented original Mick slice");
  };
  host.open_program = [&s](uint32_t id, const std::string &p, bool thoughts,
                           const FieldNpcDescriptor &d, std::string &e) {
    if (!s.active || d.id != id || id != s.in.programmes->npc().id ||
        s.generation == UINT32_MAX)
      return fail(e, "Outdoor NPC source programme branch unavailable");
    return s.programme.open_selected_npc(id, p, thoughts, ++s.generation, e);
  };
  host.mark_seen = [&s](uint32_t id, const FieldNpcDialogue &d,
                        std::string &e) {
    std::string path;
    auto tree =
        s.in.continuation->registry()->tree_owner(s.in.player->body().object());
    if (!s.active || !tree || id != s.in.programmes->npc().id ||
        !s.in.state->actual_path(tree->source_object(id), path, e))
      return false;
    return s.in.state->mark_seen(
        path + ":" + d.flag + ":" + std::to_string(d.ordinal) + ":" + d.program,
        e);
  };
  host.begin_talker = [&s](uint32_t id, std::string &e) {
    if (!s.active || id != s.in.programmes->npc().id)
      return fail(e, "Outdoor source talker owner unavailable");
    auto tree =
        s.in.continuation->registry()->tree_owner(s.in.player->body().object());
    return s.in.continuation->global()->core().set_object(
        FieldGlobalMemberRole::Talker, tree->source_object(id), e);
  };
  host.telepathy_effect = [this, &s](uint32_t source, bool enabled,
                                     std::string &error) {
    auto tree =
        s.in.continuation->registry()->tree_owner(s.in.player->body().object());
    auto actual = tree ? tree->source_object(source) : 0;
    if (!actual || source != s.in.programmes->npc().id)
      return fail(error, "Mick telepathy target is not actual source NPC");
    return telepathy_effect(actual, enabled, error);
  };
  host.close_commands = [&s](std::string &e) {
    return s.active && s.closed_commands(e);
  };
}
bool PodunkMickSession::candidate(const FieldNodeDescriptor &d,
                                  const FieldIdentity &i) const {
  auto &s = *state_;
  if (s.canvas.candidate(d, i))
    return true;
  const auto *r = s.recipe.record(d.id);
  return s.prepared && same(i, s.recipe.identity()) && r &&
         r->native_class == d.native_class && r->script == d.script &&
         r->script_sha == d.script_sha;
}
bool PodunkMickSession::owns(FieldObjectId id) const {
  return state_->canvas.owns(id) || state_->dialogue_objects.count(id);
}
bool PodunkMickSession::construct(FieldObjectId id,
                                  const FieldNodeDescriptor &d,
                                  const FieldIdentity &i, std::string &e) {
  auto &s = *state_;
  if (s.canvas.candidate(d, i))
    return s.canvas.construct(id, d, i, e);
  auto t = s.in.continuation->registry()->tree_owner(id);
  if (!s.active || !candidate(d, i) || !t || !t->state(id) ||
      s.dialogue_objects.count(id))
    return fail(e, "Mick actual dialogue source allocation rejected");
  s.dialogue_objects.insert(id);s.dialogue_history.insert(id);
  e.clear();
  return true;
}
bool PodunkMickSession::bind(FieldObjectId id, const FieldNodeDescriptor &d,
                             FieldNodeBinding &out, std::string &e) {
  auto &s = *state_;
  State::SourceCall call(s,id);
  if (s.canvas.owns(id))
    return s.canvas.bind(id, out, e);
  if (!s.dialogue.bind(id, d, out, e))
    return false;
  if (d.id == s.recipe.identity().scene_id)
    return s.connect_wait(id, e);
  return true;
}
bool PodunkMickSession::phase(FieldObjectId id, const FieldNodeBinding &b,
                              FieldTreePhase p, float dt, bool paused,
                              bool update, std::string &e) {
  auto &s = *state_;
  State::SourceCall call(s,id);
  if (s.canvas.owns(id))
    return s.canvas.phase(id, p, e);
  if (!s.active || !s.dialogue_objects.count(id))
    return fail(e, "Mick notification has no owning source instance");
  s.notifying = id;
  s.current = {dt, paused, false, p, {}};
  s.update_pending = update;
  if (p == FieldTreePhase::Input && s.input_live) {
    for (const auto &a : s.root_data.actions())
      if (std::find(s.event.pressed.begin(), s.event.pressed.end(), a) !=
          s.event.pressed.end()) {
        s.current.pressed = true;
        s.current.action = a;
        break;
      }
  }
  const bool ok = s.dialogue.dispatch(id, b, p, e);
  s.notifying = 0;
  return ok;
}
bool PodunkMickSession::deferred(const FieldDeferredMessage &m,
                                 std::string &e) {
  auto &s = *state_;
  if(s.deferred_live)return fail(e,"Mick nested source deferred callback rejected");
  State::SourceCall call(s,m.object);
  s.deferred_live=true;
  const bool result=s.canvas.owns(m.object)?s.canvas.deferred(m,e):s.dialogue.deferred(m,e);
  s.deferred_live=false;return result;
}
bool PodunkMickSession::release(FieldObjectId id, std::string &e) {
  auto &s = *state_;
  State::SourceCall call(s,id);
  if (s.canvas.owns(id))
    return s.canvas.release(id, e);
  if (!s.dialogue_objects.erase(id))
    return fail(e, "Mick source release owner missing");
  s.wait_connections.erase(id);
  e.clear();
  return true;
}
bool PodunkMickSession::declaration(FieldObjectId id, std::string_view name,
                                    uint32_t &arity, std::string &e) const {
  auto &s = *state_;
  if (s.canvas.owns(id)) {
    if (name == "ready" || name == "tree_entered" || name == "tree_exiting" ||
        name == "tree_exited") {
      arity = 0;
      e.clear();
      return true;
    }
    return fail(e, "Stable canvas unknown engine signal");
  }
  if (!s.dialogue_objects.count(id))
    return fail(e, "Mick signal source not owned");
  auto tree = s.in.continuation->registry()->tree_owner(id);
  const auto *d = tree ? tree->descriptor(id) : nullptr;
  if (d && d->id == s.recipe.identity().scene_id) {
    if (name == s.life.done_signal()) {
      arity = 1;
      e.clear();
      return true;
    }
    if (name == s.life.ready_signal() ||
        name == s.in.continuation->ui_continuation_data()
                    ->business_policy()
                    .end_signal) {
      arity = 0;
      e.clear();
      return true;
    }
  }
  return s.native.declaration(id, name, arity, e);
}
bool PodunkMickSession::emits_ready(FieldObjectId id) const {
  return state_->dialogue_objects.count(id);
}
bool PodunkMickSession::idle_begin(std::string &e) {
  if (!state_->active) {
    e.clear();
    return true;
  }
  auto &s=*state_;
  if(s.source_scene_retired){e.clear();return true;}
  if(s.idle_frame_live)return fail(e,"Mick source idle wrapper is already open");
  State::SourceCall call(s);
  if(!s.programme.idle_begin(e))return false;
  s.idle_frame_live=true;return true;
}
bool PodunkMickSession::idle_end(uint64_t epoch, float dt, bool paused,
                                 std::string &e) {
  auto &s = *state_;
  if (!s.active) {
    e.clear();
    return true;
  }
  if (!s.failure.empty())
    return fail(e, s.failure.c_str());
  if(s.source_scene_retired){
    // The real Deferred freed this source scene inside the current outer idle
    // wrapper. Admission proved every actual producer/wait closed. Complete
    // only that scheduling wrapper; never process the freed scene or tick the
    // destination printer before its own mapped Ready/frame.
    if(s.source_callback_depth)return fail(e,"Retired Mick source callback is still executing");
    s.idle_frame_live=false;e.clear();return true;
  }
  State::SourceCall call(s);
  if (!s.in.printer->idle_frame(dt))
    return fail(e, "Outdoor shared printer idle step rejected");
  if (s.printer_owner) {
    auto *ui = s.dialogue.ui(s.printer_owner);
    if (!ui || !ui->sync_text(s.printer_owner, *s.in.printer, e))
      return false;
  }
  if (!s.programme.idle_process(dt, e) ||
      !s.native.idle_tail(epoch, dt, paused, e) ||
      !s.business.idle(epoch, dt, e) || !s.in.state->writeback(e))
    return false;
  s.pressed_directions.clear();
  s.released_directions.clear();
  if(!s.audio_player.pump_streams(e))return false;
  s.idle_frame_live=false;return true;
}
bool PodunkMickSession::begin_input(const PlayerInputEvent &event,
                                    std::string &e) {
  auto &s = *state_;
  if(s.source_scene_retired)return fail(e,"Old Mick input traversal is retired after House return");
  return s.source_input(event,e);
}
bool PodunkMickSession::begin_house_input(FreshHouseState &house,
    const PlayerInputEvent &event,std::string &e){
 PodunkMickHouseNativeState n;
 if(!house_native_state(house,n,e))return false;
 auto &s=*state_;
 if(!s.house_talker||s.house_talker->house()!=&house||!house.world.house_programme_owner()||
    n.callback_depth||n.callback_receiver||n.notifying||n.input_live||s.idle_frame_live)
  return fail(e,"House input must borrow its exact actual closed source event boundary");
 return s.source_input(event,e);
}
bool PodunkMickSession::house_idle_tail(FreshHouseState &house,uint64_t epoch,
    float dt,bool paused,std::string &e){
 PodunkMickHouseNativeState n;
 if(!house_native_state(house,n,e))return false;
 auto &s=*state_;
 if(!s.house_talker||s.house_talker->house()!=&house||!house.world.house_programme_owner()||
    n.callback_depth||n.callback_receiver||n.notifying||n.input_live||s.idle_frame_live||
    !std::isfinite(dt)||dt<0)
  return fail(e,"House idle tail must follow its actual canonical source frame exactly once");
 State::SourceCall call(s);
 if(!house.presentation.idle_frame(dt))return fail(e,house.presentation.error());
 if(s.printer_owner){auto *ui=s.dialogue.ui(s.printer_owner);
  if(!ui||!ui->sync_text(s.printer_owner,house.presentation,e))return false;}
 if(!s.native.idle_tail(epoch,dt,paused,e)||!s.business.idle(epoch,dt,e)||
    !s.audio_player.pump_streams(e))return false;
 s.pressed_directions.clear();s.released_directions.clear();e.clear();return true;
}
void PodunkMickSession::end_input() { state_->input_live = false; }
bool PodunkMickSession::input_handled() const {
  return state_->input_live && state_->handled;
}
bool PodunkMickSession::draw(uint64_t epoch, std::string &e) {
  auto &s = *state_;
  if (!s.active) {
    e.clear();
    return true;
  }
  auto size = s.in.continuation->native_root()->viewport().size;
  const auto bars = s.in.bars->pose(size.x, size.y);
  for (const auto &r : bars.bars)
    if (r.w > 0 && r.h > 0 &&
        !C2D_DrawRectSolid(r.x, r.y, 0, r.w, r.h, bars.color))
      return fail(e, "Outdoor source blackbar GPU submission rejected");
  return s.native.draw(epoch, size.x, size.y, 0, 0, 0xffffffffu, e);
}
bool PodunkMickSession::telepathy_effect(FieldObjectId target, bool enabled,
                                         std::string &e) {
  auto &s = *state_;
  if (!s.active || !s.session(e) ||
      !s.in.continuation->registry()->object_exists(target))
    return fail(e, "Mick telepathy actual session/target is not active");
  State::SourceCall call(s,target);
  return s.business.set_telepathy_effect(enabled, target, e);
}
bool PodunkMickSession::observe(const HouseUiReentryInput &in,
    const HouseUiContinuation &ui,bool rebound,HouseUiReentryBorrowState &out,
    std::string &e)const{
 const auto &s=*state_;
 if(!s.prepared||!s.active||!s.in.continuation||in.borrowers!=this||
    !in.registry||in.registry->poisoned()||s.in.continuation->registry()!=in.registry||
    s.in.continuation->ui()!=&ui||in.registry->external_object(ui.binding().object)!=&ui||
    !in.old_house||!in.next_house||in.old_house==in.next_house||!in.source||
    !in.doors||!in.door_runtime||in.door_runtime->data()!=in.doors||
    in.door_runtime->phase()!=FieldDoorPhase::Deferred||
    in.door_runtime->active_door()!=in.source->door_id()||
    !in.door_runtime->source_ready(in.source->door_id())||
    in.random!=s.in.continuation->random()||in.uid_ledger!=s.in.continuation->uid_ledger()||
    !in.random||!in.uid_ledger||!in.old_tree||!in.next_tree||
    !same(in.old_identity,in.doors->identity())||!same(in.next_identity,in.source->identity())||
    !same(in.old_identity,s.in.sources->tree().identity())||
    s.in.house.bytes()!=in.house_data.bytes()||s.in.house.byte_size()!=in.house_data.byte_size()||
    !in.source->matches(*in.doors,in.old_house->world.content(),in.house_data,e)||
    !in.source->matches(*in.doors,in.next_house->world.content(),in.house_data,e))
  return fail(e,"Mick House printer rebind actual source/Registry/session owners differ");
 const auto *printer=rebound?&in.next_house->presentation:&in.old_house->presentation;
 FieldIdentity old_id{},next_id{};
 const auto *old_root=in.old_tree->state(in.old_root);
 const auto *next_root=in.next_tree->state(in.next_root);
 if(s.in.printer!=printer||!old_root||!old_root->alive||old_root->queued||
    !next_root||!next_root->alive||next_root->inside||next_root->queued||
    next_root->ready_notified||!next_root->ready_first||
    in.registry->tree_owner(in.old_root)!=in.old_tree||
    in.registry->tree_owner(in.next_root)!=in.next_tree||
    !in.old_tree->object_identity(in.old_root,old_id)||!same(old_id,in.old_identity)||
    !in.next_tree->object_identity(in.next_root,next_id)||!same(next_id,in.next_identity)||
    !in.next_house->scene_ready_pending()||!s.failure.empty()||s.input_live||s.notifying||
    s.printer_owner||!s.in.fade||s.in.fade->restore_pending()||
    s.source_callback_depth||s.source_callback_receiver||s.deferred_live||
    s.source_scene_retired!=rebound||s.programme.source_scene_retired()!=rebound||
    s.dialogue.lifecycle_tree()!=(rebound?in.next_tree.get():in.old_tree.get())||!s.dialogue_objects.empty()||
    !s.wait_connections.empty())
  return fail(e,"Mick House reentry rejects live source frames/dialogue/input/printer work");
 if(!s.root_script.observes_closed_printer(*printer,e)||
    !s.dialogue.observes_closed_printer(*printer,e)||
    !s.native.observes_closed_printer(*printer,e)||
    !s.programme.observes_closed_printer(*printer,e)||
    !s.lifecycle.source_frame_closed(e))return false;
 const auto old_callbacks=in.old_house->presentation.callback_bindings();
 const auto callbacks=printer->callback_bindings();
 if(callbacks.random!=in.random||callbacks.completion!=State::text_completed||
    callbacks.completion_state!=&s||callbacks.value!=old_callbacks.value||
    callbacks.value_state!=(rebound?static_cast<const void*>(&in.next_house->world):
                                    static_cast<const void*>(&in.old_house->world))||
    callbacks.locale!=old_callbacks.locale||callbacks.locale_state!=old_callbacks.locale_state||
    callbacks.glyph!=old_callbacks.glyph||callbacks.glyph_state!=old_callbacks.glyph_state)
  return fail(e,"Mick House printer has unknown/stale actual callback function receivers");
 auto targets=s.dialogue_history;
 targets.insert(ui.binding().object);targets.insert(in.registry->stable_canvas());
 const auto pending=in.registry->pending_messages_to(targets);
 if(pending)return fail(e,"Mick House reentry has queued actual UI/dialogue/native messages");
 if(!rebound&&(!in.next_house->presentation.admit_source_callback_rebind(
       in.old_house->presentation,&in.old_house->world,&in.next_house->world,
       State::text_completed,const_cast<State*>(&s),e)||
    !s.root_script.admit_printer_rebind(*printer,in.next_house->presentation,e)||
    !s.dialogue.admit_printer_rebind(*printer,in.next_house->presentation,e)||
    !s.native.admit_printer_rebind(*printer,in.next_house->presentation,e)||
    !s.programme.admit_printer_rebind(*printer,in.next_house->presentation,e)||
    !s.dialogue.admit_house_tree_rebind(in,e)))return false;
 HouseUiReentryBorrowState next;
 next.registry=in.registry;next.printer=printer;next.dialogue_script=&s.root_script;
 next.random=in.random;next.uid_ledger=in.uid_ledger;
 next.dialogue_objects.assign(s.dialogue_objects.begin(),s.dialogue_objects.end());
 next.pending_ui_callbacks=pending;
 // Every concrete native/factory/wait map was inspected above. No receiver,
 // waiter, event, cache or actual ObjectID is cleared to obtain this receipt.
 next.complete=true;out=std::move(next);e.clear();return true;
}
bool PodunkMickSession::rebind(const HouseUiReentryInput &in,
    HouseUiContinuation &ui,std::string &e){
 HouseUiReentryBorrowState before;
 if(!observe(in,ui,false,before,e))return false;
 auto &s=*state_;auto &next=in.next_house->presentation;
 const auto &old=in.old_house->presentation;
 const auto random_state=in.random->state(),draws=in.random->raw_draw_count();
 const auto uids=*in.uid_ledger;
 // Existing endpoints capture this same fixed State by reference. Updating
 // only its printer and the four actual consumers preserves their callbacks,
 // source fields, coroutine/signal ledgers, native IDs and resource owners.
 if(!next.rebind_source_callbacks(old,&in.old_house->world,&in.next_house->world,
       State::text_completed,&s,e)||!s.dialogue.rebind_house_tree(in,e)||
    !s.root_script.rebind_printer(old,next,e)||
    !s.dialogue.rebind_printer(old,next,e)||!s.native.rebind_printer(old,next,e)||
    !s.programme.rebind_printer(old,next,e))return false;
 s.in.printer=&next;
 s.return_house=in.next_house;
 s.return_callbacks=next.callback_bindings();
 if(!s.programme.retire_source_scene(e))return false;
 s.source_scene_retired=true;
 HouseUiReentryBorrowState after;
 if(!observe(in,ui,true,after,e))return false;
 if(after.dialogue_objects!=before.dialogue_objects||in.random->state()!=random_state||
    in.random->raw_draw_count()!=draws||*in.uid_ledger!=uids)
  return fail(e,"Mick printer rebind changed actual objects/entropy/UID ledger");
 e.clear();return true;
}
bool PodunkMickSession::active() const { return state_->active; }
bool PodunkMickSession::source_scene_retired() const { return state_->source_scene_retired; }
bool PodunkMickSession::house_native_state(FreshHouseState &house,
    PodunkMickHouseNativeState &out,std::string &e)const{
 auto &s=*state_;
 if(!s.prepared||!s.active||!s.in.continuation||!s.source_scene_retired||
    !s.programme.source_scene_retired()||s.return_house!=&house||
    s.in.printer!=&house.presentation||!s.failure.empty())
  return fail(e,"House native borrow is not the actual retired Mick/fixed House receiver");
 auto *registry=s.in.continuation->registry();
 const auto callbacks=house.presentation.callback_bindings();
 if(!registry||registry->poisoned()||callbacks.random!=s.in.continuation->random()||
    callbacks.completion!=State::text_completed||callbacks.completion_state!=&s||
    callbacks.value!=s.return_callbacks.value||callbacks.value_state!=&house.world||
    callbacks.locale!=s.return_callbacks.locale||callbacks.locale_state!=s.return_callbacks.locale_state||
    callbacks.glyph!=s.return_callbacks.glyph||callbacks.glyph_state!=s.return_callbacks.glyph_state||
    s.dialogue.lifecycle_tree()!=registry->tree_owner(registry->current_scene()).get())
  return fail(e,"House native borrow has stale printer/World/Tree callback receivers");
 PodunkMickHouseNativeState n;
 n.registry=registry;n.ui=s.in.continuation->ui();n.dialogue=&s.dialogue;
 n.root=&s.root_script;n.lifecycle=&s.lifecycle;n.old_programme=&s.programme;
 n.native=&s.native;
 n.recipe=&s.recipe;n.life=&s.life;n.choice_data=s.in.choice_data;n.locale=s.in.locale;
 n.audio_data=&s.audio;
 n.input_sound_node=s.in.continuation->ui_continuation_data()->dialogue_policy().input_sound_node;
 n.choices=s.in.choices;n.random=s.in.continuation->random();
 n.global_data=&s.in.continuation->characters()->runtime();
 n.uid_ledger=s.in.continuation->uid_ledger();
 n.objects.assign(s.dialogue_objects.begin(),s.dialogue_objects.end());
 auto targets=s.dialogue_history;
 targets.insert(n.ui->binding().object);targets.insert(registry->stable_canvas());
 n.history.assign(targets.begin(),targets.end());
 for(const auto &w:s.wait_connections)n.wait_connections.push_back(w.second);
 n.frame=s.current;n.notifying=s.notifying;n.printer_owner=s.printer_owner;
 n.callback_depth=s.source_callback_depth;n.callback_receiver=s.source_callback_receiver;
 n.input_live=s.input_live;n.retired=s.source_scene_retired;
 if(!s.in.fade)return fail(e,"House native borrow has no actual retained Fade coroutine owner");
 n.business_pending=s.in.fade->restore_pending();
 n.pending_messages=registry->pending_messages_to(targets);
 out=std::move(n);e.clear();return true;
}
bool PodunkMickSession::bind_house_talker(FreshHouseState &house,
    PodunkMickHouseTalkerOwner &owner,std::string &e){
 PodunkMickHouseNativeState n;
 if(!house_native_state(house,n,e))return false;
 auto &s=*state_;
 if(s.house_talker||owner.house()!=&house||n.callback_depth||n.callback_receiver||
    n.input_live||n.notifying||n.printer_owner||n.pending_messages||
    !n.objects.empty()||!n.wait_connections.empty()||
    !s.root_script.observes_closed_printer(house.presentation,e)||
    !s.dialogue.observes_closed_printer(house.presentation,e)||
    !s.native.observes_closed_printer(house.presentation,e)||
    !s.lifecycle.source_frame_closed(e))
  return fail(e,"House talker binding requires the exact closed source callback boundary");
 s.house_talker=&owner;e.clear();return true;
}
bool PodunkMickSession::rebind_house_programme(FreshHouseState &house,
    const PodunkDialogueProgrammeBinding &expected,PodunkDialogueProgrammePort &next,
    std::string &e){
 PodunkMickHouseNativeState n;
 if(!house_native_state(house,n,e))return false;
 auto &s=*state_;
 if(!s.house_talker||s.house_talker->house()!=&house||n.callback_depth||
    n.callback_receiver||n.input_live||n.notifying||n.printer_owner||
    n.pending_messages||!n.objects.empty()||!n.wait_connections.empty()||
    !s.lifecycle.source_frame_closed(e)||
    !s.root_script.admit_programme_rebind(s.programme,expected,next,house.presentation,e))
  return fail(e,"House programme transfer is outside the actual closed source boundary");
 // The old VM remains retired. Preserve both concrete Root and Options owners.
 if(!s.root_script.rebind_programme(s.programme,expected,next,e)||
    !s.options.rebind_programme(s.programme,expected,next,e))return false;
 e.clear();return true;
}
} // namespace encore::ctr
