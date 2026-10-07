#pragma once
#include "encore/basement_progression.hpp"
#include "encore/dialogue.hpp"
#include "encore/house_presentation.hpp"
#include <array>
#include <functional>
#include <map>
namespace encore::upstream {
struct FieldProgrammeNpcRow {
  bool thoughts = false, last = false, supported = false;
  uint32_t group = 0, ordinal = 0;
  std::string flag, program, source;
  std::array<uint8_t, 32> sha{};
};
struct FieldProgrammeNpc {
  uint32_t id = 0, ready_ordinal = 0;
  std::string node;
  std::vector<FieldProgrammeNpcRow> rows;
};
struct FieldProgrammeToken {
  uint32_t kind = 0;
  std::string text;
};
struct FieldProgrammeSegment {
  bool bullet = false;
  std::vector<FieldProgrammeToken> tokens;
};
struct FieldProgrammeLocale {
  std::string speaker;
  std::vector<FieldProgrammeSegment> segments;
};
struct FieldProgrammeText {
  uint32_t id = 0;
  std::string source, label, key, speaker_key, voice;
  std::map<std::string, FieldProgrammeLocale> locales;
};
struct FieldProgrammeOption {
  std::string key;
  uint32_t target_pc = 0;
  std::map<std::string, std::string> texts;
};
struct FieldProgrammeChoice {
  std::string identity, program, label;
  uint32_t initial_selection = 0, cancel_target_pc = 0;
  std::vector<FieldProgrammeOption> options;
};
struct FieldProgrammeRecord {
  RoomProgram table;
  std::string path, source;
};
// Data owns commands/text and provenance. Reloading is forbidden while a
// DialoguePlayer borrows this owner; failed loads preserve every existing view.
class FieldProgrammeData final : public DialogueProgrammeSource {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool flag_emit() const { return flag_emit_; }
  bool valid() const override { return valid_; }
  uint32_t program_count() const override { return uint32_t(programs_.size()); }
  RoomProgram program(uint32_t) const override;
  RoomCommand command(uint32_t) const override;
  uint32_t flag_count() const override { return uint32_t(flags_.size()); }
  uint32_t string_count() const override { return uint32_t(strings_.size()); }
  std::string_view string(uint32_t) const override;
  const FieldProgrammeNpc &npc() const { return npc_; }
  const std::string &scene() const { return scene_; }
  const std::array<uint8_t, 20> &commit() const { return pin_; }
  const FieldProgrammeRecord *record(uint32_t) const;
  const FieldProgrammeText *text(uint32_t) const;
  const FieldProgrammeChoice *choice(uint32_t) const;
  const BasementKeyItem *key(uint32_t) const;
  const std::string *flag(uint32_t) const;
  const std::string *sound(uint32_t) const;
  const std::string *source_label(uint32_t) const;
  bool find_program(std::string_view, uint32_t &) const;
  bool source_hash(std::string_view, std::array<uint8_t, 32> &) const;
  // Source formatting tokens remain separate until the actual presenter
  // supplies checked input labels/player/favorite-food substitutions and source
  // layout.
  bool localize(uint32_t, std::string_view,
                const std::function<bool(std::string_view, std::string &,
                                         std::string &)> &input_label,
                LocalizedHouseSpan &, std::string &) const;

private:
  bool valid_ = false, flag_emit_ = false;
  std::array<uint8_t, 20> pin_{};
  std::string scene_, hint_color_;
  FieldProgrammeNpc npc_;
  std::vector<std::string> strings_{""}, flags_, sounds_, labels_;
  std::vector<RoomCommand> commands_;
  std::vector<FieldProgrammeRecord> programs_;
  std::vector<FieldProgrammeText> texts_;
  std::vector<FieldProgrammeChoice> choices_;
  std::vector<BasementKeyItem> keys_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
struct FieldProgrammeContext {
  uint32_t source_npc = 0;
  uint64_t actor_object = 0, dialogue_object = 0;
  bool thoughts = false;
};
struct FieldProgrammeHost {
  BasementFlagQuery flag;
  std::function<bool(uint32_t, uint64_t &, std::string &, std::string &)> actor;
  // Actual get_path(); stable source node path cannot replace scene-global
  // path.
  std::function<bool(std::string_view, bool &, std::string &)> seen;
  std::function<bool(std::string_view, std::string &)> mark_seen;
  // Must validate the complete action's live endpoints/font coverage/NDSP
  // resources/source flags/key UID ledger before any source mutation or PP
  // debit.
  std::function<bool(const FieldProgrammeData &, uint32_t,
                     const FieldProgrammeContext &, const DialogueAction &,
                     std::string &)>
      admit;
  std::function<bool(const FieldProgrammeData &, uint32_t,
                     const FieldProgrammeContext &, const DialogueAction &,
                     std::string &)>
      apply;
  // Instantiate the genuine dialogue node and enqueue its source tree add;
  // return the actual new ObjectID. Never synthesize a Ready callback here.
  std::function<bool(const FieldProgrammeData &, uint32_t,
                     const FieldProgrammeContext &, uint32_t, uint64_t &,
                     std::string &)>
      open_dialogue;
  std::function<bool(uint64_t, uint32_t, std::string &)> admit_dialogue_ready;
  BasementProgressionHost key_effects;
};
// Complete programme admission precedes mark_seen/start. One original Room
// scheduler owns text/choice suspension and source timing; no field VM/clock.
class FieldProgrammeRuntime final : private DialogueSink {
public:
  bool initialize(const FieldProgrammeData *, const BasementProgressionData *,
                  FieldProgrammeHost, std::string &);
  bool select_npc(uint32_t, bool, uint32_t &, std::string &seen_key,
                  std::string &);
  bool admit_npc(uint32_t, bool, std::string &);
  bool start_npc(uint32_t, bool, uint32_t generation, std::string &);
  bool start_selected_npc(uint32_t, std::string_view, bool, uint32_t generation,
                          std::string &);
  bool dialogue_ready(uint64_t dialogue_object, uint32_t generation,
                      std::string &);
  bool finish_dialogue(bool automatic = false);
  bool select_option(uint32_t selection, bool cancel, uint32_t generation);
  bool idle_begin();
  bool idle_process(double);
  void cancel() {
    scheduler_.cancel();
    awaiting_ready_ = false;
  }
  const DialoguePlayer &scheduler() const { return scheduler_; }
  bool source_ready_pending() const { return awaiting_ready_; }
  uint32_t program_index() const { return programme_; }
  const FieldProgrammeContext &context() const { return context_; }
  const std::string &error() const { return error_; }

private:
  bool admit_programme(uint32_t, const FieldProgrammeContext &, std::string &);
  bool apply(const DialogueAction &) override;
  const FieldProgrammeData *data_ = nullptr;
  const BasementProgressionData *basement_ = nullptr;
  FieldProgrammeHost host_;
  BasementProgressionConsumer key_effects_;
  DialoguePlayer scheduler_;
  FieldProgrammeContext context_;
  uint32_t programme_ = kRoomNoIndex, pending_choice_ = kRoomNoIndex,
           source_generation_ = 0;
  bool awaiting_ready_ = false;
  std::string error_;
};
} // namespace encore::upstream
