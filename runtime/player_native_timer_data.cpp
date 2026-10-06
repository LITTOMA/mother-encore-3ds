#include "encore/field_native_timer.hpp"
#include "encore/player_initialization.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
namespace encore::upstream {
namespace {
using V = std::shared_ptr<const GlobalYamlValue>;
bool fail(std::string &e, const char *s) {
  e = s;
  return false;
}
V get(const V &v, std::string_view k) {
  return v && v->kind == 6 ? v->get(k) : nullptr;
}
std::string text(const V &v) {
  return v && v->kind == 4 ? v->string : std::string{};
}
bool integer(const V &v, uint32_t &out) {
  if (!v || text(get(v, "type")) != "int64")
    return false;
  auto s = text(get(v, "value"));
  if (s.empty() || !std::all_of(s.begin(), s.end(),
                                [](char c) { return c >= '0' && c <= '9'; }))
    return false;
  char *end = nullptr;
  auto value = std::strtoul(s.c_str(), &end, 10);
  if (end != s.c_str() + s.size() ||
      value > std::numeric_limits<uint32_t>::max())
    return false;
  out = uint32_t(value);
  return true;
}
bool number(const V &v, float &out) {
  if (!v || text(get(v, "type")) != "real")
    return false;
  auto s = text(get(v, "value"));
  if (s.empty())
    return false;
  char *end = nullptr;
  auto value = std::strtod(s.c_str(), &end);
  if (end != s.c_str() + s.size() || !std::isfinite(value) || value <= 0 ||
      value > std::numeric_limits<float>::max())
    return false;
  out = float(value);
  return std::isfinite(out) && out > 0;
}
bool flag(const V &v, bool &out) {
  if (!v || v->kind != 1)
    return false;
  out = v->boolean;
  return true;
}
} // namespace
bool FieldNativeTimerData::load_player(const PlayerInitializationData &p,
                                       std::string &e) {
  if (!p.valid() || !p.recipe().valid())
    return fail(e, "Player native Timer checked initialization source missing");
  auto source = p.native_source();
  auto nodes = get(source, "nodes");
  if (!nodes || nodes->kind != 5 ||
      nodes->array.size() != p.recipe().records().size() ||
      text(get(source, "source")) != "res://" + p.recipe().source_scene())
    return fail(
        e, "Player native Timer complete snapshot/recipe identity rejected");
  std::map<std::string, const FieldNodeRecipeRecord *> expected;
  for (const auto &record : p.recipe().records())
    if (!expected.emplace(record.path, &record).second)
      return fail(e, "Player native Timer duplicate recipe path rejected");
  std::set<std::string> seen;
  std::vector<FieldNativeTimerDescriptor> rows;
  for (const auto &node : nodes->array) {
    auto path = text(get(node, "path"));
    auto it = expected.find(path);
    if (it == expected.end() || !seen.insert(path).second ||
        text(get(node, "class")) != it->second->native_class)
      return fail(e, "Player native Timer native node closure rejected");
    const auto &record = *it->second;
    if (record.native_class != "Timer")
      continue;
    auto props = get(node, "properties");
    if (!props || props->kind != 6)
      return fail(e, "Player native Timer property dictionary missing");
    // Exact native serialized property schema. These names are engine members,
    // not scene content. Paused is a native ctor field, not a source export.
    for (const auto &kv : props->dictionary)
      if (kv.first != "_import_path" && kv.first != "pause_mode" &&
          kv.first != "physics_interpolation_mode" &&
          kv.first != "unique_name_in_owner" &&
          kv.first != "process_priority" && kv.first != "process_mode" &&
          kv.first != "wait_time" && kv.first != "one_shot" &&
          kv.first != "autostart" && kv.first != "script")
        return fail(e,
                    "Player native Timer unknown serialized property rejected");
    auto script = get(props, "script");
    bool one = false, autostart = false;
    FieldNativeTimerDescriptor d;
    if (!script || script->kind != 0 ||
        !integer(get(props, "process_mode"), d.mode) || d.mode > 1 ||
        !number(get(props, "wait_time"), d.wait) ||
        !flag(get(props, "one_shot"), one) ||
        !flag(get(props, "autostart"), autostart))
      return fail(e, "Player native Timer property type/mode/wait rejected");
    d.flags = uint32_t(one) | (uint32_t(autostart) << 1);
    d.id = record.id;
    d.identity = p.identity();
    d.script_sha = record.script_sha;
    rows.push_back(d);
  }
  if (seen.size() != expected.size() || rows.empty())
    return fail(e, "Player native Timer complete native class closure absent");
  // Keep recipe (source tree construction) order, not snapshot dictionary
  // order.
  std::vector<FieldNativeTimerDescriptor> ordered;
  for (const auto &record : p.recipe().records())
    if (record.native_class == "Timer") {
      auto i = std::find_if(rows.begin(), rows.end(),
                            [&](const auto &v) { return v.id == record.id; });
      if (i == rows.end())
        return fail(e, "Player native Timer recipe record omitted");
      ordered.push_back(*i);
    }
  if (ordered.size() != rows.size())
    return fail(e, "Player native Timer closure mismatch");
  records_ = std::move(ordered);
  valid_ = true;
  e.clear();
  return true;
}
} // namespace encore::upstream
