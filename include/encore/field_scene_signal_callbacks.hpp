#pragma once
#include "encore/field_data.hpp"
#include <map>
namespace encore::upstream {
enum class SceneSignalSymbol : uint32_t {
  Flags = 1,
  AreaLeft,
  Switches,
  NearbyEntered,
  NearbyExited,
  Paused,
  Unpaused,
  InputsChanged,
  LocaleChanged,
  Hide,
  FrameChanged,
  AnimationFinished,
  FunctionCallback,
  FunctionClass,
  SwitchSource
};
enum class SceneCallbackRole : uint32_t {
  CheckFlags = 1,
  LeaveArea,
  Switches,
  UpdateDoor,
  Nearby,
  Pause,
  KeyName,
  ArrowFinished,
  DroppedPaused,
  DroppedUnpaused
};
struct SceneCallbackBinding {
  uint32_t node = 0;
  SceneCallbackRole role{};
  std::string script, method_source, method;
  std::array<uint8_t, 32> leaf_sha{}, method_sha{};
  uint32_t arguments = 0;
};
struct SceneSignalBinding {
  SceneSignalSymbol role{};
  std::string name;
  uint32_t declaration_arguments = 0, emission_arguments = 0;
};
// Only source symbols and signatures; loading grants no lifecycle or methods.
class FieldSceneSignalCallbacksData {
public:
  bool load(const uint8_t *, size_t, const FieldIdentity &, std::string &);
  bool load_file(const char *, const FieldIdentity &, std::string &);
  bool valid() const { return valid_; }
  const FieldIdentity &identity() const { return identity_; }
  const std::string &source_scene() const { return scene_; }
  const SceneSignalBinding *symbol(SceneSignalSymbol) const;
  const SceneCallbackBinding *callback(uint32_t, SceneCallbackRole) const;
  const std::vector<SceneCallbackBinding> &callbacks() const {
    return callbacks_;
  }
  const std::map<std::string, std::array<uint8_t, 32>> &sources() const {
    return sources_;
  }
  const std::string &wait_source() const { return wait_source_; }
  const std::array<uint8_t, 32> &wait_sha() const { return wait_sha_; }
  uint32_t wait_id() const { return wait_id_; }
  uint32_t wait_flags() const { return wait_flags_; }
  uint32_t party_member_declaration() const { return party_member_; }
  const std::string &party_member_name() const { return party_member_name_; }
  const std::array<uint8_t, 32> &constructor_sha() const { return constructor_sha_; }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::string scene_, wait_source_;
  std::array<uint8_t, 32> wait_sha_{};
  uint32_t wait_id_ = 0, wait_flags_ = 0;
  uint32_t party_member_ = 0;
  std::string party_member_name_;
  std::array<uint8_t, 32> constructor_sha_{};
  std::vector<SceneSignalBinding> symbols_;
  std::vector<SceneCallbackBinding> callbacks_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
} // namespace encore::upstream
