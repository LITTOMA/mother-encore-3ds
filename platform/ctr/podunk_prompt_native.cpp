#include "podunk_prompt_native.hpp"
#include "encore/utf8.hpp"
#include "field_canvas_art_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace encore::ctr {
using namespace upstream;
namespace {
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool same(const FieldIdentity &a, const FieldIdentity &b) {
  return a.scene_id == b.scene_id && a.upstream_commit == b.upstream_commit &&
         a.source_sha256 == b.source_sha256;
}
uint32_t color(const FieldColor &v) {
  return C2D_Color32(uint8_t(std::lround(std::clamp(v[0], 0.f, 1.f) * 255)),
                     uint8_t(std::lround(std::clamp(v[1], 0.f, 1.f) * 255)),
                     uint8_t(std::lround(std::clamp(v[2], 0.f, 1.f) * 255)),
                     uint8_t(std::lround(std::clamp(v[3], 0.f, 1.f) * 255)));
}
} // namespace
struct PodunkPromptNative::State {
  struct Leaf {
    const PromptNativeRecord *source = nullptr;
    FieldNodeBinding binding{};
    Vec2 position{}, size{};
    std::string text;
    bool entered = false, ready = false;
  };
  const PromptNativeData *data = nullptr;
  const FieldNodeTreeData *source = nullptr;
  const FieldPromptData *prompt = nullptr;
  FieldPromptRuntime *core = nullptr;
  FieldNodeTreeRuntime *tree = nullptr;
  FieldGlobalRegistry *registry = nullptr;
  FieldObjectSignals *signals = nullptr;
  const FieldGlobalDataRuntime *global = nullptr;
  PodunkPlayerHost *player = nullptr;
  SourceFontRenderer *font = nullptr;
  LoadingSpriteSheet sheet = nullptr;
  std::map<FieldObjectId, Leaf> leaves;
  std::map<uint32_t, FieldObjectId> actuals;
  ~State() { loading_sprite_sheet_free(sheet); }
  bool actual(FieldObjectId id, std::string &e) const {
    const auto i = leaves.find(id);
    const auto *n = tree ? tree->descriptor(id) : nullptr;
    const auto *s = tree ? tree->state(id) : nullptr;
    FieldIdentity identity;
    if (!data || i == leaves.end() || !n || !s || !s->alive ||
        !registry->object_exists(id) ||
        registry->tree_owner(id).get() != tree || registry->poisoned() ||
        !tree->object_identity(id, identity) ||
        !same(identity, data->identity()) || n->id != i->second.source->id ||
        n->native_class != i->second.source->native_class || !n->script.empty())
      return fail(e, "Prompt native actual leaf/ObjectDB/source rejected");
    return true;
  }
  bool find(uint32_t stable, FieldObjectId &out, std::string &e) const {
    auto i = actuals.find(stable);
    if (i == actuals.end() || !actual(i->second, e))
      return fail(e, "Prompt native leaf not actually allocated");
    out = i->second;
    return true;
  }
  bool parent(uint32_t stable, FieldObjectId &out, std::string &e) const {
    out = tree->source_object(stable);
    FieldIdentity id;
    if (!out || !registry->object_exists(out) ||
        registry->tree_owner(out).get() != tree ||
        !tree->object_identity(out, id) || !same(id, data->identity()))
      return fail(e, "Prompt actual source root/parent rejected");
    return true;
  }
  bool publish(uint32_t id, const FieldPromptInstance &p, std::string &e) {
    const auto *r = prompt->record(id);
    FieldObjectId root = 0;
    if (!r || p.id != id || !p.ready || !parent(id, root, e))
      return fail(e, "Prompt publish requires same source instance");
    auto transform = tree->state(root)->local;
    transform[0] = {p.scale.x, 0};
    transform[1] = {0, p.scale.y};
    transform[2] = p.position;
    if (!tree->set_local(root, transform, e) ||
        !tree->set_modulate(root, p.properties[4], false, e) ||
        !tree->set_process(root, false, p.process, e))
      return false;
    FieldObjectId box = 0, label = 0, arrow = 0;
    if (!find(data->leaf(id, PromptNativeRole::Box)->id, box, e) ||
        !find(data->leaf(id, PromptNativeRole::Label)->id, label, e) ||
        !find(data->leaf(id, PromptNativeRole::Arrow)->id, arrow, e))
      return false;
    float width = 0;
    if (!font->catalog().selected() ||
        font->catalog().selected()->source != data->font() ||
        !font->catalog().measure(p.label, width, e))
      return fail(e, "Prompt actual font/label measurement rejected");
    const auto *face = font->catalog().selected();
    const Vec2 box_size = data->box_size();
    auto &b = leaves.at(box);
    auto &l = leaves.at(label);
    auto &a = leaves.at(arrow);
    b.position = {p.properties[1][0], p.properties[1][1]};
    b.size = box_size;
    // Godot BoxContainer uses integer minimum-size/alignment offsets. Its only
    // source Label is SHRINK_CENTER vertically, and aligns centered
    // horizontally.
    l.position = {float(int(box_size.x - std::floor(width)) / 2),
                  float(int(box_size.y - std::floor(face->height)) / 2)};
    l.size = {std::floor(width), std::floor(face->height)};
    l.text = p.label;
    a.position = {p.properties[0][0], p.properties[0][1]};
    a.size = data->control(PromptNativeRole::Arrow)->size;
    const std::array<std::pair<FieldObjectId, Leaf *>, 3> controls = {
        {{box, &b}, {label, &l}, {arrow, &a}}};
    for (const auto &item : controls) {
      auto local = tree->state(item.first)->local;
      local[2] = item.second->position;
      if (!tree->set_local(item.first, local, e))
        return false;
    }
    return tree->set_modulate(label, p.properties[3], false, e) &&
           tree->set_modulate(arrow, p.properties[5], false, e);
  }
  bool animation(uint32_t id, FieldPromptClipRole clip, uint32_t event,
                 std::string &e) {
    const auto *r = data->leaf(id, PromptNativeRole::Player);
    FieldObjectId actual = 0;
    const auto *c = prompt->clip(clip);
    if (!r || !c || !find(r->id, actual, e))
      return fail(e, "Prompt native player source role absent");
    if (event == 1) {
      if (!tree->add_group(actual, data->internal_group(), e))
        return false;
      return signals->emit(actual, data->started_signal(), {c->name}, e);
    }
    if (event != 2 && event != 3)
      return fail(e, "Prompt native unknown animation event");
    if (!tree->remove_group(actual, data->internal_group(), e))
      return false;
    return event == 3 ||
           signals->emit(actual, data->finished_signal(), {c->name}, e);
  }
};
PodunkPromptNative::PodunkPromptNative() : state_(std::make_unique<State>()) {}
PodunkPromptNative::~PodunkPromptNative() = default;
bool PodunkPromptNative::prepare(
    const PromptNativeData &d, const FieldNodeTreeData &t,
    const FieldPromptData &p, FieldPromptRuntime &core,
    FieldNodeTreeRuntime &tree, FieldGlobalRegistry &r, FieldObjectSignals &bus,
    const FieldGlobalDataRuntime &g, PodunkPlayerHost &player,
    FieldEquipmentView eq, SourceFontRenderer &font, const char *root,
    std::string &e) {
  auto &s = *state_;
  if (s.data || !d.valid() || !p.valid() || !t.valid() ||
      !same(d.identity(), t.identity()) || bus.registry() != &r ||
      (tree.object_domain() && tree.object_domain() != r.kernel()) || !root ||
      !font.catalog().selected() ||
      font.catalog().selected()->source != d.font())
    return fail(
        e, "Prompt native checked sources/actual GPU/font owners rejected");
  const auto &inputs = d.inputs();
  if (inputs.size() != 3)
    return fail(e, "Prompt native adapter coverage rejected");
  for (const auto &i : inputs)
    if (!eq.valid() || i.parameter > uint32_t(FieldParameter::OwnerId) ||
        eq.parameter(FieldParameter(i.parameter)) != i.mask ||
        !font.admit_text(i.label, e))
      return fail(e,
                  "Prompt native input adapter/source-font admission rejected");
  const auto &a = p.art();
  std::string path = std::string(root) + a.path;
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f)
    return fail(e, "Prompt native GPU atlas unavailable");
  std::vector<uint8_t> b(a.bytes);
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size() &&
            std::fgetc(f) == EOF && !std::ferror(f);
  ok = std::fclose(f) == 0 && ok;
  if (!ok ||
      field_canvas_art_renderer_detail::sha256(b.data(), b.size()) != a.sha256)
    return fail(e, "Prompt native GPU atlas bytes rejected");
  auto sheet = new (std::nothrow) LoadingSpriteSheetData;
  if (!sheet)
    return fail(e, "Prompt native GPU allocation failed");
  sheet->metadata =
      Tex3DS_TextureImport(b.data(), b.size(), &sheet->texture, nullptr, false);
  if (!sheet->metadata) {
    delete sheet;
    return fail(e, "Prompt native tex3ds import failed");
  }
  auto image = loading_sprite_sheet_get_image(sheet, 0);
  if (loading_sprite_sheet_count(sheet) != 1 || !image.tex || !image.subtex ||
      Tex3DS_SubTextureRotated(image.subtex) ||
      image.subtex->width != a.width || image.subtex->height != a.height) {
    loading_sprite_sheet_free(sheet);
    return fail(e, "Prompt native GPU source image rejected");
  }
  C3D_TexSetFilter(image.tex, GPU_NEAREST, GPU_NEAREST);
  C3D_TexSetWrap(image.tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
  sheet->texture.border = 0;
  s.data = &d;
  s.source = &t;
  s.prompt = &p;
  s.core = &core;
  s.tree = &tree;
  s.registry = &r;
  s.signals = &bus;
  s.global = &g;
  s.player = &player;
  s.font = &font;
  s.sheet = sheet;
  e.clear();
  return true;
}
bool PodunkPromptNative::apply(FieldPromptHost &h, std::string &e) {
  auto &s = *state_;
  if (!s.data)
    return fail(e, "Prompt native host not prepared");
  h.key_name = [&s](std::string_view action, std::string &out,
                    std::string &error) {
    for (const auto &i : s.data->inputs())
      if (i.action == action) {
        out = i.label;
        error.clear();
        return true;
      }
    return fail(error, "Prompt action has no checked native input binding");
  };
  h.observe = [&s](uint32_t id, FieldPromptObservation &out,
                   std::string &error) {
    FieldObjectId parent = 0;
    if (!s.parent(id, parent, error))
      return false;
    FieldGlobalDataMemberState setting;
    PlayerInitializationMember paused;
    if (!s.global->read_global_member(s.data->settings_member(), setting,
                                      error) ||
        !setting.value || setting.value->kind != 2 ||
        !s.player->body().member(s.data->paused_member(), paused, error) ||
        !paused.value || paused.kind != 1 || paused.value->kind != 1)
      return fail(error, "Prompt actual settings/Player paused body absent");
    if (setting.value->integer < 0 ||
        size_t(setting.value->integer) >= s.prompt->choices().size())
      return fail(error, "Prompt source settings enum rejected");
    out.settings_choice = uint32_t(setting.value->integer);
    out.paused = paused.value->boolean;
    const auto *node = s.tree->state(parent);
    const auto &v = node->local;
    float det = v[0].x * v[1].y - v[0].y * v[1].x;
    out.parent_scale = {std::hypot(v[0].x, v[0].y),
                        std::copysign(std::hypot(v[1].x, v[1].y), det)};
    return true;
  };
  h.publish = [&s](uint32_t id, const FieldPromptInstance &p,
                   std::string &error) { return s.publish(id, p, error); };
  h.native_animation = [&s](uint32_t id, FieldPromptClipRole c, uint32_t event,
                            std::string &error) {
    return s.animation(id, c, event, error);
  };
  e.clear();
  return true;
}
bool PodunkPromptNative::owns(const FieldNodeDescriptor &n) const {
  return state_->data && state_->data->record(n.id) &&
         state_->data->record(n.id)->native_class == n.native_class &&
         n.script.empty();
}
bool PodunkPromptNative::owns(FieldObjectId id) const {
  return state_->leaves.count(id) != 0;
}
bool PodunkPromptNative::construct(FieldObjectId id,
                                   const FieldNodeDescriptor &n,
                                   const FieldIdentity &identity,
                                   std::string &e) {
  auto &s = *state_;
  if (!owns(n) || s.leaves.count(id) || !same(identity, s.data->identity()) ||
      s.registry->tree_owner(id).get() != s.tree ||
      !s.registry->object_exists(id) || !s.tree->descriptor(id) ||
      s.tree->descriptor(id)->id != n.id)
    return fail(e, "Prompt native constructor actual allocation rejected");
  State::Leaf l;
  l.source = s.data->record(n.id);
  if (const auto *c = s.data->control(l.source->role)) {
    l.position = c->position;
    l.size = c->size;
    l.text = c->text;
  }
  l.binding = {identity, n.id,         n.class_index, 0x454e006b,
               1,        n.script_sha, n.native_class};
  s.leaves.emplace(id, std::move(l));
  s.actuals.emplace(n.id, id);
  e.clear();
  return true;
}
bool PodunkPromptNative::bind(FieldObjectId id, FieldNodeBinding &out,
                              std::string &e) {
  if (!state_->actual(id, e))
    return false;
  out = state_->leaves.at(id).binding;
  return true;
}
bool PodunkPromptNative::phase(FieldObjectId id, FieldTreePhase p, float dt,
                               bool paused, bool, std::string &e) {
  auto &s = *state_;
  if (!s.actual(id, e))
    return false;
  auto &l = s.leaves.at(id);
  const auto *n = s.tree->state(id);
  switch (p) {
  case FieldTreePhase::EnterNative:
    if (l.entered || !n->inside)
      return fail(e, "Prompt native Enter order rejected");
    l.entered = true;
    return true;
  case FieldTreePhase::ReadyNative:
    if (!l.entered || !n->ready_notified || n->ready_first)
      return fail(e, "Prompt native Ready cursor rejected");
    l.ready = true;
    return true;
  case FieldTreePhase::IdleInternal:
    if (l.source->role != PromptNativeRole::Player || !l.ready || !l.entered ||
        !n->inside || !s.tree->can_process(id, paused) ||
        s.core->data() != s.prompt)
      return fail(e, "Prompt native player internal clock owner rejected");
    if (!s.core->idle_frame(l.source->prompt, dt)) {
      e = s.core->error();
      return false;
    }
    return true;
  case FieldTreePhase::ExitNative:
    if (!l.entered)
      return fail(e, "Prompt native Exit order rejected");
    l.entered = false;
    return true;
  case FieldTreePhase::Deleting:
    return release(id, e);
  case FieldTreePhase::PostEnterNative:
  case FieldTreePhase::Parented:
  case FieldTreePhase::Unparented:
  case FieldTreePhase::ChildMoved:
  case FieldTreePhase::PathChanged:
  case FieldTreePhase::TransformChanged:
  case FieldTreePhase::LocalTransformChanged:
  case FieldTreePhase::VisibilityChanged:
  case FieldTreePhase::Hide:
    return true;
  default:
    return fail(e, "Prompt native unsupported phase rejected");
  }
}
bool PodunkPromptNative::deferred(const FieldDeferredMessage &,
                                  std::string &e) {
  return fail(e, "Prompt native unknown deferred member rejected");
}
bool PodunkPromptNative::release(FieldObjectId id, std::string &e) {
  auto &s = *state_;
  if (!s.actual(id, e) || s.leaves.at(id).entered)
    return fail(e, "Prompt native release before actual Exit");
  s.actuals.erase(s.leaves.at(id).source->id);
  s.leaves.erase(id);
  return true;
}
bool PodunkPromptNative::declaration(FieldObjectId id, std::string_view name,
                                     uint32_t &arity, std::string &e) const {
  if (!state_->actual(id, e) ||
      state_->leaves.at(id).source->role != PromptNativeRole::Player ||
      (name != state_->data->started_signal() &&
       name != state_->data->finished_signal()))
    return fail(e, "Prompt native unknown signal rejected");
  arity = 1;
  return true;
}
bool PodunkPromptNative::snapshot(FieldObjectId id, Vec2 &position, Vec2 &size,
                                  std::string &text, std::string &e) const {
  if (!state_->actual(id, e))
    return false;
  const auto &l = state_->leaves.at(id);
  if (l.source->role == PromptNativeRole::Player)
    return fail(e, "AnimationPlayer has no Control body");
  position = l.position;
  size = l.size;
  text = l.text;
  return true;
}
bool PodunkPromptNative::finish_factory(std::string &e) const {
  const auto &s = *state_;
  if (!s.data)
    return fail(e, "Prompt native factory unprepared");
  for (const auto &r : s.data->records()) {
    FieldObjectId id = 0;
    if (!s.find(r.id, id, e))
      return false;
  }
  return true;
}
const FieldNodeTreeRuntime *PodunkPromptNative::canvas_tree() const {
  return state_->tree;
}
const FieldGlobalRegistry *PodunkPromptNative::canvas_registry() const {
  return state_->registry;
}
bool PodunkPromptNative::owns_drawable(FieldObjectId id) const {
  auto i = state_->leaves.find(id);
  return i != state_->leaves.end() &&
         (i->second.source->role == PromptNativeRole::Label ||
          i->second.source->role == PromptNativeRole::Arrow);
}
bool PodunkPromptNative::draw_leaf(const FieldCanvasOrderSlot &slot,
                                   const FieldTransform &viewport, bool snap,
                                   std::string &e) {
  if (!owns_drawable(slot.object) ||
      !same(slot.identity, state_->data->identity()) ||
      slot.source != state_->leaves.at(slot.object).source->id ||
      viewport[0].x != 1 || viewport[0].y != 0 || viewport[1].x != 0 ||
      viewport[1].y != 1 || !std::isfinite(viewport[2].x) ||
      !std::isfinite(viewport[2].y))
    return fail(e, "Prompt actual Canvas order/viewport binding rejected");
  return draw_leaf(slot.object, {-viewport[2].x, -viewport[2].y}, 0, snap, e);
}
bool PodunkPromptNative::draw_leaf(FieldObjectId id, Vec2 camera, float depth,
                                   bool snap, std::string &e) const {
  const auto &s = *state_;
  if (!s.actual(id, e))
    return false;
  const auto &l = s.leaves.at(id);
  if (l.source->role != PromptNativeRole::Label &&
      l.source->role != PromptNativeRole::Arrow)
    return fail(e, "Prompt non-drawing leaf submitted to GPU");
  if (!l.ready)
    return fail(e, "Prompt GPU leaf lacks actual native Ready");
  if (!s.tree->visible_in_tree(id))
    return true;
  FieldTransform world;
  FieldColor modulation;
  if (!s.tree->world_transform(id, world, e) ||
      !s.tree->effective_color(id, modulation, e))
    return false;
  if (l.source->role == PromptNativeRole::Label) {
    if (std::abs(world[0].y) > 1e-6f || std::abs(world[1].x) > 1e-6f)
      return fail(e, "Prompt source Label affine font drawing unsupported");
    const auto *face = s.font->catalog().selected();
    if (!face || face->source != s.data->font())
      return fail(e, "Prompt native draw source font changed");
    float x = world[2].x - camera.x,
          y = world[2].y - camera.y + face->ascent * world[1].y;
    if (snap) {
      x = std::floor(x + .5f);
      y = std::floor(y + .5f);
    }
    size_t cursor = 0;
    uint32_t cp = 0;
    while (cursor < l.text.size()) {
      if (!utf8_next(l.text, cursor, cp))
        return fail(e, "Prompt Label invalid UTF8");
      const auto *g = s.font->glyph(cp);
      float advance = 0;
      if (!g || !s.font->glyph_advance(cp, advance) ||
          !s.font->draw_glyph(*g, x, y, world[0].x, world[1].y,
                              color(modulation), depth))
        return fail(e, "Prompt actual Label glyph GPU submission rejected");
      x += (advance + s.font->following_spacing(cp, cursor < l.text.size())) *
           world[0].x;
    }
    return true;
  }
  FieldObjectId root = 0;
  if (!s.parent(l.source->prompt, root, e) ||
      !s.tree->world_transform(root, world, e))
    return false;
  if (std::abs(world[0].y) > 1e-6f || std::abs(world[1].x) > 1e-6f)
    return fail(e, "Prompt Arrow affine atlas drawing unsupported");
  const auto *p = s.core->instance(l.source->prompt);
  if (!p || !p->ready)
    return fail(e, "Prompt actual Arrow source pose unavailable");
  const auto &a = s.prompt->art();
  auto image = loading_sprite_sheet_get_image(s.sheet, 0);
  auto sub = *image.subtex;
  float du = (sub.right - sub.left) / a.width,
        dv = (sub.bottom - sub.top) / a.height, u = sub.left, v = sub.top;
  sub.left = u + a.arrow_crop[0] * du;
  sub.right = u + (a.arrow_crop[0] + a.arrow_crop[2]) * du;
  sub.top = v + a.arrow_crop[1] * dv;
  sub.bottom = v + (a.arrow_crop[1] + a.arrow_crop[3]) * dv;
  sub.width = uint16_t(a.arrow_crop[2]);
  sub.height = uint16_t(a.arrow_crop[3]);
  float x = world[2].x + (p->properties[0][0] + a.arrow_origin.x) * world[0].x -
            camera.x,
        y = world[2].y + (p->properties[0][1] + a.arrow_origin.y) * world[1].y -
            camera.y;
  if (world[0].x < 0) {
    x += sub.width * world[0].x;
    std::swap(sub.left, sub.right);
  }
  if (world[1].y < 0) {
    y += sub.height * world[1].y;
    std::swap(sub.top, sub.bottom);
  }
  if (snap) {
    x = std::floor(x + .5f);
    y = std::floor(y + .5f);
  }
  FieldColor flash = p->properties[8];
  flash[3] = modulation[3];
  C2D_ImageTint tint;
  C2D_PlainImageTint(&tint, color(flash), p->properties[9][0]);
  if (!C2D_DrawImageAt({image.tex, &sub}, x, y, depth, &tint,
                       std::abs(world[0].x), std::abs(world[1].y)))
    return fail(e, "Prompt Arrow actual GPU submission rejected");
  return true;
}
} // namespace encore::ctr
