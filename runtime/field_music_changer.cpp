#include "encore/field_music_changer.hpp"
#include <cmath>
#include <utility>
namespace encore::upstream {
const FieldMusicChangerState*FieldMusicChangerRuntime::state(uint32_t id)const{for(const auto&s:states_)if(s.id==id)return&s;return nullptr;}
FieldMusicChangerState*FieldMusicChangerRuntime::active(uint32_t id,std::string&e){for(auto&s:states_)if(s.id==id){if(s.alive&&s.ready)return&s;e="MusicChanger source lifecycle inactive";return nullptr;}e="MusicChanger source identity unknown";return nullptr;}
bool FieldMusicChangerRuntime::initialize(const FieldMusicChangerData&d,uint64_t epoch,FieldMusicChangerHost h,std::string&e){
 if(!d.valid()||!epoch||!h.admit_service||!h.admit_ready||!h.set_shape_disabled||!h.context||!h.area_enter||!h.area_exit||!h.play_explicit||!h.stop_explicit||!h.tree_exit||!h.idle_frame){e="MusicChanger checked data/real service host incomplete";return false;}
 if(!h.admit_service(d,epoch,e))return false;
 std::vector<FieldMusicChangerState>s;for(const auto&b:d.bindings()){FieldMusicChangerState v;v.id=b.id;v.first_disabled=b.shapes.front().disabled;s.push_back(v);}data_=&d;epoch_=epoch;host_=std::move(h);states_=std::move(s);ready_index_=0;e.clear();return true;
}
bool FieldMusicChangerRuntime::ready(uint32_t id,std::string&e){
 if(!data_||ready_index_>=states_.size()||states_[ready_index_].id!=id||!states_[ready_index_].alive||states_[ready_index_].ready){e="MusicChanger source Ready identity/order";return false;}
 auto&s=states_[ready_index_];const auto&b=data_->bindings()[ready_index_];if(!host_.admit_ready(b,data_->connections(),e))return false;
 const auto*region=&data_->music().regions()[ready_index_];
 if(!host_.set_shape_disabled(b.shapes.front().id,region->disabled,e))return false;
 s.first_disabled=region->disabled;s.ready=true;++ready_index_;return true;
}
bool FieldMusicChangerRuntime::set_disabled(uint32_t id,bool value,std::string&e){
 auto*s=active(id,e);if(!s)return false;const auto*b=data_->binding(id);
 if(!host_.set_shape_disabled(b->shapes.front().id,value,e))return false;
 s->first_disabled=value;return true;
}
bool FieldMusicChangerRuntime::body_enter(uint32_t id,uint32_t body,std::string&e){
 if(!active(id,e))return false;
 if(!body){e="MusicChanger body identity absent";return false;}
 MusicRegionContext context;if(!host_.context(body,context,e)||!context.flag){if(!context.flag)e="MusicChanger source flag resolver absent";return false;}
 return host_.area_enter(epoch_,data_->binding(id)->node,context,e);
}
bool FieldMusicChangerRuntime::body_exit(uint32_t id,uint32_t body,std::string&e){
 if(!active(id,e))return false;
 if(!body){e="MusicChanger body identity absent";return false;}
 MusicRegionContext context;if(!host_.context(body,context,e))return false;
 return host_.area_exit(epoch_,data_->binding(id)->node,context,e);
}
bool FieldMusicChangerRuntime::play_music(uint32_t id,std::string&e){if(!active(id,e))return false;return host_.play_explicit(epoch_,data_->binding(id)->node,e);}
bool FieldMusicChangerRuntime::stop_music(uint32_t id,double duration,std::string&e){
 if(!active(id,e))return false;
 if(!std::isfinite(duration)||duration<0||duration>60){e="MusicChanger source stop fade duration rejected";return false;}
 return host_.stop_explicit(epoch_,data_->binding(id)->node,duration,e);
}
bool FieldMusicChangerRuntime::idle_frame(std::string&e){if(!data_||ready_index_!=states_.size()){e="MusicChanger idle boundary before source Ready";return false;}return host_.idle_frame(epoch_,e);}
bool FieldMusicChangerRuntime::tree_exiting(uint32_t id,std::string&e){auto*s=active(id,e);if(!s)return false;if(!host_.tree_exit(epoch_,data_->binding(id)->node,e))return false;s->alive=false;return true;}
}
