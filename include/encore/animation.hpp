#pragma once
#include <cstdint>
namespace encore::upstream {
struct FrameKey { float time; uint16_t frame; };
struct FrameClip {
    float length; bool loop; uint8_t count; FrameKey keys[16];
    // -1 retains the incoming property; 0 hides; 1 shows.
    int8_t main_visibility=-1, special_visibility=-1;
};
struct AnimationSample { uint16_t frame; bool main_visible, special_visible; };
struct AnimationPlayback { float position; AnimationSample sample; };
// Discrete Sprite.frame tracks only. This does not evaluate AnimationTree
// transitions, blend spaces, method tracks or arbitrary Godot properties.
bool begin_clip(const FrameClip& clip,uint16_t frame_count,AnimationPlayback& playback);
bool advance_clip(const FrameClip& clip,float delta,uint16_t frame_count,AnimationPlayback& playback);
// Start at zero and advance once, as in the native probe. Repeated playback
// must use advance_clip: discrete events depend on the previous position.
bool sample_frame(const FrameClip& clip,float elapsed,uint16_t frame_count,uint16_t& frame);
bool sample_clip(const FrameClip& clip,float elapsed,uint16_t frame_count,AnimationSample& sample);
}
