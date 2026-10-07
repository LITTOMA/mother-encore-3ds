#pragma once
#include "encore/field_door.hpp"
#include "encore/house_data.hpp"
#include "encore/introduction.hpp"
#include "encore/house_ui_continuation.hpp"
#include <functional>
namespace encore::ctr {
// Continue the already reviewed House UI Fade profile. Door colors/speeds
// remain in its checked source pack; no separate world or animation clock.
class PodunkHouseDoorFade {
public:
  bool initialize(upstream::HouseView, const upstream::FieldDoorData &, const upstream::IntroductionData&, std::string &);
  bool start(uint32_t door, bool in, std::string_view animation,
             const std::array<float,4> &, float speed, std::string &);
  bool start_source(const upstream::FieldDoorData &, uint32_t door, bool in,
                    std::string_view animation, const std::array<float,4> &,
                    float speed, std::string &);
  bool frame(uint64_t epoch, float delta, bool &in_done, bool &out_mostly,
             std::string &);
  void focus(upstream::Vec2 actual_screen_position){focus_=actual_screen_position;focus_source_=false;}
  bool cut(std::string &);
  bool bind_dialogue_restore(const upstream::HouseUiContinuationData &, std::string &);
  bool restore_cut(std::function<bool(std::string &)>, std::string &);
  bool restore_idle(uint64_t epoch, float delta, std::string &);
  bool stop_dialogue_spin(std::string &);
  bool telepathy_cut(upstream::Vec2 focused_screen_position, std::string &);
  bool restore_pending() const { return restoring_; }
  bool draw(float x, float y, float width, float height, std::string &) const;
  bool playing() const { return active_; }
private:
  const upstream::IntroductionData*intro_=nullptr;
  upstream::Vec2 focus_{};
  uint32_t kind_=0;
  bool drawn_=false;
  upstream::HouseView source_{};
  const upstream::FieldDoorData *doors_ = nullptr;
  upstream::HouseDoor profile_{};
  std::array<float,4> color_{};
  double position_ = 0;
  float speed_ = 0, cut_ = 0;
  uint64_t epoch_ = 0;
  bool active_ = false, in_ = false, emitted_ = false, initialized_ = false;
  const upstream::HouseUiContinuationData *restore_source_ = nullptr;
  std::function<bool(std::string &)> restore_done_;
  float restore_from_ = 0;
  float restore_elapsed_ = 0;
  uint64_t restore_epoch_ = 0;
  bool restoring_ = false, restore_started_ = false;
  double restored_path_unit_offset_ = 0;
  bool restored_path_ = false;
  upstream::Vec2 path_position_{};
  double spin_position_ = 0;
  bool spinning_ = false, positive_cut_ = false, focus_source_ = false;
  upstream::Vec2 curve_position(float) const;
};
}
