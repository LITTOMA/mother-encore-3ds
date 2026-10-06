#pragma once
#include "battle_renderer.hpp"
#include "encore/field_shop.hpp"
#include "field_payphone_renderer.hpp"
#include "source_font_renderer.hpp"
struct FieldShopRenderHost {
  // Admit real shared ItemDetails/TextTools and party source identity mappings.
  // This is mandatory: unresolved name controls/inline images must not become
  // literal text or a generic description label.
  std::function<bool(const encore::upstream::FieldShopData &, std::string &)>
      bind;
  std::function<bool(uint32_t, std::string &, std::string &)> character_source;
  std::function<bool(const std::string &, uint32_t, uint32_t, std::string &,
                     std::string &)>
      text;
  std::function<bool(const encore::upstream::FieldShopRuntime &,
                     const std::array<float, 4> &, std::string &)>
      description;
};
class FieldShopRenderer {
  const encore::upstream::FieldShopData *data_ = nullptr;
  FieldShopRenderHost host_;
  std::vector<encore::ctr::LoadingSpriteSheet> sheets_;
  static bool fail(std::string &e, const char *s) {
    e = s;
    return false;
  }
  bool region(uint32_t id, float u, float v, float sw, float sh, float x,
              float y, float w, float h, float depth, uint32_t tint) const {
    if (id >= sheets_.size() || sw <= 0 || sh <= 0 || w <= 0 || h <= 0)
      return false;
    auto image = encore::ctr::loading_sprite_sheet_get_image(sheets_[id], 0);
    auto sub = *image.subtex;
    const auto &t = data_->textures()[id];
    const float du = (sub.right - sub.left) / t.width,
                dv = (sub.top - sub.bottom) / t.height;
    sub.left += u * du;
    sub.right = sub.left + sw * du;
    sub.top -= v * dv;
    sub.bottom = sub.top - sh * dv;
    sub.width = uint16_t(sw);
    sub.height = uint16_t(sh);
    image.subtex = &sub;
    C2D_ImageTint c;
    C2D_PlainImageTint(&c, tint, 0);
    return C2D_DrawImageAt(image, std::floor(x + .5f), std::floor(y + .5f),
                           depth, &c, w / sw, h / sh);
  }
  bool patch(uint32_t id, const std::array<float, 4> &r,
             const std::array<float, 4> &p, float depth,
             uint32_t tint = 0xffffffff) const {
    if (id >= sheets_.size())
      return false;
    const auto &t = data_->textures()[id];
    float dx[] = {r[0], r[0] + p[0], r[2] - p[2], r[2]},
          dy[] = {r[1], r[1] + p[1], r[3] - p[3], r[3]},
          sx[] = {0, p[0], float(t.width) - p[2], float(t.width)},
          sy[] = {0, p[1], float(t.height) - p[3], float(t.height)};
    if (dx[2] < dx[1] || dy[2] < dy[1] || sx[2] < sx[1] || sy[2] < sy[1])
      return false;
    for (unsigned j = 0; j < 3; ++j)
      for (unsigned i = 0; i < 3; ++i)
        if (dx[i + 1] > dx[i] && dy[j + 1] > dy[j] &&
            !region(id, sx[i], sy[j], sx[i + 1] - sx[i], sy[j + 1] - sy[j],
                    dx[i], dy[j], dx[i + 1] - dx[i], dy[j + 1] - dy[j], depth,
                    tint))
          return false;
    return true;
  }
  static std::array<float, 4> shift(std::array<float, 4> r, float x, float y) {
    r[0] += x;
    r[2] += x;
    r[1] += y;
    r[3] += y;
    return r;
  }
  static uint32_t color(const std::array<float, 4> &c) {
    uint32_t r = 0;
    for (unsigned i = 0; i < 4; ++i)
      r |= uint32_t(std::round(c[i] * 255)) << (8 * i);
    return r;
  }
  bool label(const BattleRenderer &font, const std::string &s,
             const std::array<float, 4> &r, uint32_t align, uint32_t col,
             float fontheight = 0, uint32_t valign = 0) const {
    const float width = font.text_width(s.c_str());
    float x = r[0];
    if (align == 1)
      x += (r[2] - r[0] - width) * .5f;
    else if (align == 2)
      x = r[2] - width;
    float y = r[1];
    if (valign == 1)
      y += (r[3] - r[1] - fontheight) * .5f;
    else if (valign == 2)
      y = r[3] - fontheight;
    return font.draw_text_clipped(s.c_str(), x, y, r[0], r[1], r[2], r[3], col);
  }
  bool cursor(float x, float y, double time, float depth, std::string &e,
              float rotation = 0) const {
    const auto &frames = data_->cursor_frames();
    if (!std::isfinite(time) || time < 0 || frames.empty())
      return fail(e, "Shop source cursor time rejected");
    const auto frame = frames[size_t(std::floor(time * data_->cursor_speed())) %
                              frames.size()];
    const auto &size = data_->cursor_size();
    auto id = data_->texture_role("cursor");
    auto image = encore::ctr::loading_sprite_sheet_get_image(sheets_[id], 0);
    auto sub = *image.subtex;
    const auto &t = data_->textures()[id];
    const float du = (sub.right - sub.left) / t.width,
                dv = (sub.top - sub.bottom) / t.height;
    sub.left += frame * size[0] * du;
    sub.right = sub.left + size[0] * du;
    sub.bottom = sub.top - size[1] * dv;
    sub.width = size[0];
    sub.height = size[1];
    image.subtex = &sub;
    return C2D_DrawImageAtRotated(image, std::floor(x + .5f),
                                  std::floor(y + .5f), depth, rotation, nullptr,
                                  1, 1);
  }

public:
  FieldShopRenderer() = default;
  FieldShopRenderer(const FieldShopRenderer &) = delete;
  FieldShopRenderer &operator=(const FieldShopRenderer &) = delete;
  ~FieldShopRenderer() { free(); }
  void free() {
    for (auto *s : sheets_)
      encore::ctr::loading_sprite_sheet_free(s);
    sheets_.clear();
    data_ = nullptr;
    host_ = {};
  }
  bool load(const encore::upstream::FieldShopData &d, const std::string &prefix,
            const std::vector<std::array<uint32_t, 8>> &palettes, uint32_t base,
            FieldShopRenderHost h, std::string &e) {
    free();
    if (!d.valid() || prefix.empty() || !h.bind || !h.character_source ||
        !h.text || !h.description || !h.bind(d, e))
      return fail(e, "Shop source rendering host unbound");
    std::vector<std::string> paths;
    for (const auto &t : d.textures())
      if (t.flavor)
        paths.push_back(t.path);
    if (!encore::ctr::loading_menu_flavor_register_checked_paths(
            paths, d.source_colors(), palettes, d.threshold(), base))
      return fail(e, "Shop actual flavor shader source binding rejected");
    for (const auto &t : d.textures()) {
      FILE *f = std::fopen((prefix + t.path).c_str(), "rb");
      if (!f) {
        free();
        return fail(e, "Shop actual atlas unavailable");
      }
      std::vector<uint8_t> raw(t.bytes);
      bool ok = std::fread(raw.data(), 1, raw.size(), f) == raw.size() &&
                std::fgetc(f) == EOF && !std::ferror(f);
      if (std::fclose(f))
        ok = false;
      if (!ok || field_payphone_renderer_detail::sha256(raw.data(),
                                                        raw.size()) != t.sha) {
        free();
        return fail(e, "Shop actual atlas bytes/SHA rejected");
      }
      auto *s = encore::ctr::loading_sprite_sheet_acquire_memory(
          (prefix + t.path).c_str(), raw, e);
      if (!s) {
        free();
        return false;
      }
      auto image = encore::ctr::loading_sprite_sheet_get_image(s, 0);
      if (encore::ctr::loading_sprite_sheet_count(s) != 1 || !image.tex ||
          !image.subtex || Tex3DS_SubTextureRotated(image.subtex) ||
          image.subtex->width != t.width || image.subtex->height != t.height ||
          image.tex->fmt != GPU_RGBA8) {
        encore::ctr::loading_sprite_sheet_free(s);
        free();
        return fail(e, "Shop GPU image source extent/format rejected");
      }
      C3D_TexSetFilter(&s->texture, GPU_NEAREST, GPU_NEAREST);
      C3D_TexSetWrap(&s->texture, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
      s->texture.border = 0;
      sheets_.push_back(s);
    }
    data_ = &d;
    host_ = std::move(h);
    return true;
  }
  // Four source fonts are already admitted outside the GPU frame. origin is
  // the application's checked 1:1 UI adaptation (default top 400x240). We do
  // not scale source panels or select/reload fonts inside drawing.
  bool
  draw(const encore::upstream::FieldShopRuntime &r,
       const std::array<const BattleRenderer *, 4> &fonts,
       const std::array<const encore::ctr::SourceFontRenderer *, 4> &sources,
       float originx, float originy, double idle_time, float depth,
       uint32_t text_color, uint32_t restricted_color, uint32_t highlight_color,
       std::string &e) const {
    using Phase = encore::upstream::FieldShopPhase;
    const auto phase = r.phase();
    if (phase == Phase::Closed)
      return true;
    if (!data_ || r.data() != data_ || !std::isfinite(originx) ||
        !std::isfinite(originy) || !std::isfinite(depth) || depth < 0 ||
        depth > 1)
      return fail(e, "Shop source GPU live binding rejected");
    for (unsigned i = 0; i < 4; ++i)
      if (!fonts[i] || !sources[i] || !sources[i]->catalog().selected() ||
          sources[i]->catalog().selected()->source != data_->fonts()[i])
        return fail(e, "Shop actual source font binding rejected");
    const auto &v = data_->layout();
    const auto &a = data_->aux();
    const auto &p = data_->panels();
    originy += v[43];
    const float sx = originx + v[0], sy = originy + v[1];
    auto panel = [&](size_t i, float x, float y) {
      return patch(p[i].texture, shift(p[i].rect, x, y), p[i].patch, depth);
    };
    if (!panel(0, sx, sy) || !panel(1, sx, sy) || !panel(4, originx, originy) ||
        !panel(5, originx, originy) || !panel(7, sx, sy))
      return fail(e, "Shop source panel submission failed");
    std::string text;
    if (!host_.text(data_->text_key(encore::upstream::FieldShopTextRole::Title),
                    0, 0, text, e) ||
        !label(*fonts[0], text, shift(p[1].rect, sx, sy), uint32_t(a[13][0]),
               text_color, sources[0]->catalog().selected()->height,
               uint32_t(a[13][1])))
      return fail(e, "Shop actual title draw failed");
    std::string cash = std::to_string(r.snapshot().cash);
    if (cash.size() < data_->cash_digits())
      cash.insert(0, data_->cash_digits() - cash.size(), '0');
    auto cashrect =
        shift(a[0], sx + p[7].rect[0] + a[15][0], sy + p[7].rect[1] + a[15][1]);
    if (!label(*fonts[3], cash, cashrect, uint32_t(a[14][2]), text_color,
               sources[3]->catalog().selected()->height, uint32_t(a[14][3])))
      return fail(e, "Shop source cash font draw failed");
    for (unsigned j = 1; j <= 2; ++j) {
      auto id = data_->texture_role(j == 1 ? "dollar_left" : "dollar_right");
      const auto &t = data_->textures()[id];
      auto rect = shift(a[j], sx + p[7].rect[0] + a[15][0],
                        sy + p[7].rect[1] + a[15][1]);
      if (!region(id, 0, 0, t.width, t.height, rect[0], rect[1], t.width,
                  t.height, depth, 0xffffffff))
        return fail(e, "Shop source currency image draw failed");
    }
    for (unsigned j = 0; j < (data_->can_sell() ? 2u : 1u); ++j) {
      auto rect = shift({v[10], v[11] + float(j) * v[46], v[12],
                         v[11] + float(j) * v[46] + a[10][j]},
                        originx + p[5].rect[0], originy + p[5].rect[1]);
      if (phase == Phase::BuySell && r.main_selected() == j &&
          !C2D_DrawRectSolid(rect[0], rect[1], depth, rect[2] - rect[0],
                             rect[3] - rect[1], highlight_color))
        return fail(e, "Shop main source highlight draw failed");
      if (!host_.text(
              data_->text_key(j ? encore::upstream::FieldShopTextRole::Sell
                                : encore::upstream::FieldShopTextRole::Buy),
              0, 0, text, e) ||
          !label(*fonts[1], text, rect, 0, text_color))
        return fail(e, "Shop source main label failed");
    }
    if (phase == Phase::BuySell &&
        !cursor(originx + p[5].rect[0] + v[39],
                originy + p[5].rect[1] + v[40] + r.main_selected() * v[46],
                idle_time, depth, e))
      return false;
    const bool selling = phase == Phase::Sell || phase == Phase::ConfirmSell;
    const bool list = phase != Phase::BuySell;
    const bool confirm = phase == Phase::ConfirmBuy ||
                         phase == Phase::ConfirmSell ||
                         phase == Phase::Insufficient;
    if (list) {
      const size_t index = selling ? 3 : 2;
      if (!panel(index, sx, sy))
        return fail(e, "Shop source list panel failed");
      const float x = sx + v[4], y = sy + p[index].rect[1] + v[5],
                  stride = v[8] + v[9];
      auto separator = shift(a[17], sx, sy + p[index].rect[1]);
      if (!C2D_DrawRectSolid(separator[0], separator[1], depth,
                             separator[2] - separator[0],
                             separator[3] - separator[1], text_color))
        return fail(e, "Shop source list separator failed");
      for (uint32_t n = 0; n < data_->lines() && n + r.page() < r.rows().size();
           ++n) {
        const uint32_t at = n + r.page();
        const auto &item = r.rows()[at];
        auto *d = data_->policy(item.definition);
        const bool restricted = r.restricted(at);
        auto rect = std::array<float, 4>{x, y + n * stride, x + v[45],
                                         y + n * stride + v[8]};
        if (at == r.selected() && !restricted &&
            !C2D_DrawRectSolid(rect[0], rect[1], depth, rect[2] - rect[0],
                               rect[3] - rect[1], highlight_color))
          return fail(e, "Shop actual selected row highlight failed");
        if (!host_.text(d->name_key, d->id, item.doses, text, e))
          return false;
        auto nameRect = rect;
        nameRect[2] = x + v[44];
        if (!label(*fonts[1], text, nameRect, uint32_t(a[13][2]),
                   restricted ? restricted_color : text_color,
                   sources[1]->catalog().selected()->height,
                   uint32_t(a[13][3])))
          return fail(e, "Shop source item name draw failed");
        auto priceRect = rect;
        priceRect[0] = nameRect[2];
        if (!label(*fonts[2], std::to_string(r.price(at)), priceRect,
                   uint32_t(a[14][0]), text_color,
                   sources[2]->catalog().selected()->height,
                   uint32_t(a[14][1])))
          return fail(e, "Shop actual price draw failed");
      }
      if (!confirm && !r.rows().empty() &&
          !cursor(sx + v[35] + v[37],
                  sy + p[index].rect[1] + v[36] + v[38] +
                      (r.selected() - r.page()) * stride,
                  idle_time, depth, e))
        return false;
      if (r.rows().size() > data_->lines()) {
        auto sr = shift(a[3], sx, sy + p[index].rect[1]);
        auto bg = std::array<float, 4>{sr[0] + a[4][0], sr[1] + a[4][1],
                                       sr[2] + a[4][2], sr[3] + a[4][3]};
        if (!patch(data_->texture_role("scroll_bg"), bg, a[6], depth,
                   color(a[18])))
          return fail(e, "Shop source scrollbar background failed");
        const float body = bg[3] - bg[1],
                    top = bg[1] + body * r.page() / r.rows().size();
        auto thumb =
            std::array<float, 4>{bg[0] + a[5][0], top, bg[2] + a[5][2],
                                 top + body * data_->lines() / r.rows().size()};
        if (!patch(data_->texture_role("scroll_thumb"), thumb, a[7], depth))
          return fail(e, "Shop source scrollbar thumb failed");
        const float cx = (sr[0] + sr[2]) * .5f + a[16][2];
        if (r.page() > 0 && !cursor(cx, sr[1] + a[8][1] + a[16][3], idle_time,
                                    depth, e, a[16][0]))
          return false;
        if (r.page() + data_->lines() < r.rows().size() &&
            !cursor(cx, sr[3] + a[9][1] + a[16][3], idle_time, depth, e,
                    a[16][1]))
          return false;
      }
      if (!r.rows().empty()) {
        if (!panel(6, originx, originy))
          return fail(e, "Shop source description box failed");
        if (!host_.description(r, shift(p[6].rect, originx, originy), e))
          return false;
      }
    }
    const auto &order = r.snapshot().natural_order;
    const size_t half = data_->portraits().size() / 2;
    for (size_t j = 0; j < order.size() && j < data_->portrait_limit(); ++j) {
      std::string name;
      if (!host_.character_source(order[j], name, e))
        return false;
      auto it = std::find_if(
          data_->portraits().begin(), data_->portraits().begin() + half,
          [&](const auto &entry) { return entry.character == name; });
      if (it == data_->portraits().begin() + half)
        return fail(e, "Shop actual party portrait mapping unavailable");
      auto id = it->texture;
      if (order[j] == r.owner())
        id = data_->portraits()[size_t(it - data_->portraits().begin()) + half]
                 .texture;
      const auto &t = data_->textures()[id];
      const float x = originx + p[4].rect[0] + v[14] +
                      j * (v[16] - v[14] + v[18]),
                  y = originy + p[4].rect[1] + v[15] +
                      (v[17] - v[15] - t.height) * .5f;
      if (!region(id, 0, 0, t.width, t.height, x, y, t.width, t.height, depth,
                  0xffffffff))
        return fail(e, "Shop actual source portrait draw failed");
    }
    e.clear();
    return true;
  }
};
