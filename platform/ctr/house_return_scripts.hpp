#pragma once
#include "encore/field_global_constructor.hpp"
#include "encore/field_global_data.hpp"
#include "encore/field_global_flags.hpp"
#include "encore/field_object_signals.hpp"
#include "encore/field_scene_data.hpp"
#include "encore/field_scene_signal_callbacks.hpp"
#include "encore/house_reentry.hpp"

namespace encore::ctr {
// Source methods only. The actual target owner still constructs the native
// subclass, applies the remaining exported properties and executes native
// notifications. This adapter cannot issue a complete Node binding receipt.
class HouseReturnScripts {
public:
  enum class Role { Unmapped, AreaRoom, FlagLandmark };
  struct RosterEntry {
    uint32_t source = 0;
    std::string path, script;
    Role role = Role::Unmapped;
  };
  struct Input {
    const upstream::HouseReentryData *reentry = nullptr;
    const upstream::FieldNodeTreeData *nodes = nullptr;
    const upstream::FieldDoorData *doors = nullptr;
    upstream::RoomView room;
    upstream::HouseView house;
    upstream::FieldNodeTreeRuntime *tree = nullptr;
    upstream::FieldGlobalRegistry *registry = nullptr;
    upstream::FieldObjectSignals *signals = nullptr;
    upstream::FieldGlobalConstructorRuntime *global = nullptr;
    upstream::FieldGlobalDataRuntime *globaldata = nullptr;
    upstream::FieldGlobalFlagsRuntime *flags = nullptr;
    const upstream::FieldSceneSignalCallbacksData *callbacks = nullptr;
  };
  bool prepare(Input, std::string &);
  Role mapped_source(uint32_t) const;
  const std::vector<RosterEntry> &roster() const { return roster_; }
  // Called at construct_source, after native allocation/publication, before
  // PackedScene name/parent/owner assignment. Unknown scripts fail closed.
  bool construct(upstream::FieldObjectId, std::string &);
  // ONLY source notifications; no native phase is accepted here.
  bool script_phase(upstream::FieldObjectId, upstream::FieldTreePhase,
                    std::string &);
  // Called by the same ObjectDB's actual source dispatch. Signal callbacks
  // additionally verify the bus's active emitter/target/method frame.
  bool dispatch(const upstream::FieldDeferredMessage &, std::string &);
  bool leave_for(upstream::FieldObjectId actual_destination,
                 const upstream::FieldSceneData &destination, std::string &);
  bool switches_state(upstream::FieldObjectId, bool &, std::string &) const;
  // Exit is not destruction: original source connections survive remove_child.
  // Invoke only after actual native deletion and ObjectDB collection.
  bool release_deleted(upstream::FieldObjectId, std::string &);
private:
  struct Body {
    uint32_t source = 0;
    Role role = Role::Unmapped;
    bool entered = false, ready = false, switches = false;
    upstream::HouseReentryLandmark landmark;
  };
  Input input_;
  std::vector<RosterEntry> roster_;
  std::map<uint32_t, upstream::HouseReentryLandmark> landmarks_;
  std::map<upstream::FieldObjectId, Body> bodies_;
  std::array<uint8_t,32> reentry_ir_{};
  upstream::FieldIdentity node_identity_{};
  uint32_t root_source_ = 0;
  upstream::FieldObjectId root_ = 0;
  std::string flags_signal_, check_method_;
  bool poisoned_ = false;
  bool available(std::string &) const;
  bool actual(upstream::FieldObjectId, std::string &) const;
  bool shared_flags(std::string &) const;
  bool area_ready(std::string &);
  bool check_landmark(upstream::FieldObjectId, Body &, std::string &);
  bool read_flag(std::string_view, bool &, std::string &) const;
};
} // namespace encore::ctr
