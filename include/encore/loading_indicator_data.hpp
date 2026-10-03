#pragma once
#include "encore/animation.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace encore::upstream {
struct LoadingIndicatorPlacement { int x=0, y=0; uint16_t frame=0; };

// Presentation resource only: no game session, input, clock or RNG ownership.
class LoadingIndicatorData {
public:
    bool load(const uint8_t*, size_t, std::string&);
    bool load_file(const char*, std::string&);
    bool valid() const { return valid_; }
    uint32_t background_color() const { return background_; }
    uint32_t texture_width() const { return texture_width_; }
    uint32_t texture_height() const { return texture_height_; }
    uint32_t frame_width() const { return frame_width_; }
    uint32_t frame_height() const { return frame_height_; }
    const std::string& texture_path() const { return texture_path_; }
    // Original source-sheet frame, or -1 for invalid elapsed time/content.
    int frame_at(double elapsed_seconds) const;
    bool sample(int width, int height, double elapsed_seconds, LoadingIndicatorPlacement&) const;
    // Platform presentation adaptation: reuse the external right margin on
    // both sides, with 0 at the left and 1 at sample()'s right-hand position.
    // Progress must be finite and in [0,1]. Round to the nearest pixel (ties
    // toward the right); reject viewports that cannot fit both margins.
    bool sample_progress(int width, int height, double elapsed_seconds, double fraction,
                         LoadingIndicatorPlacement&) const;
private:
    struct Viewport { uint32_t width, height; };
    bool valid_=false;
    uint32_t background_=0, texture_width_=0, texture_height_=0, frame_width_=0, frame_height_=0;
    uint32_t frame_count_=0, right_margin_=0, bottom_margin_=0;
    FrameClip clip_{};
    std::vector<uint16_t> source_frames_;
    std::string texture_path_;
    std::vector<Viewport> viewports_;
    bool sample_frame_index(double elapsed_seconds, uint16_t&) const;
};
}
