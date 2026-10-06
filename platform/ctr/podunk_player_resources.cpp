#include "podunk_player_resources.hpp"
#include <cmath>
#include <fstream>
namespace encore::ctr {
namespace {
inline uint32_t rotate(uint32_t v, unsigned n) {
  return (v >> n) | (v << (32 - n));
}
// Integrity machinery only. The digest binds converted tex3ds bytes to the
// checked resource; all sprite pixels, grids and transforms come from data.
inline std::array<uint8_t, 32> sha256(const uint8_t *data, size_t size) {
  static constexpr uint32_t constants[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const size_t blocks = (size + 9 + 63) / 64;
  for (size_t block = 0; block < blocks; ++block) {
    uint8_t raw[64]{};
    for (size_t j = 0; j < 64; ++j) {
      const size_t at = block * 64 + j;
      if (at < size)
        raw[j] = data[at];
      else if (at == size)
        raw[j] = 0x80;
    }
    if (block + 1 == blocks) {
      const uint64_t bits = uint64_t(size) * 8;
      for (unsigned j = 0; j < 8; ++j)
        raw[63 - j] = uint8_t(bits >> (j * 8));
    }
    uint32_t w[64];
    for (unsigned j = 0; j < 16; ++j)
      w[j] = uint32_t(raw[j * 4]) << 24 | uint32_t(raw[j * 4 + 1]) << 16 |
             uint32_t(raw[j * 4 + 2]) << 8 | raw[j * 4 + 3];
    for (unsigned j = 16; j < 64; ++j) {
      const auto a = w[j - 15], b = w[j - 2];
      w[j] = w[j - 16] + (rotate(a, 7) ^ rotate(a, 18) ^ (a >> 3)) + w[j - 7] +
             (rotate(b, 17) ^ rotate(b, 19) ^ (b >> 10));
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5],
             g = h[6], v = h[7];
    for (unsigned j = 0; j < 64; ++j) {
      const auto t1 = v + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) +
                      ((e & f) ^ (~e & g)) + constants[j] + w[j],
                 t2 = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) +
                      ((a & b) ^ (a & c) ^ (b & c));
      v = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += v;
  }
  std::array<uint8_t, 32> out{};
  for (unsigned i = 0; i < 8; ++i)
    for (unsigned j = 0; j < 4; ++j)
      out[i * 4 + j] = uint8_t(h[i] >> (24 - j * 8));
  return out;
}

bool fail(std::string &e, const std::string &s) {
  e = s;
  return false;
}

bool bytes(const std::string &path, uint32_t size,
           const std::array<uint8_t, 32> &hash, std::vector<uint8_t> &out,
           std::string &e) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f || f.tellg() != std::streamoff(size))
    return fail(e, "Player resource payload size rejected: " + path);
  std::vector<uint8_t> b(size);
  f.seekg(0);
  if (!f.read(reinterpret_cast<char *>(b.data()), size) ||
      sha256(b.data(), b.size()) != hash)
    return fail(e, "Player resource payload read/SHA rejected: " + path);
  out = std::move(b);
  return true;
}
} // namespace
struct PodunkPlayerResources::Image {
  upstream::PlayerResourceImage binding;
  LoadingSpriteSheet sheet = nullptr;
  ~Image() {
    if (sheet) {
      C3D_FrameSync();
      loading_sprite_sheet_free(sheet);
    }
  }
};
struct PodunkPlayerResources::State {
  const upstream::PlayerResourcesData *data = nullptr;
  std::array<uint8_t, 32> ir{};
  upstream::FieldGlobalExternalBinding binding;
  upstream::FieldGlobalRegistry *registry = nullptr;
  upstream::FieldObjectId player = 0;
  bool audio = false;
  upstream::PlayerResource resource;
  upstream::PlayerResourceAudio audio_binding;
  std::shared_ptr<Image> image;
  std::shared_ptr<const std::vector<uint8_t>> pcm;
  Tex3DS_SubTexture region{};
  std::vector<upstream::PlayerResourceParameter> parameters;
  bool valid() const {
    return data && data->valid() && data->ir_sha256() == ir && registry &&
           !registry->poisoned() &&
           (audio ? (pcm && pcm->size() == audio_binding.bytes)
                  : (resource.kind == 3 ||
                     (image && image->sheet && image->sheet->texture.data)));
  }
};
namespace {
class Owner final : public upstream::FieldGlobalSourceResource {
  std::shared_ptr<PodunkPlayerResources::State> state_;
  std::string native_;

public:
  Owner(std::shared_ptr<PodunkPlayerResources::State> s, const char *n)
      : state_(std::move(s)), native_(n) {}
  upstream::FieldGlobalExternalBinding binding() const override {
    return state_->binding;
  }
  const char *resource_class() const override { return native_.c_str(); }
  bool state(upstream::FieldGlobalExternalState &out,
             std::string &e) const override {
    if (!state_->valid())
      return fail(e, "Player actual Resource backing no longer valid");
    out = {};
    out.name = state_->binding.source.name;
    e.clear();
    return true;
  }
  bool deferred(const upstream::FieldDeferredMessage &,
                std::string &e) override {
    return fail(e, "Player Resource is not a Node/deferred script owner");
  }
  bool persist_append(upstream::FieldObjectId, std::string &e) override {
    return fail(e, "Player Resource has no persistent Node array");
  }
  bool assign_stable_canvas(upstream::FieldObjectId, std::string &e) override {
    return fail(e, "Player Resource cannot own canvas singleton");
  }
};
} // namespace
bool PodunkPlayerResources::live(std::string &e) const {
  if (!data_ || !data_->valid() || data_->ir_sha256() != ir_ || !registry_ ||
      registry_->poisoned())
    return fail(e, "Player resource owner/data unavailable");
  return true;
}
bool PodunkPlayerResources::load(const upstream::PlayerResourcesData &d,
                                 const char *root,
                                 upstream::FieldGlobalRegistry &registry,
                                 std::string &e) {
  if (data_ || !d.valid() || !root || !*root || registry.poisoned())
    return fail(e, "Player resource load owner/root rejected");
  std::map<uint32_t, std::shared_ptr<Image>> images;
  std::map<uint32_t, std::shared_ptr<const std::vector<uint8_t>>> audio;
  for (const auto &a : d.images()) {
    std::vector<uint8_t> b;
    const auto path = std::string(root) + a.path;
    if (!bytes(path, a.bytes, a.output_sha, b, e))
      return false;
    auto x = std::make_shared<Image>();
    x->binding = a;
    x->sheet = loading_sprite_sheet_acquire(path.c_str(), &e);
    if (!x->sheet || loading_sprite_sheet_count(x->sheet) != 1)
      return fail(e, "Player GPU requires actual single image atlas");
    auto im = loading_sprite_sheet_get_image(x->sheet, 0);
    if (!im.tex || !im.tex->data || !im.subtex || im.tex->fmt != GPU_RGBA8 ||
        Tex3DS_SubTextureRotated(im.subtex) || im.subtex->width != a.width ||
        im.subtex->height != a.height)
      return fail(e, "Player GPU actual image dimensions/format rejected");
    C3D_TexSetFilter(im.tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(im.tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    images.emplace(a.id, x);
  }
  for (const auto &a : d.audios()) {
    auto b = std::make_shared<std::vector<uint8_t>>();
    if (!bytes(std::string(root) + a.path, a.bytes, a.output_sha, *b, e))
      return false;
    audio.emplace(a.id, b);
  }
  C3D_FrameSync();
  data_ = &d;
  ir_ = d.ir_sha256();
  registry_ = &registry;
  images_ = std::move(images);
  audios_ = std::move(audio);
  e.clear();
  return true;
}
bool PodunkPlayerResources::publish(std::shared_ptr<State> s,
                                    const char *native, std::string &source,
                                    upstream::FieldObjectId &out,
                                    std::string &e) {
  upstream::FieldGlobalExternalSpec spec;
  spec.identity = data_->identity();
  if (!data_->source_hash(source, spec.source_sha))
    return fail(e, "Player Resource actual source proof missing");
  spec.identity.source_sha256 = spec.source_sha;
  spec.source = source;
  spec.name = s->audio ? s->audio_binding.source
                       : (s->resource.kind < 3
                              ? data_->image(s->resource.texture)->source
                              : source);
  spec.stable_id = (s->audio ? s->audio_binding.id : s->resource.id) + 1;
  spec.role = 4;
  spec.native_class = native;
  upstream::FieldObjectId id = 0;
  if (!registry_->allocate_object(id, e))
    return false;
  s->data = data_;
  s->ir = ir_;
  s->registry = registry_;
  s->binding = {id, spec, 0x454e005f, 1};
  if (!registry_->publish_source_resource(
          spec, id, std::make_unique<Owner>(s, native), e)) {
    std::string ignored;
    registry_->retire_object(id, ignored);
    return false;
  }
  out = id;
  e.clear();
  return true;
}
bool PodunkPlayerResources::construct_resource(uint32_t source_id,
                                               upstream::FieldObjectId player,
                                               upstream::FieldObjectId &out,
                                               std::string &e) {
  if (!live(e))
    return false;
  const auto *r = data_->resource(source_id);
  if (!r)
    return fail(e, "Player actual native resource ID unavailable");
  if (r->instanced) {
    auto tree = registry_->tree_owner(player);
    upstream::FieldIdentity actual;
    if (!player || !tree || !tree->state(player) ||
        !registry_->object_exists(player) ||
        !tree->object_identity(player, actual) ||
        (actual.scene_id != data_->identity().scene_id ||
         actual.source_sha256 != data_->identity().source_sha256 ||
         actual.upstream_commit != data_->identity().upstream_commit))
      return fail(e, "Player local material requires actual player Node owner");
  } else if (player)
    return fail(
        e, "Shared/template Player Resource cannot impersonate local clone");
  auto key = std::make_pair(source_id, player);
  auto prior = resources_.find(key);
  if (prior != resources_.end()) {
    if (!prior->second->valid() ||
        registry_->source_resource(prior->second->binding.object) == nullptr)
      return fail(e, "Player Resource existing owner retired");
    out = prior->second->binding.object;
    return true;
  }
  auto s = std::make_shared<State>();
  s->resource = *r;
  s->player = player;
  s->parameters = r->parameters;
  if (r->kind < 3) {
    s->image = images_.at(r->texture);
    auto im = loading_sprite_sheet_get_image(s->image->sheet, 0);
    s->region = *im.subtex;
    const auto &w = r->rect;
    s->region.width = uint16_t(w[2]);
    s->region.height = uint16_t(w[3]);
    const float dx =
        (im.subtex->right - im.subtex->left) / s->image->binding.width;
    const float dy =
        (im.subtex->top - im.subtex->bottom) / s->image->binding.height;
    s->region.left = im.subtex->left + w[0] * dx;
    s->region.right = s->region.left + w[2] * dx;
    s->region.top = im.subtex->top - w[1] * dy;
    s->region.bottom = s->region.top - w[3] * dy;
  }
  auto source = r->source;
  if (!publish(s,
               r->kind == 1   ? "StreamTexture"
               : r->kind == 2 ? "AtlasTexture"
                              : "ShaderMaterial",
               source, out, e))
    return false;
  resources_.emplace(key, s);
  return true;
}
bool PodunkPlayerResources::construct_audio(uint32_t id,
                                            upstream::FieldObjectId &out,
                                            std::string &e) {
  if (!live(e))
    return false;
  const auto *a = data_->audio(id);
  if (!a)
    return fail(e, "Player actual Audio Resource ID unavailable");
  auto i = audio_resources_.find(id);
  if (i != audio_resources_.end()) {
    if (!i->second->valid() ||
        !registry_->source_resource(i->second->binding.object))
      return fail(e, "Player Audio Resource retired");
    out = i->second->binding.object;
    return true;
  }
  auto s = std::make_shared<State>();
  s->audio = true;
  s->audio_binding = *a;
  s->pcm = audios_.at(id);
  auto source = a->source;
  if (!publish(s, a->kind == 1 ? "AudioStreamSample" : "AudioStreamMP3", source,
               out, e))
    return false;
  audio_resources_.emplace(id, s);
  return true;
}
bool PodunkPlayerResources::resolve(
    const upstream::PlayerInitializationField &field,
    upstream::FieldObjectId &out, std::string &e) {
  if (!live(e) || field.adapter != 2)
    return fail(e, "Player constructor resource adapter unsupported");
  for (const auto &a : data_->audios())
    if (a.source == field.resource &&
        field.native == (a.kind == 1 ? "AudioStreamSample" : "AudioStreamMP3"))
      return construct_audio(a.id, out, e);
  return fail(e, "Player constructor actual source audio resource unavailable");
}
bool PodunkPlayerResources::load_texture(std::string_view source,
                                         upstream::FieldObjectId &out,
                                         std::string &e) {
  if (!live(e))
    return false;
  if (source.substr(0, 6) == "res://")
    source.remove_prefix(6);
  for (const auto &r : data_->resources())
    if (r.kind == 1) {
      const auto *im = data_->image(r.texture);
      if (im && im->source == source)
        return construct_resource(r.id, 0, out, e);
    }
  return fail(
      e, "Player texture load outside checked actual StreamTexture closure: " +
             std::string(source));
}
bool PodunkPlayerResources::resource_exists(std::string_view source, bool &out,
                                            std::string &e) const {
  if (!live(e))
    return false;
  if (source.substr(0, 6) == "res://")
    source.remove_prefix(6);
  for (const auto &a : data_->images())
    if (a.source == source) {
      out = true;
      e.clear();
      return true;
    }
  return fail(
      e, "Player Resource existence query outside checked source closure: " +
             std::string(source));
}
bool PodunkPlayerResources::image(std::string_view source,
                                  const std::array<uint8_t, 32> &hash,
                                  C2D_Image &out, std::string &e) const {
  if (!live(e))
    return false;
  for (const auto &a : data_->images())
    if (a.source == source && a.source_sha == hash) {
      auto i = images_.find(a.id);
      if (i == images_.end() || !i->second->sheet)
        return fail(e, "Player actual GPU texture unavailable");
      out = loading_sprite_sheet_get_image(i->second->sheet, 0);
      e.clear();
      return true;
    }
  return fail(e, "Player visual texture source/SHA not owned");
}
const PodunkPlayerResources::State *
PodunkPlayerResources::owned(upstream::FieldObjectId id, std::string &e) const {
  if (!live(e))
    return nullptr;
  if (!registry_->source_resource(id)) {
    fail(e, "Player actual Resource ObjectDB owner retired");
    return nullptr;
  }
  for (const auto &x : resources_)
    if (x.second->binding.object == id && x.second->valid())
      return x.second.get();
  for (const auto &x : audio_resources_)
    if (x.second->binding.object == id && x.second->valid())
      return x.second.get();
  fail(e, "Player actual source Resource handle not owned");
  return nullptr;
}
bool PodunkPlayerResources::texture(upstream::FieldObjectId id, C2D_Image &out,
                                    std::string &e) const {
  auto s = owned(id, e);
  if (!s)
    return false;
  if (s->audio || s->resource.kind == 3 || !s->image)
    return fail(e, "Player Resource is not actual Texture");
  auto im = loading_sprite_sheet_get_image(s->image->sheet, 0);
  out = {im.tex, &s->region};
  e.clear();
  return true;
}
bool PodunkPlayerResources::height(upstream::FieldObjectId id, uint32_t &out,
                                   std::string &e) const {
  C2D_Image im;
  if (!texture(id, im, e))
    return false;
  out = im.subtex->height;
  return true;
}
bool PodunkPlayerResources::source_path(upstream::FieldObjectId id,
                                        std::string &out,
                                        std::string &e) const {
  auto s = owned(id, e);
  if (!s)
    return false;
  out = "res://" + (s->audio               ? s->audio_binding.source
                    : s->resource.kind < 3 ? s->image->binding.source
                                           : s->resource.source);
  e.clear();
  return true;
}
bool PodunkPlayerResources::set_shader_parameter(upstream::FieldObjectId id,
                                                 std::string_view name,
                                                 uint32_t kind,
                                                 const std::array<float, 4> &v,
                                                 std::string &e) {
  const auto *cs = owned(id, e);
  if (!cs || cs->audio || cs->resource.kind != 3)
    return fail(e, "Player shader actual Material owner required");
  auto *s = const_cast<State *>(cs);
  auto p = std::find_if(s->parameters.begin(), s->parameters.end(),
                        [&](const auto &x) { return x.name == name; });
  if (p == s->parameters.end() || p->kind != kind ||
      std::any_of(v.begin(), v.end(),
                  [](float x) { return !std::isfinite(x); }) ||
      (kind == 3 && v[0] != 0 && v[0] != 1) ||
      (kind == 4 && std::trunc(v[0]) != v[0]))
    return fail(e, "Player shader source parameter/type rejected");
  p->value = v;
  e.clear();
  return true;
}
bool PodunkPlayerResources::shader_parameters(
    upstream::FieldObjectId id,
    std::vector<upstream::PlayerResourceParameter> &out, std::string &e) const {
  auto *s = owned(id, e);
  if (!s || s->audio || s->resource.kind != 3)
    return fail(e, "Player shader actual Material required");
  out = s->parameters;
  e.clear();
  return true;
}
bool PodunkPlayerResources::neutral_material(upstream::FieldObjectId id,
                                             std::string &e) const {
  auto *s = owned(id, e);
  if (!s || s->audio || s->resource.kind != 3)
    return fail(e, "Player shader actual Material required");
  if (s->resource.shader == 1) {
    if (s->parameters[2].value[0] != 0 || s->parameters[3].value[0] != 0)
      return fail(e, "Player nonneutral Flash GPU kernel unavailable");
  } else if (s->parameters[1].value[0] != 0)
    return fail(e, "Player nonneutral Outline GPU kernel unavailable");
  e.clear();
  return true;
}
bool PodunkPlayerResources::pcm(upstream::FieldObjectId id,
                                const std::vector<uint8_t> *&out,
                                upstream::PlayerResourceAudio &binding,
                                std::string &e) const {
  auto *s = owned(id, e);
  if (!s || !s->audio || !s->pcm)
    return fail(e, "Player actual decoded PCM Resource required");
  out = s->pcm.get();
  binding = s->audio_binding;
  e.clear();
  return true;
}
bool PodunkPlayerResources::release_player(upstream::FieldObjectId player,
                                           std::string &e) {
  if (!live(e))
    return false;
  if (registry_->object_exists(player))
    return fail(e,
                "Player local Resource release precedes actual Node teardown");
  C3D_FrameSync();
  for (auto i = resources_.begin(); i != resources_.end();) {
    if (i->first.second == player && player) {
      if (!registry_->retire_object(i->second->binding.object, e))
        return false;
      i = resources_.erase(i);
    } else
      ++i;
  }
  e.clear();
  return true;
}
bool PodunkPlayerResources::shutdown(std::string &e) {
  if (!data_) {
    e.clear();
    return true;
  }
  for (const auto &x : resources_)
    if (x.first.second && registry_->object_exists(x.first.second))
      return fail(e,
                  "Player resource shutdown requires actual player teardown");
  C3D_FrameSync();
  for (const auto &x : resources_)
    if (!registry_->retire_object(x.second->binding.object, e))
      return false;
  for (const auto &x : audio_resources_)
    if (!registry_->retire_object(x.second->binding.object, e))
      return false;
  resources_.clear();
  audio_resources_.clear();
  images_.clear();
  audios_.clear();
  data_ = nullptr;
  registry_ = nullptr;
  e.clear();
  return true;
}
} // namespace encore::ctr
