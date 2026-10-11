#pragma once
#include <cstdint>

namespace encore {
// Platform presentation policy supplied by a checked binary resource.
struct LoadingFeedbackPolicy {
    uint32_t show_delay_ms=0,minimum_visible_ms=0,frame_interval_ms=0;
    bool valid()const {
        return show_delay_ms>0&&show_delay_ms<=10000&&minimum_visible_ms>0&&minimum_visible_ms<=2000&&
               frame_interval_ms>0&&frame_interval_ms<=1000&&frame_interval_ms<=minimum_visible_ms;
    }
};

// Wall-clock presentation gate. It owns no game/session clock, input or RNG.
// A coordinator shares one gate across nested work and records only frames
// actually submitted. Completing hidden work can never reveal the indicator.
class LoadingFeedback {
public:
    bool begin(uint64_t now,LoadingFeedbackPolicy policy){
        *this=LoadingFeedback{};if(!policy.valid())return false;
        policy_=policy;started_=now;active_=true;return true;
    }
    bool should_draw(uint64_t now)const{
        if(!active_||now<started_)return false;
        if(!shown_)return !finished_&&now-started_>=policy_.show_delay_ms;
        if(now<last_frame_||(finished_&&!remaining_visible_ms(now)))return false;
        return now-last_frame_>=policy_.frame_interval_ms;
    }
    bool presented(uint64_t now){
        if(!should_draw(now))return false;
        if(!shown_){first_frame_=now;shown_=true;}
        last_frame_=now;return true;
    }
    void finish(){finished_=true;}
    uint64_t remaining_visible_ms(uint64_t now)const{
        if(!finished_||!shown_||now<first_frame_)return 0;
        const auto elapsed=now-first_frame_;
        return elapsed<policy_.minimum_visible_ms?policy_.minimum_visible_ms-elapsed:0;
    }
    double animation_seconds(uint64_t now)const{
        return shown_&&now>=first_frame_?double(now-first_frame_)/1000.0:0.0;
    }
    bool shown()const{return shown_;}
    const LoadingFeedbackPolicy& policy()const{return policy_;}
private:
    LoadingFeedbackPolicy policy_{};
    uint64_t started_=0,first_frame_=0,last_frame_=0;
    bool active_=false,shown_=false,finished_=false;
};
}
