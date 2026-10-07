#pragma once
#include "encore/field_node_tree.hpp"
#include "encore/field_prompts.hpp"
namespace encore::upstream {
enum class PromptNativeRole : uint32_t { Box = 1, Label, Arrow, Player };
struct PromptNativeRecord {
  uint32_t id = 0, prompt = 0, parent = 0;
  PromptNativeRole role{};
  std::string path, native_class;
};
struct PromptNativeInput {
  std::string action, label;
  uint32_t mask = 0, parameter = 0;
};
struct PromptNativeControl {
  Vec2 position{}, size{};
  std::string text;
};
class PromptNativeData {
public:
  bool load(const uint8_t *, size_t, const FieldNodeTreeData &,
            const FieldPromptData &, std::string &);
  bool load_file(const char *, const FieldNodeTreeData &,
                 const FieldPromptData &, std::string &);
  bool valid() const { return valid_; }
  const FieldIdentity &identity() const { return identity_; }
  const std::vector<PromptNativeRecord> &records() const { return records_; }
  const PromptNativeRecord *record(uint32_t) const;
  const PromptNativeRecord *leaf(uint32_t, PromptNativeRole) const;
  const std::string &settings_member() const { return strings_[0]; }
  const std::string &paused_member() const { return strings_[1]; }
  const std::string &font() const { return strings_[2]; }
  const std::string &internal_group() const { return strings_[3]; }
  const std::string &started_signal() const { return strings_[4]; }
  const std::string &finished_signal() const { return strings_[5]; }
  Vec2 box_size() const { return box_; }
  const std::vector<PromptNativeInput> &inputs() const { return inputs_; }
  const PromptNativeControl *control(PromptNativeRole r) const {
    return uint32_t(r) >= 1 && uint32_t(r) <= 3 ? &controls_[uint32_t(r) - 1]
                                                : nullptr;
  }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<std::string, 6> strings_{};
  Vec2 box_{};
  std::vector<PromptNativeRecord> records_;
  std::vector<PromptNativeInput> inputs_;
  std::array<PromptNativeControl, 3> controls_{};
};
} // namespace encore::upstream
