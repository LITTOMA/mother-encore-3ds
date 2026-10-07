#include "podunk_scene_visibility.hpp"
#include <algorithm>
#include <cmath>
#include <climits>
namespace encore::ctr {
using namespace upstream;
namespace {
bool same(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;}
uint64_t key(int32_t x,int32_t y){return uint64_t(uint32_t(x))|(uint64_t(uint32_t(y))<<32);}
bool equal(const FieldMapRect&a,const FieldMapRect&b){return a.minimum.x==b.minimum.x&&a.minimum.y==b.minimum.y&&a.maximum.x==b.maximum.x&&a.maximum.y==b.maximum.y;}
const char*screen_signal(uint32_t n){return n==1?"screen_entered":"screen_exited";}
}
bool PodunkSceneVisibility::fail(std::string&e,const char*m){failed_=true;e=m;return false;}
bool PodunkSceneVisibility::prepare(const FieldVisibilityData&data,const FieldNodeTreeData&source,FieldNodeTreeRuntime&tree,FieldGlobalRegistry&registry,FieldObjectSignals&signals,PodunkNativeRoot&viewport,PodunkSceneScripts&scripts,FieldSceneConsumers consumers,PodunkVisibilityNativeOwners native,std::string&e){
 if(data_||!data.valid()||!source.valid()||!same(data.identity(),source.identity())||signals.registry()!=&registry||(tree.object_domain()&&tree.object_domain()!=viewport.kernel_object())||(!tree.object_domain()&&tree.object_count())||viewport.kernel_object()!=registry.kernel()||!registry.object_exists(viewport.viewport_object())||!native.animation_active)return fail(e,"Scene visibility requires actual same-world Tree/Viewport/AnimationPlayer owners");
 data_=&data;source_=&source;tree_=&tree;registry_=&registry;signals_=&signals;viewport_=&viewport;scripts_=&scripts;consumers_=consumers;native_=std::move(native);return true;
}
bool PodunkSceneVisibility::owns(const FieldNodeDescriptor&d)const{return data_&&data_->record(d.id)&&(d.native_class=="VisibilityNotifier2D"||d.native_class=="VisibilityEnabler2D");}
bool PodunkSceneVisibility::owns(FieldObjectId id)const{return instances_.count(id)!=0;}
bool PodunkSceneVisibility::construct(FieldObjectId id,const FieldNodeDescriptor&d,const FieldIdentity&i,std::string&e){
 const auto*r=data_?data_->record(d.id):nullptr;const auto*actual=tree_?tree_->descriptor(id):nullptr;
 if(failed_||!r||!actual||actual->id!=d.id||instances_.count(id)||objects_.count(d.id)||!d.script.empty()||!same(i,data_->identity())||d.path!=r->path||d.parent!=r->parent||!owns(d)||!registry_->object_exists(id)||tree_->object_domain()!=registry_->kernel()||registry_->tree_owner(id).get()!=tree_)return fail(e,"Visibility constructor source/native object mismatch");
 Instance x;x.source=r;x.binding={i,d.id,d.class_index,0x454e0069,1,d.script_sha,d.native_class};instances_.emplace(id,std::move(x));objects_.emplace(d.id,id);return tree_->set_transform_notification(id,false,true,e);
}
bool PodunkSceneVisibility::actual(FieldObjectId id,Instance*&out,std::string&e){auto it=instances_.find(id);if(failed_||it==instances_.end()||!tree_->state(id)||!tree_->state(id)->alive||!registry_->object_exists(id))return fail(e,"Visibility live native owner rejected");out=&it->second;return true;}
bool PodunkSceneVisibility::bind(FieldObjectId id,FieldNodeBinding&b,std::string&e){Instance*x=nullptr;if(!actual(id,x,e))return false;b=x->binding;return true;}
bool PodunkSceneVisibility::finish_factory(std::string&e){
 if(failed_||!data_||finished_||instances_.size()!=data_->records().size())return fail(e,"Visibility full native factory not allocated");
 for(size_t i=0;i<data_->connections().size();++i){const auto&c=data_->connections()[i];auto from=objects_.find(c.emitter);auto to=tree_->source_object(c.target);const auto*d=tree_->descriptor(to);if(from==objects_.end()||!d||d->script!=c.script||d->script_sha!=c.script_sha||!registry_->object_exists(to)||!signals_->connect(from->second,screen_signal(c.signal),to,c.method,FieldSignalPersist,{},e))return fail(e,"Visibility actual source signal connection rejected");connections_.insert(i);auto old=methods_.find({to,c.method});if(old!=methods_.end()){const auto&previous=data_->connections()[old->second];if(previous.adapter!=c.adapter||previous.signal!=c.signal)return fail(e,"Visibility ambiguous source callback binding");}else methods_.emplace(std::make_pair(to,c.method),i);}
 finished_=true;return true;
}
bool PodunkSceneVisibility::range(const FieldMapRect&r,Range&o,std::string&e)const{
 auto convert=[this](float v,int32_t&out){if(!std::isfinite(v)||double(v)<double(INT32_MIN)||double(v)>double(INT32_MAX))return false;out=int32_t(v)/data_->cell_size();return true;};
 if(!data_||r.maximum.x<r.minimum.x||r.maximum.y<r.minimum.y||!convert(r.minimum.x,o.x0)||!convert(r.minimum.y,o.y0)||!convert(r.maximum.x,o.x1)||!convert(r.maximum.y,o.y1)){e="Visibility world rectangle overflow/negative extent";return false;}
 // Admission allocation limit, not a gameplay visibility threshold.
 auto w=int64_t(o.x1)-o.x0+1,h=int64_t(o.y1)-o.y0+1;if(w<=0||h<=0||w>10000000||h>10000000||w*h>10000000){e="Visibility spatial cell allocation exceeds source host capacity";return false;}return true;
}
bool PodunkSceneVisibility::world_rect(FieldObjectId id,FieldMapRect&out,std::string&e){Instance*x=nullptr;FieldTransform t{};if(!actual(id,x,e)||!tree_->world_transform(id,t,e))return false;const auto&r=x->source->rect;Vec2 points[]={{r.x,r.y},{r.x+r.width,r.y},{r.x,r.y+r.height},{r.x+r.width,r.y+r.height}};bool first=true;for(auto p:points){Vec2 q{t[0].x*p.x+t[1].x*p.y+t[2].x,t[0].y*p.x+t[1].y*p.y+t[2].y};if(!std::isfinite(q.x)||!std::isfinite(q.y))return fail(e,"Visibility world transform overflow");if(first){out={q,q};first=false;}else{out.minimum.x=std::min(out.minimum.x,q.x);out.minimum.y=std::min(out.minimum.y,q.y);out.maximum.x=std::max(out.maximum.x,q.x);out.maximum.y=std::max(out.maximum.y,q.y);}}return true;}
bool PodunkSceneVisibility::change_cells(FieldObjectId id,const Range&r,bool add,std::string&e){for(int64_t x=r.x0;x<=r.x1;++x)for(int64_t y=r.y0;y<=r.y1;++y){auto k=key(int32_t(x),int32_t(y));if(add){++cells_[k][id];}else{auto c=cells_.find(k);if(c==cells_.end()||!c->second.count(id))return fail(e,"Visibility spatial index removal missing");auto q=c->second.find(id);if(!--q->second)c->second.erase(q);if(c->second.empty())cells_.erase(c);}}changed_=true;return true;}
bool PodunkSceneVisibility::transform_changed(FieldObjectId id,std::string&e){Instance*x=nullptr;FieldMapRect r;Range next;if(!actual(id,x,e))return false;if(!x->inside)return true;if(!world_rect(id,r,e)||!range(r,next,e))return false;if(equal(r,x->world_rect))return true;if(!change_cells(id,next,true,e)||!change_cells(id,x->cells,false,e))return false;x->cells=next;x->world_rect=r;return true;}
bool PodunkSceneVisibility::change_tracked(const Instance&x,FieldObjectId id,uint32_t kind,bool enabled,std::string&e){
 if(kind==1){if(!(x.source->flags&1))return true;return native_.animation_active(id,enabled,e);}
 return fail(e,"Visibility enabler native tracked class not implemented");
}
bool PodunkSceneVisibility::enable(Instance&x,bool enabled,std::string&e){for(const auto&q:x.tracked)if(!change_tracked(x,q.first,q.second,enabled,e))return false;auto parent=tree_->source_object(x.source->parent);if(!parent)return fail(e,"Visibility enabler actual parent missing");if((x.source->flags&32)&&!tree_->set_process(parent,true,enabled,e))return false;if((x.source->flags&16)&&!tree_->set_process(parent,false,enabled,e))return false;x.enabler_visible=enabled;return true;}
bool PodunkSceneVisibility::enabler_enter(FieldObjectId id,Instance&x,std::string&e){
 for(const auto&q:x.source->tracked){auto object=tree_->source_object(q.id);const auto*d=tree_->descriptor(object);if(!object||!d||!registry_->object_exists(object)||q.kind!=1)return fail(e,"Visibility tracked actual AnimationPlayer unavailable");if(!signals_->connect(object,"tree_exiting",id,"_node_removed",FieldSignalOneShot,{FieldObjectRef{object}},e))return false;x.tracked.emplace(object,q.kind);if(!change_tracked(x,object,q.kind,false,e))return false;}
 auto parent=tree_->source_object(x.source->parent);if(!parent)return fail(e,"Visibility actual enabler parent missing");if((x.source->flags&32)&&!signals_->connect(parent,"ready",parent,"set_physics_process",FieldSignalReferenceCounted,{false},e))return false;if((x.source->flags&16)&&!signals_->connect(parent,"ready",parent,"set_process",FieldSignalReferenceCounted,{false},e))return false;if(x.source->flags&48)parents_.insert(parent);return true;
}
bool PodunkSceneVisibility::enabler_exit(FieldObjectId id,Instance&x,std::string&e){for(const auto&q:x.tracked){if(!x.enabler_visible&&!change_tracked(x,q.first,q.second,true,e))return false;bool connected=false;if(!signals_->connected(q.first,"tree_exiting",id,"_node_removed",connected,e))return false;if(connected&&!signals_->disconnect(q.first,"tree_exiting",id,"_node_removed",e))return false;}x.tracked.clear();return true;}
bool PodunkSceneVisibility::screen(FieldObjectId id,bool entered,std::string&e){Instance*x=nullptr;if(!actual(id,x,e)||x->on_screen==entered)return fail(e,"Visibility duplicate screen transition");x->on_screen=entered;if(entered){if(!signals_->emit(id,"screen_entered",{},e))return false;if(x->source->kind==2&&!enable(*x,true,e))return false;return signals_->emit(id,"viewport_entered",{FieldObjectRef{viewport_->viewport_object()}},e);}if(!signals_->emit(id,"viewport_exited",{FieldObjectRef{viewport_->viewport_object()}},e)||!signals_->emit(id,"screen_exited",{},e))return false;return x->source->kind!=2||enable(*x,false,e);}
bool PodunkSceneVisibility::phase(FieldObjectId id,FieldTreePhase phase,float,bool,bool,std::string&e){Instance*x=nullptr;if(!actual(id,x,e))return false;
 switch(phase){
 case FieldTreePhase::EnterNative:{if(!finished_||x->inside)return fail(e,"Visibility native Enter requires full factory once");FieldMapRect r;Range cells;if(!world_rect(id,r,e)||!range(r,cells,e)||!change_cells(id,cells,true,e))return false;x->cells=cells;x->world_rect=r;x->inside=true;if(x->source->kind==2&&!enabler_enter(id,*x,e))return false;return true;}
 case FieldTreePhase::TransformChanged:return transform_changed(id,e);
 case FieldTreePhase::ExitNative:{if(!x->inside)return fail(e,"Visibility native Exit without Enter");if(!change_cells(id,x->cells,false,e))return false;if(x->on_screen){visible_.erase(id);if(!screen(id,false,e))return false;}if(x->source->kind==2&&!enabler_exit(id,*x,e))return false;x->inside=false;return true;}
 case FieldTreePhase::ReadyNative:if(!x->inside)return fail(e,"Visibility Ready native not in world");return true;
 case FieldTreePhase::PostEnterNative:case FieldTreePhase::Parented:case FieldTreePhase::Unparented:case FieldTreePhase::ChildMoved:case FieldTreePhase::ChildEntered:case FieldTreePhase::ChildExiting:case FieldTreePhase::VisibilityChanged:case FieldTreePhase::Hide:case FieldTreePhase::LocalTransformChanged:case FieldTreePhase::PathChanged:case FieldTreePhase::Deleting:return true;
 default:return fail(e,"Visibility unimplemented native lifecycle phase");}
}
bool PodunkSceneVisibility::signal_declaration(FieldObjectId id,std::string_view name,uint32_t&arity,std::string&e)const{if(!owns(id)){e="Visibility signal object not owned";return false;}if(name=="screen_entered"||name=="screen_exited"){arity=0;return true;}if(name=="viewport_entered"||name=="viewport_exited"){arity=1;return true;}e="Visibility native signal unknown";return false;}
bool PodunkSceneVisibility::is_on_screen(FieldObjectId id,bool&on,std::string&e)const{auto i=instances_.find(id);if(failed_||i==instances_.end()||!registry_->object_exists(id)){e="Visibility screen query actual object unavailable";return false;}on=i->second.on_screen;return true;}
bool PodunkSceneVisibility::handles_method(const FieldDeferredMessage&m)const{if(m.kind!=FieldDeferredKind::Call)return false;if(owns(m.object)&&m.member=="_node_removed")return true;if(parents_.count(m.object)&&(m.member=="set_process"||m.member=="set_physics_process"))return true;return methods_.count({m.object,m.member})!=0;}
bool PodunkSceneVisibility::source_callback(const FieldVisibilityConnection&c,FieldObjectId object,std::string&e){
 const auto*d=tree_->descriptor(object);if(!d||d->id!=c.target||d->script!=c.script||d->script_sha!=c.script_sha)return fail(e,"Visibility source callback owner mismatch");const bool enter=c.signal==1;
 switch(c.adapter){
 case 1:return scripts_->grass_screen(object,enter,e);
 case 2:return scripts_->npc_screen(object,enter,e);
 case 3:if(!consumers_.butterfly||!consumers_.random)return fail(e,"Visibility Butterfly actual body/RNG missing");return enter?consumers_.butterfly->screen_enter(c.target,*consumers_.random,e):consumers_.butterfly->screen_exit(c.target,e);
 case 4:if(!consumers_.dandelion)return fail(e,"Visibility Dandelion actual factory missing");return enter?consumers_.dandelion->screen_entered(c.target,e):consumers_.dandelion->screen_exited(c.target,e);
 case 5:if(!consumers_.birds)return fail(e,"Visibility Birds actual body missing");if(!(enter?consumers_.birds->screen_enter(c.target):consumers_.birds->screen_exit(c.target))){e=consumers_.birds->error();return false;}return true;
 case 6:if(!enter||!consumers_.enemy)return fail(e,"Visibility EnemySpawner actual factory missing");if(!consumers_.enemy->spawner_screen_entered(c.target)){e=consumers_.enemy->error();return false;}return true;
 default:return fail(e,"Visibility source adapter unsupported");}
}
bool PodunkSceneVisibility::deferred(const FieldDeferredMessage&m,std::string&e){
 if(failed_||!handles_method(m))return fail(e,"Visibility method/source endpoint rejected");
 if(m.member=="_node_removed"&&owns(m.object)){Instance*x=nullptr;if(!actual(m.object,x,e)||m.args.size()!=1||!std::holds_alternative<FieldObjectRef>(m.args[0]))return fail(e,"Visibility removal bind rejected");auto id=std::get<FieldObjectRef>(m.args[0]).id;auto q=x->tracked.find(id);if(q==x->tracked.end())return fail(e,"Visibility removal not tracked");if(!x->enabler_visible&&!change_tracked(*x,id,q->second,true,e))return false;x->tracked.erase(q);return true;}
 if(parents_.count(m.object)&&(m.member=="set_process"||m.member=="set_physics_process")){if(m.args.size()!=1||!std::holds_alternative<bool>(m.args[0]))return fail(e,"Visibility parent ready setter arguments rejected");return tree_->set_process(m.object,m.member=="set_physics_process",std::get<bool>(m.args[0]),e);}
 const auto*d=tree_->descriptor(m.object);if(!d||!m.args.empty())return fail(e,"Visibility source callback arguments rejected");auto method=methods_.find({m.object,m.member});if(method==methods_.end())return fail(e,"Visibility source callback not recorded");return source_callback(data_->connections()[method->second],m.object,e);
}
bool PodunkSceneVisibility::update_world(uint64_t frame,std::string&e){
 if(failed_||!finished_||updating_||(frame_seen_&&frame<=frame_)||!viewport_->viewport().inside||!viewport_->viewport().world_registered)return fail(e,"Visibility requires real unique World2D frame boundary");
 FieldMapRect rect;Range cells;if(!viewport_->world_rect(rect,e)||!range(rect,cells,e))return false;if(!viewport_registered_||!equal(rect,viewport_rect_)){viewport_rect_=rect;viewport_cells_=cells;viewport_registered_=true;changed_=true;}frame_=frame;frame_seen_=true;if(!changed_)return true;
 std::vector<FieldObjectId>added;std::set<FieldObjectId>next;auto add=[&](const auto&cell){for(const auto&q:cell)if(next.insert(q.first).second&&!visible_.count(q.first))added.push_back(q.first);};
 const auto&r=viewport_cells_;auto width=int64_t(r.x1)-r.x0+1,height=int64_t(r.y1)-r.y0+1;
 if((width-1)*(height-1)>data_->scan_cutoff()){for(const auto&c:cells_){auto x=int32_t(uint32_t(c.first)),y=int32_t(uint32_t(c.first>>32));if(x>=r.x0&&x<=r.x1&&y>=r.y0&&y<=r.y1)add(c.second);}}
 else for(int64_t x=r.x0;x<=r.x1;++x)for(int64_t y=r.y0;y<=r.y1;++y){auto c=cells_.find(key(int32_t(x),int32_t(y)));if(c!=cells_.end())add(c->second);}
 std::vector<FieldObjectId>removed;for(auto id:visible_)if(!next.count(id))removed.push_back(id);visible_=std::move(next);changed_=false;updating_=true;
 for(auto id:added)if(instances_.count(id)&&instances_.at(id).inside&&!screen(id,true,e)){updating_=false;return fail(e,"Visibility screen entry actual consumer failed");}
 for(auto id:removed)if(instances_.count(id)&&instances_.at(id).on_screen&&!screen(id,false,e)){updating_=false;return fail(e,"Visibility screen exit actual consumer failed");}
 changed_=false;updating_=false;return true;
}
bool PodunkSceneVisibility::remove_viewport(std::string&e){if(failed_||!data_||updating_)return fail(e,"Visibility viewport removal unavailable");auto copy=visible_;visible_.clear();for(auto id:copy)if(instances_.count(id)&&instances_.at(id).on_screen&&!screen(id,false,e))return false;viewport_registered_=false;changed_=true;return true;}
bool PodunkSceneVisibility::disconnect_source(FieldObjectId id,std::string&e){for(auto it=connections_.begin();it!=connections_.end();){const auto&c=data_->connections()[*it];auto emitter=objects_.find(c.emitter);if(emitter==objects_.end()||emitter->second!=id){++it;continue;}auto target=tree_->source_object(c.target);bool yes=false;if(target&&registry_->object_exists(target)){if(!signals_->connected(id,screen_signal(c.signal),target,c.method,yes,e))return false;if(yes&&!signals_->disconnect(id,screen_signal(c.signal),target,c.method,e))return false;}it=connections_.erase(it);}return true;}
bool PodunkSceneVisibility::release(FieldObjectId id,std::string&e){auto it=instances_.find(id);if(it==instances_.end()||it->second.inside||it->second.on_screen||!it->second.tracked.empty())return fail(e,"Visibility release before actual world/Enabler exit");if(!disconnect_source(id,e))return false;objects_.erase(it->second.source->id);instances_.erase(it);return true;}
}
