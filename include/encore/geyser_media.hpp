#pragma once
#include "encore/geyser_timeline.hpp"
#include "encore/audio_data.hpp"
namespace encore::upstream {
// ENCGMD01 f1/cap1 owns nested timeline/bank and exact source-digest joins.
class GeyserMediaData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}uint32_t sample_rate()const;uint32_t capacity()const;
 const GeyserTimelineData&timeline()const;const AudioBank&bank()const;
private:struct State;std::shared_ptr<const State>state_;
};
// Audited AudioStreamPlayer2D translation-only listener profile. Unknown
// canvas scale/skew/multiple viewports must be rejected by the scene owner.
struct GeyserListener {Vec2 center{};float width=0,global_panning=0;};
struct GeyserStereoGain {float left=0,right=0;};
bool geyser_spatial_gain(const GeyserPolicy&,Vec2 source,GeyserListener,float db,
 GeyserStereoGain&,std::string&);
}
