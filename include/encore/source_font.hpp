#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore {
struct SourceFontGlyph {
  uint32_t codepoint = 0, page = 0, u = 0, v = 0, width = 0, height = 0;
  float advance = 0, offset_x = 0, offset_y = 0;
};
struct SourceFontPage {
  std::string path;
  uint32_t width = 0, height = 0, texture_bytes = 0, file_bytes = 0, crc32 = 0;
};
struct SourceFontFace {
  std::string source;
  bool legacy_ascii = false;
  uint32_t first_glyph = 0, glyph_count = 0, first_page = 0, page_count = 0;
  float ascent = 0, descent = 0, height = 0, spacing_char = 0,
        spacing_space = 0;
};
// Checked metadata only. This class allocates no GPU resources. Every glyph and
// remap is external data, including baseline/advance and legacy ASCII identity.
class SourceFontCatalog {
public:
  static constexpr size_t maximum_resident_bytes = 2 * 1024 * 1024;
  bool load(const char *path, std::string &error);
  bool load_bytes(const uint8_t *bytes, size_t size, std::string &error);
  bool select_font(std::string_view source_tres, std::string &error);
  const SourceFontFace *selected() const;
  const SourceFontGlyph *glyph(uint32_t codepoint) const;
  bool advance(uint32_t codepoint, float &result, std::string &error) const;
  float following_spacing(uint32_t cp, bool has_next) const;
  bool measure(std::string_view text, float &result, std::string &error) const;
  bool legacy_ascii() const {
    const auto *f = selected();
    return f && f->legacy_ascii;
  }
  const std::vector<SourceFontPage> &pages() const { return pages_; }
  const std::vector<SourceFontFace> &faces() const { return faces_; }
  static std::string missing_glyph(uint32_t codepoint, std::string_view source);

private:
  std::vector<SourceFontFace> faces_;
  std::vector<SourceFontPage> pages_;
  std::vector<SourceFontGlyph> glyphs_;
  size_t selected_ = size_t(-1);
};
} // namespace encore
