#pragma once
#include <citro2d.h>
#include "encore/native_input.hpp"

namespace encore::ctr {
// Call on the LOWER target after existing debug text. No persistent decoration,
// text replacement, upper-screen changes, or clamping of the logical origin.
inline void draw_native_input(const upstream::NativeTouchView& view,const upstream::NativeInputTuning& tuning){
    if(!view.active)return;
    C2D_DrawCircleSolid(view.origin_x,view.origin_y,0.5f,tuning.base_radius,tuning.base_rgba);
    C2D_DrawCircleSolid(view.thumb_x,view.thumb_y,0.5f,tuning.thumb_radius,tuning.thumb_rgba);
}
}
