#pragma once
#include "encore/field_canvas_art.hpp"
#include "loading_texture.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <vector>
namespace field_canvas_art_renderer_detail {

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

} // namespace field_canvas_art_renderer_detail
class FieldCanvasArtRenderer {
  using Data = encore::upstream::FieldCanvasArtData;
  using Draw = encore::upstream::FieldCanvasDraw;
  using Action = encore::upstream::FieldCanvasAction;
  struct Vertex {
    float x, y, z, u, v;
    uint32_t color;
  };
  const Data *data_ = nullptr;
  std::map<uint32_t, encore::ctr::LoadingSpriteSheet> sheets_;
  std::vector<uint8_t> shader_;
  DVLB_s *dvlb_ = nullptr;
  shaderProgram_s program_{};
  bool initialized_ = false;
  int projection_ = -1;
  Vertex *buffer_ = nullptr;
  size_t capacity_ = 0, used_ = 0;
  C3D_AttrInfo attributes_{};
  C3D_BufInfo buffers_{};
  static bool fail(std::string &e, const char *s) {
    e = s;
    return false;
  }
  static bool read(const std::string &p,
                   const encore::upstream::FieldCanvasAsset &a,
                   std::vector<uint8_t> &out, std::string &e) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
      return fail(e, "Canvas GPU resource unavailable");
    f.seekg(0, std::ios::end);
    if (f.tellg() != a.bytes || !a.bytes || a.bytes > 16 * 1024 * 1024)
      return fail(e, "Canvas GPU resource byte extent rejected");
    f.seekg(0);
    out.resize(a.bytes);
    if (!f.read(reinterpret_cast<char *>(out.data()), a.bytes) ||
        field_canvas_art_renderer_detail::sha256(out.data(), out.size()) !=
            a.output_sha)
      return fail(e, "Canvas GPU actual bytes/SHA rejected");
    return true;
  }
  bool load_impl(const Data &d, const char *prefix, std::string &e) {
    if (!d.valid() || !prefix)
      return fail(e, "Canvas checked source resource required");
    std::vector<uint8_t> raw;
    for (const auto &t : d.textures()) {
      if (!read(std::string(prefix) + t.path, t, raw, e))
        return false;
      auto *sheet = new (std::nothrow) encore::ctr::LoadingSpriteSheetData;
      if (!sheet)
        return fail(e, "Canvas texture owner allocation failed");
      sheet->metadata = Tex3DS_TextureImport(raw.data(), raw.size(),
                                             &sheet->texture, nullptr, false);
      if (!sheet->metadata) {
        delete sheet;
        return fail(e, "Canvas actual tex3ds import failed");
      }
      sheet->source_path = encore::ctr::loading_texture_key(
          (std::string(prefix) + t.path).c_str());
      sheets_.emplace(t.id, sheet);
      auto image = encore::ctr::loading_sprite_sheet_get_image(sheet, 0);
      if (encore::ctr::loading_sprite_sheet_count(sheet) != 1 || !image.tex ||
          !image.subtex || Tex3DS_SubTextureRotated(image.subtex) ||
          image.subtex->width != t.width || image.subtex->height != t.height ||
          image.tex->fmt != GPU_RGBA8)
        return fail(e, "Canvas actual single-atlas extent/format rejected");
      C3D_TexSetFilter(image.tex, GPU_NEAREST, GPU_NEAREST);
      C3D_TexSetWrap(image.tex, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
      image.tex->border = 0;
    }
    if (!read(std::string(prefix) + d.program().path, d.program(), shader_,
              e) ||
        shader_.size() % 4)
      return fail(e, "Canvas real PICA bytes rejected");
    dvlb_ = DVLB_ParseFile(reinterpret_cast<uint32_t *>(shader_.data()),
                           shader_.size());
    if (!dvlb_ || dvlb_->numDVLE != 1)
      return fail(e, "Canvas real PICA vertex program rejected");
    shaderProgramInit(&program_);
    initialized_ = true;
    if (R_FAILED(shaderProgramSetVsh(&program_, &dvlb_->DVLE[0])))
      return fail(e, "Canvas PICA program initialization failed");
    projection_ =
        shaderInstanceGetUniformLocation(program_.vertexShader, "projection");
    if (projection_ < 0)
      return fail(e, "Canvas PICA projection unavailable");
    capacity_ = d.records().size() * 6;
    buffer_ = static_cast<Vertex *>(linearAlloc(capacity_ * sizeof(Vertex)));
    if (!buffer_)
      return fail(e, "Canvas GPU vertex allocation failed");
    static_assert(sizeof(Vertex) == 24, "Canvas PICA vertex ABI");
    AttrInfo_Init(&attributes_);
    AttrInfo_AddLoader(&attributes_, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(&attributes_, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(&attributes_, 2, GPU_UNSIGNED_BYTE, 4);
    BufInfo_Init(&buffers_);
    if (BufInfo_Add(&buffers_, buffer_, sizeof(Vertex), 3, 0x210) < 0)
      return fail(e, "Canvas GPU vertex layout rejected");
    data_ = &d;
    e.clear();
    return true;
  }
  void issue(uint32_t texture, size_t first, size_t count, float width,
             float height) {
    auto image =
        encore::ctr::loading_sprite_sheet_get_image(sheets_.at(texture), 0);
    GSPGPU_FlushDataCache(buffer_ + first, count * sizeof(Vertex));
    C2D_Flush();
    C3D_BindProgram(&program_);
    C3D_SetAttrInfo(&attributes_);
    C3D_SetBufInfo(&buffers_);
    C3D_Mtx projection;
    Mtx_OrthoTilt(&projection, 0, width, height, 0, 1, -1, true);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_, &projection);
    for (unsigned i = 0; i < 6; ++i)
      C3D_TexEnvInit(C3D_GetTexEnv(i));
    auto *env = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
                  GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    C3D_TexBind(0, image.tex);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(true, GPU_GREATER, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_DrawArrays(GPU_TRIANGLES, int(first), int(count));
    C2D_Prepare();
  }

public:
  using Delegate = std::function<bool(const Draw &, encore::upstream::Vec2,
                                      float, float, std::string &)>;
  FieldCanvasArtRenderer() = default;
  FieldCanvasArtRenderer(const FieldCanvasArtRenderer &) = delete;
  FieldCanvasArtRenderer &operator=(const FieldCanvasArtRenderer &) = delete;
  ~FieldCanvasArtRenderer() { free(); }
  bool load(const Data &d, const char *prefix, std::string &e) {
    FieldCanvasArtRenderer next;
    if (!next.load_impl(d, prefix, e))
      return false;
    using std::swap;
    swap(data_, next.data_);
    swap(sheets_, next.sheets_);
    swap(shader_, next.shader_);
    swap(dvlb_, next.dvlb_);
    swap(program_, next.program_);
    swap(initialized_, next.initialized_);
    swap(projection_, next.projection_);
    swap(buffer_, next.buffer_);
    swap(capacity_, next.capacity_);
    swap(used_, next.used_);
    swap(attributes_, next.attributes_);
    swap(buffers_, next.buffers_);
    return true;
  }
  // Only after actual GPU frame begin/fence. Each draw consumes a unique linear
  // vertex region until the next frame, including calls interleaved by
  // delegates.
  void begin_frame() { used_ = 0; }
  const Data *data() const { return data_; }
  bool image(uint32_t texture, C2D_Image &out, std::string &e) const {
    auto i = sheets_.find(texture);
    if (!data_ || !data_->texture(texture) || i == sheets_.end())
      return fail(e, "Canvas borrowed checked texture unavailable");
    out = encore::ctr::loading_sprite_sheet_get_image(i->second, 0);
    if (!out.tex || !out.subtex)
      return fail(e, "Canvas actual GPU texture owner expired");
    e.clear();
    return true;
  }
  bool draw(const std::vector<Draw> &commands, encore::upstream::Vec2 camera,
            float width, float height, const Delegate &delegate,
            std::string &e) {
    if (!data_ || !buffer_ || !std::isfinite(camera.x) ||
        !std::isfinite(camera.y) || !std::isfinite(width) ||
        !std::isfinite(height) || width <= 0 || height <= 0 || width > 4096 ||
        height > 4096)
      return fail(e, "Canvas actual GPU viewport/camera rejected");
    uint32_t texture = 0;
    size_t first = used_, count = 0;
    uint64_t previous = 0;
    int32_t z = 0;
    bool any = false;
    auto flush = [&]() {
      if (count) {
        issue(texture, first, count, width, height);
        count = 0;
        first = used_;
      }
    };
    for (const auto &p : commands) {
      auto *r = data_->record(p.source);
      auto *t = data_->texture(p.texture);
      if (!r || !p.object || p.owner != r->owner || p.shader != r->shader ||
          p.owner_source != r->owner_id ||
          (any && (p.z < z || p.order <= previous)))
        return fail(e, "Canvas ordered command/owner/source rejected");
      any = true;
      previous = p.order;
      z = p.z;
      if (p.action == Action::Delegate) {
        flush();
        if (p.owner == encore::upstream::FieldCanvasOwner::Native ||
            !p.owner_object)
          return fail(
              e, "Canvas native command cannot impersonate typed delegate");
        if (!delegate || !delegate(p, camera, width, height, e))
          return fail(e,
                      "Canvas actual shader owner renderer unbound/rejected");
        continue;
      }
      if (!t || p.action != Action::Default ||
          p.shader != encore::upstream::FieldCanvasShader::Default)
        return fail(e, "Canvas source shader cannot be substituted by default "
                       "GPU pipeline");
      if (capacity_ - used_ < 6)
        return fail(e, "Canvas GPU frame capacity exhausted");
      const auto &f = p.source_rect;
      for (float v : f)
        if (!std::isfinite(v))
          return fail(e, "Canvas GPU source UV nonfinite");
      if (f[0] < 0 || f[1] < 0 || f[2] <= 0 || f[3] <= 0 ||
          f[0] + f[2] > t->width || f[1] + f[3] > t->height)
        return fail(e, "Canvas GPU source UV bounds rejected");
      if (count && texture != p.texture)
        flush();
      if (!count) {
        texture = p.texture;
        first = used_;
      }
      auto image =
          encore::ctr::loading_sprite_sheet_get_image(sheets_.at(p.texture), 0);
      const auto &s = *image.subtex;
      const float du = (s.right - s.left) / t->width,
                  dv = (s.bottom - s.top) / t->height;
      std::array<float, 2> u{s.left + f[0] * du, s.left + (f[0] + f[2]) * du},
          v{s.top + f[1] * dv, s.top + (f[1] + f[3]) * dv};
      if (p.flip_h)
        std::swap(u[0], u[1]);
      if (p.flip_v)
        std::swap(v[0], v[1]);
      for (float c : p.color)
        if (!std::isfinite(c) || c < 0 || c > 1)
          return fail(e, "Canvas GPU color rejected");
      auto channel = [](float c) {
        return uint32_t(std::floor(c * 255 + .5f));
      };
      uint32_t color = channel(p.color[0]) | channel(p.color[1]) << 8 |
                       channel(p.color[2]) << 16 | channel(p.color[3]) << 24;
      std::array<Vertex, 4> corners{};
      for (size_t i = 0; i < 4; ++i) {
        float x = p.vertices[i].x - camera.x, y = p.vertices[i].y - camera.y;
        if (!std::isfinite(x) || !std::isfinite(y))
          return fail(e, "Canvas GPU affine corner rejected");
        if (p.pixel_snap) {
          x = std::floor(x + .5f);
          y = std::floor(y + .5f);
        }
        corners[i] = {x, y, 0, u[i == 1 || i == 2], v[i >= 2], color};
      }
      for (size_t i :
           {size_t(0), size_t(1), size_t(2), size_t(0), size_t(2), size_t(3)})
        buffer_[used_++] = corners[i];
      count += 6;
    }
    flush();
    e.clear();
    return true;
  }
  void free() {
    if (!sheets_.empty() || buffer_ || initialized_)
      C3D_FrameSync();
    if (buffer_)
      linearFree(buffer_);
    buffer_ = nullptr;
    capacity_ = used_ = 0;
    for (auto &row : sheets_)
      encore::ctr::loading_sprite_sheet_free(row.second);
    sheets_.clear();
    if (initialized_)
      shaderProgramFree(&program_);
    initialized_ = false;
    if (dvlb_)
      DVLB_Free(dvlb_);
    dvlb_ = nullptr;
    shader_.clear();
    projection_ = -1;
    data_ = nullptr;
  }
};
