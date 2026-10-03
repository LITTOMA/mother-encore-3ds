#include "encore/animation.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
bool valid(const FrameClip& clip,uint16_t frame_count) {
    if(!frame_count||!std::isfinite(clip.length)||clip.length<=0||!clip.count||clip.count>16
       ||clip.keys[0].time!=0||clip.main_visibility < -1||clip.main_visibility>1
       ||clip.special_visibility < -1||clip.special_visibility>1)return false;
    for(unsigned i=0;i<clip.count;++i) {
        if(!std::isfinite(clip.keys[i].time)||clip.keys[i].time<0||clip.keys[i].frame>=frame_count
           ||(i&&clip.keys[i].time<=clip.keys[i-1].time))return false;
    }
    return true;
}
float modulo(float time,float length) {
    float result=std::fmod(time,length);
    return result<0?result+length:result;
}
// Godot Animation::_value_track_get_key_indices_in_range uses [from,to),
// with explicit equal-time and end-of-clip exceptions. Do not replace this
// with interpolation or last-key-at-or-before-time sampling.
bool in_range(float time,float from,float to,float length) {
    if(from!=length&&to==length)to=length*1.001f;
    if(from==to&&time==from)return true;
    return time>=from&&time<to;
}
void range(const FrameClip& clip,float from,float to,AnimationSample& sample) {
    for(unsigned i=0;i<clip.count;++i) {
        if(in_range(clip.keys[i].time,from,to,clip.length))sample.frame=clip.keys[i].frame;
    }
    if(in_range(0,from,to,clip.length)) {
        if(clip.main_visibility>=0)sample.main_visible=clip.main_visibility==1;
        if(clip.special_visibility>=0)sample.special_visible=clip.special_visibility==1;
    }
}
}
bool begin_clip(const FrameClip& clip,uint16_t frame_count,AnimationPlayback& playback) {
    if(!valid(clip,frame_count))return false;
    playback.position=0;playback.sample.frame=clip.keys[0].frame;
    if(clip.main_visibility>=0)playback.sample.main_visible=clip.main_visibility==1;
    if(clip.special_visibility>=0)playback.sample.special_visible=clip.special_visibility==1;
    return true;
}
bool advance_clip(const FrameClip& clip,float delta,uint16_t frame_count,AnimationPlayback& playback) {
    if(!valid(clip,frame_count)||!std::isfinite(delta)||delta<0||!std::isfinite(playback.position)
       ||playback.position<0||playback.position>clip.length||playback.sample.frame>=frame_count)return false;
    auto next=playback;
    float pos=playback.position+delta;
    if(!std::isfinite(pos))return false;
    if(clip.loop) {
        const float phase=modulo(pos,clip.length);
        pos=phase==0&&pos!=0?clip.length:phase;
    } else {
        pos=std::min(pos,clip.length);delta=pos-playback.position;
    }
    next.position=pos;
    if(delta==0) {
        unsigned index=0;
        for(unsigned i=1;i<clip.count&&clip.keys[i].time<=pos;++i)index=i;
        next.sample.frame=clip.keys[index].frame;
        if(clip.main_visibility>=0)next.sample.main_visible=clip.main_visibility==1;
        if(clip.special_visibility>=0)next.sample.special_visible=clip.special_visibility==1;
    } else {
        float from=pos-delta,to=pos;
        if(clip.loop) {
            from=modulo(from,clip.length);to=modulo(to,clip.length);
            if(from>to) {
                range(clip,from,clip.length,next.sample);
                range(clip,0,to,next.sample);
            } else range(clip,from,to,next.sample);
        } else range(clip,std::max(0.0f,from),to,next.sample);
    }
    playback=next;return true;
}
bool sample_clip(const FrameClip& clip,float elapsed,uint16_t frame_count,AnimationSample& sample) {
    if(!std::isfinite(elapsed)||elapsed<0)return false;
    AnimationPlayback playback{0,sample};
    if(!begin_clip(clip,frame_count,playback)||!advance_clip(clip,elapsed,frame_count,playback))return false;
    sample=playback.sample;return true;
}
bool sample_frame(const FrameClip& clip,float elapsed,uint16_t frame_count,uint16_t& frame) {
    AnimationSample sample{frame,false,false};
    if(!sample_clip(clip,elapsed,frame_count,sample))return false;
    frame=sample.frame;return true;
}
}
