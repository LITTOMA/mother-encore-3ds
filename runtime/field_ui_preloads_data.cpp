#include "encore/field_ui_preloads.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {
namespace {
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (unsigned i = 0; i < 8; ++i)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0u);
  }
  return ~c;
}
bool hash_valid(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](uint8_t v) { return v != 0; });
}
bool path(const std::string &s) {
  if (s.empty() || s.front() == '/' || s.find_first_of("\\:") != s.npos)
    return false;
  size_t b = 0;
  while (b < s.size()) {
    auto at = s.find('/', b);
    if (at == s.npos)
      at = s.size();
    auto p = s.substr(b, at - b);
    if (p.empty() || p == "." || p == "..")
      return false;
    b = at + 1;
  }
  return true;
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 128;
  bool ok = true;
  uint32_t u() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p + at);
    at += 4;
    return v;
  }
  std::string text() {
    auto len = u();
    if (!ok || len > 65536 || at > n || len > n - at) {
      ok = false;
      return {};
    }
    std::string out(reinterpret_cast<const char *>(p + at), len);
    at += len;
    size_t count = 0;
    if (out.find('\0') != out.npos || !utf8_count(out, count))
      ok = false;
    return out;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return h;
    }
    std::copy_n(p + at, 32, h.begin());
    at += 32;
    return h;
  }
};
struct Scalar {
  uint8_t kind = 0;
  uint32_t count = 0;
  int64_t integer = 0;
  std::string text;
  bool boolean = false;
};
// Streaming structural validation. The immutable graph remains typed TLV,
// avoiding a large temporary object graph during 3DS startup.
struct GraphReader {
  Reader r;
  const FieldUiPreloadRecipe &entry;
  size_t budget = 2000000;
  std::map<std::string, std::string> nodes;
  std::set<std::string> actual_nodes;
  std::set<int64_t> resource_ids;
  uint32_t actual_resources = 0;
  int64_t max_resource_ref = -1;
  bool schema = false, closed = false, source = false, nodes_seen = false,
       resources_seen = false;
  GraphReader(Reader input, const FieldUiPreloadRecipe &source_entry)
      : r(input), entry(source_entry) {}
  Scalar read(unsigned depth = 0, unsigned mode = 0) {
    Scalar out;
    if (!r.ok || depth >= 64 || !budget-- || r.at >= r.n) {
      r.ok = false;
      return out;
    }
    out.kind = r.p[r.at++];
    if (out.kind > 6) {
      r.ok = false;
      return out;
    }
    if (out.kind == 0)
      return out;
    if (out.kind == 1) {
      if (r.at >= r.n || r.p[r.at] > 1) {
        r.ok = false;
        return out;
      }
      out.boolean = r.p[r.at++] != 0;
      return out;
    }
    if (out.kind == 2 || out.kind == 3) {
      if (r.at > r.n || r.n - r.at < 8) {
        r.ok = false;
        return out;
      }
      uint64_t bits =
          uint64_t(word(r.p + r.at)) | (uint64_t(word(r.p + r.at + 4)) << 32);
      r.at += 8;
      if (out.kind == 2)
        std::memcpy(&out.integer, &bits, 8);
      else {
        double f;
        std::memcpy(&f, &bits, 8);
        if (!std::isfinite(f))
          r.ok = false;
      }
      return out;
    }
    if (out.kind == 4) {
      out.text = r.text();
      return out;
    }
    out.count = r.u();
    if (out.count > 100000 || out.count > budget) {
      r.ok = false;
      return out;
    }
    if (out.kind == 5) {
      if (mode == 2) {
        nodes_seen = true;
        if (out.count != entry.node_count)
          r.ok = false;
      }
      if (mode == 4) {
        resources_seen = true;
        actual_resources = out.count;
      }
      for (uint32_t i = 0; i < out.count && r.ok; ++i)
        read(depth + 1, mode == 2 ? 3 : mode == 4 ? 5 : 0);
      return out;
    }
    if (mode == 2 || mode == 4) {
      r.ok = false;
      return out;
    }
    std::set<std::string> keys;
    std::string class_name, node_path, type;
    int64_t resource_id = -1;
    for (uint32_t i = 0; i < out.count && r.ok; ++i) {
      auto key = read(depth + 1);
      if (key.kind != 4 || !keys.insert(key.text).second) {
        r.ok = false;
        break;
      }
      unsigned next = mode == 1 && key.text == "nodes"       ? 2
                      : mode == 1 && key.text == "resources" ? 4
                                                             : 0;
      auto value = read(depth + 1, next);
      if (mode == 1) {
        if (key.text == "source")
          source =
              value.kind == 4 && value.text == "res://" + entry.preload.path;
        if (key.text == "schema")
          schema = value.kind == 2 && value.integer == 1;
        if (key.text == "native_compatible")
          closed = value.kind == 1 && !value.boolean;
      }
      if ((mode == 3 || mode == 5) && key.text == "class") {
        if (value.kind != 4)
          r.ok = false;
        class_name = value.text;
      }
      if (mode == 3 && key.text == "path") {
        if (value.kind != 4)
          r.ok = false;
        node_path = value.text;
      }
      if (key.text == "type" && value.kind == 4)
        type = value.text;
      if (key.text == "id" && value.kind == 2)
        resource_id = value.integer;
    }
    if (type == "ResourceReference") {
      if (resource_id < 0)
        r.ok = false;
      max_resource_ref = std::max(max_resource_ref, resource_id);
    }
    if (mode == 3) {
      auto expected = nodes.find(node_path);
      if (expected == nodes.end() || expected->second != class_name ||
          !actual_nodes.insert(node_path).second)
        r.ok = false;
    }
    if (mode == 5) {
      static const char *const classes[] = {"AnimatedTexture",
                                            "Animation",
                                            "AtlasTexture",
                                            "AudioStreamMP3",
                                            "AudioStreamSample",
                                            "CanvasItemMaterial",
                                            "ConcavePolygonShape2D",
                                            "ConvexPolygonShape2D",
                                            "DynamicFont",
                                            "Environment",
                                            "PackedScene",
                                            "RectangleShape2D",
                                            "Shader",
                                            "ShaderMaterial",
                                            "SpriteFrames",
                                            "StreamTexture",
                                            "StyleBoxFlat",
                                            "StyleBoxTexture",
                                            "Theme",
                                            "TileSet"};
      if (resource_id < 0 || uint64_t(resource_id) >= actual_resources ||
          !resource_ids.insert(resource_id).second ||
          std::find_if(
              std::begin(classes), std::end(classes),
              [&](const char *v) { return class_name == v; }) ==
              std::end(classes))
        r.ok = false;
    }
    return out;
  }
  bool validate() {
    for (const auto &node : entry.recipe->records())
      nodes.emplace(node.path, node.native_class);
    auto root = read(0, 1);
    return r.ok && r.at == r.n && root.kind == 6 && schema && closed &&
           source && nodes_seen && resources_seen &&
           actual_nodes.size() == nodes.size() &&
           (max_resource_ref < 0 ||
            uint64_t(max_resource_ref) < actual_resources);
  }
};
} // namespace
bool FieldUiPreloadsData::load_file(const char *path, const FieldIdentity &id,
                                    std::string &e) {
  if (!path || !*path) {
    e = "UI preload resource path rejected";
    return false;
  }
  FILE *f = std::fopen(path, "rb");
  if (!f) {
    e = "Cannot open UI preload resource";
    return false;
  }
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    e = "UI preload seek rejected";
    return false;
  }
  long n = std::ftell(f);
  if (n < 128 || n > 64 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    e = "UI preload size rejected";
    return false;
  }
  std::vector<uint8_t> b(static_cast<size_t>(n));
  auto got = std::fread(b.data(), 1, b.size(), f);
  bool closed = std::fclose(f) == 0;
  if (got != b.size() || !closed) {
    e = "UI preload read rejected";
    return false;
  }
  return load(b.data(), b.size(), id, e);
}
bool FieldUiPreloadsData::load(const uint8_t *p, size_t n,
                               const FieldIdentity &id, std::string &e) {
  auto fail = [&](const char *s) {
    e = s;
    return false;
  };
  if (!p || n < 128 || n > 64 * 1024 * 1024 || std::memcmp(p, "ENCFUPR1", 8) ||
      word(p + 8) != 1 || word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 24) != 0x454e004b || word(p + 28) != 1 || word(p + 32) != 12 ||
      !id.scene_id || word(p + 36) != id.scene_id ||
      std::memcmp(p + 40, id.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, id.source_sha256.data(), 32) || word(p + 124) ||
      word(p + 20) != crc(p + 128, n - 128))
    return fail("UI preload identity/schema/capability/CRC rejected");
  FieldUiPreloadsData d;
  d.identity_ = id;
  Reader r{p, n};
  d.script_ = r.text();
  if (!path(d.script_))
    return fail("UI preload source policy path rejected");
  auto count = r.u();
  if (!count || count > 10000)
    return fail("UI preload source count rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto name = r.text();
    auto h = r.hash();
    if (!r.ok || !path(name) || !hash_valid(h) ||
        !d.sources_.emplace(name, h).second)
      return fail("UI preload source proof rejected");
  }
  auto policy = d.sources_.find(d.script_);
  if (policy == d.sources_.end() || policy->second != id.source_sha256)
    return fail("UI preload source policy identity rejected");
  count = r.u();
  if (count != 12)
    return fail("UI preload complete source roster required");
  std::set<uint32_t> ids;
  std::set<std::string> names, paths;
  unsigned borrowed = 0;
  for (uint32_t i = 0; i < count; ++i) {
    FieldUiPreloadRecipe a;
    a.preload.id = r.u();
    auto flag = r.u();
    a.borrowed = flag != 0;
    a.scene_id = r.u();
    a.node_count = r.u();
    a.preload.name = r.text();
    a.preload.path = r.text();
    a.preload.native_class = r.text();
    a.preload.sha = r.hash();
    a.ir_sha = r.hash();
    auto proof = d.sources_.find(a.preload.path);
    if (!r.ok || flag > 1 || !a.preload.id || !a.scene_id || !a.node_count ||
        a.node_count > 100000 || !ids.insert(a.preload.id).second ||
        a.preload.name.empty() || !names.insert(a.preload.name).second ||
        !path(a.preload.path) || !paths.insert(a.preload.path).second ||
        a.preload.native_class != "PackedScene" || proof == d.sources_.end() ||
        proof->second != a.preload.sha || !hash_valid(a.ir_sha))
      return fail("UI preload source declaration/identity rejected");
    auto len = r.u();
    if (a.borrowed) {
      if (len || r.u() || ++borrowed > 1)
        return fail("UI preload borrowed actual owner fields rejected");
    } else {
      if (!r.ok || len < 128 || len > 16 * 1024 * 1024 || r.at > n ||
          len > n - r.at)
        return fail("UI preload full nested Recipe size rejected");
      FieldIdentity identity;
      identity.scene_id = a.scene_id;
      identity.source_sha256 = a.preload.sha;
      identity.upstream_commit = id.upstream_commit;
      auto recipe = std::make_shared<FieldNodeRecipeData>();
      if (!recipe->load(p + r.at, len, identity, e)) {
        e = a.preload.path + ": " + e;
        return false;
      }
      if (recipe->source_scene() != a.preload.path ||
          recipe->ir_sha256() != a.ir_sha ||
          recipe->records().size() != a.node_count)
        return fail("UI preload original complete Recipe rejected");
      r.at += len;
      a.recipe = recipe;
      auto graph = r.u();
      if (!r.ok || graph < 5 || graph > 32 * 1024 * 1024 || r.at > n ||
          graph > n - r.at)
        return fail("UI preload full source graph size rejected");
      GraphReader reader({p + r.at, graph, 0, true}, a);
      if (!reader.validate())
        return fail(
            "UI preload native/resource typed TLV source graph rejected");
      a.native_graph.assign(p + r.at, p + r.at + graph);
      r.at += graph;
    }
    d.entries_.push_back(std::move(a));
  }
  if (!r.ok || r.at != n || borrowed != 1)
    return fail("UI preload trailing/truncated/borrowed ownership rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
