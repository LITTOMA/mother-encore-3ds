#pragma once
#include "encore/movement.hpp"
#include <array>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
namespace encore::upstream {
struct FieldInteractChoice {
  std::string flag, programme;
};
struct FieldInteractDescriptor {
  uint32_t id = 0, ready = 0, prompt = 0, flags = 0;
  Vec2 button_offset{};
  std::string node, dialogue, thoughts, key_item, appear, disappear;
  std::vector<FieldInteractChoice> choices;
};
class FieldInteractData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  uint32_t scene_id() const { return scene_id_; }
  const std::array<uint8_t, 20> &source_pin() const { return pin_; }
  const std::vector<FieldInteractDescriptor> &records() const {
    return records_;
  }
  const FieldInteractDescriptor *record(uint32_t) const;
  std::string_view scene() const { return scene_; }
  std::string_view script() const { return script_; }
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;

private:
  bool valid_ = false;
  uint32_t scene_id_ = 0;
  std::array<uint8_t, 20> pin_{};
  std::string scene_, script_;
  std::vector<FieldInteractDescriptor> records_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
struct FieldInteractHost {
  std::function<bool(const FieldInteractDescriptor &, std::string &)>
      admit_ready;
  std::function<bool(uint32_t, std::function<bool(std::string &)>,
                     std::string &)>
      connect_flags;
  std::function<bool(std::string_view, bool &, std::string &)> read_flag;
  std::function<bool(uint32_t, bool, std::string &)> visible;
  std::function<bool(uint32_t, std::string &)> queue_free;
  // Serialized setget runs during instancing, before ButtonPrompt's Ready.
  std::function<bool(uint32_t, Vec2, std::string &)> apply_serialized_offset;
  std::function<bool(std::string_view, std::string &)> admit_programme;
  std::function<bool(uint32_t, std::string_view, std::string &)> open_programme;
  std::function<bool(bool, std::string &)> telepathy_effect;
};
struct FieldInteractState {
  uint32_t id = 0;
  bool instantiated = false, ready = false, visible = true, queued = false,
       deleted = false;
};
class FieldInteractRuntime {
public:
 const FieldInteractData*data()const{return data_;}
  bool initialize(const FieldInteractData &, FieldInteractHost, std::string &);
  bool instantiate(uint32_t,bool source_constructor=false);
  bool complete_source_constructor(uint32_t);
  bool ready(uint32_t);
  bool flags_updated(uint32_t);
  bool interact(uint32_t);
  bool interact_item(uint32_t, std::string_view actual_item_name);
  bool telepathy(uint32_t);
  bool has_thoughts(uint32_t, bool &) const;
  bool commit_deleted(uint32_t);
  const FieldInteractState *state(uint32_t) const;
  const std::string &error() const { return error_; }

private:
  const FieldInteractData *data_ = nullptr;
  FieldInteractHost host_;
  std::set<uint32_t> pending_source_constructor_;
  std::map<uint32_t, FieldInteractState> states_;
  bool poisoned_ = false, had_ready_ = false;
  uint32_t last_ready_ = 0;
  std::string error_;
  bool fail(std::string_view);
  bool callback(bool);
  bool selected(const FieldInteractDescriptor &, std::string &);
  FieldInteractState *live(uint32_t, bool require_ready);
};
} // namespace encore::upstream