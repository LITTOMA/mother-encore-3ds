#include "encore/dialogue_actor_resource.hpp"
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
  const DialogueActorResourceData &entry;
  uint32_t node_count;
  size_t budget = 2000000;
  std::map<std::string, std::string> nodes;
  std::set<std::string> actual_nodes;
  std::set<int64_t> resource_ids;
  uint32_t actual_resources = 0;
  int64_t max_resource_ref = -1;
  bool schema = false, closed = false, source = false, nodes_seen = false,
       resources_seen = false;
  GraphReader(Reader input, const DialogueActorResourceData &source_entry)
      : r(input), entry(source_entry),
        node_count(
            static_cast<uint32_t>(source_entry.recipe()->records().size())) {}
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
        if (out.count != node_count)
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
              value.kind == 4 && value.text == "res://" + entry.source_scene();
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
    for (const auto &node : entry.recipe()->records())
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
bool DialogueActorResourceData::source_hash(
    std::string_view p, std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (!valid_ || i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool DialogueActorResourceData::load_file(const char *name,
                                          const FieldIdentity &id,
                                          std::string &e) {
  if (!name || !*name) {
    e = "Actor resource path rejected";
    return false;
  }
  FILE *f = std::fopen(name, "rb");
  if (!f) {
    e = "Cannot open Actor resource";
    return false;
  }
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    e = "Actor resource seek rejected";
    return false;
  }
  long n = std::ftell(f);
  if (n < 128 || n > 64 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    e = "Actor resource size rejected";
    return false;
  }
  std::vector<uint8_t> b(static_cast<size_t>(n));
  auto got = std::fread(b.data(), 1, b.size(), f);
  bool closed = std::fclose(f) == 0;
  if (got != b.size() || !closed) {
    e = "Actor resource read rejected";
    return false;
  }
  return load(b.data(), b.size(), id, e);
}
bool DialogueActorResourceData::load(const uint8_t *p, size_t n,
                                     const FieldIdentity &id, std::string &e) {
  auto fail = [&](const char *s) {
    e = s;
    return false;
  };
  if (!p || n < 128 || n > 64 * 1024 * 1024 || std::memcmp(p, "ENCDACT1", 8) ||
      word(p + 8) != 1 || word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 24) != 0x454e0074 || word(p + 28) != 1 || !word(p + 32) ||
      word(p + 32) > 100000 || !id.scene_id || word(p + 36) != id.scene_id ||
      std::memcmp(p + 40, id.upstream_commit.data(), 20) ||
      std::memcmp(p + 60, id.source_sha256.data(), 32) || word(p + 124) ||
      word(p + 20) != crc(p + 128, n - 128))
    return fail("Actor resource identity/schema/capability/CRC rejected");
  DialogueActorResourceData d;
  d.identity_ = id;
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (!hash_valid(d.ir_))
    return fail("Actor resource source IR rejected");
  Reader r{p, n};
  d.scene_ = r.text();
  d.name_ = r.text();
  auto count = r.u();
  if (!path(d.scene_) || d.name_.empty() || !count || count > 10000)
    return fail("Actor resource declaration rejected");
  for (uint32_t i = 0; i < count; ++i) {
    auto name = r.text();
    auto h = r.hash();
    if (!r.ok || !path(name) || !hash_valid(h) ||
        !d.sources_.emplace(name, h).second)
      return fail("Actor source closure rejected");
  }
  auto source = d.sources_.find(d.scene_);
  if (source == d.sources_.end() || source->second != id.source_sha256)
    return fail("Actor original scene proof rejected");
  auto len = r.u();
  if (!r.ok || len < 128 || len > 16 * 1024 * 1024 || r.at > n ||
      len > n - r.at)
    return fail("Actor complete recipe size rejected");
  auto recipe = std::make_shared<FieldNodeRecipeData>();
  if (!recipe->load(p + r.at, len, id, e))
    return false;
  if (recipe->source_scene() != d.scene_ ||
      recipe->records().size() != word(p + 32))
    return fail("Actor complete recipe identity rejected");
  for (const auto &row : d.sources_) {
    std::array<uint8_t, 32> h{};
    if (recipe->source_hash(row.first, h) && h != row.second)
      return fail("Actor recipe/source proof conflict");
  }
  // Every attached source script is cross-bound to the full source closure.
  for (const auto &row : recipe->records())
    if (!row.script.empty()) {
      // An inline script's digest is its exact source code, while the source
      // closure hashes its containing TSCN. Recipe checks its numeric sub-id;
      // the immutable native graph retains the original attachment receipt.
      auto at = row.script.find("::");
      auto h = d.sources_.find(row.script.substr(0, at));
      if (h == d.sources_.end() ||
          (at == row.script.npos && h->second != row.script_sha))
        return fail("Actor original script closure rejected");
    }
  d.recipe_ = recipe;
  r.at += len;
  auto graph = r.u();
  if (!r.ok || graph < 5 || graph > 32 * 1024 * 1024 || r.at > n ||
      graph > n - r.at)
    return fail("Actor native graph size rejected");
  GraphReader validator({p + r.at, graph, 0, true}, d);
  if (!validator.validate())
    return fail("Actor complete native/resource graph rejected");
  d.graph_.assign(p + r.at, p + r.at + graph);
  r.at += graph;
  if (!r.ok || r.at != n)
    return fail("Actor trailing/truncated resource rejected");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
