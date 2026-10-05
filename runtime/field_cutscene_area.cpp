#include "encore/field_cutscene_area.hpp"
#include <utility>
namespace encore::upstream {
const FieldCutsceneAreaState*FieldCutsceneAreaRuntime::state(uint32_t id)const{for(const auto&s:states_)if(s.id==id)return&s;return nullptr;}
FieldCutsceneAreaState*FieldCutsceneAreaRuntime::active(uint32_t id,std::string&e){for(auto&s:states_)if(s.id==id){if(s.alive&&s.ready)return&s;e="CutsceneArea source lifecycle inactive";return nullptr;}e="CutsceneArea source identity unknown";return nullptr;}
bool FieldCutsceneAreaRuntime::initialize(const FieldCutsceneAreaData&d,FieldCutsceneAreaHost h,std::string&e){
 if(!d.valid()||!h.admit_ready||!h.connect_battle_to_overworld||!h.body_is_current_player||!h.query_ui||!h.read_flag||!h.admit_programme||!h.close_commands||!h.pause_player||!h.open_room_and_unpause){e="CutsceneArea checked data/typed host incomplete";return false;}
 std::vector<FieldCutsceneAreaState>s;for(const auto&b:d.bindings()){FieldCutsceneAreaState v;v.id=b.id;s.push_back(v);}data_=&d;host_=std::move(h);states_=std::move(s);ready_index_=0;e.clear();return true;
}
bool FieldCutsceneAreaRuntime::ready(uint32_t id,std::string&e){
 if(!data_||ready_index_>=states_.size()||states_[ready_index_].id!=id||!states_[ready_index_].alive||states_[ready_index_].ready){e="CutsceneArea source Ready identity/order";return false;}
 auto&s=states_[ready_index_];const auto&b=data_->bindings()[ready_index_];
 if(!host_.admit_ready(b,data_->policy(),e))return false;
 s.processing=false;
 if(!host_.connect_battle_to_overworld(id,data_->policy().battle_signal,e))return false;
 s.ready=true;++ready_index_;return true;
}
bool FieldCutsceneAreaRuntime::check_flags(const FieldCutsceneAreaBinding&b,bool&on,std::string&e){
 on=true;if(!b.appear.empty()&&!host_.read_flag(b.appear,on,e))return false;
 if(on&&!b.disappear.empty()){bool off=false;if(!host_.read_flag(b.disappear,off,e))return false;on=!off;}return true;
}
bool FieldCutsceneAreaRuntime::check_start(uint32_t id,std::string&e){
 auto*s=active(id,e);if(!s)return false;const auto*b=data_->binding(id);
 if(!b->dialog.empty()){bool on=false;if(!check_flags(*b,on,e))return false;s->processing=true;}
 return true;
}
bool FieldCutsceneAreaRuntime::body_enter(uint32_t id,uint32_t body,std::string&e){
 if(!active(id,e))return false;
 bool player=false;if(!body||!host_.body_is_current_player(body,player,e)){if(!body)e="CutsceneArea body identity absent";return false;}return!player||check_start(id,e);
}
bool FieldCutsceneAreaRuntime::body_exit(uint32_t id,uint32_t body,std::string&e){
 auto*s=active(id,e);if(!s)return false;
 bool player=false;if(!body||!host_.body_is_current_player(body,player,e)){if(!body)e="CutsceneArea body identity absent";return false;}
 if(player){FieldCutsceneAreaUi ui;if(!host_.query_ui(ui,e))return false;if(!ui.battle)s->processing=false;}return true;
}
bool FieldCutsceneAreaRuntime::idle_process(uint32_t id,bool tree_can_process,std::string&e){
 auto*s=active(id,e);if(!s)return false;if(!tree_can_process||!s->processing)return true;
 FieldCutsceneAreaUi ui;if(!host_.query_ui(ui,e))return false;if(ui.cutscene||ui.battle||ui.pause)return true;
 const auto*b=data_->binding(id);bool on=false;if(!check_flags(*b,on,e))return false;
 if(on){
  if(!host_.admit_programme(*b,e))return false;
  if(!host_.close_commands(data_->policy().close,e))return false;
  if(!host_.pause_player(data_->policy().pause,e))return false;
  if(!host_.open_room_and_unpause(*b,data_->policy().completion,e))return false;
 }
 // Source call can synchronously emit signals; this trailing source assignment
 // still wins exactly after the dialogue coroutine returns its function state.
 s->processing=false;return true;
}
bool FieldCutsceneAreaRuntime::battle_to_overworld(uint32_t id,std::string&e){auto*s=active(id,e);if(!s)return false;s->processing=false;return true;}
bool FieldCutsceneAreaRuntime::exit_tree(uint32_t id,std::string&e){for(auto&s:states_)if(s.id==id){if(!s.alive){e="CutsceneArea already exited";return false;}s.alive=false;s.processing=false;return true;}e="CutsceneArea exit identity unknown";return false;}
}
