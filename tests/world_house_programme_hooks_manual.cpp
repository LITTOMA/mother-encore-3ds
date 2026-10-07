#include "room_fixture.hpp"
#include "encore/world.hpp"
#include <cstdio>
#include <string>

// Manual API boundary check only. This test double does not implement or grant
// House UI/native ownership. It uses the explicitly supplied external Room
// pack; no game command table or source assets are embedded in this file.
namespace {
using namespace encore::upstream;
struct ManualOwner final : OpeningHouseProgrammeOwner {
  OpeningWorld *receiver = nullptr;
  uint32_t programme = kRoomNoIndex, starts = 0, actions = 0;
  uint32_t previous_generation = 0, started_generation = 0;
  bool closed = true, veto_start = false, veto_action = false;
  bool check_reentrant_unbind = false, reentrant_unbind_rejected = false;
  const OpeningWorld *world() const override { return receiver; }
  bool source_frame_closed(const OpeningWorld &w, std::string &e) const override {
    if (&w != receiver || !closed) { e = "manual source frame is open"; return false; }
    e.clear(); return true;
  }
  bool source_started(OpeningWorld &w, uint32_t generation, std::string &e) override {
    ++starts;
    auto expected = previous_generation + 1; if (!expected) ++expected;
    if (&w != receiver || generation != expected ||
        generation != w.source_generation() || w.story_program_index() != programme ||
        w.story_talker().kind != DialogueTalkerKind::None ||
        w.story_generation() != 0 || !w.action_trace().empty()) {
      e = "manual source-start boundary differs"; return false;
    }
    started_generation = generation;
    if (check_reentrant_unbind) {
      std::string failure;
      reentrant_unbind_rejected = !w.unbind_house_programme_owner(*this, failure);
      if (!reentrant_unbind_rejected || w.house_programme_owner() != this) {
        e = "manual source callback unbound its executing owner"; return false;
      }
    }
    if (veto_start) { e = "manual start veto"; return false; }
    e.clear(); return true;
  }
  bool before_action(OpeningWorld &w, const DialogueAction &action, std::string &e) override {
    ++actions;
    const auto p = w.content().program(programme);
    const auto next = w.story_next_command_index();
    if (&w != receiver || w.story_program_index() != programme || !next ||
        next > p.command_count || w.story_generation() != started_generation) {
      e = "manual action generation/cursor differs"; return false;
    }
    const auto source = w.content().command(p.first_command + next - 1);
    if (source.opcode != uint16_t(action.kind) || source.actor_index != action.actor ||
        source.phrase != action.phrase || source.target_index != action.target_index ||
        source.auxiliary_index != action.auxiliary_index || source.flags != action.flags ||
        source.vector.x != action.vector.x || source.vector.y != action.vector.y ||
        source.value != action.value || source.duration != action.duration) {
      e = "manual action is outside actual source cursor"; return false;
    }
    if (veto_action) {
      if (!w.action_trace().empty()) { e = "manual rejected action was already traced"; return false; }
      e = "manual action veto"; return false;
    }
    e.clear(); return true;
  }
};
bool same_action(const DialogueAction &a, const DialogueAction &b) {
  return a.kind == b.kind && a.actor == b.actor && a.phrase == b.phrase &&
         a.target_index == b.target_index && a.auxiliary_index == b.auxiliary_index &&
         a.flags == b.flags && a.vector.x == b.vector.x && a.vector.y == b.vector.y &&
         a.value == b.value && a.duration == b.duration;
}
}
int main() {
  const auto room = encore_test::room();
  const auto programme = encore_test::opening_program();
  std::string error;
  ManualOwner owner, other;
  OpeningWorld bound, foreign;
  if (!bound.initialize(room) || !foreign.initialize(room)) return 1;
  owner.receiver = &foreign; owner.programme = programme;
  if (bound.bind_house_programme_owner(owner, error) || bound.house_programme_owner()) return 2;
  owner.receiver = &bound; owner.closed = false;
  if (bound.bind_house_programme_owner(owner, error) || bound.house_programme_owner()) return 3;
  owner.closed = true;
  if (!bound.bind_house_programme_owner(owner, error)) return 4;
  other.receiver = &bound;
  if (bound.bind_house_programme_owner(other, error) ||
      bound.unbind_house_programme_owner(other, error)) return 5;
  owner.closed = false;
  if (bound.unbind_house_programme_owner(owner, error) || bound.house_programme_owner() != &owner) return 6;
  const auto generation = bound.source_generation();
  if (bound.initialize(room) || bound.source_generation() != generation ||
      bound.house_programme_owner() != &owner) return 7;
  owner.closed = true;
  if (!bound.unbind_house_programme_owner(owner, error) || bound.house_programme_owner()) return 8;

  ManualOwner start_owner;
  OpeningWorld start_veto;
  if (!start_veto.initialize(room)) return 9;
  start_owner.receiver = &start_veto;
  start_owner.programme = programme; start_owner.previous_generation = start_veto.source_generation();
  start_owner.veto_start = true; start_owner.check_reentrant_unbind = true;
  if (!start_veto.bind_house_programme_owner(start_owner, error) ||
      start_veto.begin_house_program(programme) || start_veto.healthy() ||
      start_owner.starts != 1 || start_owner.actions || !start_owner.reentrant_unbind_rejected ||
      !start_veto.action_trace().empty() || start_veto.story_generation() != 0 ||
      std::string(start_veto.error()) != "manual start veto") return 10;
  if (!start_veto.unbind_house_programme_owner(start_owner, error)) return 11;

  ManualOwner action_owner;
  OpeningWorld action_veto;
  if (!action_veto.initialize(room)) return 12;
  action_owner.receiver = &action_veto;
  action_owner.programme = programme; action_owner.previous_generation = action_veto.source_generation();
  action_owner.veto_action = true;
  if (!action_veto.bind_house_programme_owner(action_owner, error) ||
      action_veto.begin_house_program(programme) || action_veto.healthy() ||
      action_owner.starts != 1 || action_owner.actions != 1 ||
      !action_veto.action_trace().empty() || action_veto.battle_request().queued ||
      std::string(action_veto.error()) != "manual action veto") return 13;

  ManualOwner accepting;
  OpeningWorld unchanged, accepted;
  if (!unchanged.initialize(room) || !accepted.initialize(room)) return 14;
  accepting.receiver = &accepted; accepting.programme = programme;
  accepting.previous_generation = accepted.source_generation();
  if (!accepted.bind_house_programme_owner(accepting, error) ||
      !unchanged.begin_house_program(programme) || !accepted.begin_house_program(programme)) return 15;
  if (accepting.starts != 1 || !accepting.actions ||
      accepted.source_generation() != unchanged.source_generation() ||
      accepted.story_generation() != unchanged.story_generation() ||
      accepted.action_trace().size() != unchanged.action_trace().size() ||
      accepted.stage() != unchanged.stage()) return 16;
  for (size_t i = 0; i < unchanged.action_trace().size(); ++i)
    if (!same_action(accepted.action_trace()[i].action, unchanged.action_trace()[i].action)) return 17;
  std::puts("Manual World hook boundary checks completed; no House UI/native ownership established.");
}
