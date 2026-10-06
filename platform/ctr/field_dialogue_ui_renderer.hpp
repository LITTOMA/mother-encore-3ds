#pragma once
#include "encore/field_dialogue_ui.hpp"
#include "house_renderer.hpp"
#include "source_font_renderer.hpp"
#include <algorithm>
#include <cmath>
// Borrow genuine checked House atlases and the actual admitted source font.
// No texture IO, second text clock, RNG draw, or synthesized cursor is done
// here.
class FieldDialogueUiRenderer {
  using Data = encore::upstream::FieldDialogueUiData;
  using Role = encore::upstream::FieldDialogueUiRole;
  static bool fail(std::string &e, const char *s) {
    e = s;
    return false;
  }

public:
  static bool measure(const Data &d, std::string_view source_font,
                      const BattleRenderer &font,
                      const encore::ctr::SourceFontRenderer &source,
                      std::string_view text, encore::upstream::Vec2 &out,
                      std::string &e) {
    const auto *face = source.catalog().selected();
    const encore::upstream::FieldDialogueUiControl *requested = nullptr,
                                                   *selected = nullptr;
    for (const auto &c : d.controls()) {
      if (c.font == source_font)
        requested = &c;
      if (face && c.font == face->source)
        selected = &c;
    }
    // Producer proves these original source faces have equal complete resource
    // definitions, not merely matching names or superficially equal metrics.
    if (!d.valid() || !requested || !selected ||
        face->height != requested->font_height ||
        face->ascent != requested->font_ascent ||
        face->descent != requested->font_descent)
      return fail(e, "Dialogue UI actual source font binding rejected");
    size_t at = 0, lines = 1;
    uint32_t cp = 0;
    std::string measured_text;
    while (at < text.size()) {
      if (!encore::utf8_next(text, at, cp))
        return fail(e, "Dialogue UI native text UTF-8 rejected");
      uint32_t delay = 0, wait = 0;
      size_t marker_at = 0;
      encore::utf8_next(d.delay_marker(), marker_at, delay);
      marker_at = 0;
      encore::utf8_next(d.wait_marker(), marker_at, wait);
      if (cp == delay || cp == wait)
        continue;
      if (cp == '\n') {
        ++lines;
        measured_text += '\n';
        continue;
      }
      if (source.handles(cp) && !source.glyph(cp))
        return fail(e, "Dialogue UI source glyph not admitted");
      encore::utf8_append(cp, measured_text);
    }
    out = {font.text_width(measured_text.c_str()),
           float(lines) * face->height +
               float(lines - 1) * requested->line_separation};
    if (!std::isfinite(out.x) || !std::isfinite(out.y) || out.x < 0 ||
        out.y <= 0)
      return fail(e, "Dialogue UI measured source font bound");
    e.clear();
    return true;
  }
  // Actual material admission validates the same registered GPU palette service
  // that owns the genuine House texture; it cannot approve an unloaded sheet.
  static bool material(const Data &d, std::string_view shader,
                       std::string_view texture, std::string &e) {
    const auto *box = d.role(Role::Box);
    const auto *c = box ? d.control(box->id) : nullptr;
    if (!c || c->material != shader || c->texture != texture || shader.empty())
      return fail(e, "Dialogue UI original ShaderMaterial binding rejected");
    const encore::upstream::FieldDialogueUiResource *art = nullptr;
    for (const auto &a : d.resources())
      if (a.source == texture)
        art = &a;
    const auto &service = encore::ctr::loading_menu_flavor_detail::paths;
    if (!art ||
        std::find(service.begin(), service.end(), art->path) == service.end())
      return fail(e, "Dialogue UI source palette path not registered");
    for (const auto *sheet : encore::ctr::loading_menu_flavor_detail::sheets)
      if (sheet && sheet->source_path == art->path &&
          !sheet->flavor_pixels.empty()) {
        e.clear();
        return true;
      }
    return fail(e, "Dialogue UI source material GPU texture not prepared");
  }
  bool draw(const encore::upstream::FieldDialogueUiRuntime &runtime,
            encore::upstream::FieldObjectId root, const HouseRenderer &house,
            const BattleRenderer &font,
            const encore::ctr::SourceFontRenderer &source, float width,
            float height, float origin_x, float origin_y,
            uint32_t checked_theme_text_color, std::string &e) const {
    const auto *d = runtime.data();
    if (!d || !std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(origin_x) || !std::isfinite(origin_y) || width <= 0 ||
        height <= 0)
      return fail(e, "Dialogue UI live GPU/source viewport rejected");
    encore::upstream::WorldDialoguePose pose;
    if (!runtime.pose(root, pose, e))
      return false;
    const auto display = d->display();
    if (!house.draw_dialogue(pose, font, width - display.x, height - display.y,
                             origin_x, origin_y))
      return fail(e, "Dialogue UI actual panel/tag printer GPU draw failed");
    const float ox = origin_x + (width - display.x) * display.z,
                oy = origin_y + (height - display.y) * display.w;
    for (const auto &label : runtime.option_labels(root)) {
      const auto *control = d->control(label.source);
      encore::upstream::Vec2 measured;
      if (!control ||
          !measure(*d, control->font, font, source, label.text, measured, e))
        return false;
      float x = label.rect.x + ox, y = label.rect.y + oy;
      if (control->align == 1)
        x += std::floor((label.rect.z - measured.x) * .5f);
      else if (control->align == 2)
        x += label.rect.z - measured.x;
      else if (control->align != 0)
        return fail(e, "Dialogue UI unsupported native Label fill alignment");
      if (control->valign == 1)
        y += std::floor((label.rect.w - measured.y) * .5f);
      else if (control->valign == 2)
        y += label.rect.w - measured.y;
      else if (control->valign != 0)
        return fail(e, "Dialogue UI unsupported native Label fill valign");
      if (!font.draw_text_clipped(label.text.c_str(), x, y, origin_x, origin_y,
                                  origin_x + width, origin_y + height,
                                  checked_theme_text_color))
        return fail(e, "Dialogue UI actual option Label GPU draw failed");
    }
    e.clear();
    return true;
  }
};
