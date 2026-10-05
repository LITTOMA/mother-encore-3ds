#include "encore/field_sparkles.hpp"
#include "encore/field_present.hpp"
#include "encore/field_dropped.hpp"
#include <set>
namespace encore::upstream {
namespace {
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool frames(const FieldSparklesAnimation&a,const std::vector<PresentSparklesFrame>&b){if(a.frames.size()!=b.size())return false;for(size_t i=0;i<b.size();++i)if(a.frames[i].x!=b[i].x||a.frames[i].y!=b[i].y||a.frames[i].width!=b[i].width||a.frames[i].height!=b[i].height)return false;return true;}
}
bool validate_sparkles_owner_bridge(const FieldSparklesData&s,const FieldPresentData&p,const FieldDroppedData&d,std::string&e){
 auto fail=[&](const char*m){e=m;return false;};if(!s.valid()||!p.valid()||!d.valid()||s.identity().upstream_commit!=p.source_pin()||s.identity().upstream_commit!=d.source_pin()||s.source_scene()!=p.scene()||s.source_scene()!=d.scene())return fail("Sparkles owner scene/source identity rejected");
 std::set<uint32_t>present,dropped;for(const auto&n:s.records()){
  auto*a=s.animation(n.profile);if(!a)return fail("Sparkles owner animation absent");
  if(n.owner==FieldSparklesOwner::Present){auto*b=p.binding(n.parent_id);if(!b||b->sparkles_id!=n.id||b->sparkles_ready!=n.ready||n.ready>=b->ready_ordinal||b->node!=n.parent||n.node!=b->node+"/Sparkles"||!same(b->sparkles_position,n.world[2])||!present.insert(n.parent_id).second||p.sparkles_path()!=s.texture_path()||p.sparkles_width()!=s.width()||p.sparkles_height()!=s.height()||p.random_low()!=s.random_low()||p.random_high()!=s.random_high()||p.sparkle_speed()!=a->speed||p.sparkle_scale()!=n.speed_scale||!a->loop||!frames(*a,p.sparkle_frames())||!(n.flags&1)||!(n.flags&2))return fail("Sparkles Present single-owner/source binding rejected");}
  else if(n.owner==FieldSparklesOwner::Dropped){auto*b=d.binding(n.parent_id);if(!b||b->sparkles_id!=n.id||b->sparkles_ready!=n.ready||n.ready>=b->ready_ordinal||b->node!=n.parent||n.node!=b->node+"/Sparkles"||!same(b->sparkles_offset,n.position)||!dropped.insert(n.parent_id).second||d.sparkles_path()!=s.texture_path()||d.sparkles_width()!=s.width()||d.sparkles_height()!=s.height()||d.random_low()!=s.random_low()||d.random_high()!=s.random_high()||d.sparkles_speed()!=a->speed||d.sparkles_scale()!=n.speed_scale||!a->loop||!frames(*a,d.sparkles_frames())||!(n.flags&1)||!(n.flags&2))return fail("Sparkles Dropped single-owner/source binding rejected");}
 }
 if(present.size()!=p.bindings().size()||dropped.size()!=d.bindings().size()){return fail("Sparkles owner roster incomplete");}e.clear();return true;
}
bool FieldSparklesRuntime::snapshot(uint32_t id,FieldSparklesInstance&out){
 if(!data_||poisoned_||!created_.count(id)){return fail("Sparkles snapshot lifecycle rejected");}auto*n=data_->record(id);FieldSparklesInstance s;
 if(n->owner==FieldSparklesOwner::Present){auto*p=present_->state(n->parent_id);if(!p||!p->alive)return fail("Sparkles Present owner invalidated");s.id=id;s.profile=n->profile;s.frame=p->sparkle_frame;s.timeout=p->sparkle_timeout;s.speed_scale=n->speed_scale;s.ready=p->child_ready;s.visible=p->sparkle_visible;s.playing=p->sparkle_playing;}
 else if(n->owner==FieldSparklesOwner::Dropped){auto*p=dropped_->state(n->parent_id);if(!p||!p->alive)return fail("Sparkles Dropped owner invalidated");s.id=id;s.profile=n->profile;s.frame=p->sparkles_frame;s.timeout=p->sparkles_timeout;s.speed_scale=n->speed_scale;s.ready=p->child_ready;s.visible=bool(n->flags&2);s.playing=bool(n->flags&1);}
 else {auto*p=get(id);if(!p)return false;s=*p;}
 if(s.frame>=data_->animation(s.profile)->frames.size()){return fail("Sparkles owner frame rejected");}out=s;error_.clear();return true;
}
}
