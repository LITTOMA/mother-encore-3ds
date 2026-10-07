#pragma once
#include "encore/field_node_tree.hpp"
#include <map>
namespace encore::upstream {
struct AudioServerEffect {
  uint32_t source_id = 0;
  std::string native_class, resource_name;
  float cutoff = 0, resonance = 0;
  bool enabled = false;
};
struct AudioServerBus {
  std::string name, send;
  float volume_db = 0;
  bool solo = false, muted = false, bypass = false;
  std::vector<AudioServerEffect> effects;
};
struct AudioServerSetting {
  std::string method, member, bus, signal;
};
class AudioServerData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  FieldIdentity identity() const { return identity_; }
  const auto &buses() const { return buses_; }
  const auto &settings() const { return settings_; }
  const auto &ir_sha() const { return ir_; }
  const std::string &engine_source() const { return engine_; }
  const std::string &layout_signal() const { return signal_; }
  const std::string &layout_method() const { return method_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  FieldIdentity identity_{};
  std::array<uint8_t, 32> ir_{};
  std::string engine_, signal_, method_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
  std::vector<AudioServerBus> buses_;
  std::vector<AudioServerSetting> settings_;
};
// Actual mutable bus body. No Node/Ready, independent audio clock, or guessed
// settings value is introduced here. Effects are retained and refused at use
// until the corresponding DSP processor exists.
class AudioServerRuntime {
public:
  bool initialize(const AudioServerData &, std::string &);
  bool gain(std::string_view, float &, bool &, std::string &) const;
  int index(std::string_view) const;
  bool set_volume(uint32_t, float, std::string &);
  const AudioServerData *data() const { return data_; }
  const auto &buses() const { return buses_; }

private:
  const AudioServerData *data_ = nullptr;
  std::vector<AudioServerBus> buses_;
};
} // namespace encore::upstream
