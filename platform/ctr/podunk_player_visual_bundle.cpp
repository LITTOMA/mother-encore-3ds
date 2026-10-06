#include "podunk_player_visual_bundle.hpp"
namespace encore::ctr {
using namespace upstream;
namespace { bool fail(std::string &e, const char *s) { e=s;return false; } }
bool PodunkPlayerVisualBundle::load(
    std::shared_ptr<const PlayerInitializationData> p,
    std::shared_ptr<const PlayerVisualScriptsData> v,
    std::shared_ptr<const PlayerGraphicsData> g, const char *root, std::string &e) {
  if(player_||prepared_||!p||!v||!g||!p->valid()||!v->valid()||!g->valid()||
     v->player_ir_sha256()!=p->ir_sha256()||g->visual_ir_sha256()!=v->ir_sha256())
    return fail(e,"Player visual GPU/source closure rejected");
  if(!graphics_.load(*g,root,e)) return false;
  player_=std::move(p);visual_=std::move(v);graphics_data_=std::move(g);
  return true;
}
bool PodunkPlayerVisualBundle::prepare(FieldNodeTreeRuntime &t,
    FieldGlobalRegistry &r, FieldGlobalConstructorRuntime &global,
    PlayerInitializationBody &body, PlayerVisualFetcherOwner &fetcher,
    std::string &e) {
  if(!player_||prepared_||!graphics_.loaded()||!body.constructed()||body.tree()!=&t)
    return fail(e,"Player visual actual root constructor absent");
  if(!shadow_.prepare(*player_,*visual_,t,r,graphics_,e)||
     !bat_.prepare(*player_,*visual_,t,r,graphics_,e)||
     !scripts_.initialize(*visual_,*player_,t,r,global,body,shadow_,bat_,fetcher,e))
    return false;
  tree_=&t;registry_=&r;prepared_=true;return true;
}
bool PodunkPlayerVisualBundle::owns_source(uint32_t id) const {
  return visual_&&(id==visual_->shadow().id||id==visual_->bat().id);
}
bool PodunkPlayerVisualBundle::construct(FieldObjectId id,
    const FieldNodeDescriptor &d, std::string &e) {
  if(!prepared_||!owns_source(d.id)||!tree_->descriptor(id)||
     tree_->descriptor(id)->id!=d.id)
    return fail(e,"Player visual source construction cursor rejected");
  auto &actual=d.id==visual_->shadow().id?shadow_:bat_;
  if(!actual.construct(*player_,*visual_,*tree_,*registry_,id,graphics_,e)||
     !scripts_.construct(id,d,e))return false;
  // PackedScene setget override follows script-field construction.
  if(d.id==visual_->shadow().id)
    return scripts_.source_shadow_property(id,visual_->shadow().start_member,e);
  return true;
}
PodunkPlayerVisualNative *PodunkPlayerVisualBundle::native(FieldObjectId id) {
  if(id&&id==shadow_.object())return &shadow_;
  if(id&&id==bat_.object())return &bat_;
  return nullptr;
}
bool PodunkPlayerVisualBundle::phase(FieldObjectId id, FieldTreePhase p,
    float delta, bool paused, bool pending, std::string &e) {
  auto *owner=native(id);
  if(!prepared_||!owner)return fail(e,"Player visual phase actual owner absent");
  if(p==FieldTreePhase::ReadyScript||
     (p==FieldTreePhase::Idle&&owner==&bat_))
    return scripts_.script_phase(id,p,e);
  if(p==FieldTreePhase::IdleInternal&&owner==&shadow_)
    return owner->process_internal(delta,paused,pending,e);
  return fail(e,"Player visual phase requires enclosing native Tree owner");
}
} // namespace encore::ctr
