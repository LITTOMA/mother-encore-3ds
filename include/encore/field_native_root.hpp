#pragma once
#include "encore/field_global_registry.hpp"
namespace encore::upstream {
class FieldNativeRootData {
public:
  bool load(const uint8_t *, size_t, const FieldGlobalRegistryData &,
            std::string &);
  bool load_file(const char *, const FieldGlobalRegistryData &, std::string &);
  bool valid() const { return valid_; }
  const FieldIdentity &identity() const { return identity_; }
  const FieldColor &clear_color() const { return clear_; }
  uint32_t flags() const { return flags_; }
  const std::string &root_name() const { return root_; }
  const std::string &root_native() const { return viewport_; }
  const std::string &kernel_native() const { return kernel_; }
  const std::map<std::string, std::array<uint8_t, 32>> &engine_sources() const {
    return engine_;
  }

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  FieldColor clear_{};
  uint32_t flags_ = 0;
  std::string root_, viewport_, kernel_;
  std::map<std::string, std::array<uint8_t, 32>> engine_;
};
} // namespace encore::upstream
