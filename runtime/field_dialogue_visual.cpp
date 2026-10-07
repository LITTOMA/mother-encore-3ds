#include "encore/field_dialogue_visual.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool reject(std::string &e, const char *s) {
  e = s;
  return false;
}
bool finite(Vec2 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::abs(v.x) < 1000000 &&
         std::abs(v.y) < 1000000;
}
float ease(float x, float c) {
  x = std::clamp(x, 0.0f, 1.0f);
  if (c > 0)
    return c < 1 ? 1 - std::pow(1 - x, 1 / c) : std::pow(x, c);
  if (c < 0)
    return x < .5f ? std::pow(x * 2, -c) * .5f
                   : (1 - std::pow(1 - (x - .5f) * 2, -c)) * .5f + .5f;
  return 0;
}
Vec2 transform(const FieldTransform &m, Vec2 p) {
  return {m[0].x * p.x + m[1].x * p.y + m[2].x,
          m[0].y * p.x + m[1].y * p.y + m[2].y};
}
} // namespace
const FieldDialogueVisualNode *
FieldDialogueVisualRuntime::node(FieldObjectId id) const {
  if (!tree_ || !data_)
    return nullptr;
  auto *s = tree_->state(id);
  return s && s->alive && objects_.count(s->source) &&
                 objects_.at(s->source) == id
             ? data_->node(s->source)
             : nullptr;
}
FieldObjectId FieldDialogueVisualRuntime::object(uint32_t source) const {
  auto i = objects_.find(source);
  return i == objects_.end() ? 0 : i->second;
}
const FieldDialogueCursorState *
FieldDialogueVisualRuntime::cursor_state(FieldObjectId id) const {
  auto i = cursors_.find(id);
  return i == cursors_.end() ? nullptr : &i->second;
}
FieldDialogueCursorState *FieldDialogueVisualRuntime::cursor(FieldObjectId id,
                                                             std::string &e) {
  auto i = cursors_.find(id);
  if (i == cursors_.end() || !node(id)) {
    reject(e, "Dialogue Cursor actual lifetime rejected");
    return nullptr;
  }
  return &i->second;
}
bool FieldDialogueVisualRuntime::initialize(
    const FieldDialogueVisualData &d, const FieldNodeRecipeData &r,
    FieldNodeTreeRuntime &t, FieldDialogueVisualHost h, SourceRandom &rng,
    FieldGameCameraHost camera_host, FieldCameraArrowsHost arrow_host,
    std::string &e) {
  if (data_ || !d.valid() || !r.valid() || d.recipe_sha() != r.ir_sha256() ||
      d.identity().scene_id != r.identity().scene_id ||
      d.identity().upstream_commit != r.identity().upstream_commit ||
      d.identity().source_sha256 != r.identity().source_sha256 || !h.native ||
      !h.menu || !h.controls || !h.observe || !h.connect || !h.emit ||
      !h.publish || !h.timer_left || !h.timer_start || !h.sound ||
      !h.tween_position || !h.finish_tween || !h.await_idle || !h.global)
    return reject(
        e, "Dialogue visuals real native/signal/Timer/tween owners incomplete");
  for (const auto &n : d.nodes()) {
    auto *p = r.record(n.id);
    if (!p || p->parent != n.parent || p->ready != n.ready ||
        p->path != n.path || p->native_class != n.native_class ||
        p->script != n.script || p->pause != n.pause ||
        p->priority != n.priority)
      return reject(e, "Dialogue visual recipe node differs");
  }
  if (!camera_.initialize(d.camera(), rng, std::move(camera_host), e) ||
      !arrows_.initialize(d.arrows(), std::move(arrow_host), e))
    return false;
  data_ = &d;
  recipe_ = &r;
  tree_ = &t;
  host_ = std::move(h);
  return true;
}
bool FieldDialogueVisualRuntime::attach(FieldObjectId root, std::string &e) {
  if (!data_ || root_ || !root)
    return reject(e, "Dialogue visual duplicate factory rejected");
  auto *s = tree_->state(root);
  auto *d = tree_->descriptor(root);
  FieldIdentity identity{};
  if (!s || !s->alive || s->inside || !d ||
      d->id != recipe_->identity().scene_id ||
      !tree_->object_identity(root, identity) ||
      identity.source_sha256 != data_->identity().source_sha256 ||
      identity.upstream_commit != data_->identity().upstream_commit)
    return reject(e, "Dialogue visual actual detached factory required");
  std::map<uint32_t, FieldObjectId> objects;
  for (const auto &n : data_->nodes()) {
    FieldObjectId id = 0;
    if (!tree_->get_node(root, n.path, id, e))
      return false;
    auto *p = tree_->descriptor(id);
    if (!p || p->id != n.id || p->native_class != n.native_class ||
        p->script != n.script || !objects.emplace(n.id, id).second)
      return reject(e, "Dialogue visual actual ObjectID/source differs");
  }
  objects_ = std::move(objects);
  root_ = root;
  for (const auto &c : data_->cursors()) {
    FieldDialogueCursorState state;
    state.object = object(c.id);
    state.source = c.id;
    state.frame = c.frame;
    state.position = c.position;
    state.offset = c.drawing_offset;
    state.on = c.flags & 1;
    state.playing = c.playing;
    state.visible = c.visible;
    state.timeout = state.playing ? 1 / c.fps : 0;
    if (!c.menu.empty() &&
        !tree_->get_node(state.object, c.menu, state.menu, e))
      return false;
    FieldObjectId timer = 0;
    if (!tree_->get_node(state.object, "Timer", timer, e) ||
        !tree_->descriptor(timer) || tree_->descriptor(timer)->id != c.timer)
      return reject(e, "Dialogue Cursor actual source Timer differs");
    cursors_.emplace(state.object, state);
    // PackedScene visibility connection predates source _ready connections.
    if (!host_.connect(
            state.object, state.object, data_->signals()[8],
            [this, id = state.object] {
              std::string e;
              return visibility_cursor(id, e);
            },
            e) ||
        !publish(cursors_.at(state.object), e))
      return false;
  }
  // These are source constructor fields in a detached PackedScene. Geometry,
  // Canvas registration and native child Ready happen during real tree entry.
  if (!camera_.create(data_->camera().records()[0].id, true)) {
    e = camera_.error();
    return false;
  }
  if (!arrows_.create_source_constructor(data_->arrows().records()[0].id)) {
    e = arrows_.error();
    return false;
  }
  return true;
}
bool FieldDialogueVisualRuntime::enter_native(FieldObjectId id,
                                              std::string &e) {
  auto *n = node(id);
  if (!n || entered_.count(id))
    return reject(e, "Dialogue visual native enter rejected");
  if (!host_.native(id, *n, FieldTreePhase::EnterNative, e))
    return false;
  entered_.insert(id);
  return true;
}
bool FieldDialogueVisualRuntime::ready_native(FieldObjectId id,
                                              std::string &e) {
  auto *n = node(id);
  if (!n || !entered_.count(id) || ready_.count(id))
    return reject(e, "Dialogue visual native Ready rejected");
  if (!host_.native(id, *n, FieldTreePhase::ReadyNative, e))
    return false;
  ready_.insert(id);
  return true;
}
bool FieldDialogueVisualRuntime::exit_native(FieldObjectId id, std::string &e) {
  auto *n = node(id);
  if (!n || !entered_.count(id))
    return reject(e, "Dialogue visual native exit rejected");
  if (auto *c = cursor(id, e)) {
    if (c->tween && !host_.finish_tween(c->tween, 0, false, e))
      return false;
  } else
    e.clear();
  if (data_->camera().record(n->id) && !camera_.exit_tree(n->id)) {
    e = camera_.error();
    return false;
  }
  if (data_->arrows().record(n->id) && !arrows_.exit_tree(n->id)) {
    e = arrows_.error();
    return false;
  }
  if (!host_.native(id, *n, FieldTreePhase::ExitNative, e))
    return false;
  ready_.erase(id);
  entered_.erase(id);
  return true;
}
bool FieldDialogueVisualRuntime::ready_script(FieldObjectId id,
                                              std::string &e) {
  auto *n = node(id);
  const auto *actual = tree_ ? tree_->state(id) : nullptr;
  // Godot 3.6.2 invokes Node's script _ready before the derived native
  // subclass NOTIFICATION_READY. Native EnterTree and actual source Ready
  // traversal are required; the later native notification is not forged.
  if (!n || !actual || !actual->inside || !actual->ready_notified ||
      !entered_.count(id) || n->script.empty())
    return reject(e, "Dialogue visual source Script Ready rejected");
  if (data_->camera().record(n->id)) {
    if (!camera_.ready(n->id)) {
      e = camera_.error();
      return false;
    }
    return true;
  }
  if (data_->arrows().record(n->id)) {
    if (!arrows_.ready(n->id)) {
      e = arrows_.error();
      return false;
    }
    return true;
  }
  auto *c = cursor(id, e);
  if (!c || c->ready)
    return reject(e, "Dialogue Cursor source Ready rejected");
  c->ready = true;
  if (!cursor_index(id, 0, false, e))
    return false;
  if (c->menu && !host_.connect(
                     id, c->menu, data_->signals()[9],
                     [this, id] {
                       std::string e;
                       return refresh_cursor(id, false, e);
                     },
                     e))
    return false;
  // highlight is source-false for both instances; the original callback has no
  // effects here. Connection still belongs to the actual source signal bus.
  if (!host_.connect(
          id, id, data_->signals()[8], [] { return true; }, e) ||
      !host_.connect(
          id, host_.global, data_->signals()[10],
          [this, id] {
            std::string e;
            return refresh_cursor(id, true, e);
          },
          e))
    return false;
  return tree_->set_process(id, true, true, e) &&
         tree_->set_input_process(id, 0, true, e);
}
bool FieldDialogueVisualRuntime::menu(FieldDialogueCursorState &c,
                                      FieldDialogueCursorMenu &m,
                                      std::string &e) {
  if (!c.menu) {
    m = {};
    return true;
  }
  if (!host_.menu(c.menu, m, e))
    return false;
  if (m.object != c.menu || !m.columns || m.columns > 4096 ||
      m.items.size() > 4096 || !finite(m.position))
    return reject(e, "Dialogue Cursor real source Grid rejected");
  auto *state = tree_->state(c.menu);
  if (!state || state->children.size() != m.items.size())
    return reject(e, "Cursor menu must retain all original children");
  for (size_t i = 0; i < m.items.size(); ++i)
    if (m.items[i].object != state->children[i] ||
        !finite(m.items[i].position) || !finite(m.items[i].size))
      return reject(e, "Cursor actual menu source order rejected");
  return true;
}
int32_t
FieldDialogueVisualRuntime::valid_index(const FieldDialogueCursor &c,
                                        const FieldDialogueCursorMenu &m,
                                        int32_t index, int32_t step) const {
  if (index < 0 || size_t(index) >= m.items.size() || !step)
    return -1;
  for (int64_t i = index; i >= 0 && size_t(i) < m.items.size(); i += step) {
    auto &item = m.items[size_t(i)];
    if ((c.flags & 32) && !item.visible)
      continue;
    if ((c.flags & 16) && item.label && item.text.empty())
      continue;
    return int32_t(i);
  }
  return -1;
}
bool FieldDialogueVisualRuntime::publish(FieldDialogueCursorState &c,
                                         std::string &e) {
  auto *actual = tree_->state(c.object);
  if (!actual || !actual->alive)
    return reject(e, "Cursor publish actual lifetime missing");
  c.position = actual->local[2];
  if (!finite(c.position) || !finite(c.offset) || !std::isfinite(c.timeout))
    return reject(e, "Cursor state overflow rejected");
  return host_.publish(c.object, c, e);
}
bool FieldDialogueVisualRuntime::cursor_index_property(FieldObjectId id,
                                                       int32_t index,
                                                       std::string &e) {
  auto *c = cursor(id, e);
  if (!c)
    return false;
  if (index < -4096 || index > 4096)
    return reject(e, "Cursor direct index outside checked schema budget");
  c->index = index;
  return publish(*c, e);
}
bool FieldDialogueVisualRuntime::cursor_index(FieldObjectId id, int32_t index,
                                              bool transition, std::string &e) {
  auto *c = cursor(id, e);
  if (!c)
    return false;
  FieldDialogueCursorMenu m;
  if (!menu(*c, m, e))
    return false;
  if (!m.object)
    return true;
  auto *d = data_->cursor(c->source);
  index = valid_index(*d, m, index, -1);
  index = valid_index(*d, m, index, 1);
  if (index < 0)
    return true;
  c->index = index;
  Vec2 target{m.items[size_t(index)].position.x + d->offset.x - d->size.x / 6,
              m.items[size_t(index)].position.y + d->offset.y + d->size.y / 2};
  if (!finite(target))
    return reject(e, "Cursor global target overflow rejected");
  if (c->tween && !host_.finish_tween(c->tween, 0, false, e))
    return false;
  c->tween = 0;
  if (transition) {
    if (!host_.tween_position(id, c->tween, target, data_->tween_length(),
                              data_->transition(), data_->ease(), e))
      return false;
    FieldObjectId timer = 0;
    if (!tree_->get_node(id, "Timer", timer, e) || !host_.timer_start(timer, e))
      return false;
  } else {
    FieldTransform world;
    if (!tree_->world_transform(id, world, e))
      return false;
    auto *s = tree_->state(id);
    FieldTransform parent;
    if (!s || !tree_->world_transform(s->parent, parent, e))
      return false;
    float det = parent[0].x * parent[1].y - parent[0].y * parent[1].x;
    if (!std::isfinite(det) || det == 0)
      return reject(e, "Cursor actual singular parent rejected");
    Vec2 a{target.x - parent[2].x, target.y - parent[2].y};
    c->position = {(parent[1].y * a.x - parent[1].x * a.y) / det,
                   (-parent[0].y * a.x + parent[0].x * a.y) / det};
    auto *r = recipe_->record(c->source);
    auto local = r->local;
    float angle = std::atan2(local[0].y, local[0].x);
    local[0] = {std::cos(angle), std::sin(angle)};
    local[1] = {-std::sin(angle), std::cos(angle)};
    local[2] = c->position;
    if (!tree_->set_local(id, local, e))
      return false;
  }
  return publish(*c, e);
}
bool FieldDialogueVisualRuntime::cursor_on(FieldObjectId id, bool on,
                                           std::string &e) {
  auto *c = cursor(id, e);
  if (!c)
    return false;
  c->on = on;
  if (c->playing != on) {
    c->playing = on;
    if (on)
      c->timeout = 1 / data_->cursor(c->source)->fps;
  }
  if (!on && c->tween) {
    if (!host_.finish_tween(c->tween, data_->tween_length(), true, e))
      return false;
    c->tween = 0;
  }
  if (!publish(*c, e))
    return false;
  return !on || host_.emit(id, data_->signals()[0], {}, e);
}
bool FieldDialogueVisualRuntime::refresh_cursor(FieldObjectId id, bool wait,
                                                std::string &e) {
  auto *c = cursor(id, e);
  if (!c)
    return false;
  if (wait)
    return host_.await_idle(
        id,
        [this, id] {
          std::string e;
          auto *c = cursor(id, e);
          return c && cursor_index(id, c->index, false, e);
        },
        e);
  return cursor_index(id, c->index, false, e);
}
bool FieldDialogueVisualRuntime::visibility_cursor(FieldObjectId id,
                                                   std::string &e) {
  auto *c = cursor(id, e);
  if (!c)
    return false;
  if (!(data_->cursor(c->source)->flags & 2048))
    return true;
  return host_.await_idle(
      id,
      [this, id] {
        std::string e;
        auto *c = cursor(id, e);
        if (!c)
          return false;
        c->index = 0;
        return cursor_index(id, 0, false, e);
      },
      e);
}
bool FieldDialogueVisualRuntime::physics_cursor(FieldObjectId id,
                                                std::string &e) {
  auto *c = cursor(id, e);
  if (!c || !c->ready)
    return reject(e, "Cursor physics before actual Ready");
  if (!c->on || !c->menu)
    return true;
  bool process = false, pending = false;
  if (!host_.observe(id, process, pending, e))
    return false;
  if (!process)
    return true;
  Vec2 input;
  if (!host_.controls(id, input, e))
    return false;
  if (!finite(input) || std::abs(input.x) > 1 || std::abs(input.y) > 1 ||
      std::floor(input.x) != input.x || std::floor(input.y) != input.y)
    return reject(e, "Cursor source digital vector rejected");
  if (input.x == 0 && input.y == 0)
    return true;
  FieldObjectId timer = 0;
  float left = 0;
  if (!tree_->get_node(id, "Timer", timer, e) ||
      !host_.timer_left(timer, left, e))
    return false;
  if (!std::isfinite(left) || left < 0)
    return reject(e, "Cursor real Timer time_left rejected");
  if (left != 0)
    return true;
  FieldDialogueCursorMenu m;
  if (!menu(*c, m, e))
    return false;
  if (m.items.empty())
    return true;
  int32_t old = c->index, index = 0, dir = 0, cols = int32_t(m.columns),
          row = old / cols, column = old - row * cols;
  int32_t rows = (int32_t(m.items.size()) - column + cols - 1) / cols,
          total = (int32_t(m.items.size()) + cols - 1) / cols;
  if (column == 0 && input.x != 0) {
    if (input.x == 1 && cols > 1) {
      int32_t nr = (int32_t(m.items.size()) - 1 + cols - 1) / cols;
      index = nr - 1 < row ? 1 + (nr - 1) * cols : old + 1;
      dir = int32_t(input.x + input.y);
    }
  } else if (column == cols - 1 && input.x == 1) {
  } else if (row == rows - 1 && input.y == 1) {
    if (rows < total) {
      index = int32_t(m.items.size()) - 1;
      input.y = -1;
      dir = int32_t(input.x + input.y);
    }
  } else if (row == 0 && input.y == -1) {
  } else {
    index = old + int32_t(input.x) + int32_t(input.y) * cols;
    dir = (index > old) - (index < old);
  }
  if (dir) {
    int32_t step = std::abs(index - old);
    index = valid_index(*data_->cursor(c->source), m, index,
                        dir > 0 ? step : -step);
    if (!cursor_index(id, index, true, e))
      return false;
  }
  if (c->index != old) {
    if ((data_->cursor(c->source)->flags & 128) &&
        !host_.sound(id, data_->sounds()[0], e))
      return false;
    return host_.emit(id, data_->signals()[1], input, e);
  }
  return host_.emit(id, data_->signals()[2], input, e);
}
bool FieldDialogueVisualRuntime::input_cursor(FieldObjectId id,
                                              std::string_view action,
                                              bool pressed, std::string &e) {
  auto *c = cursor(id, e);
  if (!c || !c->ready)
    return reject(e, "Cursor input before Ready");
  if (!pressed || !c->on || !c->menu)
    return true;
  if (action == data_->actions()[1])
    return true;
  if (action != data_->actions()[0])
    return reject(e, "Cursor unrecognized source input action");
  FieldDialogueCursorMenu m;
  if (!menu(*c, m, e))
    return false;
  auto index = valid_index(*data_->cursor(c->source), m, c->index, 1);
  return host_.emit(id, data_->signals()[index < 0 ? 4 : 3], int64_t(c->index),
                    e);
}
bool FieldDialogueVisualRuntime::frame(FieldDialogueCursorState &c,
                                       uint32_t frame_, std::string &e) {
  if (frame_ >= data_->arrows().frames().size())
    return reject(e, "Cursor source frame rejected");
  if (c.frame == frame_)
    return true;
  c.frame = frame_;
  if (c.playing)
    c.timeout = 1 / data_->cursor(c.source)->fps;
  return publish(c, e) && host_.emit(c.object, data_->signals()[6], {}, e);
}
bool FieldDialogueVisualRuntime::play_cursor(FieldObjectId id,
                                             std::string_view name,
                                             std::string &e) {
  auto *c = cursor(id, e);
  if (!c)
    return false;
  auto *d = data_->cursor(c->source);
  uint32_t role = 0;
  for (const auto &clip : d->clips)
    if (clip.name == name)
      role = clip.role;
  if (!role)
    return reject(e, "Cursor unknown source clip rejected");
  c->clip = role;
  c->animation_time = 0;
  c->animation_playing = true;
  ++c->revision;
  return host_.emit(object(d->player), "animation_started", std::string(name),
                    e);
}
bool FieldDialogueVisualRuntime::animate(FieldDialogueCursorState &c, float dt,
                                         std::string &e) {
  auto *d = data_->cursor(c.source);
  const auto &clip = d->clips[c.clip - 1];
  float old = c.animation_time, next = std::min(old + dt, clip.length);
  auto revision = c.revision;
  c.animation_time = next;
  auto write = [&](const FieldArrowTrack &tr, Vec2 value) {
    if (tr.property == FieldArrowProperty::Frame)
      return frame(c, uint32_t(value.x), e);
    if (tr.property == FieldArrowProperty::Offset)
      c.offset = value;
    else if (tr.property == FieldArrowProperty::Playing) {
      bool playing = value.x != 0;
      if (playing && !c.playing)
        c.timeout = 1 / d->fps;
      c.playing = playing;
    } else
      return reject(e, "Cursor unsupported animated setter");
    return publish(c, e);
  };
  for (const auto &tr : clip.tracks) {
    if (tr.update == 1 && dt > 0) {
      for (const auto &k : tr.keys)
        if (k.time >= old &&
            (k.time < next || (next == clip.length && k.time == next)))
          if (!write(tr, k.value))
            return false;
    } else {
      int at = -1;
      for (size_t i = 0; i < tr.keys.size(); ++i)
        if (tr.keys[i].time <= next)
          at = int(i);
      if (at < 0)
        continue;
      auto value = tr.keys[size_t(at)].value;
      if (tr.update == 0 && size_t(at + 1) < tr.keys.size()) {
        auto &a = tr.keys[size_t(at)];
        auto &b = tr.keys[size_t(at + 1)];
        float weight = ease((next - a.time) / (b.time - a.time), a.transition);
        value = {a.value.x + (b.value.x - a.value.x) * weight,
                 a.value.y + (b.value.y - a.value.y) * weight};
      }
      if (!write(tr, value))
        return false;
    }
    if (c.revision != revision)
      return true;
  }
  if (next == clip.length) {
    c.animation_playing = false;
    if (old < clip.length)
      return host_.emit(object(d->player), data_->signals()[7], clip.name, e);
  }
  return true;
}
bool FieldDialogueVisualRuntime::idle_cursor(FieldObjectId id, float dt,
                                             std::string &e) {
  if (!std::isfinite(dt) || dt < 0 || dt > 1)
    return reject(e, "Cursor native idle interval rejected");
  auto *n = node(id);
  if (!n)
    return reject(e, "Cursor native leaf lifetime rejected");
  FieldDialogueCursorState *c = nullptr;
  for (auto &entry : cursors_)
    if (entry.second.source == n->id ||
        data_->cursor(entry.second.source)->player == n->id)
      c = &entry.second;
  if (!c)
    return reject(e, "Cursor unknown native leaf rejected");
  bool process = false, pending = false;
  if (!host_.observe(id, process, pending, e))
    return false;
  if (!process)
    return true;
  if (n->id == data_->cursor(c->source)->player)
    return !c->animation_playing || animate(*c, dt, e);
  if (!pending || !c->playing)
    return true;
  float remaining = dt;
  while (remaining && c->playing) {
    if (c->timeout <= 0) {
      c->timeout = 1 / data_->cursor(c->source)->fps;
      if (c->frame >= data_->arrows().frames().size() - 1) {
        c->frame = 0;
        if (!host_.emit(c->object, data_->signals()[7], {}, e))
          return false;
      } else
        ++c->frame;
      if (!publish(*c, e) || !host_.emit(c->object, data_->signals()[6], {}, e))
        return false;
    }
    float slice = std::min(c->timeout, remaining);
    if (!std::isfinite(slice) || slice < 0)
      return reject(e, "Cursor native animation reentry clock rejected");
    c->timeout -= slice;
    remaining -= slice;
  }
  return publish(*c, e);
}
bool FieldDialogueVisualRuntime::cursor_draw(FieldObjectId id,
                                             FieldArrowDraw &out,
                                             std::string &e) {
  auto *c = cursor(id, e);
  if (!c || !ready_.count(id))
    return reject(e, "Cursor draw actual Ready missing");
  auto *d = data_->cursor(c->source);
  FieldTransform world;
  if (!tree_->world_transform(id, world, e))
    return false;
  auto f = data_->arrows().frames()[c->frame];
  Vec2 offset = c->offset;
  if (d->centered) {
    offset.x -= f[2] / 2;
    offset.y -= f[3] / 2;
  }
  if (data_->arrows().pixel_snap()) {
    offset.x = std::floor(offset.x);
    offset.y = std::floor(offset.y);
  }
  FieldArrowDraw pose;
  pose.id = c->source;
  pose.frame = c->frame;
  pose.visible = tree_->visible_in_tree(id);
  pose.pixel_snap = data_->arrows().pixel_snap();
  if (!tree_->effective_color(id, pose.color, e))
    return false;
  pose.world_vertices = {transform(world, offset),
                         transform(world, {offset.x + f[2], offset.y}),
                         transform(world, {offset.x + f[2], offset.y + f[3]}),
                         transform(world, {offset.x, offset.y + f[3]})};
  for (auto v : pose.world_vertices)
    if (!finite(v))
      return reject(e, "Cursor actual GPU vertex overflow rejected");
  out = pose;
  return true;
}
} // namespace encore::upstream
