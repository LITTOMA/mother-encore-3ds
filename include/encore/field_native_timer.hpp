#pragma once
#include "encore/field_node_tree.hpp"
#include <map>
namespace encore::upstream {
class PlayerInitializationData;
struct FieldNativeTimerDescriptor {
  FieldIdentity identity{};
  uint32_t id = 0, mode = 0, flags = 0;
  float wait = 0;
  std::array<uint8_t, 32> script_sha{};
};
class FieldNativeTimerData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  // Complete source Timer closure from the already checked Player recipe.
  // No arbitrary descriptor or script/native Ready approval is exposed.
  bool load_player(const PlayerInitializationData &, std::string &);
  bool valid() const { return valid_; }
  const FieldNativeTimerDescriptor *record(const FieldIdentity &,
                                           uint32_t) const;
  const std::vector<FieldNativeTimerDescriptor> &records() const {
    return records_;
  }

private:
  bool valid_ = false;
  std::vector<FieldNativeTimerDescriptor> records_;
};
struct FieldNativeTimerState {
  float wait = 0, left = -1;
  uint32_t mode = 0;
  bool one_shot = false, autostart = false, paused = false, processing = false;
};
// Original native Timer only. It grants no ancestor or Timer-attached script
// Ready. Signal delivery and process order belong to the actual Tree owner.
class FieldNativeTimers {
public:
  using Timeout = std::function<bool(FieldObjectId, std::string &)>;
  using Owner = std::function<FieldNodeTreeRuntime *(FieldObjectId)>;
  bool initialize(const FieldNativeTimerData &, Timeout, Owner, std::string &);
  bool attach(FieldNodeTreeRuntime &, FieldObjectId, FieldNodeBinding &,
              std::string &);
  bool ready(FieldObjectId, std::string &);
  bool process(FieldObjectId, FieldTreePhase, float, bool tree_paused,
               std::string &);
  bool start(FieldObjectId, float, std::string &);
  bool stop(FieldObjectId, std::string &);
  bool set_wait(FieldObjectId, float, std::string &);
  bool set_one_shot(FieldObjectId, bool, std::string &);
  bool set_autostart(FieldObjectId, bool, std::string &);
  bool set_paused(FieldObjectId, bool, std::string &);
  bool set_mode(FieldObjectId, uint32_t, std::string &);
  bool release(FieldObjectId, std::string &);
  const FieldNativeTimerState *state(FieldObjectId) const;
  float time_left(FieldObjectId) const;

private:
  struct Timer {
    FieldNodeTreeRuntime *tree = nullptr;
    FieldNativeTimerState state;
  };
  bool schedule(Timer &, FieldObjectId, bool, std::string &);
  Timer *timer(FieldObjectId, std::string &);
  const FieldNativeTimerData *data_ = nullptr;
  Timeout timeout_;
  Owner owner_;
  std::map<FieldObjectId, Timer> timers_;
};
} // namespace encore::upstream
