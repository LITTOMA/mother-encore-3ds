#pragma once
#include "encore/field_inventory.hpp"
#include "field_equipment_renderer.hpp"
#include <cstdio>
#include <functional>

// Source Goods grid/description adapter. Input phases and deferred message
// presentation belong to the separate Goods menu consumer; this draws owned
// rows, source Equipped pixels and shared actual descriptions without mutating
// UID, inventory, source RNG or session state.
class FieldInventoryRenderer {
  using Runtime = encore::upstream::FieldInventoryRuntime;
  using Data = encore::upstream::FieldInventoryData;
  using ArtRole = encore::upstream::FieldLayoutRole;
  using Parameter = encore::upstream::FieldParameter;
  using Rect = encore::upstream::BattleValue;
  const Data *data_ = nullptr;
  const FieldEquipmentRenderer *art_ = nullptr;
  const ItemDetailsRenderer *details_ = nullptr;
  encore::ctr::LoadingSpriteSheet equipped_ = nullptr;
  static float pixel(float x) { return std::floor(x + .5f); }

public:
  using NameHost = std::function<bool(const encore::upstream::FieldGoodsRow &,
                                      std::string &, std::string &)>;
  FieldInventoryRenderer() = default;
  FieldInventoryRenderer(const FieldInventoryRenderer &) = delete;
  FieldInventoryRenderer &operator=(const FieldInventoryRenderer &) = delete;
  ~FieldInventoryRenderer() { free(); }
  void free() {
    if (equipped_)
      encore::ctr::loading_sprite_sheet_free(equipped_);
    equipped_ = nullptr;
    data_ = nullptr;
    art_ = nullptr;
    details_ = nullptr;
  }
  bool load(const Data &d, const FieldEquipmentRenderer &art,
            const ItemDetailsRenderer &details, const char *root,
            const std::array<uint32_t, 8> &source,
            const std::vector<std::array<uint32_t, 8>> &palettes,
            double threshold, uint32_t base, std::string &e) {
    if (!d.valid() || !art.ready() || !details.ready() || !root || !*root) {
      e = "Goods checked visual owners absent";
      return false;
    }
    for (const auto *key :
         {"Grid", "GridOrigin", "PlatformViewport", "LabelSize", "TextInset",
          "CursorOffsets", "CursorCenter"})
      if (!d.parameter(key)) {
        e = "Goods source grid parameter absent";
        return false;
      }
    for (const auto *key : {"Inventory", "Description", "Divider"})
      if (!d.layout(key)) {
        e = "Goods source panel absent";
        return false;
      }
    const auto &g = d.equipped_glyph();
    if (!encore::ctr::loading_menu_flavor_register_checked_paths(
            {g.path}, source, palettes, threshold, base)) {
      e = "Goods source MenuFlavors registry mismatch";
      return false;
    }
    std::string path(root);
    if (path.back() != '/')
      path += '/';
    path += g.path;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
      e = "Goods equipped glyph missing";
      return false;
    }
    std::vector<uint8_t> b(g.bytes);
    bool ok = std::fread(b.data(), 1, b.size(), f) == b.size() &&
              std::fgetc(f) == EOF && !std::ferror(f);
    if (std::fclose(f))
      ok = false;
    if (!ok ||
        item_details_renderer_detail::sha256(b.data(), b.size()) != g.sha) {
      e = "Goods equipped glyph source SHA mismatch";
      return false;
    }
    auto sheet =
        encore::ctr::loading_sprite_sheet_acquire_memory(path.c_str(), b, e);
    if (!sheet)
      return false;
    const auto image = encore::ctr::loading_sprite_sheet_get_image(sheet, 0);
    if (encore::ctr::loading_sprite_sheet_count(sheet) != 1 || !image.tex ||
        !image.subtex || Tex3DS_SubTextureRotated(image.subtex) ||
        image.subtex->width != g.width || image.subtex->height != g.height) {
      encore::ctr::loading_sprite_sheet_free(sheet);
      e = "Goods equipped atlas dimensions rejected";
      return false;
    }
    C3D_TexSetFilter(image.tex, GPU_NEAREST, GPU_NEAREST);
    free();
    equipped_ = sheet;
    data_ = &d;
    art_ = &art;
    details_ = &details;
    e.clear();
    return true;
  }
  bool draw(const Runtime &r, uint32_t owner, uint32_t selected,
            uint32_t scroll_rows, bool description, double cursor_time,
            const NameHost &name, const BattleRenderer &font, float width,
            float height, float left, float top, std::string &e) const {
    if (!data_ || r.data() != data_ || !art_ || !details_ || !equipped_ ||
        !name || !std::isfinite(cursor_time) || cursor_time < 0 ||
        !std::isfinite(width) || !std::isfinite(height) || width <= 0 ||
        height <= 0) {
      e = "Goods visual state invalid";
      return false;
    }
    std::vector<encore::upstream::FieldGoodsRow> rows;
    if (!r.rows(owner, rows, e))
      return false;
    const auto grid = *data_->parameter("Grid"),
               origin = *data_->parameter("GridOrigin"),
               platform = *data_->parameter("PlatformViewport"),
               size = *data_->parameter("LabelSize");
    if (grid[0] < 1 || grid[0] > 64 || std::floor(grid[0]) != grid[0] ||
        grid[1] < 1 || std::floor(grid[1]) != grid[1] || size[0] <= 0 ||
        size[1] <= 0 || (!rows.empty() && selected >= rows.size())) {
      e = "Goods source grid selection invalid";
      return false;
    }
    const uint32_t columns = uint32_t(grid[0]),
                   visible = description ? data_->description_rows()
                                         : uint32_t(grid[1]);
    if (scroll_rows > 4096 || (!description && scroll_rows)) {
      e = "Goods source scroll invalid";
      return false;
    }
    const float dx = left + (width - platform[0]) / 2,
                dy = top + (height - platform[1]) / 2;
    auto rect = [&](const char *key) {
      const auto &a = data_->layout(key)->rect;
      return Rect{a[0] + dx, a[1] + dy, a[2], a[3]};
    };
    auto inv = rect("Inventory");
    if (!art_->draw_borrowed_art(ArtRole::DescriptionPanel, inv, 0, left, top,
                                 width, height)) {
      e = art_->error();
      return false;
    }
    const auto *div = data_->layout("Divider");
    auto box = rect("Divider");
    BattleRenderer::draw_rect(
        box.x, box.y, box.z, box.w,
        encore::ctr::loading_menu_flavor_color(C2D_Color32f(
            div->color[0], div->color[1], div->color[2], div->color[3])));
    Rect selected_rect{};
    bool selected_visible = false;
    const auto glyph =
        encore::ctr::loading_sprite_sheet_get_image(equipped_, 0);
    const auto &g = data_->equipped_glyph();
    for (uint32_t n = 0; n < columns * visible; ++n) {
      const size_t index = size_t(scroll_rows) * columns + n;
      if (index >= rows.size())
        break;
      Rect cell{inv.x + origin[0] + float(n % columns) * grid[2],
                inv.y + origin[1] + float(n / columns) * grid[3], size[0],
                size[1]};
      std::string label;
      if (!name(rows[index], label, e))
        return false;
      if (!font.draw_text_clipped(label.c_str(), pixel(cell.x), pixel(cell.y),
                                  cell.x, cell.y, cell.x + cell.z,
                                  cell.y + cell.w,
                                  C2D_Color32(255, 255, 255, 255))) {
        e = "Goods source name glyph unavailable";
        return false;
      }
      if (rows[index].equipped) {
        const float x = pixel(cell.x + g.position[0] - g.width * .5f),
                    y = pixel(cell.y + g.position[1] - g.height * .5f);
        C2D_DrawImageAt(glyph, x, y, 0, nullptr, 1, 1);
      }
      if (index == selected) {
        selected_rect = cell;
        selected_visible = true;
      }
    }
    if (!rows.empty() && !selected_visible) {
      e = "Goods selected row outside source scroll window";
      return false;
    }
    if (selected_visible) {
      Rect shape;
      if (!art_->borrowed_rect(ArtRole::PauseCursor, shape)) {
        e = art_->error();
        return false;
      }
      auto offset = *data_->parameter("CursorOffsets"),
           center = *data_->parameter("CursorCenter");
      shape.x = selected_rect.x + offset[0] + center[0] - shape.z / 2;
      shape.y = selected_rect.y + offset[1] + center[1] - shape.w / 2;
      const auto field = art_->content();
      auto frame = uint32_t(
          std::fmod(cursor_time * field.parameter(Parameter::CursorFps), 4.));
      auto source_frame = uint32_t(field.parameter(
          Parameter(uint32_t(Parameter::CursorFrame0) + frame)));
      if (!art_->draw_borrowed_art(ArtRole::PauseCursor, shape, source_frame,
                                   left, top, width, height)) {
        e = art_->error();
        return false;
      }
    }
    if (description && !rows.empty()) {
      auto panel = rect("Description");
      auto inset = *data_->parameter("TextInset");
      if (!art_->draw_borrowed_art(ArtRole::DescriptionPanel, panel, 0, left,
                                   top, width, height) ||
          !details_->draw_field(
              rows[selected].item.definition, rows[selected].item.doses, font,
              panel.x + inset[0], panel.y + inset[1],
              panel.z - inset[0] - inset[2], panel.w - inset[1] - inset[3])) {
        e = details_->error();
        if (e.empty())
          e = art_->error();
        return false;
      }
    }
    e.clear();
    return true;
  }
};
