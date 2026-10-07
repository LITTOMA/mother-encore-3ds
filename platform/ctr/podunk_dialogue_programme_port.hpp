#pragma once
#include "encore/field_node_tree.hpp"
#include "encore/dialogue.hpp"

namespace encore::upstream {
class HousePresentation;
class DialogueChoices;
}
namespace encore::ctr {
struct PodunkDialogueProgrammeContext {
  upstream::FieldObjectId dialogue_object = 0, actor_object = 0;
  uint32_t generation = 0;
  bool thoughts = false;
};
struct PodunkDialogueProgrammeBinding {
  upstream::FieldIdentity identity{};
  const void *vm_owner = nullptr;
  const upstream::HousePresentation *printer = nullptr;
  const upstream::DialogueChoices *choices = nullptr;
};
// The actual source programme owner supplies its one existing VM and printer.
// This port owns no commands, status, cursor, Ready waiter or independent clock.
class PodunkDialogueProgrammePort {
public:
  virtual ~PodunkDialogueProgrammePort() = default;
  virtual bool programme_binding(PodunkDialogueProgrammeBinding &, std::string &) const = 0;
  virtual bool dialogue_context(PodunkDialogueProgrammeContext &, std::string &) const = 0;
  virtual bool dialogue_status(upstream::DialogueStatus &, std::string &) const = 0;
  virtual bool ready_waiting(bool &, std::string &) const = 0;
  virtual bool source_hash(std::string_view, std::array<uint8_t, 32> &) const = 0;
  virtual bool text_completed(std::string &) = 0;
  virtual bool source_cursor_input(uint64_t dialogue, uint32_t generation,
                                   int32_t index, bool confirm, bool cancel,
                                   std::string &) = 0;
  virtual bool advance(bool confirm, bool cancel, std::string &) = 0;
};
} // namespace encore::ctr
