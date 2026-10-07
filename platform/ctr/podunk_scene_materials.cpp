#include "podunk_scene_materials.hpp"
#include "field_canvas_art_renderer.hpp"
#include "field_melody_background_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
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
uint32_t color(const FieldColor &c) {
  auto channel = [](float f) {
    return uint32_t(std::floor(std::clamp(f, 0.f, 1.f) * 255 + .5f));
  };
  return channel(c[0]) | channel(c[1]) << 8 | channel(c[2]) << 16 |
         channel(c[3]) << 24;
}
struct MaterialBody {
  FieldGlobalExternalBinding binding{};
  FieldMaterialRecord source;
  bool alive = true;
};
class MaterialResource final : public FieldGlobalSourceResource {
  std::shared_ptr<MaterialBody> body_;

public:
  explicit MaterialResource(std::shared_ptr<MaterialBody> b)
      : body_(std::move(b)) {}
  const char *resource_class() const override { return "ShaderMaterial"; }
  FieldGlobalExternalBinding binding() const override { return body_->binding; }
  bool state(FieldGlobalExternalState &out, std::string &e) const override {
    if (!body_->alive)
      return fail(e, "Scene actual material owner expired");
    out = {};
    out.name = body_->binding.source.name;
    e.clear();
    return true;
  }
  bool deferred(const FieldDeferredMessage &, std::string &e) override {
    return fail(e, "ShaderMaterial is not a deferred Node");
  }
  bool persist_append(FieldObjectId, std::string &e) override {
    return fail(e, "ShaderMaterial cannot own persistent Nodes");
  }
  bool assign_stable_canvas(FieldObjectId, std::string &e) override {
    return fail(e, "ShaderMaterial cannot own a CanvasLayer");
  }
};
class MaterialGpu {
  struct Vertex {
    float x, y, z, u, v;
    uint32_t color;
  };
  std::map<uint64_t, LoadingSpriteSheet> sheets_;
  const FieldCanvasArtRenderer *images_ = nullptr;
  std::vector<uint8_t> shader_;
  DVLB_s *dvlb_ = nullptr;
  shaderProgram_s program_{};
  bool initialized_ = false;
  int projection_ = -1;
  Vertex *buffer_ = nullptr;
  size_t capacity_ = 0, used_ = 0;
  C3D_AttrInfo attr_{};
  C3D_BufInfo buf_{};
  bool bytes(const std::string &path, uint32_t count,
             const std::array<uint8_t, 32> &sha, std::vector<uint8_t> &raw,
             std::string &e) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
      return fail(e, "Material GPU source unavailable");
    f.seekg(0, std::ios::end);
    if (!count || count > 16 * 1024 * 1024 || f.tellg() != count)
      return fail(e, "Material GPU source extent rejected");
    f.seekg(0);
    raw.resize(count);
    if (!f.read(reinterpret_cast<char *>(raw.data()), count) ||
        field_canvas_art_renderer_detail::sha256(raw.data(), raw.size()) != sha)
      return fail(e, "Material actual GPU source SHA rejected");
    return true;
  }
  bool sheet(uint64_t id, const std::string &path, uint32_t count,
             const std::array<uint8_t, 32> &sha, uint32_t w, uint32_t h,
             std::string &e) {
    std::vector<uint8_t> b;
    if (!bytes(path, count, sha, b, e))
      return false;
    auto *s = new (std::nothrow) LoadingSpriteSheetData;
    if (!s)
      return fail(e, "Material GPU texture allocation failed");
    s->metadata =
        Tex3DS_TextureImport(b.data(), b.size(), &s->texture, nullptr, false);
    if (!s->metadata) {
      delete s;
      return fail(e, "Material genuine texture import failed");
    }
    sheets_[id] = s;
    auto im = loading_sprite_sheet_get_image(s, 0);
    if (loading_sprite_sheet_count(s) != 1 || !im.tex || !im.subtex ||
        Tex3DS_SubTextureRotated(im.subtex) || im.subtex->width != w ||
        im.subtex->height != h)
      return fail(e, "Material GPU source texture shape rejected");
    C3D_TexSetFilter(im.tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(im.tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
    im.tex->border = 0;
    return true;
  }

public:
  ~MaterialGpu() { free(); }
  bool load(const FieldSceneMaterialsData &d, const FieldCanvasArtData &a,
            const char *root, std::string &e) {
    for (const auto &t : d.assets())
      if (!sheet(uint64_t(t.id) << 32, std::string(root) + t.path, t.bytes,
                 t.output_sha, t.width, t.height, e))
        return false;
    auto &p = a.program();
    if (!bytes(std::string(root) + p.path, p.bytes, p.output_sha, shader_, e) ||
        shader_.size() % 4)
      return fail(e, "Material actual PICA source rejected");
    dvlb_ = DVLB_ParseFile(reinterpret_cast<uint32_t *>(shader_.data()),
                           shader_.size());
    if (!dvlb_ || dvlb_->numDVLE != 1)
      return fail(e, "Material actual PICA program rejected");
    shaderProgramInit(&program_);
    initialized_ = true;
    if (R_FAILED(shaderProgramSetVsh(&program_, &dvlb_->DVLE[0])))
      return fail(e, "Material PICA binding failed");
    projection_ =
        shaderInstanceGetUniformLocation(program_.vertexShader, "projection");
    if (projection_ < 0)
      return fail(e, "Material projection binding failed");
    capacity_ = d.bindings().size() * 6;
    buffer_ = static_cast<Vertex *>(linearAlloc(capacity_ * sizeof(Vertex)));
    if (!buffer_)
      return fail(e, "Material frame buffer allocation failed");
    AttrInfo_Init(&attr_);
    AttrInfo_AddLoader(&attr_, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(&attr_, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(&attr_, 2, GPU_UNSIGNED_BYTE, 4);
    BufInfo_Init(&buf_);
    if (BufInfo_Add(&buf_, buffer_, sizeof(Vertex), 3, 0x210) < 0)
      return fail(e, "Material vertex layout rejected");
    return true;
  }
  bool bind(const FieldCanvasArtRenderer &images, const FieldCanvasArtData &a,
            const FieldSceneMaterialsData &d, std::string &e) {
    if (images_ || images.data() != &a)
      return fail(e, "Material GPU same Canvas pages required");
    for (const auto &b : d.bindings())
      if (d.record(b.material)->kind == FieldCanvasShader::Flash) {
        C2D_Image im{};
        auto *t = a.texture(b.texture);
        if (!t || !images.image(b.texture, im, e) ||
            Tex3DS_SubTextureRotated(im.subtex) ||
            im.subtex->width != t->width || im.subtex->height != t->height)
          return fail(e, "Material borrowed source texture shape rejected");
      }
    images_ = &images;
    e.clear();
    return true;
  }
  void begin() { used_ = 0; }
  bool draw(uint64_t asset, uint32_t tw, uint32_t th, const FieldCanvasDraw &p,
            Vec2 camera, float width, float height, const FieldColor *flash,
            const FieldColor *glow, float fm, float gm, std::string &e) {
    auto it = sheets_.find(asset);
    if ((!images_ && it == sheets_.end()) || !buffer_ ||
        capacity_ - used_ < 6 || !tw || !th || !std::isfinite(width) ||
        !std::isfinite(height) || width <= 0 || height <= 0 || width > 4096 ||
        height > 4096 || !std::isfinite(camera.x) || !std::isfinite(camera.y))
      return fail(e, "Material actual GPU frame/image rejected");
    C2D_Image im{};
    if (it != sheets_.end())
      im = loading_sprite_sheet_get_image(it->second, 0);
    else if (asset > UINT32_MAX || !images_->image(uint32_t(asset), im, e))
      return fail(e, "Material actual borrowed GPU image absent");
    auto &s = *im.subtex;
    auto &f = p.source_rect;
    for (float v : f)
      if (!std::isfinite(v))
        return fail(e, "Material source UV nonfinite");
    if (f[0] < 0 || f[1] < 0 || f[2] <= 0 || f[3] <= 0 || f[0] + f[2] > tw ||
        f[1] + f[3] > th)
      return fail(e, "Material source UV bounds rejected");
    for (float v : p.color)
      if (!std::isfinite(v) || v < 0 || v > 1)
        return fail(e, "Material inherited color rejected");
    float du = (s.right - s.left) / tw, dv = (s.bottom - s.top) / th;
    std::array<float, 2> u{s.left + f[0] * du, s.left + (f[0] + f[2]) * du},
        v{s.top + f[1] * dv, s.top + (f[1] + f[3]) * dv};
    if (p.flip_h)
      std::swap(u[0], u[1]);
    if (p.flip_v)
      std::swap(v[0], v[1]);
    std::array<Vertex, 4> corners{};
    for (size_t i = 0; i < 4; ++i) {
      float x = p.vertices[i].x - camera.x, y = p.vertices[i].y - camera.y;
      if (!std::isfinite(x) || !std::isfinite(y))
        return fail(e, "Material actual affine corner rejected");
      if (p.pixel_snap) {
        x = std::floor(x + .5f);
        y = std::floor(y + .5f);
      }
      corners[i] = {x, y, 0, u[i == 1 || i == 2], v[i >= 2], color(p.color)};
    }
    size_t first = used_;
    for (size_t i :
         {size_t(0), size_t(1), size_t(2), size_t(0), size_t(2), size_t(3)})
      buffer_[used_++] = corners[i];
    GSPGPU_FlushDataCache(buffer_ + first, 6 * sizeof(Vertex));
    C2D_Flush();
    C3D_BindProgram(&program_);
    C3D_SetAttrInfo(&attr_);
    C3D_SetBufInfo(&buf_);
    C3D_Mtx projection;
    Mtx_OrthoTilt(&projection, 0, width, height, 0, 1, -1, true);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_, &projection);
    for (unsigned i = 0; i < 6; ++i)
      C3D_TexEnvInit(C3D_GetTexEnv(i));
    auto *env = C3D_GetTexEnv(0);
    if (flash && glow) {
      if (!std::isfinite(fm) || !std::isfinite(gm) || fm < 0 || fm > 1 ||
          gm < 0)
        return fail(
            e, "Material live Flash modifier outside admitted source range");
      FieldColor factor{(1 - fm) * p.color[0], (1 - fm) * p.color[1],
                        (1 - fm) * p.color[2], p.color[3]},
          constant{};
      for (size_t i = 0; i < 3; ++i) {
        if (!std::isfinite((*flash)[i]) || !std::isfinite((*glow)[i]) ||
            (*flash)[i] < 0 || (*glow)[i] < 0)
          return fail(e, "Material live Flash color rejected");
        constant[i] =
            ((*glow)[i] * gm * (1 - fm) + (*flash)[i] * fm) * p.color[i];
      }
      constant[3] = 1;
      C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_CONSTANT, GPU_CONSTANT);
      C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
      C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, GPU_CONSTANT);
      C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
      C3D_TexEnvColor(env, color(factor));
      env = C3D_GetTexEnv(1);
      C3D_TexEnvSrc(env, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT, GPU_CONSTANT);
      C3D_TexEnvFunc(env, C3D_RGB, GPU_ADD);
      C3D_TexEnvSrc(env, C3D_Alpha, GPU_PREVIOUS, GPU_PREVIOUS, GPU_PREVIOUS);
      C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
      C3D_TexEnvColor(env, color(constant));
    } else {
      C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
                    GPU_PRIMARY_COLOR);
      C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    }
    C3D_TexBind(0, im.tex);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(true, GPU_GREATER, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_DrawArrays(GPU_TRIANGLES, int(first), 6);
    C2D_Prepare();
    e.clear();
    return true;
  }
  void free() {
    if (buffer_ || initialized_ || !sheets_.empty())
      C3D_FrameSync();
    if (buffer_)
      linearFree(buffer_);
    buffer_ = nullptr;
    used_ = capacity_ = 0;
    for (auto &r : sheets_)
      loading_sprite_sheet_free(r.second);
    sheets_.clear();
    if (initialized_)
      shaderProgramFree(&program_);
    initialized_ = false;
    if (dvlb_)
      DVLB_Free(dvlb_);
    dvlb_ = nullptr;
    shader_.clear();
  }
};
} // namespace
struct PodunkSceneMaterials::State {
  const FieldSceneMaterialsData *data = nullptr;
  const FieldCanvasArtData *art = nullptr;
  FieldNodeTreeRuntime *tree = nullptr;
  FieldGlobalRegistry *registry = nullptr;
  const FieldPromptData *prompt_data = nullptr;
  FieldPromptRuntime *prompt = nullptr;
  const FieldMelodyBackgroundData *melody_data = nullptr;
  const FieldMelodyBackgroundRuntime *melody = nullptr;
  DefaultDraw other;
  MaterialGpu gpu;
  FieldMelodyBackgroundRenderer melody_gpu;
  mutable std::map<uint32_t, std::shared_ptr<MaterialBody>> resources;
  mutable std::map<FieldObjectId, uint32_t> nodes;
  uint64_t epoch = 0;
  float shader_time = 0;
  bool begun = false;
  bool actual(FieldObjectId id, uint32_t source, std::string &e) const {
    auto *s = tree ? tree->state(id) : nullptr;
    auto *d = tree ? tree->descriptor(id) : nullptr;
    FieldIdentity identity;
    if (!registry || registry->tree_owner(id).get() != tree ||
        !registry->object_exists(id) || !s || !d || s->queued ||
        d->id != source || !tree->object_identity(id, identity) ||
        !same(identity, data->identity()))
      return fail(e, "Material actual same Tree/ObjectDB/source rejected");
    return true;
  }
};
PodunkSceneMaterials::PodunkSceneMaterials() : state_(new State) {}
PodunkSceneMaterials::~PodunkSceneMaterials() {
  std::string e;
  shutdown(e);
}
const FieldGlobalRegistry *PodunkSceneMaterials::registry() const {
  return state_->registry;
}
bool PodunkSceneMaterials::prepare(
    const FieldSceneMaterialsData &d, const FieldCanvasArtData &a,
    FieldNodeTreeRuntime &t, FieldGlobalRegistry &r, const FieldPromptData &pd,
    FieldPromptRuntime &p, const FieldMelodyBackgroundData &md,
    const FieldMelodyBackgroundRuntime &m, const char *root, DefaultDraw other,
    std::string &e) {
  if (state_->data || !d.valid() || !a.valid() ||
      !same(d.identity(), a.identity()) || !pd.valid() || !md.valid() ||
      pd.source_pin() != d.identity().upstream_commit ||
      md.source_pin() != d.identity().upstream_commit || !root || !*root ||
      !other)
    return fail(e,
                "Material complete checked data/typed live consumers required");
  auto next = std::make_unique<State>();
  next->data = &d;
  next->art = &a;
  next->tree = &t;
  next->registry = &r;
  next->prompt_data = &pd;
  next->prompt = &p;
  next->melody_data = &md;
  next->melody = &m;
  next->other = std::move(other);
  for (const auto &b : d.bindings()) {
    auto *rec = d.record(b.material);
    if (rec->kind == FieldCanvasShader::Flash && !pd.record(b.owner_id))
      return fail(e, "Material Flash actual Prompt owner absent");
    if (rec->kind == FieldCanvasShader::Distortion) {
      auto *mb = md.binding(b.owner_id);
      std::array<uint8_t, 32> h{}, source{};
      if (!mb || mb->bg_id != b.id || !d.source_hash(rec->shader, h) ||
          !md.source_hash(md.shader(), source) || h != source)
        return fail(e, "Material exact Melody shader owner absent");
    }
  }
  if (!next->gpu.load(d, a, root, e) || !next->melody_gpu.load(md, root, e))
    return false;
  state_ = std::move(next);
  e.clear();
  return true;
}
bool PodunkSceneMaterials::bind_images(const FieldCanvasArtRenderer &images,
                                       std::string &e) {
  auto &s = *state_;
  if (!s.data)
    return fail(e, "Material GPU source owner not prepared");
  return s.gpu.bind(images, *s.art, *s.data, e);
}
bool PodunkSceneMaterials::material(FieldObjectId id,
                                    const FieldCanvasRecord &a,
                                    PodunkSceneMaterialState &out,
                                    std::string &e) const {
  auto &s = *state_;
  auto *b = s.data ? s.data->binding(a.id) : nullptr;
  auto *m = b ? s.data->record(b->material) : nullptr;
  if (!b || !m || s.art->record(a.id) != &a || a.shader != m->kind ||
      !s.actual(id, a.id, e))
    return fail(e, "Material constructor actual Canvas source rejected");
  auto node = s.nodes.find(id);
  if (node != s.nodes.end() && node->second != m->id)
    return fail(e, "Material actual Node alias rejected");
  auto i = s.resources.find(m->id);
  if (i == s.resources.end()) {
    if (m->kind == FieldCanvasShader::Flash) {
      auto *initial = s.prompt->instance(b->owner_id);
      if (!initial || initial->ready || s.prompt->data() != s.prompt_data)
        return fail(e, "Material Arrow constructor needs already-created "
                       "actual Prompt script");
      FieldColor flash{};
      auto *fc = m->uniform(1);
      auto *fm = m->uniform(3);
      auto *gm = m->uniform(4);
      if (!fc || !fm || !gm || fc->values.size() != 4 ||
          fm->values.size() != 1 || gm->values.size() != 1)
        return fail(e, "Material source Flash constructor values absent");
      std::copy_n(fc->values.begin(), 4, flash.begin());
      if (!s.prompt->assign_source_material(b->owner_id, flash, fm->values[0],
                                            gm->values[0], e))
        return false;
    }
    auto body = std::make_shared<MaterialBody>();
    body->source = *m;
    auto &binding = body->binding;
    auto &spec = binding.source;
    spec.identity = s.data->identity();
    spec.source = m->source.substr(0, m->source.find("::"));
    if (!s.data->source_hash(spec.source, spec.source_sha))
      return fail(e, "Material original resource source proof absent");
    spec.identity.source_sha256 = spec.source_sha;
    spec.stable_id = m->id;
    spec.role = 4;
    spec.name = m->name;
    spec.native_class = "ShaderMaterial";
    binding.family = 0x454e006f;
    binding.capability = 1;
    if (!s.registry->allocate_object(binding.object, e))
      return false;
    std::unique_ptr<FieldGlobalSourceResource> owner(
        new MaterialResource(body));
    if (!s.registry->publish_source_resource(spec, binding.object,
                                             std::move(owner), e)) {
      std::string ignored;
      s.registry->retire_object(binding.object, ignored);
      return false;
    }
    i = s.resources.emplace(m->id, std::move(body)).first;
  }
  auto &body = *i->second;
  if (!body.alive || !s.registry->source_resource(body.binding.object))
    return fail(e, "Material actual Resource was retired");
  s.nodes[id] = m->id;
  out.material = body.binding.object;
  out.shader_source = m->shader;
  if (!s.data->source_hash(m->shader.substr(0, m->shader.find("::")),
                           out.shader_source_sha))
    return fail(e, "Material actual shader source absent");
  e.clear();
  return true;
}
bool PodunkSceneMaterials::shader_parameter(FieldObjectId id,
                                            std::string_view name,
                                            std::vector<float> &out,
                                            std::string &e) const {
  const auto &s = *state_;
  for (const auto &entry : s.resources) {
    const auto &body = *entry.second;
    if (body.binding.object != id)
      continue;
    if (!body.alive || !s.registry->source_resource(id))
      return fail(e, "Material actual Resource parameter owner expired");
    for (const auto &u : body.source.uniforms)
      if (u.name == name) {
        out = u.values;
        if (body.source.kind == FieldCanvasShader::Flash && u.role != 2) {
          const FieldMaterialBinding *binding = nullptr;
          for (const auto &b : s.data->bindings())
            if (b.material == entry.first) {
              if (binding && binding->owner_id != b.owner_id)
                return fail(e,
                            "Material shared Flash source owner unsupported");
              binding = &b;
            }
          auto *p = binding ? s.prompt->instance(binding->owner_id) : nullptr;
          if (!p || s.prompt->data() != s.prompt_data || !p->material_assigned)
            return fail(e, "Material actual Flash state unavailable");
          if (u.role == 1)
            out.assign(p->properties[8].begin(), p->properties[8].end());
          else if (u.role == 3)
            out = {p->properties[9][0]};
          else if (u.role == 4)
            out = {p->properties[7][0]};
        }
        e.clear();
        return true;
      }
    return fail(e, "Material unknown source uniform rejected");
  }
  return fail(e, "Material Resource ObjectID unknown");
}
bool PodunkSceneMaterials::begin_frame(uint64_t epoch, float time,
                                       std::string &e) {
  auto &s = *state_;
  if (!s.data || !epoch || (s.begun && epoch <= s.epoch) ||
      !std::isfinite(time) || time < 0 || time > 1e6f)
    return fail(e, "Material GPU frame fence/unique renderer time rejected");
  s.epoch = epoch;
  s.shader_time = time;
  s.begun = true;
  s.gpu.begin();
  e.clear();
  return true;
}
bool PodunkSceneMaterials::draw(const FieldCanvasDraw &p, Vec2 camera, float w,
                                float h, std::string &e) {
  auto &s = *state_;
  if (!s.data || !s.begun || !s.actual(p.object, p.source, e) ||
      !s.actual(p.owner_object, p.owner_source, e))
    return false;
  if (p.shader == FieldCanvasShader::Default)
    return s.other(p, camera, w, h, e);
  auto *b = s.data->binding(p.source);
  auto *m = b ? s.data->record(b->material) : nullptr;
  auto *a = s.art->record(p.source);
  PodunkSceneMaterialState resource;
  if (!b || !m || !a || p.owner != b->owner || p.owner_source != b->owner_id ||
      p.texture != b->texture || p.shader != m->kind ||
      !material(p.object, *a, resource, e))
    return fail(e, "Material GPU actual typed binding rejected");
  if (m->kind == FieldCanvasShader::Outline) {
    auto *x = s.data->asset(b->asset);
    if (!x)
      return fail(e, "Material checked outlined source absent");
    return s.gpu.draw(uint64_t(x->id) << 32, x->width, x->height, p, camera, w,
                      h, nullptr, nullptr, 0, 0, e);
  }
  if (m->kind == FieldCanvasShader::Flash) {
    auto *instance = s.prompt->instance(b->owner_id);
    if (s.prompt->data() != s.prompt_data || !instance || !instance->ready)
      return fail(e, "Material Flash unique actual clip owner not Ready");
    FieldColor flash = instance->properties[8], glow{};
    auto *u = m->uniform(2);
    if (!u || u->values.size() != 4)
      return fail(e, "Material source Glow uniform absent");
    std::copy_n(u->values.begin(), 4, glow.begin());
    auto *t = s.art->texture(p.texture);
    return t && s.gpu.draw(p.texture, t->width, t->height, p, camera, w, h,
                           &flash, &glow, instance->properties[9][0],
                           instance->properties[7][0], e);
  }
  if (m->kind == FieldCanvasShader::Distortion) {
    if (s.melody->content() != s.melody_data)
      return fail(e, "Material Melody unique actual source owner unbound");
    FieldTransform world;
    if (!s.tree->world_transform(p.owner_object, world, e))
      return false;
    if (world[0].x != 1 || world[0].y != 0 || world[1].x != 0 ||
        world[1].y != 1)
      return fail(e, "Material Melody source affine capability unavailable");
    FieldColor inherited{1, 1, 1, 1};
    auto *root = s.tree->state(p.owner_object);
    if (root->canvas_parent &&
        !s.tree->effective_color(root->canvas_parent, inherited, e))
      return false;
    return s.melody_gpu.draw(*s.melody, b->owner_id, world[2], camera,
                             s.shader_time, w, h, -w / 2, -h / 2, true,
                             color(inherited), 0, e);
  }
  return fail(e, "Material GPU shader capability unavailable");
}
bool PodunkSceneMaterials::shutdown(std::string &e) {
  auto &s = *state_;
  if (!s.data) {
    e.clear();
    return true;
  }
  C3D_FrameSync();
  s.gpu.free();
  s.melody_gpu.free();
  bool ok = true;
  for (auto &r : s.resources) {
    if (!r.second->alive)
      continue;
    // The Registry must observe the actual live, detached Resource before
    // retirement; invalidating state first would prevent that observation.
    if (!s.registry->retire_object(r.second->binding.object, e)) {
      ok = false;
    } else {
      r.second->alive = false;
    }
  }
  if (!ok)
    return false;
  state_ = std::make_unique<State>();
  e.clear();
  return true;
}
} // namespace encore::ctr
