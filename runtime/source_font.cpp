#include "encore/source_font.hpp"
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include "global_yaml_file_hash.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
namespace encore {
namespace {
constexpr size_t max_metadata_bytes = 2 * 1024 * 1024;
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}
struct Reader {
  const uint8_t *data;
  size_t size, cursor = 0;
  bool good = true;
  uint32_t integer() {
    if (!good || cursor > size || size - cursor < 4) {
      good = false;
      return 0;
    }
    const auto v = u32(data + cursor);
    cursor += 4;
    return v;
  }
  float number() {
    const auto bits = integer();
    float f;
    static_assert(sizeof(f) == sizeof(bits));
    std::memcpy(&f, &bits, 4);
    if (!std::isfinite(f))
      good = false;
    return f;
  }
  std::string string() {
    const auto length = integer();
    if (!good || !length || length > 127 || cursor > size ||
        length > size - cursor) {
      good = false;
      return {};
    }
    std::string value(reinterpret_cast<const char *>(data + cursor), length);
    cursor += length;
    for (unsigned char c : value) {
      if (c < 32 || c > 126) {
        good = false;
        break;
      }
    }
    return value;
  }
};
bool safe_path(const std::string &s, bool leaf) {
  return !s.empty() && s[0] != '/' && s.find("..") == std::string::npos &&
         s.find(':') == std::string::npos &&
         s.find('\\') == std::string::npos &&
         (!leaf || s.find('/') == std::string::npos);
}
bool fail(std::string &e, const char *reason) {
  e = reason;
  return false;
}
} // namespace
bool SourceFontCatalog::load(const char *path, std::string &error) {
  if (!path)
    return fail(error, "Source font catalog path is null");
  FILE *file = std::fopen(path, "rb");
  if (!file)
    return fail(error, "Source font catalog unavailable");
  const bool seek = std::fseek(file, 0, SEEK_END) == 0;
  const long length = seek ? std::ftell(file) : -1;
  if (length < 32 || length > long(max_metadata_bytes) ||
      std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return fail(error, "Source font catalog length rejected");
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(length));
  const bool read =
      std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
  const bool closed = std::fclose(file) == 0;
  if (!read || !closed)
    return fail(error, "Source font catalog read failed");
  return load_bytes(bytes.data(), bytes.size(), error);
}
bool SourceFontCatalog::load_bytes(const uint8_t *data, size_t size,
                                   std::string &error) {
  if (!data || size < 32 || size > max_metadata_bytes ||
      std::memcmp(data, "ENCFONT\0", 8) ||
      (u32(data + 8) != 1 && u32(data + 8) != 2))
    return fail(error, "Unknown source font format/version");
  if (u32(data + 12) != size - 32 ||
      u32(data + 16) != crc32(data + 32, size - 32))
    return fail(error, "Source font length/CRC mismatch");
  const auto nf = u32(data + 20), np = u32(data + 24), ng = u32(data + 28);
  if (!nf || nf > 32 || !np || np > 256 || !ng || ng > 40000)
    return fail(error, "Source font counts exceed bounds");
  Reader r{data + 32, size - 32};
  SourceFontCatalog candidate;
  std::set<std::string> names;
  uint32_t next_glyph = 0, next_page = 0;
  for (uint32_t i = 0; i < nf; ++i) {
    SourceFontFace f;
    f.source = r.string();
    const auto flags = r.integer();
    f.legacy_ascii = flags != 0;
    f.first_glyph = r.integer();
    f.glyph_count = r.integer();
    f.first_page = r.integer();
    f.page_count = r.integer();
    f.ascent = r.number();
    f.descent = r.number();
    f.height = r.number();
    if (u32(data + 8) == 2) {
      f.spacing_char = r.number();
      f.spacing_space = r.number();
    }
    if (!r.good || flags > 1 || !safe_path(f.source, false) ||
        !names.insert(f.source).second || f.first_glyph != next_glyph ||
        !f.glyph_count || f.glyph_count > ng - next_glyph ||
        f.first_page != next_page || !f.page_count ||
        f.page_count > np - next_page || f.height <= 0 || f.height > 128 ||
        f.ascent < 0 || f.ascent > 128 || f.descent < 0 || f.descent > 128 ||
        std::abs(f.spacing_char) > 128 || std::abs(f.spacing_space) > 128 ||
        (f.legacy_ascii && (f.spacing_char != 0 || f.spacing_space != 0)))
      return fail(error, "Invalid source font face metadata");
    next_glyph += f.glyph_count;
    next_page += f.page_count;
    candidate.faces_.push_back(std::move(f));
  }
  if (next_glyph != ng || next_page != np)
    return fail(error, "Noncontiguous source font ranges");
  names.clear();
  for (uint32_t i = 0; i < np; ++i) {
    SourceFontPage p;
    p.path = r.string();
    p.width = r.integer();
    p.height = r.integer();
    p.texture_bytes = r.integer();
    p.file_bytes = r.integer();
    p.crc32 = r.integer();
    if (!r.good || !safe_path(p.path, true) || !names.insert(p.path).second ||
        p.width != 256 || p.height != 256 ||
        p.texture_bytes != p.width * p.height * 2 ||
        p.file_bytes < p.texture_bytes || p.file_bytes > p.texture_bytes + 4096)
      return fail(error, "Invalid source font page metadata");
    candidate.pages_.push_back(std::move(p));
  }
  size_t face = 0;
  uint32_t previous = 0;
  for (uint32_t i = 0; i < ng; ++i) {
    while (i >= candidate.faces_[face].first_glyph +
                    candidate.faces_[face].glyph_count) {
      ++face;
      previous = 0;
    }
    const auto &f = candidate.faces_[face];
    SourceFontGlyph g;
    g.codepoint = r.integer();
    g.page = r.integer();
    g.u = r.integer();
    g.v = r.integer();
    g.width = r.integer();
    g.height = r.integer();
    g.advance = r.number();
    g.offset_x = r.number();
    g.offset_y = r.number();
    if (!r.good || g.codepoint < 32 || g.codepoint > 0x10ffff ||
        (g.codepoint >= 0xd800 && g.codepoint <= 0xdfff) ||
        g.codepoint <= previous || g.page < f.first_page ||
        g.page >= f.first_page + f.page_count || g.advance < 0 ||
        g.advance > 128 || g.advance + f.spacing_char < 0 ||
        g.advance + f.spacing_char > 128 || std::abs(g.offset_x) > 128 ||
        std::abs(g.offset_y) > 128)
      return fail(error, "Invalid source font glyph metadata");
    const auto &p = candidate.pages_[g.page];
    if (g.u > p.width || g.width > p.width - g.u || g.v > p.height ||
        g.height > p.height - g.v)
      return fail(error, "Source font glyph outside atlas");
    previous = g.codepoint;
    candidate.glyphs_.push_back(g);
  }
  for (const auto &f : candidate.faces_) {
    size_t bytes = 0;
    for (uint32_t i = 0; i < f.page_count; ++i)
      bytes += candidate.pages_[f.first_page + i].texture_bytes;
    if (bytes > maximum_resident_bytes)
      return fail(error, "Source font face exceeds resident admission limit");
  }
  if (!r.good || r.cursor != r.size)
    return fail(error, "Trailing/truncated source font metadata");
  candidate.binary_sha_=upstream::global_yaml_bytes_sha256(
      std::string_view(reinterpret_cast<const char*>(data),size));
  *this = std::move(candidate);
  error.clear();
  return true;
}
bool SourceFontCatalog::select_font(std::string_view source,
                                    std::string &error) {
  for (size_t i = 0; i < faces_.size(); ++i)
    if (faces_[i].source == source) {
      selected_ = i;
      error.clear();
      return true;
    }
  error = "Unmapped source font resource: " + std::string(source);
  return false;
}
const SourceFontFace *SourceFontCatalog::selected() const {
  return selected_ < faces_.size() ? &faces_[selected_] : nullptr;
}
const SourceFontGlyph *SourceFontCatalog::glyph(uint32_t cp) const {
  const auto *f = selected();
  if (!f)
    return nullptr;
  const auto begin = glyphs_.begin() + f->first_glyph,
             end = begin + f->glyph_count;
  const auto at = std::lower_bound(
      begin, end, cp,
      [](const SourceFontGlyph &g, uint32_t p) { return g.codepoint < p; });
  return at != end && at->codepoint == cp ? &*at : nullptr;
}
std::string SourceFontCatalog::missing_glyph(uint32_t cp,
                                             std::string_view source) {
  char code[20];
  std::snprintf(code, sizeof(code), "U+%04lX", static_cast<unsigned long>(cp));
  return "Missing source glyph " + std::string(code) + " in " +
         std::string(source);
}
bool SourceFontCatalog::advance(uint32_t cp, float &result,
                                std::string &error) const {
  const auto *g = glyph(cp);
  if (!g) {
    error = missing_glyph(cp, selected() ? selected()->source : "<unselected>");
    return false;
  }
  result = g->advance;
  error.clear();
  return true;
}
float SourceFontCatalog::following_spacing(uint32_t cp, bool has_next) const {
  const auto *f = selected();
  return has_next && cp != 32 && f ? f->spacing_char : 0;
}
bool SourceFontCatalog::measure(std::string_view text, float &result,
                                std::string &error) const {
  float width = 0, longest = 0;
  size_t cursor = 0;
  uint32_t cp;
  while (cursor < text.size()) {
    if (!utf8_next(text, cursor, cp))
      return fail(error, "Invalid UTF-8 source font text");
    if (cp == '\n') {
      longest = std::max(longest, width);
      width = 0;
      continue;
    }
    float advance;
    if (!this->advance(cp, advance, error))
      return false;
    width += advance + following_spacing(cp, cursor < text.size() &&
                                                 text[cursor] != '\n');
    if (!std::isfinite(width))
      return fail(error, "Source font measurement overflow");
  }
  result = std::max(longest, width);
  error.clear();
  return true;
}
} // namespace encore
