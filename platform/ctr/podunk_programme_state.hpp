#pragma once
#include "podunk_house_continuation.hpp"
#include "podunk_programme_host.hpp"

namespace encore::ctr {
struct PodunkProgrammeStateInput {
  PodunkHouseContinuation *continuation = nullptr;
  PodunkInventoryHost *inventory = nullptr;
  const upstream::FieldProgrammeData *programme = nullptr;
  const upstream::BasementProgressionData *basement = nullptr;
  upstream::FieldNpcRuntime *npc = nullptr;
  upstream::FieldNodeTreeRuntime *tree = nullptr;
  upstream::FieldSceneHost *scene = nullptr;
  upstream::FieldObjectId player = 0;
  // Same owning session, not a legacy InventoryState or a default LOAD.
  upstream::SessionSnapshot *session = nullptr;
};

// State-only bridge. Dialogue nodes, choices, audio and lifecycle execution
// remain the concrete caller's responsibilities and are never approved here.
class PodunkProgrammeState {
public:
  bool prepare(PodunkProgrammeStateInput, std::string &);
  bool apply(PodunkProgrammeOps &, std::string &);
  bool actor(uint32_t, uint64_t &, std::string &, std::string &) const;
  bool actual_path(uint64_t, std::string &, std::string &) const;
  bool flag(std::string_view, bool &, std::string &) const;
  bool seen(std::string_view, bool &, std::string &) const;
  bool mark_seen(std::string_view, std::string &);
  bool admit_key(const upstream::BasementKeyItem &, std::string &) const;
  bool grant_key(const upstream::BasementKeyItem &, std::string &);
  // Called by the actual save boundary after the programme has completed.
  // Overlays only the owned KEY/normal/object/seen domains; other session
  // fields and saved identities are preserved verbatim.
  bool writeback(std::string &);

private:
  struct Grant {
    uint32_t key = 0, definition = 0;
    std::string programme, label;
  };
  PodunkProgrammeStateInput input_;
  std::vector<Grant> grants_;
  bool prepared_ = false;
  bool owners(std::string &) const;
  bool live(std::string &) const;
  bool seen_identity(std::string_view, std::string &) const;
  const Grant *grant(const upstream::BasementKeyItem &) const;
};
} // namespace encore::ctr
