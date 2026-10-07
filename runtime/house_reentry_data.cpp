#include "encore/house_reentry.hpp"
#include "encore/utf8.hpp"
#include "global_yaml_file_hash.hpp"
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
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
bool nz(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](auto x) { return x != 0; });
}
bool path(std::string_view s) {
  if (s.empty() || s.front() == '/' || s.find_first_of("\\:") != s.npos)
    return false;
  size_t at = 0;
  while (at < s.size()) {
    auto end = s.find('/', at);
    if (end == s.npos)
      end = s.size();
    auto part = s.substr(at, end - at);
    if (part.empty() || part == "." || part == "..")
      return false;
    at = end + 1;
  }
  return true;
}
bool symbol(std::string_view s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
  });
}
bool equal(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
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
  float f() {
    auto v = u();
    float out;
    std::memcpy(&out, &v, 4);
    if (!std::isfinite(out))
      ok = false;
    return out;
  }
  Vec2 vec() {
    float x = f(), y = f();
    return {x, y};
  }
  std::string text() {
    auto len = u();
    if (!ok || len > 65536 || at > n || n - at < len) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    size_t count = 0;
    if (s.find('\0') != s.npos || !utf8_count(s, count))
      ok = false;
    return s;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> v{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return v;
    }
    std::copy_n(p + at, 32, v.begin());
    at += 32;
    return v;
  }
  uint32_t count(uint32_t max) {
    auto v = u();
    if (!v || v > max)
      ok = false;
    return ok ? v : 0;
  }
};
std::array<uint8_t, 32> digest(const uint8_t *p, size_t n) {
  return global_yaml_bytes_sha256(
      std::string_view(reinterpret_cast<const char *>(p), n));
}
} // namespace
bool HouseReentryData::source_hash(std::string_view p,
                                   std::array<uint8_t, 32> &out) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  out = i->second;
  return true;
}
bool HouseReentryData::matches(const FieldDoorData &doors, RoomView room,
                               HouseView house, std::string &e) const {
  if (!valid_ || !doors.valid() || !room || !house ||
      room.byte_size() != room_size_ || house.byte_size() != house_size_ ||
      digest(room.bytes(), room.byte_size()) != room_sha_ ||
      digest(house.bytes(), house.byte_size()) != house_sha_)
    return fail(e,
                "House return requires the exact original Room/House owners");
  auto id = doors.identity();
  FieldDoorDescriptor d;
  std::array<uint8_t, 32> source{}, target{}, script{};
  if (id.upstream_commit != identity_.upstream_commit ||
      !doors.find(door_, d) || doors.source_scene() != source_ ||
      doors.string(d.target_path) != target_ ||
      !doors.source_hash(source_, source) ||
      !doors.source_hash(target_, target) || !source_hash(source_, script) ||
      script != source || target != identity_.source_sha256 ||
      doors.body_method() != body_method_ ||
      !equal({d.target.x, d.target.y - doors.ground_offset()}, position_) ||
      !equal(d.direction, direction_))
    return fail(
        e, "House return actual exterior Door/target/source binding differs");
  if (room.count(RoomSection::Scene) != 1 ||
      room.string(room.scene().source_scene_string) != "res://" + target_ ||
      room.string(room.scene().display_name_string) != root_ ||
      house.count(HouseSection::Npcs) != actors_.size() ||
      room.flag_count() != flags_.size() ||
      room.body_rule_count() != bodies_.size())
    return fail(e, "House return native source scene/roster differs");
  for (const auto &x : flags_) {
    auto r = room.flag(x.index);
    if (r.stable_id != x.id || room.string(r.name_string) != x.name)
      return fail(e, "House return source flag mapping differs");
  }
  for (const auto &x : actors_) {
    if (x.house_index >= house.count(HouseSection::Npcs) ||
        x.room_index >= room.actor_instance_count())
      return fail(e, "House return actor index outside actual owners");
    auto h = house.npc(x.house_index);
    auto a = room.actor_instance(x.room_index);
    if (h.room_actor_index != x.room_index || h.body_id != x.body ||
        house.string(h.source_path) != x.node || a.stable_id != x.id ||
        !equal(a.position, x.position) || !equal(a.direction, x.direction))
      return fail(e, "House return source House/Room actor mapping differs");
  }
  for (const auto &x : bodies_) {
    auto r = room.body_rule(x.index);
    if (r.body_id != x.id || room.string(r.source_path_string) != x.node)
      return fail(e, "House return body ownership mapping differs");
  }
  for (const auto &entry : sources_) {
    std::array<uint8_t, 32> known;
    if (doors.source_hash(entry.first, known) && known != entry.second)
      return fail(e, "House return source dependency differs from actual Door");
  }
  e.clear();
  return true;
}
bool HouseReentryData::load(const uint8_t *p, size_t n,
                            const FieldDoorData &doors, RoomView room,
                            HouseView house, std::string &e) {
  if (!p || n < 128 || n > 4 * 1024 * 1024 || std::memcmp(p, "ENCHRET1", 8) ||
      word(p + 8) != 2 || word(p + 12) != 128 || word(p + 16) != n ||
      word(p + 20) != crc(p + 128, n - 128) || word(p + 24) != 0x454e0075 ||
      word(p + 28) != 1 || word(p + 32) != 1 || !word(p + 36) || word(p + 124))
    return fail(e, "House return format/capability/rules/CRC rejected");
  HouseReentryData d;
  d.identity_.scene_id = word(p + 36);
  std::copy_n(p + 40, 20, d.identity_.upstream_commit.begin());
  std::copy_n(p + 60, 32, d.identity_.source_sha256.begin());
  std::copy_n(p + 92, 32, d.ir_.begin());
  if (!nz(d.identity_.source_sha256) || !nz(d.ir_))
    return fail(e, "House return source proof is empty");
  Reader r{p, n};
  std::set<std::string> packpaths;
  for (uint32_t role = 1; role <= 2; ++role) {
    if (r.u() != role)
      return fail(e, "House return unknown package role");
    auto size = r.u();
    auto hash = r.hash();
    auto resource = r.text();
    if (!size || !nz(hash) || !path(resource) ||
        !packpaths.insert(resource).second)
      return fail(e, "House return package closure malformed");
    if (role == 1) {
      d.room_size_ = size;
      d.room_sha_ = hash;
    } else {
      d.house_size_ = size;
      d.house_sha_ = hash;
    }
  }
  d.door_ir_ = r.hash();
  d.door_ = r.u();
  d.source_ = r.text();
  d.target_ = r.text();
  d.root_ = r.text();
  d.source_region_ = r.text();
  d.region_ = r.text();
  d.player_parent_ = r.text();
  d.position_ = r.vec();
  d.direction_ = r.vec();
  if (r.u() != 1)
    return fail(e, "House return init_params unsupported");
  d.body_signal_ = r.text();
  d.body_method_ = r.text();
  for (auto &s : d.door_signals_)
    s = r.text();
  d.changed_ = r.text();
  d.deferred_ = r.text();
  d.left_ = r.text();
  d.left_arguments_ = r.u();
  if (d.left_arguments_ != 1)
    return fail(e, "House return AreaRoom leave_for signal arguments unsupported");
  d.party_changed_ = r.text();
  d.party_idle_ = r.text();
  auto &ready = d.ready_;
  ready.script = r.text();
  ready.visit_method = r.text();
  ready.flying_method = r.text();
  ready.visit_flag = r.text();
  ready.magicant_region = r.text();
  ready.flying_flag = r.text();
  ready.flying_character = r.text();
  ready.party_npcs_member = r.text();
  ready.switch_signal = r.text();
  ready.switch_method = r.text();
  if (!d.door_ || !nz(d.door_ir_) || !path(d.source_) || !path(d.target_) ||
      !path(d.player_parent_) || d.root_.empty() || d.source_region_.empty() ||
      d.region_.empty() || !path(ready.script) || !symbol(ready.visit_method) ||
      !symbol(ready.flying_method) || !symbol(ready.visit_flag) ||
      !symbol(ready.flying_flag) || !symbol(ready.flying_character) ||
      !symbol(ready.party_npcs_member) || ready.magicant_region.empty())
    return fail(e, "House return source scene/Ready bindings malformed");
  std::set<std::string> sig;
  for (const auto &s : d.door_signals_)
    if (!symbol(s) || !sig.insert(s).second)
      return fail(e, "House return Door signals malformed");
  for (const auto *s :
       {&d.body_signal_, &d.body_method_, &d.changed_, &d.deferred_, &d.left_,
        &ready.switch_signal, &ready.switch_method, &d.party_changed_, &d.party_idle_})
    if (!symbol(*s))
      return fail(e, "House return source callback malformed");
  auto count = r.count(4096);
  std::set<uint32_t> ids;
  std::set<std::string> names;
  for (uint32_t i = 0; i < count; ++i) {
    HouseReentryFlag x;
    x.index = r.u();
    x.id = r.u();
    x.name = r.text();
    if (x.index != i || !x.id || !symbol(x.name) || !ids.insert(x.id).second ||
        !names.insert(x.name).second)
      return fail(e, "House return flags duplicate/unknown");
    d.flags_.push_back(std::move(x));
  }
  if (!names.count(ready.visit_flag) || !names.count(ready.flying_flag))
    return fail(e, "House return AreaRoom flags outside source mapping");
  count = r.count(4096);
  ids.clear();
  std::set<uint32_t> actorindexes;
  std::set<std::string> nodes;
  for (uint32_t i = 0; i < count; ++i) {
    HouseReentryActor x;
    x.house_index = r.u();
    x.room_index = r.u();
    x.id = r.u();
    x.body = r.u();
    x.node = r.text();
    x.position = r.vec();
    x.direction = r.vec();
    if (x.house_index != i || !x.id || !x.body || !path(x.node) ||
        !ids.insert(x.id).second || !actorindexes.insert(x.room_index).second ||
        !nodes.insert(x.node).second)
      return fail(e, "House return actor ownership malformed");
    d.actors_.push_back(std::move(x));
  }
  count = r.count(4096);
  ids.clear();
  nodes.clear();
  for (uint32_t i = 0; i < count; ++i) {
    HouseReentryBody x;
    x.index = r.u();
    x.id = r.u();
    x.node = r.text();
    if (x.index != i || !x.id || !path(x.node) || !ids.insert(x.id).second ||
        !nodes.insert(x.node).second)
      return fail(e, "House return native body ownership malformed");
    d.bodies_.push_back(std::move(x));
  }
  count = r.count(4096);
  nodes.clear();
  for (uint32_t i = 0; i < count; ++i) {
    HouseReentryLandmark x;
    x.node = r.text();
    x.appear = r.text();
    x.disappear = r.text();
    auto remove = r.u();
    if (remove > 1 || !path(x.node) || !nodes.insert(x.node).second ||
        (!x.appear.empty() && !names.count(x.appear)) ||
        (!x.disappear.empty() && !names.count(x.disappear)))
      return fail(e, "House return landmark source flag unsupported");
    x.delete_if_hidden = remove;
    d.landmarks_.push_back(std::move(x));
  }
  count = r.count(2);
  if (count != 2)
    return fail(e, "House return native parent closure differs");
  ids.clear();
  for (uint32_t i = 0; i < count; ++i) {
    HouseReentryNative x;
    x.id = r.u();
    x.parent = r.u();
    x.owner_role = r.u();
    x.node = r.text();
    x.name = r.text();
    x.native_class = r.text();
    x.script = r.text();
    x.script_sha = r.hash();
    x.local.x = r.vec();
    x.local.y = r.vec();
    x.local.origin = r.vec();
    if (!x.id || !ids.insert(x.id).second || x.name.empty() ||
        x.owner_role != i + 1 ||
        x.local.x.x * x.local.y.y == x.local.x.y * x.local.y.x)
      return fail(e, "House return native parent descriptor malformed");
    if (i == 0) {
      if (x.parent || x.node != "." || x.name != d.root_ ||
          x.native_class != "Node2D" || !path(x.script) || !nz(x.script_sha))
        return fail(e, "House return root native owner unsupported");
    } else if (x.parent != d.native_[0].id || x.node != d.player_parent_ ||
               x.name != d.player_parent_ || x.native_class != "TileMap" ||
               !x.script.empty() || nz(x.script_sha))
      return fail(e, "House return player parent native owner unsupported");
    d.native_.push_back(std::move(x));
  }
  count = r.count(3);
  if (count != 3)
    return fail(e, "House return complete TileMap coverage rejected");
  ids.clear();
  nodes.clear();
  for (uint32_t i = 0; i < count; ++i) {
    HouseReentryTileMap x;
    x.id = r.u();
    x.layer = r.u();
    x.mask = r.u();
    x.cell_count = r.u();
    const auto parts = r.u();
    x.node = r.text();
    x.resource = r.text();
    x.resource_source = r.text();
    x.cell_sha = r.hash();
    x.resource_sha = r.hash();
    if (!x.id || !ids.insert(x.id).second || !path(x.node) ||
        !nodes.insert(x.node).second || !x.cell_count || x.cell_count > 1048576 ||
        parts || !nz(x.cell_sha) || !nz(x.resource_sha) ||
        !path(x.resource_source))
      return fail(e, "House return TileMap source/native collision rejected");
    const std::string prefix = "res://" + x.resource_source;
    if (x.resource != prefix) {
      if (x.resource.compare(0, prefix.size() + 2, prefix + "::") ||
          !symbol(std::string_view(x.resource).substr(prefix.size() + 2)))
        return fail(e, "House return TileSet URI rejected");
    }
    const auto tiles = r.count(65536);
    if (tiles > x.cell_count)
      return fail(e, "House return placed tile coverage rejected");
    std::set<uint32_t> tileids;
    for (uint32_t j = 0; j < tiles; ++j) {
      HouseReentryTile tile;
      tile.id = r.u();
      const auto present = r.u(), shapes = r.u();
      if (tile.id > 0x1fffffffu || present > 1 || shapes ||
          !tileids.insert(tile.id).second)
        return fail(e, "House return tile collision shape/identity rejected");
      tile.present = present;
      x.tiles.push_back(tile);
    }
    d.tilemaps_.push_back(std::move(x));
  }
  count = r.count(16);
  if (count != 16)
    return fail(e, "House return transition operation closure differs");
  for (uint32_t i = 0; i < count; ++i) {
    auto step = r.u();
    auto source = r.text();
    if (step != i || source.empty())
      return fail(e, "House return source transition order differs");
    d.steps_.push_back(FieldDoorSceneStep(step));
  }
  count = r.count(4096);
  for (uint32_t i = 0; i < count; ++i) {
    auto source = r.text();
    auto hash = r.hash();
    if (!path(source) || !nz(hash) || !d.sources_.emplace(source, hash).second)
      return fail(e, "House return source closure malformed");
  }
  std::array<uint8_t, 32> hash;
  if (!r.ok || r.at != n || !d.source_hash(d.target_, hash) ||
      hash != d.identity_.source_sha256 || !d.source_hash(d.source_, hash) ||
      !d.source_hash(ready.script, hash) ||
      !d.source_hash(d.native_[0].script, hash) ||
      hash != d.native_[0].script_sha)
    return fail(e,
                "House return truncated/unknown/source native proof rejected");
  for (const auto &tilemap : d.tilemaps_) {
    if (!d.source_hash(tilemap.resource_source, hash) || hash != tilemap.resource_sha)
      return fail(e, "House return TileSet source proof differs");
  }
  d.valid_ = true;
  if (!d.matches(doors, room, house, e))
    return false;
  *this = std::move(d);
  e.clear();
  return true;
}
bool HouseReentryData::load_file(const char *path, const FieldDoorData &doors,
                                 RoomView room, HouseView house,
                                 std::string &e) {
  if (!path)
    return fail(e, "House return file path absent");
  FILE *f = std::fopen(path, "rb");
  if (!f)
    return fail(e, "House return file cannot open");
  if (std::fseek(f, 0, SEEK_END)) {
    std::fclose(f);
    return fail(e, "House return file seek failed");
  }
  long size = std::ftell(f);
  if (size < 128 || size > 4 * 1024 * 1024 || std::fseek(f, 0, SEEK_SET)) {
    std::fclose(f);
    return fail(e, "House return file size unsupported");
  }
  std::vector<uint8_t> b(static_cast<size_t>(size));
  bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
  std::fclose(f);
  return ok ? load(b.data(), b.size(), doors, room, house, e)
            : fail(e, "House return file incomplete");
}
} // namespace encore::upstream
