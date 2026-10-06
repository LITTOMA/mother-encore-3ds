#include "../platform/ctr/podunk_player_playback.hpp"
#include <stdexcept>
namespace encore::manual {
// Explicit manual harness supplies the genuinely constructed Registry and
// checked source closure. This function is never a default test target.
void player_playback_resource(
    upstream::FieldGlobalRegistry &r,
    std::shared_ptr<const upstream::PlayerInitializationData> player,
    std::shared_ptr<const upstream::PlayerReadyData> ready) {
  auto require=[](bool ok){if(!ok)throw std::runtime_error("manual Playback resource invariant");};
  std::string e;
  upstream::FieldObjectId first=0,second=0;
  ctr::PodunkPlayerPlayback *a=nullptr,*b=nullptr;
  require(ctr::PodunkPlayerPlayback::construct(player,ready,r,first,a,e));
  require(ctr::PodunkPlayerPlayback::construct(player,ready,r,second,b,e));
  require(first!=second&&a!=b&& &a->graph()!=&b->graph());
  require(r.source_resource(first)==a&&r.source_resource(second)==b);
  require(!a->tracks_bound()&&!b->tracks_bound());
  require(!a->bind_tracks({},e)&&!a->tracks_bound());
  require(!a->graph().set_active(true,e));
  require(!a->persist_append(r.root(),e));
  require(!a->assign_stable_canvas(r.root(),e));
  upstream::FieldDeferredMessage call;call.object=first;
  require(!a->deferred(call,e));
  auto unchanged=second;auto pointer=b;
  require(!ctr::PodunkPlayerPlayback::construct({},ready,r,unchanged,pointer,e));
  require(unchanged==second&&pointer==b);
  upstream::FieldGlobalExternalState state;
  require(a->state(state,e)&&!state.inside&&!state.ready&&!state.parent&&state.children.empty());
  require(r.retire_object(first,e)&&!r.object_exists(first));
  require(r.retire_object(second,e)&&!r.object_exists(second));
}
} // namespace encore::manual
