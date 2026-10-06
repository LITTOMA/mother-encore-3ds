#pragma once
#include "encore/field_item_definitions.hpp"
#include <array>
#include <functional>
#include <map>
namespace encore::upstream {
enum class FieldShopTextRole : uint32_t {
  Title,
  Buy,
  Sell,
  PromptBuy,
  PromptSell,
  PromptCash,
  Warning,
  Yes,
  No
};
struct FieldShopPolicy {
  uint32_t id = 0, doses = 0;
  int32_t cost = 0, value = 0;
  bool key = false;
  std::string source, name, name_key, description_key, article_key, slot;
  std::array<uint8_t, 32> source_sha{};
};
struct FieldShopText {
  std::string key, en, zh;
};
struct FieldShopTexture {
  std::string source, path;
  uint32_t width = 0, height = 0, bytes = 0;
  bool flavor = false;
  std::array<uint8_t, 32> sha{};
};
struct FieldShopPanel {
  std::string node;
  std::array<float, 4> rect{}, patch{};
  uint32_t texture = 0;
};
struct FieldShopPortrait {
  std::string character;
  uint32_t texture = 0;
};
class FieldShopData {
public:
  bool load(const uint8_t *, size_t, std::string &);
  bool load_file(const char *, std::string &);
  bool valid() const { return valid_; }
  const auto &source_pin() const { return pin_; }
  const auto &name() const { return name_; }
  const auto &offers() const { return offers_; }
  const auto &policies() const { return policies_; }
  const auto &textures() const { return textures_; }
  const auto &panels() const { return panels_; }
  const auto &layout() const { return layout_; }
  const auto &source_colors() const { return colors_; }
  double threshold() const { return threshold_; }
  const FieldShopPolicy *policy(uint32_t) const;
  const FieldShopText *text(const std::string &) const;
  const std::string &sound(uint32_t) const;
  uint32_t lines() const { return lines_; }
  uint32_t cash_digits() const { return digits_; }
  double warning_seconds() const { return warning_; }
  uint32_t texture_role(const std::string &) const;
  const auto &portraits() const { return portraits_; }
  const auto &fonts() const { return fonts_; }
  const auto &cursor_frames() const { return cursor_frames_; }
  float cursor_speed() const { return cursor_speed_; }
  const auto &cursor_size() const { return cursor_size_; }
  const auto &aux() const { return aux_; }
  const std::string &text_key(FieldShopTextRole r) const {
    return text_keys_[size_t(r)];
  }
  uint32_t portrait_limit() const { return portrait_limit_; }
  bool can_sell() const { return can_sell_; }
  bool buy_loop() const { return buy_loop_; }
  bool sell_loop() const { return sell_loop_; }

private:
  bool valid_ = false, can_sell_ = false, buy_loop_ = false, sell_loop_ = false;
  std::array<uint8_t, 20> pin_{};
  std::string name_;
  uint32_t lines_ = 0, digits_ = 0;
  double warning_ = 0, threshold_ = 0;
  std::vector<uint32_t> offers_;
  std::vector<FieldShopPolicy> policies_;
  std::vector<FieldShopText> texts_;
  std::vector<std::string> sounds_;
  std::vector<FieldShopTexture> textures_;
  std::vector<FieldShopPanel> panels_;
  std::vector<float> layout_;
  std::array<uint32_t, 8> colors_{};
  std::map<std::string, uint32_t> roles_;
  std::vector<FieldShopPortrait> portraits_;
  std::array<std::string, 4> fonts_;
  std::array<uint32_t, 2> cursor_size_{};
  std::vector<uint32_t> cursor_frames_;
  float cursor_speed_ = 0;
  std::array<std::array<float, 4>, 19> aux_{};
  uint32_t portrait_limit_ = 0;
  std::array<std::string, 9> text_keys_;
};
struct FieldShopSnapshot {
  FieldItemSnapshot items;
  int64_t cash = 0;
  std::vector<uint32_t> natural_order;
  uint64_t revision = 0;
};
enum class FieldShopPhase : uint32_t {
  Closed,
  BuySell,
  Buy,
  Sell,
  ConfirmBuy,
  ConfirmSell,
  Insufficient
};
enum class FieldShopAction : uint32_t { Ready, Preview, Purchase, Sale, Close };
struct FieldShopResult {
  FieldShopAction action{};
  FieldOwnedItem item{};
  // Source description holds its preview Item/global.item separately from the
  // newly purchased inventory Item. Their default constructors have different
  // UIDs; publishing ownership must not replace source contextual identity.
  FieldOwnedItem context_item{};
  uint32_t owner = 0;
  std::string last_purchased;
};
struct FieldShopHost {
  std::function<bool(const FieldShopData &, const FieldItemDefinitions &,
                     std::string &)>
      bind;
  std::function<bool(FieldShopSnapshot &, std::string &)> read;
  // Source pauses the dialogue/world while Shop is open. Commit checks the
  // exact before revision, updates cash/ownership/equipment/global.item
  // together. UID/RNG are published by this core only after successful commit.
  std::function<bool(const FieldShopSnapshot &, const FieldShopSnapshot &,
                     const FieldShopResult &, std::string &)>
      commit;
  std::function<bool(const std::string &, std::string &)> sound;
  std::function<bool(const std::string &, std::string &)> close;
  // Optional ObjectDB slice. Called once in source offer order before Ready
  // and RNG commit. A false return must discard its temporary References.
  // Pure-value consumers do not claim this actual Reference capability.
  std::function<bool(const std::vector<FieldOwnedItem> &, std::string &)>
      source_items{};
};
class FieldShopRuntime {
public:
  bool initialize(const FieldShopData &, const FieldItemDefinitions &,
                  SourceRandom &, std::vector<uint32_t> &, LoadRngClockProvider,
                  FieldShopHost, std::string &);
  bool open(const std::string &, std::string &);
  bool move(int, bool page, std::string &);
  bool character(int, std::string &);
  bool select(std::string &);
  bool cancel(std::string &);
  bool answer(bool, std::string &);
  bool idle(double, std::string &);
  bool refresh(std::string &);
  FieldShopPhase phase() const { return phase_; }
  uint32_t selected() const { return selected_; }
  uint32_t page() const { return page_; }
  uint32_t owner() const { return owner_; }
  uint32_t main_selected() const { return main_selected_; }
  bool yes_selected() const { return yes_; }
  bool warning() const { return warning_left_ > 0; }
  const auto &snapshot() const { return snapshot_; }
  const auto &rows() const { return rows_; }
  const FieldShopData *data() const { return data_; }
  const std::string &last_purchased() const { return last_; }
  const std::string &prompt_key() const;
  bool restricted(uint32_t) const;
  int64_t price(uint32_t) const;

private:
  const FieldShopData *data_ = nullptr;
  const FieldItemDefinitions *items_ = nullptr;
  SourceRandom *random_ = nullptr;
  std::vector<uint32_t> *ledger_ = nullptr;
  LoadRngClockProvider clock_;
  FieldShopHost host_;
  FieldShopSnapshot snapshot_;
  std::vector<FieldOwnedItem> previews_, rows_;
  FieldShopPhase phase_ = FieldShopPhase::Closed;
  uint32_t selected_ = 0, page_ = 0, owner_ = 0, main_selected_ = 0;
  bool yes_ = true;
  double warning_left_ = 0;
  std::string last_;
  FieldOwnedItem selected_item_{};
  uint64_t prompt_revision_ = 0;
  bool validate(const FieldShopSnapshot &, std::string &) const;
  bool enter(FieldShopPhase, bool, std::string &);
  bool publish_context(std::string &);
  bool audio(uint32_t, std::string &);
  FieldItemInventory *inventory(FieldShopSnapshot &) const;
  const FieldItemInventory *inventory(const FieldShopSnapshot &) const;
  bool purchase(std::string &);
  bool sell(std::string &);
};
} // namespace encore::upstream
