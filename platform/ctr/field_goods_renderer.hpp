#pragma once
#include "encore/field_goods.hpp"
#include "field_equipment_renderer.hpp"
#include <cstdio>
#include <map>
// Complete original Goods presentation. Texture owner, palette and font owners
// must outlive this renderer; no CPU framebuffer sampling or rule tables.
class FieldGoodsRenderer {
  using Menu = encore::upstream::FieldGoodsMenu;
  using Data = encore::upstream::FieldGoodsData;
  using Phase = encore::upstream::FieldGoodsPhase;
  using Rect = encore::upstream::BattleValue;
  using ArtRole = encore::upstream::FieldLayoutRole;
  using Parameter = encore::upstream::FieldParameter;
  struct Texture {
    const encore::upstream::FieldGoodsTexture *data = nullptr;
    encore::ctr::LoadingSpriteSheet sheet = nullptr;
  };
  const Data *data_ = nullptr;
  const FieldEquipmentRenderer *art_ = nullptr;
  const ItemDetailsRenderer *details_ = nullptr;
  std::map<std::string, Texture> textures_;
  mutable std::string error_;
  mutable Rect clip_{};
  bool fail(const char *s) const {
    error_ = s;
    return false;
  }
  static float pixel(float x) { return std::floor(x + .5f); }
  static uint32_t color(const std::array<float, 4> &c) {
    return encore::ctr::loading_menu_flavor_color(
        C2D_Color32f(c[0], c[1], c[2], c[3]));
  }
  C2D_Image image(const std::string &k) const {
    auto i = textures_.find(k);
    return i == textures_.end() ? C2D_Image{}
                                : encore::ctr::loading_sprite_sheet_get_image(
                                      i->second.sheet, 0);
  }
  Rect layout(const std::string &k, float dx, float dy) const {
    auto a = *data_->layout(k);
    return {a[0] + dx, a[1] + dy, a[2], a[3]};
  }
  bool text(const BattleRenderer &font, const std::string &s, Rect r,
            uint32_t tint, bool right = false, bool center = false) const {
    if (r.z <= 0 || r.w <= 0)
      return true;
    float x = r.x;
    if (right)
      x += r.z - font.text_width(s.c_str());
    else if (center)
      x += (r.z - font.text_width(s.c_str())) / 2;
    auto x0 = std::max(r.x, clip_.x), y0 = std::max(r.y, clip_.y),
         x1 = std::min(r.x + r.z, clip_.x + clip_.z),
         y1 = std::min(r.y + r.w, clip_.y + clip_.w);
    return x1 <= x0 || y1 <= y0 ||
           font.draw_text_clipped(s.c_str(), pixel(x), pixel(r.y), x0, y0, x1,
                                  y1, tint) ||
           fail("Goods source font glyph unavailable");
  }
  bool region(C2D_Image im, float u, float v, float w, float h, Rect dst,
              uint32_t tint) const {
    if (!im.tex || !im.subtex)
      return fail("Goods actual atlas absent");
    if (w <= 0 || h <= 0 || dst.z <= 0 || dst.w <= 0)
      return true;
    const float x0 = std::max(pixel(dst.x), clip_.x),
                y0 = std::max(pixel(dst.y), clip_.y),
                x1 = std::min(pixel(dst.x) + dst.z, clip_.x + clip_.z),
                y1 = std::min(pixel(dst.y) + dst.w, clip_.y + clip_.w);
    if (x1 <= x0 || y1 <= y0)
      return true;
    auto sub = *im.subtex;
    const auto du = (sub.right - sub.left) / sub.width,
               dv = (sub.bottom - sub.top) / sub.height;
    const auto l = sub.left, t = sub.top;
    sub.left = l + (u + (x0 - pixel(dst.x)) * w / dst.z) * du;
    sub.right = l + (u + (x1 - pixel(dst.x)) * w / dst.z) * du;
    sub.top = t + (v + (y0 - pixel(dst.y)) * h / dst.w) * dv;
    sub.bottom = t + (v + (y1 - pixel(dst.y)) * h / dst.w) * dv;
    sub.width = uint16_t(pixel(x1 - x0));
    sub.height = uint16_t(pixel(y1 - y0));
    if (!sub.width || !sub.height)
      return true;
    C2D_ImageTint paint;
    C2D_PlainImageTint(&paint, tint, 0);
    return C2D_DrawImageAt({im.tex, &sub}, pixel(x0), pixel(y0), 0, &paint, 1,
                           1) ||
           fail("Goods GPU quad submission failed");
  }
  bool sprite(const std::string &k, Rect cell) const {
    auto im = image(k);
    if (!im.subtex)
      return fail("Goods source sprite missing");
    Rect d{cell.x + (cell.z - im.subtex->width) / 2,
           cell.y + (cell.w - im.subtex->height) / 2, float(im.subtex->width),
           float(im.subtex->height)};
    return region(im, 0, 0, d.z, d.w, d, C2D_Color32(255, 255, 255, 255));
  }
  bool nine(const std::string &k, Rect d, std::array<float, 4> patch,
            uint32_t tint = C2D_Color32(255, 255, 255, 255)) const {
    auto im = image(k);
    if (!im.subtex)
      return fail("Goods checked panel missing");
    auto w = float(im.subtex->width), h = float(im.subtex->height);
    if (patch[0] < 0 || patch[1] < 0 || patch[2] < 0 || patch[3] < 0 ||
        patch[0] + patch[2] > w || patch[1] + patch[3] > h ||
        patch[0] + patch[2] > d.z || patch[1] + patch[3] > d.w)
      return fail("Goods source panel patch invalid");
    float us[] = {0, patch[0], w - patch[2], w},
          vs[] = {0, patch[1], h - patch[3], h},
          xs[] = {d.x, d.x + patch[0], d.x + d.z - patch[2], d.x + d.z},
          ys[] = {d.y, d.y + patch[1], d.y + d.w - patch[3], d.y + d.w};
    for (unsigned y = 0; y < 3; ++y)
      for (unsigned x = 0; x < 3; ++x)
        if (!region(im, us[x], vs[y], us[x + 1] - us[x], vs[y + 1] - vs[y],
                    {xs[x], ys[y], xs[x + 1] - xs[x], ys[y + 1] - ys[y]}, tint))
          return false;
    return true;
  }
  bool borrowed(ArtRole role, Rect d,
                const std::array<float, 4> *patch = nullptr) const {
    Rect p;
    if (patch)
      p = {(*patch)[0], (*patch)[1], (*patch)[2], (*patch)[3]};
    return art_->draw_borrowed_art(role, d, 0, clip_.x, clip_.y, clip_.z,
                                   clip_.w, patch ? &p : nullptr) ||
           fail(art_->error().c_str());
  }
  bool cursor(const Menu &m, Rect row, std::array<float, 4> offset) const {
    auto p = *m.inventory()->data()->parameter("CursorCenter");
    Rect shape;
    if (!art_->borrowed_rect(ArtRole::PauseCursor, shape))
      return fail(art_->error().c_str());
    shape.x = row.x + offset[0] + p[0] - shape.z / 2;
    shape.y = row.y + offset[1] + p[1] - shape.w / 2;
    auto field = art_->content();
    auto n = uint32_t(
        std::fmod(m.cursor_time() * field.parameter(Parameter::CursorFps), 4.));
    auto frame = uint32_t(
        field.parameter(Parameter(uint32_t(Parameter::CursorFrame0) + n)));
    return art_->draw_borrowed_art(ArtRole::PauseCursor, shape, frame, clip_.x,
                                   clip_.y, clip_.z, clip_.w) ||
           fail(art_->error().c_str());
  }
  bool choice(const Menu &m, const BattleRenderer &font, Rect box,
              const std::string &title, const std::vector<std::string> &rows,
              const std::string &role, uint32_t selection, bool active) const {
    auto inset = *data_->parameter(role + "Inset"),
         patch = *data_->parameter(role + "Patch"),
         rowp = *data_->parameter("ActionRow");
    float max = 0;
    for (const auto &s : rows)
      max = std::max(max, font.text_width(s.c_str()));
    box.z = std::max(max + inset[0] + inset[2],
                     font.text_width(title.c_str()) + patch[0] + patch[2]);
    // Confirmation/Sort resize width only; source authored height persists.
    auto right = box.x + box.z;
    if (right > clip_.x + clip_.z)
      box.x -= right - (clip_.x + clip_.z);
    // Child inherits the single parent ActionSelect bounce.
    if (!borrowed(ArtRole::ItemsTargetPanel, box, &patch))
      return false;
    auto ti = *m.inventory()->data()->layout("TargetTitle");
    auto targetbase = *m.inventory()->data()->layout("Targets");
    Rect titlebox{box.x + ti.rect[0] - targetbase.rect[0],
                  box.y + ti.rect[1] - targetbase.rect[1],
                  box.z - patch[0] - patch[2], ti.rect[3]};
    if (!text(font, title, titlebox,
              color(*data_->parameter(role + "TitleColor"))))
      return false;
    for (size_t i = 0; i < rows.size(); ++i) {
      Rect r{box.x + inset[0], box.y + inset[1] + i * (rowp[0] + rowp[1]), max,
             rowp[1]};
      if (active && i == selection)
        BattleRenderer::draw_rect(r.x, r.y, r.z, r.w,
                                  color(*data_->parameter("Highlight")));
      if (!text(font, rows[i], r, C2D_Color32(255, 255, 255, 255)))
        return false;
      if (active && i == selection &&
          !cursor(m, r, *data_->parameter(role + "Cursor")))
        return false;
    }
    return true;
  }
  bool stats(const Menu &m, const BattleRenderer &font,
             const BattleRenderer &numbers, float dx, float dy) const {
    bool show = false;
    std::array<int64_t, 7> cur{}, projected{};
    if (!m.equipment_visual(show, cur, projected, error_))
      return false;
    if (!show)
      return true;
    const auto inv = m.inventory()->data()->layout("Inventory")->rect;
    auto box =
        layout("Stats", dx + inv[0], dy + inv[1] + inv[3] + m.stats_offset());
    box.z = inv[2];
    box.w = m.inventory()->data()->layout("Message")->rect[3];
    if (!nine("stats", box, *data_->parameter("StatsPatch")))
      return false;
    auto parent = *data_->parameter("StatsPortraitParent");
    auto portrait =
        layout("StatsPortrait", box.x + parent[0], box.y + parent[1]);
    if (!sprite("ninten", portrait))
      return false;
    auto body = *data_->layout("StatsNumbers"),
         style = *data_->parameter("StatValue"),
         div = *data_->parameter("StatDivider"),
         icon = *data_->parameter("StatIcon");
    auto start = box.x + box.z + body[0];
    float cell = (body[2] - div[0] * (cur.size() - 1)) / cur.size();
    if (cell < style[0])
      return fail("Goods source stats cell too narrow");
    for (size_t i = 0; i < cur.size(); ++i) {
      auto index = data_->stat_order()[i];
      Rect r{start + i * (cell + div[0]), box.y + body[1], cell, style[1]};
      if (!text(numbers, std::to_string(cur[index]), r,
                C2D_Color32(255, 255, 255, 255), true))
        return false;
      if (projected[index] != cur[index]) {
        Rect mod{r.x, r.y + style[2], r.z, style[3]};
        if (!text(numbers, std::to_string(projected[index]), mod,
                  C2D_Color32(255, 255, 255, 255), true))
          return false;
        auto im = image(projected[index] > cur[index] ? "up" : "down");
        Rect spr{mod.x + icon[0] - im.subtex->width / 2.f,
                 mod.y + icon[1] - im.subtex->height / 2.f,
                 float(im.subtex->width), float(im.subtex->height)};
        if (!region(im, 0, 0, spr.z, spr.w, spr,
                    C2D_Color32(255, 255, 255, 255)))
          return false;
      }
      if (i + 1 < cur.size())
        BattleRenderer::draw_rect(r.x + r.z, r.y + div[2], div[0], div[1],
                                  color(*data_->parameter("StatDividerColor")));
    }
    auto title = *data_->layout("StatsTitles"),
         min = *data_->parameter("StatTitleMin"),
         tdiv = *data_->parameter("StatTitleDivider");
    std::array<std::string, 7> labels;
    std::array<float, 7> widths{};
    float total = 0;
    for (size_t i = 0; i < labels.size(); ++i) {
      if (!m.source_label(data_->stat_label(i), labels[i], error_))
        return false;
      widths[i] = std::max(min[0], numbers.text_width(labels[i].c_str()));
      total += widths[i];
    }
    total += (labels.size() - 1) * (tdiv[0] + 2 * tdiv[3]);
    float x = box.x + box.z + title[0] + title[2] - total;
    for (size_t i = 0; i < labels.size(); ++i) {
      if (!text(numbers, labels[i],
                {x, box.y + title[1] + min[2], widths[i], min[1]},
                C2D_Color32(255, 255, 255, 255), true))
        return false;
      x += widths[i];
      if (i + 1 < labels.size()) {
        x += tdiv[3];
        BattleRenderer::draw_rect(x, box.y + title[1] + tdiv[2], tdiv[0],
                                  tdiv[1],
                                  color(*data_->parameter("StatTitleColor")));
        x += tdiv[0] + tdiv[3];
      }
    }
    (void)font;
    return true;
  }

public:
  FieldGoodsRenderer() = default;
  FieldGoodsRenderer(const FieldGoodsRenderer &) = delete;
  FieldGoodsRenderer &operator=(const FieldGoodsRenderer &) = delete;
  ~FieldGoodsRenderer() { free(); }
  void free() {
    for (auto &t : textures_)
      if (t.second.sheet)
        encore::ctr::loading_sprite_sheet_free(t.second.sheet);
    textures_.clear();
    data_ = nullptr;
    art_ = nullptr;
    details_ = nullptr;
  }
  bool load(const Data &d,
            const encore::upstream::FieldInventoryData &inventory,
            const FieldEquipmentRenderer &art,
            const ItemDetailsRenderer &details, const char *root,
            const std::array<uint32_t, 8> &source,
            const std::vector<std::array<uint32_t, 8>> &palettes,
            double threshold, uint32_t base, std::string &e) {
    if (!d.bind_inventory(inventory, e) || !art.ready() || !details.ready() ||
        !root || !*root)
      return false;
    std::vector<std::string> paths;
    for (const auto &t : d.textures())
      paths.push_back(t.path);
    if (!encore::ctr::loading_menu_flavor_register_checked_paths(
            paths, source, palettes, threshold, base)) {
      e = "Goods concrete MenuFlavors owner mismatch";
      return false;
    }
    free();
    data_ = &d;
    art_ = &art;
    details_ = &details;
    for (const auto &t : d.textures()) {
      std::string path(root);
      if (path.back() != '/')
        path += '/';
      path += t.path;
      FILE *f = std::fopen(path.c_str(), "rb");
      if (!f) {
        e = "Goods actual texture absent";
        free();
        return false;
      }
      std::vector<uint8_t> b(t.bytes);
      bool ok = std::fread(b.data(), 1, b.size(), f) == b.size() &&
                std::fgetc(f) == EOF && !std::ferror(f);
      if (std::fclose(f))
        ok = false;
      if (!ok ||
          item_details_renderer_detail::sha256(b.data(), b.size()) != t.sha) {
        e = "Goods atlas SHA mismatch";
        free();
        return false;
      }
      auto sheet =
          encore::ctr::loading_sprite_sheet_acquire_memory(path.c_str(), b, e);
      if (!sheet) {
        free();
        return false;
      }
      auto im = encore::ctr::loading_sprite_sheet_get_image(sheet, 0);
      if (encore::ctr::loading_sprite_sheet_count(sheet) != 1 || !im.tex ||
          !im.subtex || Tex3DS_SubTextureRotated(im.subtex) ||
          im.subtex->width != t.width || im.subtex->height != t.height) {
        encore::ctr::loading_sprite_sheet_free(sheet);
        e = "Goods source atlas geometry rejected";
        free();
        return false;
      }
      C3D_TexSetFilter(im.tex, GPU_NEAREST, GPU_NEAREST);
      textures_.emplace(t.name, Texture{&t, sheet});
    }
    e.clear();
    return true;
  }
  const std::string &error() const { return error_; }
  bool draw(const Menu &m, const BattleRenderer &font,
            const BattleRenderer &numbers, float width, float height,
            float left = 0, float top = 0) const {
    if (!m.visible())
      return true;
    if (!data_ || data_ != m.data() || !m.inventory() ||
        !std::isfinite(width) || !std::isfinite(height) || width <= 0 ||
        height <= 0)
      return fail("Goods visible checked state absent");
    clip_ = {left, top, width, height};
    const auto *id = m.inventory()->data();
    auto platform = *id->parameter("PlatformViewport");
    float dx = left + (width - platform[0]) / 2,
          dy = top + (height - platform[1]) / 2 + m.open_offset();
    auto ir = id->layout("Inventory")->rect;
    Rect inv{dx + ir[0], dy + ir[1], ir[2], ir[3]};
    if (!borrowed(ArtRole::DescriptionPanel, inv))
      return false;
    auto divider = id->layout("Divider");
    BattleRenderer::draw_rect(dx + divider->rect[0], dy + divider->rect[1],
                              divider->rect[2], divider->rect[3],
                              color(divider->color));
    auto source_viewport = *id->parameter("SourceViewport");
    auto select =
        layout("Select", inv.x, dy + (platform[1] - source_viewport[1]) / 2);
    if (!nine("bar", select, *data_->parameter("SelectPatch")))
      return false;
    auto tp = layout("SelectTitle", select.x, select.y);
    if (!nine("inside", tp, *data_->parameter("SelectTitlePatch")))
      return false;
    std::string label;
    if (!m.source_label(data_->label(0), label, error_))
      return false;
    auto ti = *data_->parameter("SelectTitleInset");
    if (!text(font, label,
              {tp.x + ti[0], tp.y + ti[1], tp.z - ti[0] - ti[2],
               tp.w - ti[1] - ti[3]},
              C2D_Color32(255, 255, 255, 255), false, true))
      return false;
    auto pp = *data_->parameter("Portraits");
    const bool normal = m.owner() == id->role(0)->id;
    Rect ninten{select.x + pp[0], select.y + pp[1], pp[2], select.w},
        key{ninten.x + pp[2], ninten.y, pp[2], select.w};
    if (!sprite(normal ? "ninten-hl" : "ninten", ninten) ||
        !sprite(normal ? "key" : "key-hl", key))
      return false;
    {
      bool suitable = false, equipped = false;
      int comparison = 0;
      if (!m.portrait(suitable, equipped, comparison, error_))
        return false;
      for (const auto &pair : {std::pair<const char *, ArtRole>{
                                   "suitable", ArtRole::PortraitSuitable},
                               {"equipped", ArtRole::PortraitEquipped},
                               {"better", ArtRole::PortraitBetter},
                               {"lower", ArtRole::PortraitLower}}) {
        const std::string kind(pair.first);
        bool show = kind == "suitable"   ? suitable
                    : kind == "equipped" ? equipped
                    : kind == "better"   ? comparison > 0
                                         : comparison < 0;
        if (!show)
          continue;
        auto pos = *data_->parameter("Portrait" + kind);
        Rect sprite;
        if (!art_->borrowed_rect(pair.second, sprite))
          return fail(art_->error().c_str());
        sprite.x = ninten.x + ninten.z / 2 + pos[0] + pos[1];
        sprite.y = ninten.y + pos[2];
        if (!borrowed(pair.second, sprite))
          return false;
      }
    }
    if (m.phase() == Phase::Items) {
      auto ip = *data_->parameter("Indicator"),
           positions = *data_->parameter("IndicatorPosition");
      float t = float(std::fmod(m.cursor_time(), ip[0])),
            a = t < ip[1]   ? (t + ip[0] - ip[2]) / (ip[0] + ip[1] - ip[2])
                : t < ip[2] ? (t - ip[1]) / (ip[2] - ip[1])
                            : (t - ip[2]) / (ip[0] + ip[1] - ip[2]);
      float offset = t >= ip[1] && t < ip[2]
                         ? positions[0] + a * (positions[1] - positions[0])
                         : positions[1] + a * (positions[0] - positions[1]);
      auto im = image("indicator");
      for (unsigned i = 0; i < 2; ++i) {
        std::string hint;
        if (!m.key_label(i ? "ui_focus_next" : "ui_focus_prev", hint, error_))
          return false;
        float w = numbers.text_width(hint.c_str()),
              x = i ? key.x + key.z : ninten.x;
        auto ibox = *data_->parameter("IndicatorBox");
        float total = w + im.subtex->width + ip[3],
              boxwidth = std::max(ibox[3] - ibox[2], total);
        x += (i ? ibox[2] : ibox[3] - boxwidth) + (boxwidth - total) / 2;
        Rect r{x + (i ? offset : -offset), select.y + positions[2], w,
               float((*data_->parameter("IndicatorBox"))[0])};
        if (i) {
          if (!text(numbers, hint, r, C2D_Color32(255, 255, 255, 255)))
            return false;
          r.x += w + ip[3];
        } else {
          auto flipped = im;
          auto sub = *im.subtex;
          std::swap(sub.left, sub.right);
          flipped.subtex = &sub;
          if (!region(
                  flipped, 0, 0, im.subtex->width, im.subtex->height,
                  {r.x, r.y, float(im.subtex->width), float(im.subtex->height)},
                  C2D_Color32(255, 255, 255, 255)))
            return false;
          r.x += im.subtex->width + ip[3];
          if (!text(numbers, hint, r, C2D_Color32(255, 255, 255, 255)))
            return false;
          continue;
        }
        if (!region(
                im, 0, 0, im.subtex->width, im.subtex->height,
                {r.x, r.y, float(im.subtex->width), float(im.subtex->height)},
                C2D_Color32(255, 255, 255, 255)))
          return false;
      }
    }
    std::vector<encore::upstream::FieldGoodsRow> rows;
    if (!m.inventory()->rows(m.owner(), rows, error_))
      return false;
    auto grid = *id->parameter("Grid"), origin = *id->parameter("GridOrigin"),
         size = *id->parameter("LabelSize");
    auto columns = uint32_t(grid[0]);
    auto visible =
        m.description_visible() ? id->description_rows() : uint32_t(grid[1]);
    Rect selected{};
    for (uint32_t i = 0; i < columns * visible; ++i) {
      auto idx = size_t(m.scroll_rows()) * columns + i;
      if (idx >= rows.size())
        break;
      Rect r{inv.x + origin[0] + (i % columns) * grid[2],
             inv.y + origin[1] + (i / columns) * grid[3], size[0], size[1]};
      const bool chosen = idx == m.selection(),
                 blink =
                     m.phase() == Phase::ManualSort && idx == m.sort_source();
      if (chosen || blink) {
        auto c = *data_->parameter("Highlight");
        if (blink) {
          auto p = *data_->parameter("Blink");
          auto t = std::fmod(m.cursor_time(), p[2] * 2) / p[2];
          double u = t <= 1 ? t : 2 - t;
          // Source SceneTreeTween default TRANS_LINEAR; ease alone does not
          // curve it.
          c[3] = float(p[0] + u * (p[1] - p[0]));
        }
        BattleRenderer::draw_rect(r.x, r.y, r.z, r.w, color(c));
      }
      if (!m.row_name(rows[idx], label, error_) ||
          !text(font, label, r, C2D_Color32(255, 255, 255, 255)))
        return false;
      if (rows[idx].equipped && !chosen && !blink) {
        auto im = image("equipped");
        auto pos = id->equipped_glyph().position;
        Rect d{r.x + pos[0] - im.subtex->width / 2.f,
               r.y + pos[1] - im.subtex->height / 2.f, float(im.subtex->width),
               float(im.subtex->height)};
        if (!region(im, 0, 0, d.z, d.w, d, C2D_Color32(255, 255, 255, 255)))
          return false;
      }
      if (chosen)
        selected = r;
    }
    if (!rows.empty() &&
        (m.phase() == Phase::Items || m.phase() == Phase::ManualSort) &&
        !cursor(m, selected, *id->parameter("CursorOffsets")))
      return false;
    if (m.description_visible() && !rows.empty()) {
      auto a = id->layout("Description")->rect;
      Rect panel{dx + a[0], dy + a[1], a[2], a[3]};
      auto inset = *id->parameter("TextInset");
      if (!borrowed(ArtRole::DescriptionPanel, panel) ||
          !details_->draw_field(rows[m.selection()].item.definition,
                                rows[m.selection()].item.doses, font,
                                panel.x + inset[0], panel.y + inset[1],
                                panel.z - inset[0] - inset[2],
                                panel.w - inset[1] - inset[3]))
        return fail(details_->error().c_str());
      const auto total =
          std::max(uint32_t((rows.size() + columns - 1) / columns),
                   m.scroll_rows() + id->description_rows());
      if (total > id->description_rows()) {
        auto sp = *data_->parameter("ScrollRect"),
             bg = *data_->parameter("ScrollBG"),
             thumb = *data_->parameter("ScrollThumb");
        Rect sr{panel.x + panel.z + sp[0], panel.y + sp[1], sp[2], sp[3]},
            bar{sr.x + bg[0], sr.y + bg[1], sr.z + bg[2] - bg[0],
                sr.w + bg[3] - bg[1]};
        if (!nine("scroll-bg", bar, *data_->parameter("ScrollBGPatch"),
                  color(*data_->parameter("ScrollColor"))))
          return false;
        Rect th{bar.x + thumb[0], bar.y + bar.w * m.scroll_rows() / total,
                bar.z + thumb[2] - thumb[0],
                bar.w * id->description_rows() / total};
        if (!nine("scroll-thumb", th, *data_->parameter("ScrollThumbPatch")))
          return false;
        auto field = art_->content();
        auto frame = uint32_t(field.parameter(Parameter(
            uint32_t(Parameter::CursorFrame0) +
            uint32_t(std::fmod(m.cursor_time() *
                                   field.parameter(Parameter::CursorFps),
                               4.)))));
        auto im = image("cursor" + std::to_string(frame));
        auto ar = *data_->parameter("ScrollArrow");
        for (unsigned k = 0; k < 2; ++k)
          if (k ? m.scroll_rows() + id->description_rows() < total
                : m.scroll_rows() > 0) {
            auto rotation = *data_->parameter("ScrollRotation");
            float y = sr.y + (k ? sr.w + rotation[2] : 0) + ar[k * 2 + 1],
                  x = sr.x + sr.z / 2 + ar[k * 2];
            C2D_DrawImageAtRotated(im, pixel(x), pixel(y), 0, rotation[k],
                                   nullptr, 1, 1);
          }
      }
    }
    if (!rows.empty() &&
        (m.phase() == Phase::Actions || m.phase() == Phase::Targets ||
         m.phase() == Phase::DropConfirm || m.phase() == Phase::SortType)) {
      auto point = *id->parameter("SubmenuPoint"),
           offset = *id->parameter("CursorOffsets"),
           center = *id->parameter("CursorCenter"),
           placement = *id->parameter("SubmenuPlacement"),
           src = *id->parameter("SourceViewport");
      float ax = dx + (platform[0] - src[0]) / 2,
            ay = dy + (platform[1] - src[1]) / 2;
      auto base = id->layout("Action")->rect;
      Rect action{selected.x - ax + offset[0] + center[0] + point[0] +
                      placement[0] * (m.selection() % columns ? placement[2]
                                                              : placement[1]) +
                      point[2] + ax,
                  std::min(placement[3],
                           selected.y - ay + offset[1] + center[1] + point[1]) +
                      point[3] + ay,
                  base[2], base[3]};
      auto inset = *id->parameter("ActionOrigin"),
           rp = *data_->parameter("ActionRow");
      std::vector<std::string> labels;
      float max = 0;
      for (const auto &a : m.actions()) {
        std::string s;
        if (!m.source_label(a.label, s, error_))
          return false;
        max = std::max(max, font.text_width(s.c_str()));
        labels.push_back(std::move(s));
      }
      auto actionpatch = *data_->parameter("ActionPatch");
      action.z = max + inset[0] + actionpatch[2];
      action.w = inset[1] + labels.size() * rp[1] +
                 (labels.empty() ? 0 : labels.size() - 1) * rp[0] +
                 actionpatch[3];
      action.y += float(m.bounce_offset());
      if (!borrowed(ArtRole::ItemsActionPanel, action))
        return false;
      for (size_t i = 0; i < labels.size(); ++i) {
        Rect r{action.x + inset[0], action.y + inset[1] + i * (rp[0] + rp[1]),
               max, rp[1]};
        if (m.phase() == Phase::Actions && i == m.submenu_selection())
          BattleRenderer::draw_rect(r.x, r.y, r.z, r.w,
                                    color(*data_->parameter("Highlight")));
        if (!text(font, labels[i], r, C2D_Color32(255, 255, 255, 255)))
          return false;
        if (m.phase() == Phase::Actions && i == m.submenu_selection() &&
            !cursor(m, r, *data_->parameter("ActionCursor")))
          return false;
      }
      if (m.phase() == Phase::Targets) {
        auto t = id->layout("Targets")->rect;
        Rect box{action.x + t[0] - base[0], action.y + t[1] - base[1], t[2],
                 t[3]};
        auto origin = *data_->parameter("TargetInset");
        std::string title;
        if (!m.source_label(
                data_->label(m.actions()[m.action_selection()].label ==
                                     data_->label(10)
                                 ? 8
                                 : 7),
                title, error_))
          return false;
        std::vector<std::string> target;
        if (m.target_present()) {
          if (m.target_all()) {
            if (!m.source_label(data_->label(9), label, error_))
              return false;
            target.push_back(label);
          } else
            target.push_back(m.nickname());
        }
        auto patch = *data_->parameter("ConfirmPatch");
        auto rowp = *data_->parameter("ActionRow");
        float textwidth =
            target.empty() ? 0 : font.text_width(target.front().c_str());
        box.z = std::max(textwidth + origin[0] + origin[2],
                         font.text_width(title.c_str()) + patch[0] + patch[2]);
        box.w = origin[1] + (target.empty() ? 0 : rowp[1]) + origin[3];
        if (!borrowed(ArtRole::ItemsTargetPanel, box))
          return false;
        auto titlegeometry = id->layout("TargetTitle")->rect;
        auto targetgeometry = id->layout("Targets")->rect;
        if (!text(font, title,
                  {box.x + titlegeometry[0] - targetgeometry[0],
                   box.y + titlegeometry[1] - targetgeometry[1],
                   box.z - patch[0] - patch[2], titlegeometry[3]},
                  color(id->layout("TargetTitle")->color)))
          return false;
        if (!target.empty()) {
          Rect r{box.x + origin[0], box.y + origin[1], textwidth, rowp[1]};
          if (!text(font, target.front(), r, C2D_Color32(255, 255, 255, 255)) ||
              !cursor(m, r,
                      {(*id->parameter("CursorOffsets"))[2],
                       (*id->parameter("CursorOffsets"))[3], 0, 0}))
            return false;
        }
      } else if (m.phase() == Phase::DropConfirm ||
                 m.phase() == Phase::SortType) {
        const bool drop = m.phase() == Phase::DropConfirm;
        auto r = layout(drop ? "Confirm" : "Sort", action.x, action.y);
        std::string title;
        if (!m.source_label(data_->label(drop ? 6 : 3), title, error_))
          return false;
        std::vector<std::string> choices;
        for (unsigned i = 0; i < 2; ++i) {
          if (!m.source_label(data_->label(drop ? 1 + i : 4 + i), label,
                              error_))
            return false;
          choices.push_back(label);
        }
        if (!choice(m, font, r, title, choices, drop ? "Confirm" : "Sort",
                    m.submenu_selection(), true))
          return false;
      }
    }
    {
      auto hint = *data_->parameter("ScopeHint");
      std::string key, textvalue;
      if (!m.source_label(data_->label(11), textvalue, error_) ||
          !m.key_label("ui_scope", key, error_))
        return false;
      float tw = numbers.text_width(textvalue.c_str()),
            kw = numbers.text_width(key.c_str());
      Rect r{inv.x + inv.z + hint[0] - tw - hint[2] - kw,
             inv.y + inv.w + hint[1], tw, hint[3]};
      if (!text(numbers, textvalue, r, C2D_Color32(255, 255, 255, 255)))
        return false;
      r.x += tw + hint[2];
      r.z = kw;
      auto tint = *data_->parameter("ScopeColor");
      if (!text(numbers, key, r,
                C2D_Color32f(tint[0], tint[1], tint[2], tint[3])))
        return false;
    }
    if (!stats(m, font, numbers, dx, dy))
      return false;
    if (m.message_visual()) {
      auto a = id->layout("Message")->rect;
      Rect box{dx + a[0], dy + a[1] + m.message_offset(), a[2], a[3]};
      auto patch = *id->parameter("MessagePatch");
      if (!borrowed(ArtRole::DescriptionPanel, box, &patch))
        return false;
      auto inset = *id->parameter("TextInset");
      Rect body{box.x + inset[0], box.y + inset[1], box.z - inset[0] - inset[2],
                box.w - inset[1] - inset[3]};
      body.y +=
          (body.w - art_->content().parameter(Parameter::MainLineHeight)) / 2;
      if (!text(font, m.message(), body, C2D_Color32(255, 255, 255, 255), false,
                true))
        return false;
    }
    error_.clear();
    return true;
  }
};
