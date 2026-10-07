#pragma once
#include "encore/field_door.hpp"
#include "encore/house_data.hpp"
#include "encore/introduction.hpp"
namespace encore::ctr {
// Continue the already reviewed House UI Fade profile. Door colors/speeds
// remain in its checked source pack; no separate world or animation clock.
class PodunkHouseDoorFade {
public:
  bool initialize(upstream::HouseView, const upstream::FieldDoorData &, const upstream::IntroductionData&, std::string &);
  bool start(uint32_t door, bool in, std::string_view animation,
             const std::array<float,4> &, float speed, std::string &);
  bool frame(uint64_t epoch, float delta, bool &in_done, bool &out_mostly,
             std::string &);
  void focus(upstream::Vec2 actual_screen_position){focus_=actual_screen_position;}
  bool cut(std::string &);
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
};
}
