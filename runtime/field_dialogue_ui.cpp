#include "encore/field_dialogue_ui.hpp"
#include "encore/battle_entry.hpp"
#include "encore/field_dialogue_audio.hpp"
#include "encore/field_dialogue_visual.hpp"
#include "encore/field_native_timer.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace encore::upstream {
namespace {
bool finite(Vec2 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::abs(v.x) <= 1e6 &&
         std::abs(v.y) <= 1e6;
}
float cubic(float a, float b, float c, float d, float f) {
  return .5f * ((2 * b) + (-a + c) * f + (2 * a - 5 * b + 4 * c - d) * f * f +
                (-a + 3 * b - 3 * c + d) * f * f * f);
}
bool reject(std::string &e, const char *s) {
  e = s;
  return false;
}
} // namespace
FieldDialogueUiRuntime::Instance *
FieldDialogueUiRuntime::instance(FieldObjectId object) {
  auto id = owners_.find(object);
  if (id == owners_.end())
    return nullptr;
  auto i = instances_.find(id->second);
  return i == instances_.end() ? nullptr : &i->second;
}
const FieldDialogueUiRuntime::Instance *
FieldDialogueUiRuntime::instance(FieldObjectId object) const {
  auto id = owners_.find(object);
  if (id == owners_.end())
    return nullptr;
  auto i = instances_.find(id->second);
  return i == instances_.end() ? nullptr : &i->second;
}
bool FieldDialogueUiRuntime::initialize(const FieldDialogueUiData &data,
                                        const FieldNodeRecipeData &recipe,
                                        HouseView house,
                                        FieldNodeTreeRuntime &tree,
                                        FieldDialogueUiHost host,
                                        std::string &e) {
  if (data_ || !data.valid() || !recipe.valid() ||
      data.identity().scene_id != recipe.identity().scene_id ||
      data.identity().upstream_commit != recipe.identity().upstream_commit ||
      data.identity().source_sha256 != recipe.identity().source_sha256 ||
      data.recipe_ir_sha() != recipe.ir_sha256() || !host.font_minimum ||
      !host.canvas_enter || !host.canvas_exit || !host.emit || !host.rename ||
      !host.material_admit || !host.range_value || !host.control_enter ||
      !host.control_exit || !data.verify_house(house, e))
    return reject(
        e, "Dialogue UI actual checked owners/native endpoints incomplete");
  for (const auto &n : data.nodes()) {
    const auto *r = recipe.record(n.id);
    if (!r || r->parent != n.parent || r->ready != n.ready ||
        r->path != n.path || r->native_class != n.native_class ||
        r->script != n.script)
      return reject(e, "Dialogue UI native recipe source differs");
  }
  data_ = &data;
  recipe_ = &recipe;
  house_ = house;
  tree_ = &tree;
  host_ = std::move(host);
  e.clear();
  return true;
}
bool FieldDialogueUiRuntime::attach(FieldObjectId root, std::string &e) {
  return attach_owned(root, nullptr, e);
}
bool FieldDialogueUiRuntime::attach(
    FieldObjectId root, const FieldDialogueUiOwnershipRoster &roster,
    std::string &e) {
  return attach_owned(root, &roster, e);
}
bool FieldDialogueUiRuntime::attach_owned(
    FieldObjectId root, const FieldDialogueUiOwnershipRoster *roster,
    std::string &e) {
  if (!data_ || !root || instances_.count(root) || instances_.size() >= 64)
    return reject(e, "Dialogue UI actual factory ownership rejected");
  if (roster && (!roster->visual || !roster->audio || !roster->timers ||
                 !roster->visual->valid() || !roster->audio->valid() ||
                 !roster->timers->valid() ||
                 roster->visual->recipe_sha() != recipe_->ir_sha256() ||
                 roster->audio->recipe_sha() != recipe_->ir_sha256()))
    return reject(e,
                  "Dialogue UI complete typed native ownership roster absent");
  if (roster) {
    for (auto identity : {roster->visual->identity(), roster->audio->identity()})
      if (identity.scene_id != data_->identity().scene_id ||
          identity.upstream_commit != data_->identity().upstream_commit ||
          identity.source_sha256 != data_->identity().source_sha256)
        return reject(e, "Dialogue UI foreign ownership scene identity differs");
  }
  Instance i;
  i.root = root;
  const auto own_count = size_t(std::count_if(
      data_->nodes().begin(), data_->nodes().end(), [](const auto &node) {
        return node.kind != FieldDialogueUiKind::Pending;
      }));
  std::vector<FieldObjectId> pending{root};
  std::set<uint32_t> seen;
  while (!pending.empty()) {
    const auto id = pending.back();
    pending.pop_back();
    const auto *n = tree_->state(id);
    const auto *d = tree_->descriptor(id);
    const auto *source = d ? recipe_->record(d->id) : nullptr;
    FieldIdentity identity{};
    if (!n || !n->alive || !d || !source ||
        !tree_->object_identity(id, identity) ||
        identity.scene_id != data_->identity().scene_id ||
        identity.upstream_commit != data_->identity().upstream_commit ||
        identity.source_sha256 != data_->identity().source_sha256 ||
        d->native_class != source->native_class ||
        d->script_sha != source->script_sha || d->parent != source->parent ||
        d->ready != source->ready || !seen.insert(n->source).second ||
        owners_.count(id))
      return reject(e,
                    "Dialogue UI actual ObjectDB/factory provenance rejected");
    const auto *own = data_->node(n->source);
    const bool supported = own && own->kind != FieldDialogueUiKind::Pending;
    unsigned count = supported ? 1 : 0;
    if (roster) {
      const auto *visual = roster->visual->node(n->source);
      const auto *audio = roster->audio->node(n->source);
      const auto *timer = roster->timers->record(identity, n->source);
      if (visual) {
        if (visual->parent != d->parent || visual->ready != d->ready ||
            visual->native_class != d->native_class ||
            visual->script != d->script)
          return reject(e, "Dialogue UI foreign Visual source differs");
        ++count;
      }
      if (audio) {
        if (audio->parent != d->parent || audio->ready != d->ready ||
            d->native_class != "AudioStreamPlayer" || !d->script.empty())
          return reject(e, "Dialogue UI foreign Audio source differs");
        ++count;
      }
      if (timer) {
        if (d->native_class != "Timer" || timer->script_sha != d->script_sha)
          return reject(e, "Dialogue UI foreign Timer source differs");
        ++count;
      }
    }
    if (count != 1)
      return reject(
          e, "Dialogue UI native ownership coverage missing or overlapping");
    if (supported)
      i.ids.emplace(n->source, id);
    pending.insert(pending.end(), n->children.rbegin(), n->children.rend());
  }
  if (seen.size() !=
          (roster ? recipe_->records().size() : own_count) ||
      i.ids.size() != own_count ||
      !i.ids.count(data_->identity().scene_id) ||
      i.ids.at(data_->identity().scene_id) != root)
    return reject(e, "Dialogue UI actual full factory incomplete");
  for (const auto &c : data_->controls()) {
    const auto *r = recipe_->control(c.id);
    if (!r)
      return reject(e, "Dialogue UI exact source Control absent");
    FieldDialogueUiNativeControl state;
    state.object = i.ids.at(c.id);
    state.source = c.id;
    state.rect = {r->position.x, r->position.y, r->size.x, r->size.y};
    state.margins = r->margins;
    state.anchors = r->anchors;
    state.minimum = r->min_size;
    state.visible = bool(c.flags & 1);
    state.text = c.text;
    state.bbcode = c.bbcode;
    state.visible_characters = c.visible_characters;
    for (size_t j = 0; j < 5; ++j)
      state.range[j] = c.range[j];
    i.controls.emplace(c.id, std::move(state));
  }
  for (const auto &a : data_->animations())
    i.animations.emplace(a.owner, Animation{});
  for (const auto &entry : i.ids)
    owners_.emplace(entry.second, root);
  instances_.emplace(root, std::move(i));
  e.clear();
  return true;
}
bool FieldDialogueUiRuntime::admits_native(FieldObjectId object,
                                           std::string &e) const {
  auto *i = instance(object);
  const auto *s = tree_ ? tree_->state(object) : nullptr;
  const auto *d = tree_ ? tree_->descriptor(object) : nullptr;
  const auto *n = s && data_ ? data_->node(s->source) : nullptr;
  if (!i || !s || !s->alive || !d || !n ||
      n->kind == FieldDialogueUiKind::Pending ||
      d->native_class != n->native_class || d->ready != n->ready)
    return reject(e, "Dialogue UI native source outside 20 admitted owners");
  e.clear();
  return true;
}
bool FieldDialogueUiRuntime::enter_native(FieldObjectId object,
                                          std::string &e) {
  if (!admits_native(object, e))
    return false;
  auto *i = instance(object);
  auto *s = tree_->state(object);
  auto *n = data_->node(s->source);
  if (n->kind == FieldDialogueUiKind::CanvasLayer) {
    const auto *layer = recipe_->canvas_layer(n->id);
    if (!layer || !host_.canvas_enter(object, *layer, e))
      return false;
    i->canvas_entered = true;
  }
  auto found = i->controls.find(n->id);
  if (found != i->controls.end()) {
    const auto *control = recipe_->control(n->id);
    if (found->second.entered || !control ||
        !host_.control_enter(object, *control, e))
      return reject(e,
                    "Dialogue UI actual native Control registration rejected");
    found->second.entered = true;
  }
  return layout(*i, e);
}
bool FieldDialogueUiRuntime::exit_native(FieldObjectId object, std::string &e) {
  if (!admits_native(object, e))
    return false;
  auto *i = instance(object);
  const auto *state = tree_->state(object);
  const auto *n = data_->node(state->source);
  if (n->kind == FieldDialogueUiKind::CanvasLayer) {
    if (!i->canvas_entered || !host_.canvas_exit(object, e))
      return false;
    i->canvas_entered = false;
  }
  auto c = i->controls.find(n->id);
  if (c != i->controls.end()) {
    if (!c->second.entered || !host_.control_exit(object, e))
      return false;
    c->second.entered = false;
  }
  auto a = i->animations.find(n->id);
  if (a != i->animations.end()) {
    if (a->second.playing &&
        !tree_->remove_group(object, "idle_process_internal", e))
      return false;
    a->second.playing = false;
    a->second.data = nullptr;
    ++a->second.revision;
  }
  i->pending_sort.erase(n->id);
  e.clear();
  return true;
}
bool FieldDialogueUiRuntime::font_size(Instance &i, uint32_t source,
                                       std::string &e) {
  auto *c = data_->control(source);
  auto s = i.controls.find(source);
  if (!c || s == i.controls.end())
    return reject(e, "Dialogue UI source font/control absent");
  auto &v = s->second;
  const auto *r = recipe_->control(source);
  v.minimum = r->min_size;
  if (data_->node(source)->kind == FieldDialogueUiKind::VScrollBar) {
    v.minimum.x = std::max(v.minimum.x, c->minimum.x);
    v.minimum.y = std::max(v.minimum.y, c->minimum.y);
  }
  if (!c->font.empty()) {
    Vec2 size{};
    if (!host_.font_minimum(c->font, v.text, size, e) || !finite(size) ||
        size.x < 0 || size.y <= 0)
      return reject(
          e, "Dialogue UI actual source DynamicFont glyph/metrics rejected");
    if (data_->node(source)->kind == FieldDialogueUiKind::Label) {
      v.minimum.x = std::max(v.minimum.x, size.x);
      v.minimum.y = std::max(v.minimum.y, size.y);
    }
  }
  return true;
}
bool FieldDialogueUiRuntime::ready_native(FieldObjectId object,
                                          std::string &e) {
  if (!admits_native(object, e))
    return false;
  auto *i = instance(object);
  auto *s = tree_->state(object);
  auto *n = data_->node(s->source);
  if (!s->inside || !i->canvas_entered)
    return reject(e, "Dialogue UI actual native ENTER/Viewport absent");
  if (n->kind == FieldDialogueUiKind::AnimationPlayer) {
    e.clear();
    return true;
  }
  if (n->kind == FieldDialogueUiKind::CanvasLayer) {
    e.clear();
    return true;
  }
  auto &control = i->controls.at(n->id);
  if (!control.entered)
    return reject(e, "Dialogue UI actual native Control ENTER absent");
  if (control.ready)
    return reject(e, "Dialogue UI duplicate native Ready");
  if (!font_size(*i, n->id, e))
    return false;
  const auto *source_control = data_->control(n->id);
  if (!source_control->material.empty() &&
      !host_.material_admit(object, source_control->material,
                            source_control->texture, e))
    return false;
  control.ready = true;
  if (n->kind == FieldDialogueUiKind::RichTextLabel) {
    const auto *c = data_->control(n->id);
    if ((c->flags & 4) || (c->flags & 16))
      return reject(
          e, "Dialogue UI interactive/fit-height RichText branch pending");
    // Native _validate_line_caches keeps the actual constructor scrollbar
    // hidden when scroll_active=false, even though scroll_following remains
    // active.
    for (const auto &child : data_->nodes())
      if (child.parent == n->id &&
          child.kind == FieldDialogueUiKind::VScrollBar) {
        auto &bar = i->controls.at(child.id);
        bar.visible = false;
        if (!tree_->set_visible(bar.object, false, e))
          return false;
      }
  }
  if (!layout(*i, e))
    return false;
  if (n->kind == FieldDialogueUiKind::GridContainer ||
      n->kind == FieldDialogueUiKind::HBoxContainer)
    return queue_sort(*i, n->id, e);
  return true;
}
bool FieldDialogueUiRuntime::layout(Instance &i, std::string &e) {
  // Containers derive their combined minimum from actual visible source
  // children, after their native font metrics have been resolved, in postorder.
  for (auto ni = data_->nodes().rbegin(); ni != data_->nodes().rend(); ++ni) {
    if (ni->kind != FieldDialogueUiKind::HBoxContainer &&
        ni->kind != FieldDialogueUiKind::GridContainer)
      continue;
    auto &c = i.controls.at(ni->id);
    const auto *extra = data_->control(ni->id);
    std::vector<uint32_t> children;
    for (const auto &n : data_->nodes())
      if (n.parent == ni->id && i.controls.count(n.id) &&
          i.controls.at(n.id).visible)
        children.push_back(n.id);
    Vec2 minimum = recipe_->control(ni->id)->min_size;
    if (!children.empty()) {
      if (ni->kind == FieldDialogueUiKind::HBoxContainer) {
        float w = 0, h = 0;
        for (auto id : children) {
          const auto &v = i.controls.at(id);
          w += v.minimum.x;
          h = std::max(h, v.minimum.y);
        }
        minimum.x = std::max(minimum.x, w + extra->hseparation *
                                                float(children.size() - 1));
        minimum.y = std::max(minimum.y, h);
      } else {
        if (!extra->columns)
          return reject(e, "Dialogue UI source columns absent");
        size_t cols = std::min<size_t>(extra->columns, children.size()),
               rows = (children.size() + extra->columns - 1) / extra->columns;
        std::vector<float> w(cols, 0), h(rows, 0);
        for (size_t j = 0; j < children.size(); ++j) {
          const auto &v = i.controls.at(children[j]);
          w[j % extra->columns] =
              std::max(w[j % extra->columns], std::floor(v.minimum.x));
          h[j / extra->columns] =
              std::max(h[j / extra->columns], std::floor(v.minimum.y));
        }
        float width = extra->hseparation * float(cols - 1),
              height = extra->vseparation * float(rows - 1);
        for (float value : w)
          width += value;
        for (float value : h)
          height += value;
        minimum.x = std::max(minimum.x, width);
        minimum.y = std::max(minimum.y, height);
      }
    }
    c.minimum = minimum;
  }
  for (const auto &node : data_->nodes()) {
    auto it = i.controls.find(node.id);
    if (it == i.controls.end())
      continue;
    auto &c = it->second;
    const auto *r = recipe_->control(node.id);
    Vec2 parent = {data_->display().x, data_->display().y};
    auto p = i.controls.find(node.parent);
    if (p != i.controls.end())
      parent = {p->second.rect.z, p->second.rect.w};
    auto old = c.rect;
    float x = c.margins[0] + c.anchors[0] * parent.x,
          y = c.margins[1] + c.anchors[1] * parent.y,
          w = c.margins[2] + c.anchors[2] * parent.x - x,
          h = c.margins[3] + c.anchors[3] * parent.y - y;
    if (c.minimum.x > w) {
      if (r->grow[0] == 0)
        x += w - c.minimum.x;
      else if (r->grow[0] == 2)
        x += (w - c.minimum.x) * .5f;
      w = c.minimum.x;
    }
    if (c.minimum.y > h) {
      if (r->grow[1] == 0)
        y += h - c.minimum.y;
      else if (r->grow[1] == 2)
        y += (h - c.minimum.y) * .5f;
      h = c.minimum.y;
    }
    c.rect = {x, y, w, h};
    if (!finite({x, y}) || !finite({w, h}) || w < 0 || h < 0)
      return reject(e, "Dialogue UI native Control layout bound");
    if (old.x != x || old.y != y || old.z != w || old.w != h) {
      auto transform = tree_->state(c.object)->local;
      transform[2] = {x, y};
      if (!tree_->set_local(c.object, transform, e))
        return false;
      const auto *state = tree_->state(c.object);
      if (state->inside && !host_.emit(c.object, data_->rect_signal(), {}, e))
        return false;
    }
  }
  e.clear();
  return true;
}
bool FieldDialogueUiRuntime::queue_sort(Instance &i, uint32_t source,
                                        std::string &e) {
  auto *node = data_->node(source);
  auto found = i.ids.find(source);
  if (!node || found == i.ids.end())
    return reject(e, "Dialogue UI native container source absent");
  if (node->kind != FieldDialogueUiKind::GridContainer &&
      node->kind != FieldDialogueUiKind::HBoxContainer)
    return true;
  if (!tree_->state(found->second)->inside)
    return true;
  if (i.pending_sort.count(source))
    return true;
  FieldDeferredMessage m;
  m.object = found->second;
  m.kind = FieldDeferredKind::Call;
  m.member = "_sort_children";
  if (!tree_->enqueue(std::move(m), e))
    return false;
  i.pending_sort.insert(source);
  return true;
}
bool FieldDialogueUiRuntime::position(Instance &i, uint32_t id, Vec2 position,
                                      std::string &e) {
  auto it = i.controls.find(id);
  if (it == i.controls.end() || !finite(position))
    return reject(e, "Dialogue UI animation target property rejected");
  auto &v = it->second;
  const auto dx = position.x - v.rect.x, dy = position.y - v.rect.y;
  v.margins[0] += dx;
  v.margins[2] += dx;
  v.margins[1] += dy;
  v.margins[3] += dy;
  return layout(i, e);
}
bool FieldDialogueUiRuntime::scalar_sample(const FieldDialogueClip &c,
                                           float time, Vec2 &out,
                                           std::string &e) const {
  if (c.keys.empty() || !std::isfinite(time) || time < 0)
    return reject(e, "Dialogue UI animation sample rejected");
  size_t index = 0;
  while (index + 1 < c.keys.size() && c.keys[index + 1].time <= time)
    ++index;
  const auto &from = c.keys[index];
  out = from.value;
  if (index + 1 < c.keys.size()) {
    const auto &to = c.keys[index + 1];
    const float f =
        battle_ease(float((time - from.time) / (to.time - from.time)),
                    float(from.transition));
    if (c.interpolation == 2) {
      auto before = c.keys[index ? index - 1 : 0].value,
           after = c.keys[std::min(index + 2, c.keys.size() - 1)].value;
      out = {cubic(before.x, from.value.x, to.value.x, after.x, f),
             cubic(before.y, from.value.y, to.value.y, after.y, f)};
    } else
      out = {from.value.x + (to.value.x - from.value.x) * f,
             from.value.y + (to.value.y - from.value.y) * f};
  }
  return finite(out) || reject(e, "Dialogue UI animation result nonfinite");
}
bool FieldDialogueUiRuntime::play(FieldObjectId actor, std::string_view name,
                                  std::string &e) {
  if (!admits_native(actor, e))
    return false;
  auto *i = instance(actor);
  auto *state = tree_->state(actor);
  auto found = i->animations.find(state->source);
  if (found == i->animations.end() || !state->inside || !state->ready_notified)
    return reject(e, "Dialogue UI actual AnimationPlayer Ready absent");
  const FieldDialogueUiAnimation *clip = nullptr;
  for (const auto &a : data_->animations())
    if (a.owner == state->source && a.clip.name == name)
      clip = &a;
  if (!clip)
    return reject(e, "Dialogue UI animation outside reviewed source clips");
  auto &player = found->second;
  player.data = clip;
  player.time = 0;
  player.playing = true;
  ++player.revision;
  if (!tree_->add_group(actor, "idle_process_internal", e))
    return false;
  // Original play() emits started synchronously; property application belongs
  // to native INTERNAL_PROCESS, not a made-up Ready/animation-complete
  // callback.
  return host_.emit(actor, data_->started_signal(), clip->clip.name, e);
}
bool FieldDialogueUiRuntime::animation_process(FieldObjectId actor, double dt,
                                               std::string &e) {
  if (!admits_native(actor, e) || !std::isfinite(dt) || dt < 0 || dt > 1e6)
    return reject(e, "Dialogue UI native animation delta rejected");
  auto *i = instance(actor);
  auto *state = tree_->state(actor);
  auto it = i->animations.find(state->source);
  if (it == i->animations.end() || !state->inside)
    return reject(e, "Dialogue UI native internal-process owner absent");
  auto &player = it->second;
  if (!player.playing) {
    e.clear();
    return true;
  }
  const auto *c = player.data;
  const auto revision = player.revision;
  const float previous = player.time, length = float(c->clip.length);
  player.time = std::min(length, float(player.time + float(dt)));
  Vec2 value{};
  if (!scalar_sample(c->clip, player.time, value, e) ||
      !position(*i, c->target, value, e))
    return false;
  // Rect-change listeners can synchronously play a replacement clip.
  // Preserve that actual new playback instead of finishing/unregistering it.
  if (player.revision != revision) {
    e.clear();
    return true;
  }
  if (previous < length && player.time == length) {
    player.playing = false;
    if (!tree_->remove_group(actor, "idle_process_internal", e))
      return false;
    return host_.emit(actor, data_->finished_signal(), c->clip.name, e);
  }
  e.clear();
  return true;
}
bool FieldDialogueUiRuntime::native_step(const FieldDialogueStep &step,
                                         FieldObjectId actor, std::string &e) {
  if (!admits_native(actor, e))
    return false;
  auto *i = instance(actor);
  const auto *s = tree_->state(actor);
  auto found = i->controls.find(s->source);
  if (found == i->controls.end())
    return reject(e, "Dialogue UI native property requires actual Control");
  auto &c = found->second;
  switch (step.op) {
  case FieldDialogueOp::TextClear:
  case FieldDialogueOp::BulletClear:
    c.text.clear();
    c.bbcode.clear();
    c.parsed_lines.clear();
    break;
  case FieldDialogueOp::VisibleCharacters:
    if (step.value < 0 || step.value > 1000000 ||
        std::floor(step.value) != step.value)
      return reject(e, "Dialogue UI source character count rejected");
    c.visible_characters = int32_t(step.value);
    break;
  case FieldDialogueOp::TextHide:
  case FieldDialogueOp::ArrowHide:
    c.visible = false;
    return tree_->set_visible(actor, false, e);
  default:
    return reject(e, "Dialogue UI native method requires separate checked "
                     "script/input/audio consumer");
  }
  if (!font_size(*i, s->source, e) || !layout(*i, e))
    return false;
  if ((step.op == FieldDialogueOp::TextClear ||
       step.op == FieldDialogueOp::BulletClear) &&
      data_->node(s->source)->kind == FieldDialogueUiKind::RichTextLabel)
    return update_scroll(*i, s->source, 1, e);
  return true;
}
bool FieldDialogueUiRuntime::sync_text(FieldObjectId root,
                                       const HousePresentation &p,
                                       std::string &e) {
  auto *i = instance(root);
  if (!i || i->root != root || !i->canvas_entered)
    return reject(e, "Dialogue UI actual canvas/text owner absent");
  const auto text = p.dialogue_pose();
  i->text_pose = text;
  i->text_bound = true;
  auto role = [&](FieldDialogueUiRole r) -> FieldDialogueUiNativeControl & {
    return i->controls.at(data_->role(r)->id);
  };
  auto &name = role(FieldDialogueUiRole::Name);
  name.text = text.speaker;
  auto &body = role(FieldDialogueUiRole::Text);
  const auto full = p.source_text_state();
  auto &bullet = role(FieldDialogueUiRole::Bullet);
  const auto previous_body = body.text, previous_bullet = bullet.text;
  body.text.clear();
  bullet.text.clear();
  body.bbcode.clear();
  bullet.bbcode.clear();
  body.parsed_lines = full.lines;
  for (const auto &line : full.lines) {
    body.text += '\n';
    bullet.text += '\n';
    for (char32_t cp : line.cells) {
      if (!cp)
        body.text += data_->delay_marker();
      else
        encore::utf8_append(uint32_t(cp), body.text);
    }
    if (line.wait)
      body.text += data_->wait_marker();
    if (line.bullet)
      bullet.text += text.bullet;
  }
  body.text.append(full.choice_rows, '\n');
  bullet.text.append(full.choice_rows, '\n');
  body.visible_characters =
      int32_t(std::min(p.visible_characters(), uint32_t(1000000)));
  body.visible = text.text_visible;
  // The checked existing HousePresentation owns source text tags/printing and
  // original name-size SceneTreeTween. This copies its native property result,
  // never advances a second text/RNG/tween clock.
  auto &tag = role(FieldDialogueUiRole::NameBox);
  tag.margins[2] = tag.margins[0] + text.name.z;
  if (!font_size(*i, name.source, e) ||
      !tree_->set_visible(body.object, body.visible, e) || !layout(*i, e))
    return false;
  if (body.text != previous_body &&
      !update_scroll(*i, body.source,
                     uint32_t(full.lines.size()) + full.choice_rows + 1, e))
    return false;
  if (bullet.text != previous_bullet &&
      !update_scroll(*i, bullet.source,
                     uint32_t(full.lines.size()) + full.choice_rows + 1, e))
    return false;
  e.clear();
  return true;
}
bool FieldDialogueUiRuntime::update_scroll(Instance &i, uint32_t source,
                                           uint32_t lines, std::string &e) {
  const auto *c = data_->control(source);
  auto found = i.controls.find(source);
  if (!c || found == i.controls.end() ||
      data_->node(source)->kind != FieldDialogueUiKind::RichTextLabel ||
      lines > 1000000)
    return reject(e, "Dialogue UI actual RichText cache owner rejected");
  const float total =
      float(lines) * (c->font_height + c->line_separation) + c->style.y;
  for (const auto &node : data_->nodes())
    if (node.parent == source && node.kind == FieldDialogueUiKind::VScrollBar) {
      auto &bar = i.controls.at(node.id);
      const auto *actual = tree_->state(bar.object);
      if (!actual || !actual->alive)
        return reject(e, "Dialogue UI actual constructor scrollbar deleted");
      auto value = [&](double requested) -> bool {
        const auto before = bar.range[4];
        if (bar.range[2] > 0)
          requested = std::floor(requested / bar.range[2] + .5) * bar.range[2];
        requested = std::max(bar.range[0],
                             std::min(requested, bar.range[1] - bar.range[3]));
        bar.range[4] = requested;
        if (actual->inside && before != requested)
          return host_.range_value(bar.object, data_->value_signal(), requested,
                                   e);
        return true;
      };
      // Actual Range setters clamp/emit value_changed before changed, even when
      // the requested max/page itself matches its preceding property value.
      bar.range[1] = total;
      if (!value(bar.range[4]))
        return false;
      if (actual->inside &&
          !host_.emit(bar.object, data_->range_signal(), {}, e))
        return false;
      bar.range[3] = found->second.rect.w;
      if (!value(bar.range[4]))
        return false;
      if (actual->inside &&
          !host_.emit(bar.object, data_->range_signal(), {}, e))
        return false;
      if ((c->flags & 8) && !value(total - bar.range[3]))
        return false;
      e.clear();
      return true;
    }
  return reject(e, "Dialogue UI original RichText scrollbar absent");
}
bool FieldDialogueUiRuntime::option_children(
    Instance &i, std::array<FieldObjectId, 6> &objects, std::string &e) const {
  const auto *source = data_->role(FieldDialogueUiRole::Options);
  const auto found = source ? i.controls.find(source->id) : i.controls.end();
  const auto *grid = found == i.controls.end()
                         ? nullptr
                         : tree_->state(found->second.object);
  if (!source || !grid || !grid->alive || !grid->inside ||
      !grid->ready_notified || !found->second.entered || !found->second.ready ||
      grid->source != source->id || grid->children.size() != objects.size())
    return reject(e, "Dialogue option Grid actual native Ready absent");
  for (size_t j = 0; j < objects.size(); ++j) {
    const auto *label = data_->role(FieldDialogueUiRole(
        uint32_t(FieldDialogueUiRole::Option1) + j));
    const auto c = label ? i.controls.find(label->id) : i.controls.end();
    const auto *state = c == i.controls.end()
                            ? nullptr
                            : tree_->state(c->second.object);
    if (!label || label->kind != FieldDialogueUiKind::Label ||
        label->parent != source->id || !state || !state->alive ||
        !state->inside || !state->ready_notified || !c->second.entered ||
        !c->second.ready || state->source != label->id ||
        state->parent != grid->object || grid->children[j] != state->object)
      return reject(e, "Dialogue option Label source child/Ready differs");
    objects[j] = state->object;
  }
  e.clear();
  return true;
}
bool FieldDialogueUiRuntime::prepare_choice_labels(
    FieldObjectId root, const DialogueChoices &choices,
    const LocaleSelection *locale, std::string &e) {
  auto *i = instance(root);
  const auto *g = choices.group();
  const auto *d = choices.data();
  const auto *grid = data_ ? data_->role(FieldDialogueUiRole::Options) : nullptr;
  const auto *control = grid ? data_->control(grid->id) : nullptr;
  std::array<FieldObjectId, 6> objects{};
  if (!i || i->root != root || !choices.active() || !g || !d || !d->valid() ||
      g->options.empty() || g->options.size() > 3 || !control ||
      d->columns() != control->columns || d->child_count() != objects.size() ||
      !option_children(*i, objects, e))
    return reject(e, "Dialogue source option preparation outside ready Mick scope");
  // Resolve all real glyph metrics before changing any source Label. This
  // rejects missing resources/unsupported text without partial visibility.
  std::array<std::string, 6> text;
  std::array<Vec2, 6> minimum{};
  for (size_t j = 0; j < g->options.size(); ++j) {
    const auto *state = tree_->state(objects[j]);
    const auto *c = data_->control(state->source);
    const auto *r = recipe_->control(state->source);
    Vec2 measured{};
    text[j] = locale
                  ? std::string(locale->text(g->options[j].translation_key).text)
                  : g->options[j].text;
    if (!c || !r || c->font.empty() ||
        !host_.font_minimum(c->font, text[j], measured, e) ||
        !finite(measured) || measured.x < 0 || measured.y <= 0)
      return reject(e, "Dialogue option source font/glyph admission rejected");
    minimum[j] = {std::max(r->min_size.x, measured.x),
                  std::max(r->min_size.y, measured.y)};
  }
  // Source first hides all children; text/set_name/show then occurs only for
  // the reviewed visibleOptions in their original dictionary order.
  for (auto object : objects) {
    i->controls.at(tree_->state(object)->source).visible = false;
    if (!tree_->set_visible(object, false, e))
      return false;
  }
  for (size_t j = 0; j < g->options.size(); ++j) {
    auto &c = i->controls.at(tree_->state(objects[j])->source);
    c.text = text[j];
    c.minimum = minimum[j];
    if (!host_.rename(c.object, c.text, e))
      return false;
    c.visible = true;
    if (!tree_->set_visible(c.object, true, e))
      return false;
  }
  return layout(*i, e) && queue_sort(*i, grid->id, e);
}
bool FieldDialogueUiRuntime::hide_choice_labels(FieldObjectId root,
                                               std::string &e) {
  auto *i = instance(root);
  std::array<FieldObjectId, 6> objects{};
  if (!i || i->root != root || !option_children(*i, objects, e))
    return reject(e, "Dialogue source option hide owner absent");
  for (auto object : objects) {
    i->controls.at(tree_->state(object)->source).visible = false;
    if (!tree_->set_visible(object, false, e))
      return false;
  }
  const auto *grid = data_->role(FieldDialogueUiRole::Options);
  return layout(*i, e) && queue_sort(*i, grid->id, e);
}
bool FieldDialogueUiRuntime::sync_choices(FieldObjectId root,
                                          const DialogueChoices &choices,
                                          const LocaleSelection *locale,
                                          std::string &e) {
  auto *i = instance(root);
  if (!i || i->root != root)
    return reject(e, "Dialogue UI source options owner absent");
  const auto *g = choices.group();
  const auto *d = choices.data();
  const auto visible = choices.pose().visible;
  if (visible &&
      (!g || !d || !d->valid() || g->options.size() > 3 ||
       d->columns() !=
           data_->control(data_->role(FieldDialogueUiRole::Options)->id)
               ->columns))
    return reject(
        e, "Dialogue UI source choice layout outside admitted Mick scope");
  for (unsigned j = 0; j < 6; ++j) {
    const auto *source = data_->role(
        FieldDialogueUiRole(uint32_t(FieldDialogueUiRole::Option1) + j));
    auto &c = i->controls.at(source->id);
    c.visible = visible && g && j < g->options.size();
    if (c.visible) {
      c.text =
          locale ? std::string(locale->text(g->options[j].translation_key).text)
                 : g->options[j].text;
      if (!host_.rename(c.object, c.text, e) || !font_size(*i, c.source, e))
        return false;
    }
    if (!tree_->set_visible(c.object, c.visible, e))
      return false;
  }
  auto &grid = i->controls.at(data_->role(FieldDialogueUiRole::Options)->id);
  grid.visible = visible;
  if (!tree_->set_visible(grid.object, visible, e))
    return false;
  if (!layout(*i, e))
    return false;
  return queue_sort(*i, grid.source, e);
}
bool FieldDialogueUiRuntime::sort_children(FieldObjectId actor,
                                           std::string &e) {
  if (!admits_native(actor, e))
    return false;
  auto *i = instance(actor);
  auto *state = tree_->state(actor);
  auto *n = data_->node(state->source);
  const auto *extra = data_->control(n->id);
  if (n->kind != FieldDialogueUiKind::GridContainer &&
      n->kind != FieldDialogueUiKind::HBoxContainer)
    return reject(e, "Dialogue UI actual deferred container type rejected");
  i->pending_sort.erase(n->id);
  auto &container = i->controls.at(n->id);
  std::vector<uint32_t> children;
  for (auto object : state->children) {
    auto *s = tree_->state(object);
    if (s && i->controls.count(s->source) && i->controls.at(s->source).visible)
      children.push_back(s->source);
  }
  if (children.empty()) {
    e.clear();
    return true;
  }
  if (n->kind == FieldDialogueUiKind::HBoxContainer) {
    float minimum = 0;
    unsigned expanded = 0;
    for (auto id : children) {
      minimum += i->controls.at(id).minimum.x;
      if (recipe_->control(id)->size_flags[0] & 2)
        ++expanded;
    }
    minimum += extra->hseparation * float(children.size() - 1);
    float leftover = std::max(0.f, container.rect.z - minimum), x = 0;
    for (auto id : children) {
      auto &c = i->controls.at(id);
      float w = c.minimum.x;
      if (recipe_->control(id)->size_flags[0] & 2)
        w += expanded ? leftover / expanded : 0;
      const float h = std::max(container.rect.w, c.minimum.y);
      c.margins = {x, 0, x + w, h};
      c.anchors = {};
      x += w + extra->hseparation;
    }
  } else if (n->kind == FieldDialogueUiKind::GridContainer) {
    const auto cols = extra->columns;
    if (!cols)
      return reject(e, "Dialogue UI source grid columns absent");
    const auto used = std::min<size_t>(cols, children.size());
    const auto rows = (children.size() + cols - 1) / cols;
    std::vector<float> width(cols, 0), height(rows, 0);
    std::vector<bool> expand(cols, false);
    for (size_t j = 0; j < children.size(); ++j) {
      auto id = children[j];
      width[j % cols] =
          std::max(width[j % cols], std::floor(i->controls.at(id).minimum.x));
      height[j / cols] =
          std::max(height[j / cols], std::floor(i->controls.at(id).minimum.y));
      expand[j % cols] =
          expand[j % cols] || bool(recipe_->control(id)->size_flags[0] & 2);
    }
    for (size_t j = children.size(); j < cols; ++j)
      expand[j] = true;
    float remaining = container.rect.z - extra->hseparation * float(used - 1);
    for (size_t j = 0; j < cols; ++j)
      if (!expand[j])
        remaining -= width[j];
    for (;;) {
      size_t count = 0, widest = 0;
      for (size_t j = 0; j < cols; ++j)
        if (expand[j]) {
          if (!count || width[j] > width[widest])
            widest = j;
          ++count;
        }
      if (!count)
        break;
      if (remaining / count >= width[widest]) {
        const float whole = std::floor(remaining / count);
        int pixels = int(remaining - whole * count);
        for (size_t j = 0; j < cols; ++j)
          if (expand[j])
            width[j] = whole + (pixels-- > 0 ? 1 : 0);
        break;
      }
      expand[widest] = false;
      remaining -= width[widest];
    }
    for (size_t j = 0; j < children.size(); ++j) {
      float x = 0, y = 0;
      for (size_t col = 0; col < j % cols; ++col)
        x += width[col] + extra->hseparation;
      for (size_t row = 0; row < j / cols; ++row)
        y += height[row] + extra->vseparation;
      auto &c = i->controls.at(children[j]);
      const auto *r = recipe_->control(c.source);
      float w = width[j % cols], h = height[j / cols];
      if (!(r->size_flags[0] & 1)) {
        if (r->size_flags[0] & 4)
          x += std::floor((w - c.minimum.x) * .5f);
        else if (r->size_flags[0] & 8)
          x += w - c.minimum.x;
        w = c.minimum.x;
      }
      if (!(r->size_flags[1] & 1)) {
        if (r->size_flags[1] & 4)
          y += std::floor((h - c.minimum.y) * .5f);
        else if (r->size_flags[1] & 8)
          y += h - c.minimum.y;
        h = c.minimum.y;
      }
      c.margins = {x, y, x + w, y + h};
      c.anchors = {};
    }
  } else
    return reject(e,
                  "Dialogue UI deferred container method outside source scope");
  return layout(*i, e);
}
const FieldDialogueUiNativeControl *
FieldDialogueUiRuntime::control(FieldObjectId id) const {
  auto *i = instance(id);
  const auto *s = tree_ ? tree_->state(id) : nullptr;
  if (!i || !s)
    return nullptr;
  auto it = i->controls.find(s->source);
  return it == i->controls.end() ? nullptr : &it->second;
}
bool FieldDialogueUiRuntime::pose(FieldObjectId root, WorldDialoguePose &out,
                                  std::string &e) const {
  const auto *i = instance(root);
  const auto *s = tree_ ? tree_->state(root) : nullptr;
  if (!i || i->root != root || !s || !s->alive || !s->inside ||
      !s->ready_notified || !i->text_bound)
    return reject(e, "Dialogue UI source script Ready/text bind not complete");
  out = i->text_pose;
  const auto &box = i->controls.at(data_->role(FieldDialogueUiRole::Box)->id),
             &tag =
                 i->controls.at(data_->role(FieldDialogueUiRole::NameBox)->id),
             &clip = i->controls.at(data_->role(FieldDialogueUiRole::Clip)->id);
  out.box = box.rect;
  out.name = {box.rect.x + tag.rect.x, box.rect.y + tag.rect.y, tag.rect.z,
              tag.rect.w};
  out.clip = {box.rect.x + clip.rect.x, box.rect.y + clip.rect.y, clip.rect.z,
              clip.rect.w};
  const auto &body = i->controls.at(data_->role(FieldDialogueUiRole::Text)->id),
             &bullet =
                 i->controls.at(data_->role(FieldDialogueUiRole::Bullet)->id),
             &hbox = i->controls.at(data_->role(FieldDialogueUiRole::HBox)->id),
             &name = i->controls.at(data_->role(FieldDialogueUiRole::Name)->id),
             &name_clip =
                 i->controls.at(data_->role(FieldDialogueUiRole::NameClip)->id);
  out.text_layout = {hbox.rect.x + body.rect.x, hbox.rect.y + body.rect.y,
                     body.rect.z, body.rect.w};
  out.bullet_layout = {hbox.rect.x + bullet.rect.x, hbox.rect.y + bullet.rect.y,
                       bullet.rect.z, bullet.rect.w};
  const auto *name_source = data_->control(name.source);
  const auto fm = house_.parameter(HouseParameter::FontMetrics);
  float name_x = name.rect.x + name_clip.rect.x,
        name_y = name.rect.y + name_clip.rect.y;
  if (name_source->valign == 1)
    name_y += std::floor((name.rect.w - name_source->font_height) * .5f);
  else if (name_source->valign == 2)
    name_y += name.rect.w - name_source->font_height;
  else if (name_source->valign != 0)
    return reject(e, "Dialogue UI source name fill valign pending");
  // The borrowed generic House draw centers its name label. Cancel that generic
  // centering so this native Control's actual source valign remains
  // authoritative.
  out.name_label = {name_x, name_y - (name.rect.w - fm.x) * .5f, name.rect.z,
                    name.rect.w};
  out.text_visible = body.visible;
  out.cursor_visible = false;
  e.clear();
  return true;
}
std::vector<FieldDialogueUiNativeControl>
FieldDialogueUiRuntime::option_labels(FieldObjectId root) const {
  std::vector<FieldDialogueUiNativeControl> out;
  auto *i = instance(root);
  if (!i || i->root != root)
    return out;
  const auto &grid =
      i->controls.at(data_->role(FieldDialogueUiRole::Options)->id);
  const auto &box = i->controls.at(data_->role(FieldDialogueUiRole::Box)->id);
  // Source may prepare Label visibility while Options and its Canvas parents
  // remain hidden. Local Label flags alone are not a drawable Canvas receipt.
  if (!tree_->visible_in_tree(grid.object))
    return out;
  for (unsigned j = 0; j < 6; ++j) {
    auto c = i->controls.at(data_
                                ->role(FieldDialogueUiRole(
                                    uint32_t(FieldDialogueUiRole::Option1) + j))
                                ->id);
    if (!c.visible || !tree_->visible_in_tree(c.object))
      continue;
    c.rect.x += grid.rect.x + box.rect.x;
    c.rect.y += grid.rect.y + box.rect.y;
    out.push_back(std::move(c));
  }
  return out;
}
bool FieldDialogueUiRuntime::release(FieldObjectId root, std::string &e) {
  auto it = instances_.find(root);
  if (it == instances_.end())
    return reject(e, "Dialogue UI release requires original factory root");
  if (it->second.canvas_entered && !host_.canvas_exit(root, e))
    return false;
  for (const auto &id : it->second.ids)
    owners_.erase(id.second);
  instances_.erase(it);
  e.clear();
  return true;
}
} // namespace encore::upstream
