#pragma once
#include "encore/field_dialogue_visual.hpp"
#include "loading_texture.hpp"
#include <cmath>
#include <fstream>
#include <utility>
#include <vector>
class FieldDialogueVisualRenderer {
  struct Vertex {
    float x, y, z, u, v;
    uint32_t color;
  };
  const encore::upstream::FieldDialogueVisualData *visual_ = nullptr;
  const encore::upstream::FieldCameraArrowsData *data_ = nullptr;
  encore::ctr::LoadingSpriteSheet sheet_ = nullptr;
  std::vector<uint8_t> shader_;
  DVLB_s *dvlb_ = nullptr;
  shaderProgram_s program_{};
  bool initialized_ = false;
  int projection_ = -1;
  Vertex *buffer_ = nullptr;
  size_t count_ = 0, capacity_ = 0;
  C3D_AttrInfo attributes_{};
  C3D_BufInfo buffers_{};
  static uint32_t crc(const std::vector<uint8_t> &b) {
    uint32_t c = ~0u;
    for (uint8_t v : b) {
      c ^= v;
      for (unsigned i = 0; i < 8; ++i)
        c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0u);
    }
    return ~c;
  }
  static bool read(const std::string &path,
                   const encore::upstream::FieldArrowAsset &a,
                   std::vector<uint8_t> &out, std::string &e) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
      e = "Camera arrows GPU resource unavailable";
      return false;
    }
    f.seekg(0, std::ios::end);
    if (f.tellg() != a.bytes || !a.bytes || a.bytes > 16 * 1024 * 1024) {
      e = "Camera arrows GPU byte count rejected";
      return false;
    }
    f.seekg(0);
    out.resize(a.bytes);
    if (!f.read(reinterpret_cast<char *>(out.data()), a.bytes) ||
        crc(out) != a.crc) {
      e = "Camera arrows GPU resource CRC rejected";
      return false;
    }
    return true;
  }

