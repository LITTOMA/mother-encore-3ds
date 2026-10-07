#pragma once
#include "encore/field_node_recipe.hpp"
#include "encore/field_runtime.hpp"
namespace encore::upstream {
enum class GrassNativeRole : uint32_t {
  Area = 1,
  Shape,
  Sprite,
  Animation,
  Graph,
  Timer
};
struct GrassNativeConnection {
  uint32_t emitter = 0, receiver = 0, kind = 0;
  std::string signal, method;
};
class GrassNativeData {
public:
  bool load(const uint8_t *, size_t, const FieldData &,
            const FieldNodeTreeData &, std::string &);
  bool load_file(const char *, const FieldData &, const FieldNodeTreeData &,
                 std::string &);
  bool valid() const { return valid_; }
  const FieldNodeRecipeData &recipe() const { return recipe_; }
  FieldIdentity identity() const { return recipe_.identity(); }
  const auto &ir_sha() const { return ir_; }
  const auto &connections() const { return connections_; }
  uint32_t node(GrassNativeRole r) const {
    return nodes_.at(static_cast<size_t>(r) - 1);
  }
  const FieldGrassProfile &profile() const { return profile_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;
  bool monitoring() const { return monitoring_; }
  bool monitorable() const { return monitorable_; }
  bool disabled() const { return disabled_; }
  bool centered() const { return centered_; }
  const std::string &script() const { return script_; }
  const std::string &tween_source() const { return tween_source_; }
  const std::string &tween_signal() const { return tween_signal_; }
  bool native_matches(const FieldNodeDescriptor &) const;

private:
  bool valid_ = false, monitoring_ = false, monitorable_ = false,
       disabled_ = false, centered_ = false;
  std::string script_, tween_source_, tween_signal_;
  std::array<uint8_t, 32> ir_{};
  FieldNodeRecipeData recipe_;
  std::array<uint32_t, 6> nodes_{};
  FieldGrassProfile profile_{};
  std::vector<GrassNativeConnection> connections_;
};
} // namespace encore::upstream
