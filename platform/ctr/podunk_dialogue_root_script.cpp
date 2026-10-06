#include "podunk_dialogue_root_script.hpp"
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
namespace encore::ctr {
using namespace upstream;
namespace {
bool reject(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t u() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p + at);
    at += 4;
    return v;
  }
  std::string text() {
    auto z = u();
    if (!ok || z > 4096 || at > n || z > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), z);
    at += z;
    size_t count = 0;
    if (s.empty() || s.find('\0') != s.npos || !utf8_count(s, count))
      ok = false;
    return s;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return h;
    }
    std::copy_n(p + at, 32, h.begin());
    at += 32;
    if (std::all_of(h.begin(), h.end(), [](uint8_t b) { return b == 0; }))
      ok = false;
    return h;
  }
  bool boolean() {
    auto v = u();
    if (v > 1)
      ok = false;
    return v != 0;
  }
  int64_t integer() {
    uint64_t v = u();
    v |= uint64_t(u()) << 32;
    int64_t q;
    std::memcpy(&q, &v, 8);
    return q;
  }
};
} // namespace
bool PodunkDialogueRootData::load(const uint8_t *p, size_t n, std::string &e) {
  if (!p || n < 128 || n > 65536 || std::memcmp(p, "ENCDROT1", 8) ||
      word(p + 8) != 1 || word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc32(p + 128, n - 128) || word(p + 24) != 0x454e004e ||
      word(p + 28) != 1 || word(p + 32) != 47 || !word(p + 36) || word(p + 124))
    return reject(e, "DialogueRoot header/version/capability/CRC rejected");
  PodunkDialogueRootData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  if (std::all_of(p + 40, p + 60, [](uint8_t b) { return !b; }) ||
      std::all_of(p + 60, p + 92, [](uint8_t b) { return !b; }) ||
      std::all_of(p + 92, p + 124, [](uint8_t b) { return !b; }))
    return reject(e, "DialogueRoot zero provenance rejected");
  Reader r{p, n};
  d.recipe_ = r.hash();
  d.script_ = r.hash();
  d.base_ = r.hash();
  d.options_ = r.u();
  auto reset = r.u();
  if (reset > uint32_t(std::numeric_limits<int32_t>::max()))
    r.ok = false;
  d.cursor_reset_ = r.ok ? int32_t(reset) : 0;
  d.base_source_ = r.text();
  std::set<std::string> actions;
  for (auto &v : d.actions_) {
    v = r.text();
    if (!actions.insert(v).second)
      r.ok = false;
  }
  d.actor_ = r.text();
  d.bullet_format_ = r.text();
  d.bullet_key_ = r.text();
  d.wait_method_ = r.text();
  d.name_method_ = r.text();
  d.open_animation_ = r.text();
  d.wait_signal_ = r.text();
  d.actor_sha_ = r.hash();
  for (auto &v : d.defaults_)
    v = r.boolean();
  d.phrase_ = r.text();
  d.response_ = r.integer();
  if (!r.ok || r.at != n || !d.options_ ||
      d.actor_.find("..") != d.actor_.npos || d.actor_.front() == '/' ||
      d.actor_.find(':') != d.actor_.npos ||
      d.bullet_format_.find("%s") == d.bullet_format_.npos ||
      d.wait_method_ == d.name_method_)
    return reject(e, "DialogueRoot typed source bindings rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
bool PodunkDialogueRootData::load_file(const char *path, std::string &e) {
  auto *f = path ? std::fopen(path, "rb") : nullptr;
  if (!f)
    return reject(e, "Cannot open dialogue root resource");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return reject(e, "Dialogue root seek failed");
  }
  auto n = std::ftell(f);
  if (n < 128 || n > 65536 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return reject(e, "Dialogue root size rejected");
  }
  std::vector<uint8_t> b(static_cast<size_t>(n));
  auto got = std::fread(b.data(), 1, b.size(), f);
  auto close = std::fclose(f);
  if (got != b.size() || close)
    return reject(e, "Dialogue root short read");
  return load(b.data(), b.size(), e);
}
bool PodunkDialogueRootOwner::initialize(
    const PodunkDialogueRootData &d, const FieldDialogueLifecycleData &life,
    const FieldNodeRecipeData &recipe, HousePresentation &printer,
    PodunkProgrammeHost &programme, PodunkDialogueHost &dialogue,
    DialogueChoices &choices, FieldNativeTimers &timers,
    const LocaleSelection *locale, PodunkDialogueRootEndpoints host,
    std::string &e) {
  if (!instances_.empty())
    return reject(e, "Live dialogue script cannot be rebound");
  if (!d.valid() || !life.valid() || !recipe.valid() ||
      !same(d.identity(), recipe.identity()) ||
      d.recipe_sha() != recipe.ir_sha256() || !host.tree || !host.frame ||
      !host.player_position || !host.animation_playing || !host.preload_actor ||
      !host.translate_bullet || !host.input_handled ||
      !host.printer_ownership || !host.name_rect_changed ||
      !host.prepare_options_labels || !host.hide_options_labels ||
      !host.release_actor || !host.talker_talking || !host.wait_connection)
    return reject(e, "DialogueRoot checked sources/actual endpoints missing");
  const auto *root = recipe.record(recipe.identity().scene_id);
  const auto *option = recipe.record(d.options_source());
  if (!root || root->script_sha != d.script_sha() || !option ||
      !option->script.empty())
    return reject(e, "DialogueRoot script/Options exact recipe mismatch");
  std::array<uint8_t, 32> base{};
  if (!recipe.source_hash(d.base_source(), base) || base != d.base_sha())
    return reject(e, "DialogueRoot inherited script source mismatch");
  data_ = &d;
  life_ = &life;
  recipe_ = &recipe;
  printer_ = &printer;
  programme_ = &programme;
  dialogue_ = &dialogue;
  choices_ = &choices;
  timers_ = &timers;
  locale_ = locale;
  host_ = std::move(host);
  return admit(life, recipe, printer, e);
}
bool PodunkDialogueRootOwner::admit(const FieldDialogueLifecycleData &life,
                                    const FieldNodeRecipeData &recipe,
                                    HousePresentation &printer,
                                    std::string &e) const {
  if (!data_ || !life_ || !recipe_ || life_ != &life || recipe_ != &recipe ||
      printer_ != &printer || !data_->valid() ||
      !same(data_->identity(), recipe.identity()) ||
      life.factory_ir_sha() != recipe.ir_sha256() ||
      life.factory_node_count() != recipe.records().size())
    return reject(e, "DialogueRoot source factory/printer ownership rejected");
  auto *option = recipe.record(data_->options_source());
  if (!option || option->id == recipe.identity().scene_id)
    return reject(e, "DialogueRoot actual Options child absent");
  e.clear();
  return true;
}
PodunkDialogueRootOwner::Instance *
PodunkDialogueRootOwner::live(FieldObjectId id, bool ready, std::string &e) {
  auto i = instances_.find(id);
  auto *tree = host_.tree ? host_.tree(id) : nullptr;
  auto *state = tree ? tree->state(id) : nullptr;
  if (i == instances_.end() || !state || !state->alive ||
      !i->second.state.constructed ||
      (ready && (!state->inside || !state->ready_notified ||
                 !i->second.state.ready || !i->second.state.entered))) {
    reject(e, "DialogueRoot actual source instance lifecycle absent");
    return nullptr;
  }
  return &i->second;
}
bool PodunkDialogueRootOwner::construct(FieldObjectId id,
                                        const FieldNodeDescriptor &source,
                                        HousePresentation &printer,
                                        std::string &e) {
  if (!data_ || !id || instances_.count(id) || printer_ != &printer ||
      source.id != recipe_->identity().scene_id ||
      source.script_sha != data_->script_sha())
    return reject(e, "DialogueRoot construction source/duplicate rejected");
  auto *tree = host_.tree(id);
  FieldIdentity identity;
  auto *actual = tree ? tree->descriptor(id) : nullptr;
  if (!actual || actual->id != source.id ||
      actual->script_sha != source.script_sha ||
      !tree->object_identity(id, identity) ||
      !same(identity, data_->identity()))
    return reject(
        e, "DialogueRoot construction has no actual ObjectDB source instance");
  Instance i;
  i.state.identity = identity;
  i.state.object = id;
  i.state.printer = &printer;
  i.state.constructed = true;
  const auto &v = data_->defaults();
  i.can_input = v[0];
  i.auto_advance = v[1];
  i.finished = v[2];
  i.stopped = v[3];
  i.box_shown = v[4];
  i.name_shown = v[5];
  i.phrase = data_->initial_phrase();
  i.response = data_->initial_response();
  if (!host_.translate_bullet(data_->bullet_format(), data_->bullet_key(),
                              i.bullet, e) ||
      i.bullet.empty())
    return reject(
        e, "DialogueRoot real translated constructor bullet unavailable");
  instances_.emplace(id, std::move(i));
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::checked_refs(
    Instance &i, const std::array<FieldObjectId, 12> &refs, std::string &e) {
  auto *tree = host_.tree(i.state.object);
  if (!tree)
    return reject(e, "DialogueRoot current Tree owner absent");
  for (size_t n = 0; n < refs.size(); ++n) {
    auto *r = life_->reference(uint32_t(n + 1));
    auto *s = tree->state(refs[n]);
    auto *d = tree->descriptor(refs[n]);
    FieldObjectId found = 0;
    if (!r || !s || !d || !s->alive || !s->inside || !s->ready_notified ||
        d->id != r->id || d->native_class != r->native_class ||
        !tree->get_node(i.state.object, life_->node(uint32_t(n + 1)), found,
                        e) ||
        found != refs[n])
      return reject(e, "DialogueRoot actual source onready reference mismatch");
  }
  const auto *option = recipe_->record(data_->options_source());
  FieldObjectId actual = 0;
  if (!option || !tree->get_node(i.state.object, option->path, actual, e) ||
      !tree->state(actual) || !tree->state(actual)->inside ||
      !tree->state(actual)->ready_notified)
    return reject(e, "DialogueRoot actual source Options onready absent");
  i.options = actual;
  i.state.references = refs;
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::phase(FieldObjectId id, FieldTreePhase phase,
                                    const std::array<FieldObjectId, 12> &refs,
                                    std::string &e) {
  auto *i = live(id, false, e);
  if (!i)
    return false;
  auto *tree = host_.tree(id);
  const auto *actual = tree->state(id);
  if (phase == FieldTreePhase::EnterScript) {
    if (!actual->inside || i->state.entered)
      return reject(e, "DialogueRoot source enter order rejected");
    i->state.entered = true;
    e.clear();
    return true;
  }
  if (phase == FieldTreePhase::ReadyScript) {
    if (!actual->inside || !actual->ready_notified || !i->state.entered ||
        i->state.ready || !checked_refs(*i, refs, e))
      return reject(e, "DialogueRoot source Ready/onready order rejected");
    if (!host_.preload_actor(data_->actor_source(), data_->actor_sha(),
                             i->actor_resource, e) ||
        !i->actor_resource)
      return reject(e,
                    "DialogueRoot source ActorChar ResourceLoader unavailable");
    PodunkDialogueWaitConnection connection;
    if (!timers_->state(i->state.references[9]) ||
        !host_.wait_connection(id, connection, e) ||
        connection.emitter != i->state.references[9] ||
        connection.receiver != id ||
        connection.signal != data_->wait_signal() ||
        connection.method != data_->wait_method() || connection.flags != 0)
      return reject(e, "DialogueRoot actual source timeout connection absent");
    i->state.ready = true;
    e.clear();
    return true;
  }
  if (phase == FieldTreePhase::ExitScript) {
    if (!i->state.entered)
      return reject(e, "DialogueRoot duplicate source exit");
    if (i->owns_printer && !host_.printer_ownership(id, *printer_, false, e))
      return false;
    i->owns_printer = false;
    i->running = false;
    i->state.entered = false;
    e.clear();
    return true;
  }
  if (!live(id, true, e) || !checked_refs(*i, refs, e))
    return false;
  PodunkDialogueFrame frame;
  if (!host_.frame(id, frame, e) || frame.phase != phase ||
      !std::isfinite(frame.delta) || frame.delta < 0 || frame.delta > 1e6 ||
      !tree->can_process(id, frame.tree_paused))
    return reject(e, "DialogueRoot real shared frame/pause rejected");
  if (phase == FieldTreePhase::Input)
    return input(*i, frame, e);
  if (phase != FieldTreePhase::Physics)
    return reject(e,
                  "DialogueRoot no source method for requested notification");
  if (!i->running && i->closing && i->finished) {
    auto *audio = dialogue_->audio(id);
    return audio && audio->stop(i->state.references[8], e);
  }
  if (!i->running || !i->owns_printer)
    return reject(
        e, "DialogueRoot physics without actual programme/printer ownership");
  Vec2 position;
  if (!host_.player_position(position, e) || !std::isfinite(position.x) ||
      !std::isfinite(position.y) ||
      !printer_->physics_frame(frame.delta, position))
    return reject(e, "DialogueRoot actual printer physics failed");
  i->stopped = printer_->dialogue_stopped();
  auto actor = programme_->programme().context().actor_object;
  if (actor && !host_.talker_talking(actor, printer_->talking(), e))
    return false;
  if (!flush_audio(*i, e))
    return false;
  if (!i->finished && printer_->dialogue_finished())
    return reject(
        e, "DialogueRoot same-printer synchronous finish callback absent");
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::state(FieldObjectId id,
                                    PodunkDialogueScriptState &out,
                                    std::string &e) const {
  auto i = instances_.find(id);
  auto *tree = host_.tree ? host_.tree(id) : nullptr;
  auto *actual = tree ? tree->state(id) : nullptr;
  if (i == instances_.end() || !actual || !actual->alive ||
      !tree->descriptor(id) ||
      tree->descriptor(id)->script_sha != data_->script_sha())
    return reject(e, "DialogueRoot observed source ObjectID absent");
  out = i->second.state;
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::begin(FieldObjectId id, uint32_t generation,
                                    std::string &e) {
  auto *i = live(id, true, e);
  if (!i || !generation || i->running ||
      programme_->programme().context().dialogue_object != id)
    return reject(e, "DialogueRoot programme generation/context rejected");
  if (!host_.printer_ownership(id, *printer_, true, e))
    return false;
  i->owns_printer = true;
  i->running = true;
  i->generation = generation;
  i->choices_shown = false;
  return phrase_begin(id, e);
}
bool PodunkDialogueRootOwner::phrase_begin(FieldObjectId id, std::string &e) {
  auto *i = live(id, true, e);
  if (!i || !i->running || !i->owns_printer ||
      programme_->programme().context().dialogue_object != id)
    return reject(e,
                  "DialogueRoot phrase reset lacks actual running programme");
  auto *visual = dialogue_->visual(id);
  if (!visual || !timers_->state(i->state.references[9]) ||
      !timers_->stop(i->state.references[9], e) ||
      !visual->cursor_index_property(i->state.references[5],
                                     data_->cursor_reset(), e))
    return false;
  i->can_input = data_->defaults()[0];
  i->auto_advance = data_->defaults()[1];
  i->finished = false;
  i->stopped = false;
  i->choices_shown = false;
  i->phrase_prepared = true;
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::presented_text(FieldObjectId id,
                                             const FieldProgrammeText &text,
                                             std::string &e) {
  auto *i = live(id, true, e);
  if (!i || !i->running || !i->phrase_prepared ||
      !printer_->dialogue_active() ||
      programme_->programme().context().dialogue_object != id)
    return reject(e,
                  "DialogueRoot presented text precedes source phrase reset");
  if (!i->box_shown) {
    auto *ui = dialogue_->ui(id);
    auto *tree = host_.tree(id);
    if (!ui || !tree ||
        !ui->play(i->state.references[6], data_->open_animation(), e) ||
        !tree->set_input_process(id, 0, true, e) ||
        !tree->set_process(id, true, true, e))
      return false;
  }
  if (!flush_audio(*i, e) || !dialogue_->presented_text(id, text, e))
    return false;
  i->phrase_prepared = false;
  i->finished = false;
  i->stopped = printer_->dialogue_stopped();
  i->box_shown = true;
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::flush_audio(Instance &i, std::string &e) {
  for (const auto &event : printer_->take_audio_events())
    if (!dialogue_->audio_event(i.state.object, event, e))
      return false;
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::finish(Instance &i, std::string &e) {
  i.finished = true;
  auto *tree = host_.tree(i.state.object);
  auto *visual = dialogue_->visual(i.state.object);
  if (!tree || !visual)
    return reject(e, "DialogueRoot source finish native owners absent");
  if (programme_->programme().scheduler().status() ==
      DialogueStatus::AwaitChoices) {
    if ((choices_->phase() == DialogueChoicesPhase::WaitingText &&
         !programme_->text_completed(e)) ||
        !choices_->active() ||
        !host_.prepare_options_labels(i.state.object, *choices_, locale_, e) ||
        !tree->set_visible(i.state.references[5], true, e) ||
        !visual->cursor_on(i.state.references[5], true, e) ||
        !visual->cursor_index(i.state.references[5], data_->cursor_reset(),
                              false, e) ||
        !tree->set_visible(i.options, true, e) ||
        !tree->set_visible(i.state.references[11], false, e))
      return false;
    i.choices_shown = true;
  } else {
    if (!tree->set_visible(i.state.references[5], false, e) ||
        !visual->cursor_on(i.state.references[5], false, e) ||
        !tree->set_visible(i.options, false, e))
      return false;
  }
  if (!timers_->state(i.state.references[9]))
    return reject(e, "DialogueRoot actual WaitTimer absent");
  if (timers_->time_left(i.state.references[9]) == 0 && i.can_input) {
    if (!tree->set_visible(i.state.references[11], true, e))
      return false;
    if (i.auto_advance)
      return reject(
          e,
          "DialogueRoot automatic phrase requires reviewed programme action");
  }
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::input(Instance &i,
                                    const PodunkDialogueFrame &frame,
                                    std::string &e) {
  const auto &a = data_->actions();
  if (!frame.pressed ||
      std::find(a.begin(), a.end(), frame.action) == a.end()) {
    e.clear();
    return true;
  }
  bool playing = false;
  if (!i.running && i.closing && i.finished) {
    if (!host_.animation_playing(i.state.references[6], playing, e) || !playing)
      return reject(
          e, "DialogueRoot closing input outside actual Close animation");
    return host_.input_handled(i.state.object, e);
  }
  if (!i.running || !i.owns_printer || !timers_->state(i.state.references[9]) ||
      !host_.animation_playing(i.state.references[6], playing, e))
    return reject(e, "DialogueRoot source input actual native gates absent");
  const bool next = frame.action == a[0] || frame.action == a[1],
             cancel = frame.action == a[1];
  if (!playing && timers_->time_left(i.state.references[9]) == 0 &&
      i.can_input) {
    if (!i.finished && !i.stopped) {
      printer_->input(true, cancel, true, false);
    } else if (next) {
      if (i.choices_shown) {
        auto *v = dialogue_->visual(i.state.object);
        const auto *cursor =
            v ? v->cursor_state(i.state.references[5]) : nullptr;
        if (!cursor || !choices_->active())
          return reject(e, "DialogueRoot actual Cursor selection absent");
        const auto index = cursor->index;
        auto *tree = host_.tree(i.state.object);
        if (!tree->set_visible(i.state.references[5], false, e) ||
            !host_.hide_options_labels(i.state.object, e))
          return false;
        i.choices_shown = false;
        if (!programme_->source_cursor_input(i.state.object, i.generation,
                                             index, next, cancel, e))
          return false;
      } else if (!programme_->input({0, 0, next, cancel}, 0, e))
        return false;
    }
  }
  return flush_audio(i, e) && host_.input_handled(i.state.object, e);
}
bool PodunkDialogueRootOwner::text_completed(FieldObjectId id, std::string &e) {
  auto *i = live(id, true, e);
  if (!i || !i->running || !i->owns_printer || i->finished ||
      !printer_->dialogue_finished() ||
      programme_->programme().context().dialogue_object != id)
    return reject(e, "DialogueRoot source finish callback is stale/duplicate");
  return finish(*i, e);
}
bool PodunkDialogueRootOwner::wait_timeout(FieldObjectId id, std::string &e) {
  auto *i = live(id, true, e);
  if (!i || !i->running || !timers_->state(i->state.references[9]) ||
      timers_->time_left(i->state.references[9]) != 0)
    return reject(e, "DialogueRoot timeout outside real WaitTimer completion");
  auto *tree = host_.tree(id);
  if (i->finished && !tree->set_visible(i->state.references[11], true, e))
    return false;
  if (i->auto_advance)
    return reject(
        e, "DialogueRoot autowait not present in admitted Mick programme");
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::native_step(FieldObjectId id,
                                          const FieldDialogueStep &step,
                                          std::string &e) {
  auto *i = live(id, true, e);
  bool source = false;
  if (life_)
    for (const auto &s : life_->steps())
      if (&s == &step)
        source = true;
  if (!i || !source || step.op != FieldDialogueOp::ResetPhrase || !i->running ||
      !i->finished || !i->owns_printer)
    return reject(e, "DialogueRoot unknown/stale native script source step");
  if (!host_.printer_ownership(id, *printer_, false, e))
    return false;
  i->phrase = step.text;
  i->owns_printer = false;
  i->running = false;
  i->closing = true;
  e.clear();
  return true;
}
bool PodunkDialogueRootOwner::deferred(const FieldDeferredMessage &m,
                                       std::string &e) {
  auto *i = live(m.object, true, e);
  if (!i || m.kind != FieldDeferredKind::Call || !m.args.empty())
    return reject(e, "DialogueRoot unknown source deferred signature");
  if (m.member == data_->wait_method())
    return wait_timeout(m.object, e);
  if (m.member == data_->name_method())
    return host_.name_rect_changed(i->state.references[2], e);
  return reject(e, "DialogueRoot unknown deferred script method rejected");
}
bool PodunkDialogueRootOwner::release(FieldObjectId id, std::string &e) {
  auto i = instances_.find(id);
  if (i == instances_.end())
    return reject(e, "DialogueRoot duplicate source release");
  if (i->second.state.entered)
    return reject(e, "DialogueRoot release before actual script exit");
  if (i->second.owns_printer &&
      !host_.printer_ownership(id, *printer_, false, e))
    return false;
  if (i->second.actor_resource &&
      !host_.release_actor(i->second.actor_resource, e))
    return false;
  instances_.erase(i);
  e.clear();
  return true;
}
} // namespace encore::ctr
