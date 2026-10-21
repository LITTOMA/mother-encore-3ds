#include "encore/geyser_contacts.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1e6f&&std::abs(p.y)<=1e6f;}
bool sample(const PoisonBodySample&b){return finite(b.position)&&b.layer<=0xfffff&&b.mask<=0xfffff;}
template<class T>bool convex(const T&p){double area=0;bool positive=false,negative=false;for(size_t i=0;i<p.size();++i){const auto a=p[i],b=p[(i+1)%p.size()],c=p[(i+2)%p.size()];if(!finite(a))return false;for(size_t j=0;j<i;++j)if(a.x==p[j].x&&a.y==p[j].y)return false;area+=double(a.x)*b.y-double(a.y)*b.x;const double turn=(double(b.x)-a.x)*(double(c.y)-b.y)-(double(b.y)-a.y)*(double(c.x)-b.x);positive|=turn>0;negative|=turn<0;}return area!=0&&!(positive&&negative);}
bool overlaps(const std::vector<Vec2>&a,const std::array<Vec2,4>&b){
 auto separated=[&](const auto&edges){for(size_t i=0;i<edges.size();++i){const auto first=edges[i],last=edges[(i+1)%edges.size()];const double x=double(last.y)-first.y,y=double(first.x)-last.x;double amin=0,amax=0,bmin=0,bmax=0;for(size_t j=0;j<a.size();++j){const double v=double(a[j].x)*x+double(a[j].y)*y;if(!j)amin=amax=v;else{amin=std::min(amin,v);amax=std::max(amax,v);}}for(size_t j=0;j<b.size();++j){const double v=double(b[j].x)*x+double(b[j].y)*y;if(!j)bmin=bmax=v;else{bmin=std::min(bmin,v);bmax=std::max(bmax,v);}}if(amax<bmin||bmax<amin)return true;}return false;};return !separated(a)&&!separated(b);
}
}
bool GeyserPlayerContacts::prepare(const PoisonContactsData&data,const GeyserLifecycle&life,uint64_t epoch,uint64_t ticket,uint64_t body,FieldStepBody kind,std::string&e){
 if(valid()||!data.valid()||!life.valid()||!epoch||epoch!=life.epoch()||epoch<=last_epoch_||!ticket||!body||(kind!=FieldStepBody::Player&&kind!=FieldStepBody::Other))return bad(e,"Geyser contacts fresh checked owner/body rejected");
 const auto&a=data.poison().policy();const auto&b=life.content().poison().policy();if(a.scene!=b.scene||a.source!=b.source)return bad(e,"Geyser contacts body/factory source differs");GeyserInstanceSnapshot pose;if(!life.inspect(epoch,ticket,pose,e)||!pose.pose)return bad(e,"Geyser contacts Ready factory instance required");
 GeyserPlayerContacts next;next.data_=data;next.family_=life.content();next.registry_=&life;next.epoch_=next.last_epoch_=epoch;next.ticket_=ticket;next.body_=body;next.kind_=kind;*this=std::move(next);e.clear();return true;
}
bool GeyserPlayerContacts::owner(const GeyserLifecycle&life,uint64_t epoch,GeyserInstanceSnapshot&out,std::string&e)const{
 if(!valid()||registry_!=&life||epoch!=epoch_||!life.valid()||life.epoch()!=epoch||!family_.same_owner(life.content())||!life.inspect(epoch,ticket_,out,e)||!out.pose)return bad(e,"Geyser contacts stale registry/instance/Ready owner rejected");e.clear();return true;
}
bool GeyserPlayerContacts::borrowed(const GeyserLifecycle&life,uint64_t epoch,WorldDamageActor&actor,const SessionCharacter&member,FieldRunLoop&run,const std::vector<WorldDamageEvent>&events,GeyserInstanceSnapshot&snapshot,std::string&e)const{
 if(!owner(life,epoch,snapshot,e)||!events.empty()||!actor.validates(epoch,body_,member,e)||!family_.poison().matches(run.content(),e))return bad(e,"Geyser contacts borrowed owner/fresh batch rejected");return true;
}
bool GeyserPlayerContacts::enabled(const GeyserInstanceSnapshot&s,const PoisonBodySample&body)const{
 const auto&p=family_.geyser().policy();return body.enabled&&s.pose->monitoring&&!s.pose->disabled&&((p.layer&body.mask)||(p.mask&body.layer));
}
bool GeyserPlayerContacts::shape(const GeyserLifecycle&life,uint64_t epoch,std::array<Vec2,4>&out,std::string&e)const{
 GeyserInstanceSnapshot snapshot;if(!owner(life,epoch,snapshot,e))return false;const auto&p=family_.geyser().policy();const auto c=std::cos(p.rotation),s=std::sin(p.rotation);const Vec2 origin{snapshot.source.position.x+p.shape.x,snapshot.source.position.y+p.shape.y};std::array<Vec2,4>next{{{-p.extents.x,-p.extents.y},{p.extents.x,-p.extents.y},{p.extents.x,p.extents.y},{-p.extents.x,p.extents.y}}};
 for(auto&v:next){const float x=c*v.x-s*v.y,y=s*v.x+c*v.y;v={x+origin.x,y+origin.y};if(!finite(v))return bad(e,"Geyser contacts transformed source rectangle domain rejected");}if(!convex(next))return bad(e,"Geyser contacts transformed source rectangle degenerates");out=next;e.clear();return true;
}
bool GeyserPlayerContacts::begin(const GeyserLifecycle&life,uint64_t epoch,const PoisonBodySample&body,bool paused,WorldDamageActor&actor,SessionCharacter&member,FieldRunLoop&run,FieldStepShadowState*shadow,std::vector<WorldDamageEvent>&out,std::string&e){
 GeyserInstanceSnapshot snapshot;if(!sample(body)||sampling_||!borrowed(life,epoch,actor,member,run,out,snapshot,e))return bad(e,"Geyser contacts physics begin rejected");auto next=*this;const bool current=pending_&&enabled(snapshot,body);next.events_.clear();std::vector<WorldDamageEvent>events;
 if(current!=inside_){next.events_.push_back({epoch,ticket_,body_,current});if(!deliver_poison_player_damage(family_.poison(),PoisonContactKind::Geyser,0,current,kind_,paused,epoch,body_,actor,member,run,shadow,events,e))return false;}
 next.inside_=current;next.sampling_=true;*this=std::move(next);out=std::move(events);e.clear();return true;
}
bool GeyserPlayerContacts::end(const GeyserLifecycle&life,uint64_t epoch,const PoisonBodySample&body,std::string&e){
 GeyserInstanceSnapshot snapshot;if(!sample(body)||!sampling_||!owner(life,epoch,snapshot,e))return bad(e,"Geyser contacts physics end rejected");std::array<Vec2,4>rectangle;if(!shape(life,epoch,rectangle,e))return false;std::vector<Vec2>polygon;for(auto p:data_.body()){Vec2 v{p.x+body.position.x,p.y+body.position.y};if(!finite(v))return bad(e,"Geyser contacts translated body domain rejected");polygon.push_back(v);}if(!convex(polygon))return bad(e,"Geyser contacts translated body degenerates");
 pending_=enabled(snapshot,body)&&overlaps(polygon,rectangle);sampling_=false;e.clear();return true;
}
bool GeyserPlayerContacts::retire(const GeyserLifecycle&life,uint64_t epoch,bool paused,WorldDamageActor&actor,SessionCharacter&member,FieldRunLoop&run,FieldStepShadowState*shadow,std::vector<WorldDamageEvent>&out,std::string&e){
 GeyserInstanceSnapshot snapshot;if(sampling_||pending_!=inside_||!borrowed(life,epoch,actor,member,run,out,snapshot,e))return bad(e,"Geyser contacts unfinished or stale removal boundary rejected");if(snapshot.pose->pending_rumbles||std::any_of(life.commands().begin(),life.commands().end(),[&](const auto&v){return v.instance==ticket_;}))return bad(e,"Geyser contacts removal requires callback/command delivery first");auto next=*this;next.events_.clear();std::vector<WorldDamageEvent>events;
 if(inside_){next.events_.push_back({epoch,ticket_,body_,false});if(!deliver_poison_player_damage(family_.poison(),PoisonContactKind::Geyser,0,false,kind_,paused,epoch,body_,actor,member,run,shadow,events,e))return false;}
 next.data_={};next.family_={};next.registry_=nullptr;next.epoch_=next.ticket_=next.body_=0;next.sampling_=next.inside_=next.pending_=false;*this=std::move(next);out=std::move(events);e.clear();return true;
}
}
