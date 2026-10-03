#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace encore::upstream {
// Native 3DS usability policy, deliberately separate from source movement rules.
// These values have no compiled defaults: load the checked ENCINP01 resource.
struct NativeInputTuning {
    float circle_nominal_radius=0, circle_activate=0, circle_release=0;
    float angular_hysteresis_degrees=0;
    float touch_activate=0, touch_release=0, tap_max_travel=0, tap_max_seconds=0;
    float base_radius=0, thumb_radius=0;
    uint32_t screen_width=0, screen_height=0, base_rgba=0, thumb_rgba=0;
};
class NativeInputData {
public:
    bool load(const uint8_t* bytes,size_t size,std::string& error);
    bool load_file(const char* path,std::string& error);
    bool valid() const { return valid_; }
    const NativeInputTuning& tuning() const { return tuning_; }
private:
    NativeInputTuning tuning_{};
    bool valid_=false;
};
struct NativeInputDirection { int x=0,y=0; };
struct NativeDpad { bool left=false,right=false,up=false,down=false; };
enum class NativeInputSource : uint8_t { None,Dpad,Touch,Pad };
struct NativeTouchView {
    bool active=false,dragged=false;
    float origin_x=0,origin_y=0,thumb_x=0,thumb_y=0;
};
struct NativeInputResult {
    NativeInputDirection direction{},touch_direction{};
    NativeInputSource source=NativeInputSource::None;
    bool confirm_pulse=false;
    NativeTouchView gesture{};
};
// Call ONCE per physical HID poll, outside fixed-step catch-up. Circle Pad input
// is signed raw HID counts (+Y up); output/touch coordinates are +Y down. D-pad
// means actual D-pad buttons, never KEY_CPAD bits. Do not normalize diagonals.
// Context 0 disables input. First sample, context changes, and reset quarantine
// already held devices until each reaches neutral/lift. A context must change
// for a new input owner, pause, reset, or blocking transition.
class NativeInputAdapter {
public:
    bool configure(const NativeInputData& data,std::string& error);
    void reset();
    NativeInputResult sample(uint32_t context,int16_t circle_x,int16_t circle_y,
        NativeDpad dpad,bool touch_down,int touch_x,int touch_y,double real_delta);
private:
    NativeInputTuning tuning_{};
    uint32_t context_=0;
    bool configured_=false,context_seen_=false;
    bool pad_blocked_=false,dpad_blocked_=false,touch_blocked_=false;
    bool touch_active_=false,touch_dragged_=false,touch_timed_out_=false;
    int pad_sector_=-1,touch_sector_=-1;
    int origin_x_=0,origin_y_=0;
    double touch_seconds_=0,touch_max_distance_=0;
};
}