public:
  FieldDialogueVisualRenderer() = default;
  FieldDialogueVisualRenderer(const FieldDialogueVisualRenderer &) = delete;
  FieldDialogueVisualRenderer &
  operator=(const FieldDialogueVisualRenderer &) = delete;
  ~FieldDialogueVisualRenderer() { free(); }
  bool load(const encore::upstream::FieldDialogueVisualData &d,
            const char *prefix, std::string &e) {
    FieldDialogueVisualRenderer next;
    next.visual_ = &d;
    if (!d.valid() || !next.load_impl(d.arrows(), prefix, e))
      return false;
    using std::swap;
    swap(visual_, next.visual_);
    swap(data_, next.data_);
    swap(sheet_, next.sheet_);
    swap(shader_, next.shader_);
    swap(dvlb_, next.dvlb_);
    swap(program_, next.program_);
    swap(initialized_, next.initialized_);
    swap(projection_, next.projection_);
    swap(buffer_, next.buffer_);
    swap(count_, next.count_);
    swap(capacity_, next.capacity_);
    swap(attributes_, next.attributes_);
    swap(buffers_, next.buffers_);
    return true;
  }
  bool load_impl(const encore::upstream::FieldCameraArrowsData &d,
                 const char *prefix, std::string &e) {
    if (!d.valid() || !prefix) {
      e = "Camera arrows checked source data required";
      return false;
    }
    std::vector<uint8_t> image, shader;
    if (!read(std::string(prefix) + d.asset().path, d.asset(), image, e) ||
        !read(std::string(prefix) + d.program().path, d.program(), shader, e))
      return false;
    if (shader.size() % 4) {
      e = "Camera arrows PICA word alignment rejected";
      return false;
    }
    auto *visual = visual_;
    free();
    visual_ = visual;
    shader_ = std::move(shader);
    dvlb_ = DVLB_ParseFile(reinterpret_cast<uint32_t *>(shader_.data()),
                           shader_.size());
    if (!dvlb_ || dvlb_->numDVLE != 1) {
      e = "Camera arrows PICA vertex program rejected";
      free();
      return false;
    }
    shaderProgramInit(&program_);
    initialized_ = true;
    if (R_FAILED(shaderProgramSetVsh(&program_, &dvlb_->DVLE[0]))) {
      e = "Camera arrows vertex shader initialization failed";
      free();
      return false;
    }
    projection_ =
        shaderInstanceGetUniformLocation(program_.vertexShader, "projection");
    if (projection_ < 0) {
      e = "Camera arrows PICA projection missing";
      free();
      return false;
    }
    sheet_ = encore::ctr::loading_sprite_sheet_acquire(
        (std::string(prefix) + d.asset().path).c_str(), &e);
    if (!sheet_ || encore::ctr::loading_sprite_sheet_count(sheet_) != 1) {
      e = "Camera arrows GPU atlas rejected";
      free();
      return false;
    }
    auto im = encore::ctr::loading_sprite_sheet_get_image(sheet_, 0);
    if (!im.tex || !im.subtex || Tex3DS_SubTextureRotated(im.subtex) ||
        im.subtex->width != d.asset().width ||
        im.subtex->height != d.asset().height) {
      e = "Camera arrows source atlas extent rejected";
      free();
      return false;
    }
    C3D_TexSetFilter(im.tex, GPU_NEAREST, GPU_NEAREST);
    capacity_ = (d.sprites().size() + visual_->cursors().size()) * 6;
    buffer_ = static_cast<Vertex *>(linearAlloc(capacity_ * sizeof(Vertex)));
    if (!buffer_) {
      e = "Camera arrows GPU vertex buffer allocation failed";
      free();
      return false;
    }
    static_assert(sizeof(Vertex) == 24, "Camera arrows vertex ABI");
    AttrInfo_Init(&attributes_);
    AttrInfo_AddLoader(&attributes_, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(&attributes_, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(&attributes_, 2, GPU_UNSIGNED_BYTE, 4);
    BufInfo_Init(&buffers_);
    if (BufInfo_Add(&buffers_, buffer_, sizeof(Vertex), 3, 0x210) < 0) {
      e = "Camera arrows GPU attribute layout rejected";
      free();
      return false;
    }
    data_ = &d;
    count_ = 0;
    e.clear();
    return true;
  }
  // Call only after the actual frame fence/begin. A unique region is consumed
  // per source arrow draw; no in-flight vertices are overwritten in a frame.
  void begin_frame() { count_ = 0; }
  bool draw(const encore::upstream::FieldArrowDraw &p, float camera_x,
            float camera_y, float depth = 0) {
    if (!data_ || !sheet_ || !buffer_ ||
        (!data_->sprite(p.id) && !visual_->cursor(p.id)) ||
        p.frame >= data_->frames().size() || !std::isfinite(camera_x) ||
        !std::isfinite(camera_y) || !std::isfinite(depth))
      return false;
    if (!p.visible)
      return true;
    if (capacity_ - count_ < 6)
      return false;
    auto f = data_->frames()[p.frame];
    auto im = encore::ctr::loading_sprite_sheet_get_image(sheet_, 0);
    const auto &s = *im.subtex;
    float du = (s.right - s.left) / data_->asset().width,
          dv = (s.bottom - s.top) / data_->asset().height;
    std::array<float, 2> u{s.left + f[0] * du, s.left + (f[0] + f[2]) * du},
        v{s.top + f[1] * dv, s.top + (f[1] + f[3]) * dv};
    for (float c : p.color)
      if (!std::isfinite(c) || c < 0 || c > 1)
        return false;
    auto channel = [](float c) { return uint32_t(std::floor(c * 255 + .5f)); };
    uint32_t color = channel(p.color[0]) | channel(p.color[1]) << 8 |
                     channel(p.color[2]) << 16 | channel(p.color[3]) << 24;
    std::array<Vertex, 4> corners{};
    for (size_t i = 0; i < 4; ++i) {
      float x = p.world_vertices[i].x - camera_x,
            y = p.world_vertices[i].y - camera_y;
      if (!std::isfinite(x) || !std::isfinite(y))
        return false;
      if (p.pixel_snap) {
        x = std::floor(x + .5f);
        y = std::floor(y + .5f);
      }
      corners[i] = {x, y, depth, u[i == 1 || i == 2], v[i >= 2], color};
    }
    const size_t first = count_;
    for (size_t i :
         {size_t(0), size_t(1), size_t(2), size_t(0), size_t(2), size_t(3)})
      buffer_[count_++] = corners[i];
    GSPGPU_FlushDataCache(buffer_ + first, 6 * sizeof(Vertex));
    C2D_Flush();
    C3D_BindProgram(&program_);
    C3D_SetAttrInfo(&attributes_);
    C3D_SetBufInfo(&buffers_);
    C3D_Mtx projection;
    Mtx_OrthoTilt(&projection, 0, 400, 240, 0, 1, -1, true);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_, &projection);
    for (unsigned i = 0; i < 6; ++i)
      C3D_TexEnvInit(C3D_GetTexEnv(i));
    auto *env = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
                  GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
    C3D_TexBind(0, im.tex);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(true, GPU_GREATER, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_DrawArrays(GPU_TRIANGLES, int(first), 6);
    C2D_Prepare();
    C3D_AlphaTest(true, GPU_GREATER, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
    return true;
  }
  void free() {
    if (sheet_ || buffer_ || initialized_)
      C3D_FrameSync();
    if (buffer_)
      linearFree(buffer_);
    buffer_ = nullptr;
    capacity_ = count_ = 0;
    if (sheet_)
      encore::ctr::loading_sprite_sheet_free(sheet_);
    sheet_ = nullptr;
    if (initialized_)
      shaderProgramFree(&program_);
    initialized_ = false;
    if (dvlb_)
      DVLB_Free(dvlb_);
    dvlb_ = nullptr;
    shader_.clear();
    projection_ = -1;
    data_ = nullptr;
    visual_ = nullptr;
  }
};
