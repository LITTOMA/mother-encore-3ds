#include "encore/field_dialogue_lifecycle.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {
namespace {
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = ~0u;
  for (size_t i = 0; i < n; ++i) {
    c ^= i >= 16 && i < 20 ? 0 : p[i];
    for (unsigned j = 0; j < 8; ++j)
      c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
  }
  return ~c;
}
bool path(std::string_view p) {
  return !p.empty() && p.front() != '/' && p.back() != '/' &&
         p.find("..") == p.npos && p.find_first_of(":\\") == p.npos;
}
bool nonzero(const std::array<uint8_t, 32> &h) {
  return std::any_of(h.begin(), h.end(), [](auto x) { return x != 0; });
}
struct Reader {
  const uint8_t *p;
  size_t n, at = 64;
  bool ok = true;
  uint32_t integer() {
    if (!ok || at > n || n - at < 4) {
      ok = false;
      return 0;
    }
    auto v = word(p + at);
    at += 4;
    return v;
  }
  uint32_t count(uint32_t limit) {
    auto v = integer();
    if (v > limit)
      ok = false;
    return ok ? v : 0;
  }
  bool boolean() {
    auto v = integer();
    if (v > 1)
      ok = false;
    return v != 0;
  }
  double scalar() {
    if (!ok || at > n || n - at < 8) {
      ok = false;
      return 0;
    }
    uint64_t b = uint64_t(word(p + at)) | uint64_t(word(p + at + 4)) << 32;
    at += 8;
    double v;
    std::memcpy(&v, &b, 8);
    if (!std::isfinite(v) || std::abs(v) > 1e6)
      ok = false;
    return v;
  }
  std::string text() {
    auto len = count(65536);
    if (!ok || at > n || len > n - at) {
      ok = false;
      return {};
    }
    std::string s(reinterpret_cast<const char *>(p + at), len);
    at += len;
    size_t i = 0;
    uint32_t cp = 0;
    while (i < s.size())
      if (!utf8_next(s, i, cp) || cp == 0 || cp < 32) {
        ok = false;
        break;
      }
    return s;
  }
  std::array<uint8_t, 32> hash() {
    std::array<uint8_t, 32> h{};
    if (!ok || at > n || n - at < 32) {
      ok = false;
      return h;
    }
    std::copy_n(p + at, 32, h.begin());
    at += 32;
    if (!nonzero(h))
      ok = false;
    return h;
  }
  FieldDialogueClip clip() {
    FieldDialogueClip c;
    c.name = text();
    c.path = text();
    c.resource = integer();
    c.loop = boolean();
    c.interpolation = integer();
    c.loop_wrap = boolean();
    c.length = scalar();
    c.step = scalar();
    auto count_keys = count(64);
    if (c.name.empty() || c.path.empty() || !c.resource || c.loop ||
        c.interpolation > 2 || c.length <= 0 || c.length > 60 || c.step <= 0 ||
        !count_keys)
      ok = false;
    for (uint32_t i = 0; i < count_keys; ++i) {
      FieldDialogueKey k;
      k.time = scalar();
      k.transition = scalar();
      const auto x = scalar(), y = scalar();
      k.value = {float(x), float(y)};
      if (k.time < 0 || k.time > c.length || k.transition <= 0 ||
          (!c.keys.empty() && k.time <= c.keys.back().time))
        ok = false;
      c.keys.push_back(k);
    }
    return c;
  }
};
} // namespace
std::string_view FieldDialogueLifecycleData::node(uint32_t role) const {
  return role && role <= nodes_.size() ? std::string_view(nodes_[role - 1])
                                       : std::string_view{};
}
bool FieldDialogueLifecycleData::source_hash(std::string_view p,
                                             std::array<uint8_t, 32> &h) const {
  auto i = sources_.find(std::string(p));
  if (i == sources_.end())
    return false;
  h = i->second;
  return true;
}
bool FieldDialogueLifecycleData::supports(const FieldProgrammeData &d,
                                          uint32_t p, std::string &e) const {
  const auto *r = d.record(p);
  if (!valid_ || !d.valid() || d.commit() != pin_ || !r) {
    e = "Dialogue lifecycle programme source owner mismatch";
    return false;
  }
  for (const auto &v : programmes_)
    if (v.path == r->path) {
      std::array<uint8_t, 32> h{};
      if (v.source != r->source || !d.source_hash(v.source, h) || h != v.sha) {
        e = "Dialogue lifecycle programme source changed";
        return false;
      }
      e.clear();
      return true;
    }
  e = "Dialogue lifecycle actor/battle/respawn programme capability pending";
  return false;
}
bool FieldDialogueLifecycleData::load_file(const char *p, std::string &e) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f) {
    e = "Cannot open dialogue lifecycle pack";
    return false;
  }
  auto end = f.tellg();
  if (end < 0 || uint64_t(end) > 2u * 1024u * 1024u) {
    e = "Dialogue lifecycle file bound";
    return false;
  }
  std::vector<uint8_t> b(static_cast<size_t>(end));
  f.seekg(0);
  if (!b.empty() &&
      !f.read(reinterpret_cast<char *>(b.data()), std::streamsize(b.size()))) {
    e = "Truncated dialogue lifecycle file";
    return false;
  }
  return load(b.data(), b.size(), e);
}
bool FieldDialogueLifecycleData::load(const uint8_t *p, size_t n,
                                      std::string &e) {
  auto reject = [&](const char *m) {
    e = m;
    return false;
  };
  if (!p || n < 64 || n > 2u * 1024u * 1024u)
    return reject("Dialogue lifecycle pack bound");
  if (std::memcmp(p, "ENCDLIF1", 8) || word(p + 8) != 1 || word(p + 12) != n ||
      word(p + 16) != crc(p, n) || word(p + 20) != 1 || word(p + 24) != 1 ||
      word(p + 28) > 256)
    return reject("Dialogue lifecycle version/capability/CRC rejected");
  for (size_t i = 52; i < 64; ++i)
    if (p[i])
      return reject("Dialogue lifecycle header reserved");
  FieldDialogueLifecycleData d;
  std::copy_n(p + 32, 20, d.pin_.begin());
  if (std::all_of(d.pin_.begin(), d.pin_.end(), [](auto v) { return !v; }))
    return reject("Dialogue lifecycle source pin absent");
  Reader r{p, n};
  d.scene_ = r.text();
  d.scene_sha_ = r.hash();
  d.factory_scene_id_ = r.integer();
  d.factory_node_count_ = r.count(100000);
  d.factory_ir_sha_ = r.hash();
  if (!d.factory_scene_id_ || !d.factory_node_count_)
    return reject("Dialogue lifecycle factory identity absent");
  d.closed_y_ = r.scalar();
  d.response_ = r.integer();
  d.pause_[0] = r.boolean();
  d.pause_[1] = r.boolean();
  d.done_ = r.text();
  d.ready_ = r.text();
  d.animation_ = r.text();
  d.name_signal_ = r.text();
  d.paused_method_ = r.text();
  if (!path(d.scene_) || d.response_ != 0 || d.done_.empty() ||
      d.ready_.empty() || d.animation_.empty() || d.name_signal_.empty() ||
      d.paused_method_.empty())
    return reject("Dialogue lifecycle source policy missing");
  auto nodes = r.count(12);
  if (nodes != 12)
    return reject("Dialogue lifecycle exact native refs missing");
  std::set<std::string> paths;
  std::set<uint32_t> source_ids;
  for (uint32_t i = 0; i < nodes; ++i) {
    auto role = r.integer();
    FieldDialogueNativeRef ref;
    ref.id = r.integer();
    ref.ready = r.integer();
    auto s = r.text();
    ref.native_class = r.text();
    if (!ref.id || !source_ids.insert(ref.id).second ||
        ref.ready >= d.factory_node_count_ || ref.native_class.empty())
      return reject("Dialogue lifecycle factory native ref incomplete");
    d.references_[i] = std::move(ref);
    if (role != i + 1 || (i ? s.empty() || !path(s) : s != ".") ||
        !paths.insert(s).second)
      return reject("Dialogue lifecycle native path binding rejected");
    d.nodes_[i] = std::move(s);
  }
  auto count = r.count(256);
  if (count != word(p + 28) || !count)
    return reject("Dialogue lifecycle operation count");
  std::array<bool, 9> stages{};
  uint32_t previous = 0;
  for (uint32_t i = 0; i < count; ++i) {
    FieldDialogueStep s;
    auto stage = r.integer(), op = r.integer();
    s.role = r.integer();
    s.value = r.scalar();
    s.text = r.text();
    if (stage >= stages.size() || stage < previous ||
        op > uint32_t(FieldDialogueOp::StopTalker) || s.role > 12)
      return reject("Dialogue lifecycle unknown stage/op/native ref");
    previous = stage;
    stages[stage] = true;
    s.stage = FieldDialogueStage(stage);
    s.op = FieldDialogueOp(op);
    switch (s.op) {
    case FieldDialogueOp::UiCutscene:
    case FieldDialogueOp::GlobalCutscene:
    case FieldDialogueOp::BlackBars:
    case FieldDialogueOp::SetTalker:
      if (s.value != 0 && s.value != 1)
        return reject("Dialogue lifecycle source bool");
      break;
    case FieldDialogueOp::ConnectName:
    case FieldDialogueOp::InputRelease:
    case FieldDialogueOp::EmitDone:
    case FieldDialogueOp::EmitCutsceneEnded:
    case FieldDialogueOp::DeferredAdd:
    case FieldDialogueOp::ResetPhrase:
    case FieldDialogueOp::NameClose:
      if (s.text.empty())
        return reject("Dialogue lifecycle source signal/member absent");
      break;
    case FieldDialogueOp::ReturnCamera:
    case FieldDialogueOp::ReturnOffset:
      if (s.value <= 0 || s.value > 60)
        return reject("Dialogue lifecycle source camera duration");
      break;
    default:
      break;
    }
    d.steps_.push_back(std::move(s));
  }
  if (!std::all_of(stages.begin(), stages.end(), [](bool v) { return v; }))
    return reject("Dialogue lifecycle missing source phase");
  auto programmes = r.count(128);
  std::set<std::string> identities;
  for (uint32_t i = 0; i < programmes; ++i) {
    FieldDialogueProgramme v;
    v.path = r.text();
    v.source = r.text();
    v.sha = r.hash();
    if (!path(v.path) || !path(v.source) || !identities.insert(v.path).second)
      return reject("Dialogue lifecycle programme refs");
    d.programmes_.push_back(std::move(v));
  }
  if (!programmes)
    return reject("Dialogue lifecycle source programmes absent");
  d.close_ = r.clip();
  d.name_close_ = r.clip();
  auto sources = r.count(2048);
  for (uint32_t i = 0; i < sources; ++i) {
    auto file = r.text();
    auto h = r.hash();
    if (!path(file) || !d.sources_.emplace(file, h).second)
      return reject("Dialogue lifecycle source identity duplicate");
  }
  if (!r.ok || r.at != n)
    return reject("Dialogue lifecycle malformed/truncated payload");
  std::array<uint8_t, 32> h{};
  if (!d.source_hash(d.scene_, h) || h != d.scene_sha_)
    return reject("Dialogue lifecycle original scene source differs");
  for (const auto &v : d.programmes_)
    if (!d.source_hash(v.source, h) || h != v.sha)
      return reject("Dialogue lifecycle original programme source differs");
  d.valid_ = true;
  *this = std::move(d);
  e.clear();
  return true;
}
} // namespace encore::upstream
