#pragma once
#include "encore/field_inventory.hpp"
namespace encore::upstream {
struct FieldGoodsClip {
  struct Key {
    float time = 0, value = 0, transition = 0;
  };
  std::string name;
  float length = 0;
  std::vector<Key> keys;
  float value(double) const;
};
struct FieldGoodsTexture {
  std::string name, path;
  uint32_t bytes = 0, width = 0, height = 0;
  std::array<uint8_t, 32> sha{};
};
class FieldGoodsData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  bool bind_inventory(const FieldInventoryData &, std::string &) const;
  const auto &source_pin() const { return pin_; }
  const auto &identity() const { return identity_; }
  const auto &textures() const { return textures_; }
  bool source_hash(const std::string &path,
                   std::array<uint8_t, 32> &out) const {
    auto it = sources_.find(path);
    if (it == sources_.end())
      return false;
    out = it->second;
    return true;
  }
  const auto &stat_order() const { return order_; }
  const std::array<float, 4> *layout(const std::string &) const;
  const std::array<float, 4> *parameter(const std::string &) const;
  const FieldGoodsClip *clip(const std::string &) const;
  const FieldInventoryText *text(const std::string &) const;
  const std::string &label(uint32_t n) const { return labels_.at(n); }
  const std::string &stat_label(uint32_t n) const { return stat_labels_.at(n); }
  const std::string &sound(uint32_t n) const { return sounds_.at(n); }

private:
  bool valid_ = false;
  std::array<uint8_t, 20> pin_{};
  std::array<uint8_t, 32> identity_{}, inventory_identity_{};
  std::map<std::string, std::array<float, 4>> layouts_, parameters_;
  std::array<uint32_t, 7> order_{};
  std::array<std::string, 7> stat_labels_{};
  std::array<std::string, 12> labels_{};
  std::array<std::string, 5> sounds_{};
  std::vector<FieldInventoryText> texts_;
  std::vector<FieldGoodsClip> clips_;
  std::vector<FieldGoodsTexture> textures_;
  std::map<std::string, std::array<uint8_t, 32>> sources_;
};
enum class FieldGoodsPhase : uint32_t {
  Closed,
  Items,
  Actions,
  Targets,
  DropConfirm,
  SortType,
  ManualSort,
  Message,
  Closing
};
enum class FieldGoodsInput : uint32_t {
  Up = 1,
  Down,
  Left,
  Right,
  Accept,
  Cancel,
  Scope,
  OwnerNext,
  OwnerPrev
};
enum class FieldGoodsEventKind : uint32_t {
  Back = 1,
  UseItem,
  DescriptionChanged,
  InventoryChanged
};
struct FieldGoodsEvent {
  FieldGoodsEventKind kind{};
  FieldOwnedItem item{};
  uint32_t owner = 0;
  bool description = false;
};
struct FieldGoodsTextContext {
  std::string key, nickname, stat;
  FieldOwnedItem item{};
  uint32_t target = 0;
  int64_t value = 0;
};
struct FieldGoodsHost {
  // Source TextTools owns dynamic nickname/FavFood/value/stat substitution.
  std::function<bool(const FieldGoodsData &, const FieldInventoryData &,
                     std::string &)>
      bind;
  std::function<bool(const FieldGoodsTextContext &, std::string &,
                     std::string &)>
      format;
  std::function<bool(const std::string &, std::string &)> sound;
  std::function<bool(bool, std::string &)> description;
  std::function<bool(const std::string &source_action, std::string &label,
                     std::string &)>
      key_name;
};
class FieldGoodsMenu {
public:
  bool initialize(const FieldGoodsData &, FieldInventoryRuntime &,
                  FieldItemDefinitionsRuntime &, FieldGoodsHost, std::string &);
  bool open(const std::string &nickname, bool description, bool chinese,
            std::string &);
  bool input(FieldGoodsInput, std::string &);
  bool idle(double, std::string &);
  bool visible() const { return phase_ != FieldGoodsPhase::Closed; }
  FieldGoodsPhase phase() const { return phase_; }
  const FieldGoodsData *data() const { return data_; }
  const FieldInventoryRuntime *inventory() const { return inventory_; }
  uint32_t owner() const { return owner_; }
  uint32_t selection() const { return selected_; }
  uint32_t scroll_rows() const { return scroll_; }
  uint32_t submenu_selection() const { return sub_; }
  bool description_visible() const { return description_; }
  bool chinese() const { return chinese_; }
  const std::string &nickname() const { return nickname_; }
  const std::string &message() const { return message_; }
  const auto &actions() const { return actions_; }
  bool target_present() const { return target_present_; }
  bool target_all() const { return target_all_; }
  double cursor_time() const { return time_; }
  double bounce_offset() const;
  float open_offset() const;
  float message_offset() const;
  float stats_offset() const;
  bool message_visual() const;
  bool equipment_visual(bool &, std::array<int64_t, 7> &,
                        std::array<int64_t, 7> &, std::string &) const;
  bool source_label(const std::string &, std::string &, std::string &) const;
  bool row_name(const FieldGoodsRow &, std::string &, std::string &) const;
  bool key_label(const std::string &, std::string &, std::string &) const;
  uint32_t sort_source() const { return sort_source_; }
  uint32_t action_selection() const { return action_index_; }
  bool portrait(bool &suitable, bool &equipped, int &comparison,
                std::string &) const;
  bool equipment_preview(bool &visible, std::array<int64_t, 7> &current,
                         std::array<int64_t, 7> &projected,
                         std::string &) const;
  std::vector<FieldGoodsEvent> take_events();

private:
  const FieldGoodsData *data_ = nullptr;
  FieldInventoryRuntime *inventory_ = nullptr;
  FieldItemDefinitionsRuntime *definitions_ = nullptr;
  FieldGoodsHost host_;
  FieldGoodsPhase phase_ = FieldGoodsPhase::Closed;
  uint32_t owner_ = 0, selected_ = 0, scroll_ = 0, sub_ = 0, sort_source_ = 0,
           action_index_ = 0;
  bool description_ = false, description_preference_ = false, chinese_ = false,
       target_present_ = false, target_all_ = false;
  std::string nickname_, message_;
  std::vector<FieldGoodsAction> actions_;
  FieldOwnedItem selected_item_{};
  bool has_selected_ = false;
  double time_ = 0, phase_started_ = 0, open_started_ = 0, bounce_started_ = -1,
         message_started_ = 0, stats_started_ = 0, move_ready_ = 0;
  uint64_t idle_epoch_ = 0, opened_revision_ = 0;
  bool stats_visible_ = false, stats_ever_ = false, message_ever_ = false,
       message_closing_ = false;
  double message_close_started_ = 0;
  std::array<int64_t, 7> stats_current_{}, stats_projected_{};
  std::vector<FieldGoodsEvent> events_;
  struct Scheduled {
    FieldInventoryEvent event;
    double due = 0;
    uint64_t earliest_idle = 0;
  };
  std::vector<Scheduled> scheduled_;
  bool synchronize(bool, std::string &);
  bool select(std::string &);
  bool perform_consume(std::string &);
  bool apply_result(const FieldInventoryResult &, std::string &);
  bool close(bool, std::string &);
  bool play(uint32_t, std::string &);
  void set_phase(FieldGoodsPhase);
  bool selected_rows(std::vector<FieldGoodsRow> &, std::string &) const;
};
} // namespace encore::upstream
