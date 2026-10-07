#include "encore/field_scene_tree_timer.hpp"
#include <algorithm>
#include <cmath>

namespace encore::upstream {
namespace {
bool fail(std::string &e, const char *m) {
  e = m;
  return false;
}
constexpr const char *engine_source = "scene/main/scene_tree.cpp";
} // namespace
FieldSceneTreeTimer::~FieldSceneTreeTimer() {
  if (registry_ && binding_.object) {
    std::string e;
    // This runs only once the last source Ref is gone, so ObjectDB's weak Ref
    // has expired. Signal cleanup follows actual ObjectDB retirement.
    if (registry_->retire_object(binding_.object, e) && signals_)
      signals_->release(binding_.object, e);
  }
}
bool FieldSceneTreeTimer::checked_source_hash(
    std::string_view path, std::array<uint8_t, 32> &out) const {
  if (path != binding_.source.source)
    return false;
  out = binding_.source.source_sha;
  return true;
}
bool FieldSceneTreeTimer::set_time_left(float value, std::string &e) {
  if (!registry_ || !registry_->object_exists(binding_.object) ||
      !std::isfinite(value))
    return fail(e, "SceneTreeTimer actual Reference/time value rejected");
  time_left_ = value;
  e.clear();
  return true;
}
bool FieldSceneTreeTimers::initialize(const FieldNativeRootData &d,
                                      FieldGlobalRegistry &r,
                                      FieldObjectSignals &signals,
                                      std::string &e) {
  std::array<uint8_t, 32> hash{};
  auto proof = d.engine_sources().find(engine_source);
  if (data_ || !d.valid() || !r.data() || r.poisoned() ||
      signals.registry() != &r || !r.object_exists(r.kernel()) ||
      d.identity().upstream_commit != r.data()->identity().upstream_commit ||
      proof == d.engine_sources().end() ||
      !r.data()->engine_hash(engine_source, hash) || proof->second != hash)
    return fail(e,
                "SceneTreeTimer real SceneTree/ObjectDB/engine proof absent");
  data_ = &d;
  registry_ = &r;
  signals_ = &signals;
  e.clear();
  return true;
}
bool FieldSceneTreeTimers::create_timer(
    float seconds, bool process_pause,
    std::shared_ptr<FieldSceneTreeTimer> &out, std::string &e) {
  if (!data_ || failed_ || !std::isfinite(seconds) || registry_->poisoned())
    return fail(e, "SceneTree.create_timer actual owner/value unavailable");
  auto timer = std::make_shared<FieldSceneTreeTimer>();
  FieldGlobalExternalSpec spec;
  spec.identity = data_->identity();
  spec.identity.source_sha256 = data_->engine_sources().at(engine_source);
  spec.source_sha = spec.identity.source_sha256;
  spec.stable_id =
      1; // Native class identity, independent of saved content IDs.
  spec.role = 5;
  spec.native_class = "SceneTreeTimer";
  spec.source = engine_source;
  FieldObjectId id = 0;
  if (!registry_->allocate_object(id, e))
    return false;
  timer->binding_ = {id, spec, 0x454e006c, 1};
  timer->registry_ = registry_;
  timer->signals_ = signals_;
  // Same native constructor and create_timer setters, before list insertion.
  timer->process_pause_ = process_pause;
  timer->time_left_ = seconds;
  if (!registry_->publish_native_reference(spec, id, timer, e))
    return false;
  timers_.push_back(timer);
  objects_.emplace(id, timer);
  out = std::move(timer);
  e.clear();
  return true;
}
bool FieldSceneTreeTimers::owns(FieldObjectId id) const {
  auto i = objects_.find(id);
  return registry_ && i != objects_.end() && !i->second.expired() &&
         registry_->object_exists(id);
}
bool FieldSceneTreeTimers::signal_declaration(FieldObjectId id,
                                              std::string_view name,
                                              uint32_t &arity,
                                              std::string &e) const {
  if (!owns(id) || name != timeout_signal())
    return fail(e, "SceneTreeTimer unsupported native signal");
  arity = 0;
  e.clear();
  return true;
}
bool FieldSceneTreeTimers::idle(uint64_t epoch, float delta, bool paused,
                                std::string &e) {
  if (!data_ || failed_ || processing_ || !epoch || epoch <= epoch_ ||
      !std::isfinite(delta) || delta < 0 || registry_->poisoned())
    return fail(e, "SceneTreeTimer actual idle cursor/reentry rejected");
  epoch_ = epoch;
  processing_ = true;
  // Capture the source list's old last element. Timers created by a timeout
  // callback stay in the list but are never advanced in this same idle frame.
  const auto last = timers_.empty() ? 0 : timers_.back()->binding().object;
  for (auto i = timers_.begin(); i != timers_.end();) {
    auto current = i++;
    auto &timer = *current;
    const auto id = timer->binding().object;
    if (!paused || timer->process_pause()) {
      const float remaining = timer->time_left() - delta;
      if (!std::isfinite(remaining) || !timer->set_time_left(remaining, e)) {
        failed_ = true;
        processing_ = false;
        return false;
      }
      if (remaining < 0) {
        emitting_ = id;
        const bool ok = signals_->emit(id, timeout_signal(), {}, e);
        emitting_ = 0;
        // Source removes the list Ref even if callback changes time_left.
        timers_.erase(current);
        if (!ok) {
          failed_ = true;
          processing_ = false;
          return false;
        }
      }
    }
    if (id == last)
      break;
  }
  processing_ = false;
  for (auto i = objects_.begin(); i != objects_.end();)
    if (i->second.expired())
      i = objects_.erase(i);
    else
      ++i;
  e.clear();
  return true;
}
bool FieldSceneTreeTimers::shutdown(std::string &e) {
  if (processing_)
    return fail(e, "SceneTreeTimer finalization during idle traversal");
  // This bounded source caller exposes no external Ref aliases. Removing the
  // SceneTree list therefore releases its actual native timer Objects.
  timers_.clear();
  for (const auto &i : objects_)
    if (!i.second.expired())
      return fail(e,
                  "SceneTreeTimer external Ref alias not owned by source tail");
  objects_.clear();
  emitting_ = 0;
  e.clear();
  return true;
}
} // namespace encore::upstream
