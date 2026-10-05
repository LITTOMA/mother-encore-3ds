#include "encore/field_geometry_space.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace encore::upstream {
namespace {
constexpr uint32_t none=0xffffffffu;
Vec2 add(Vec2 a,Vec2 b){return {a.x+b.x,a.y+b.y};}Vec2 sub(Vec2 a,Vec2 b){return {a.x-b.x,a.y-b.y};}Vec2 mul(Vec2 a,float s){return {a.x*s,a.y*s};}
float dot(Vec2 a,Vec2 b){return a.x*b.x+a.y*b.y;}float cross(Vec2 a,Vec2 b){return a.x*b.y-a.y*b.x;}
float length(Vec2 a){return std::sqrt(dot(a,a));}Vec2 normalized(Vec2 a){float n=length(a);return n?mul(a,1/n):Vec2{};}
Vec2 basis(FieldGeometryTransform t,Vec2 p){return add(mul(t.x,p.x),mul(t.y,p.y));}Vec2 xf(FieldGeometryTransform t,Vec2 p){return add(basis(t,p),t.origin);}
FieldGeometryTransform compose(FieldGeometryTransform a,FieldGeometryTransform b){return {basis(a,b.x),basis(a,b.y),xf(a,b.origin)};}
Vec2 inverse(FieldGeometryTransform t,Vec2 p){p=sub(p,t.origin);const float d=cross(t.x,t.y);return {(t.y.y*p.x-t.y.x*p.y)/d,(t.x.x*p.y-t.x.y*p.x)/d};}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<1000000&&std::abs(p.y)<1000000;}
bool finite(FieldGeometryTransform t){return finite(t.x)&&finite(t.y)&&finite(t.origin)&&cross(t.x,t.y)!=0;}
bool bounds_ok(FieldGeometryBounds b){return finite(b.minimum)&&finite(b.maximum)&&b.minimum.x<=b.maximum.x&&b.minimum.y<=b.maximum.y;}
bool intersects(FieldGeometryBounds a,FieldGeometryBounds b){return a.minimum.x<=b.maximum.x&&a.maximum.x>=b.minimum.x&&a.minimum.y<=b.maximum.y&&a.maximum.y>=b.minimum.y;}
bool actor_ok(const FieldGeometryActor& a){
    if(!finite(a.transform))return false;
    if(a.kind==FieldGeometryKind::Circle){const float x=dot(a.transform.x,a.transform.x),y=dot(a.transform.y,a.transform.y);return std::isfinite(a.radius)&&a.radius>0&&x>0&&x==y&&dot(a.transform.x,a.transform.y)==0;}
    if(a.kind==FieldGeometryKind::Rectangle)return finite(a.extents)&&a.extents.x>0&&a.extents.y>0&&dot(a.transform.x,a.transform.y)==0;
    if(a.kind!=FieldGeometryKind::Convex||a.points.size()<3||a.points.size()>4096)return false;
    int winding=0;for(size_t i=0;i<a.points.size();++i){auto p=a.points[i],q=a.points[(i+1)%a.points.size()],r=a.points[(i+2)%a.points.size()];if(!finite(p))return false;float turn=cross(sub(q,p),sub(r,q));if(!turn)return false;int sign=turn>0?1:-1;if(winding&&sign!=winding)return false;winding=sign;}return true;
}
std::vector<Vec2> polygon(const FieldGeometryActor& a){
    std::vector<Vec2> p=a.points;
    if(a.kind==FieldGeometryKind::Rectangle)p={{-a.extents.x,-a.extents.y},{a.extents.x,-a.extents.y},{a.extents.x,a.extents.y},{-a.extents.x,a.extents.y}};
    for(auto& v:p)v=xf(a.transform,v);
    return p;
}
FieldGeometryBounds actor_bounds(const FieldGeometryActor& a){
    if(a.kind==FieldGeometryKind::Circle){const float r=a.radius*length(a.transform.x);return {sub(a.transform.origin,{r,r}),add(a.transform.origin,{r,r})};}
    auto p=polygon(a);FieldGeometryBounds b{p[0],p[0]};for(auto v:p){b.minimum.x=std::min(b.minimum.x,v.x);b.minimum.y=std::min(b.minimum.y,v.y);b.maximum.x=std::max(b.maximum.x,v.x);b.maximum.y=std::max(b.maximum.y,v.y);}return b;
}
bool overlap(const FieldGeometryActor& a,const FieldGeometryActor& b){
    const bool ca=a.kind==FieldGeometryKind::Circle,cb=b.kind==FieldGeometryKind::Circle;
    auto pa=ca?std::vector<Vec2>{}:polygon(a),pb=cb?std::vector<Vec2>{}:polygon(b);
    auto projection=[](const FieldGeometryActor& shape,const std::vector<Vec2>& p,Vec2 axis){
        if(shape.kind==FieldGeometryKind::Circle){const float center=dot(shape.transform.origin,axis),r=shape.radius*length(Vec2{dot(shape.transform.x,axis),dot(shape.transform.y,axis)});return std::array<float,2>{center-r,center+r};}
        std::array<float,2> result{dot(p[0],axis),dot(p[0],axis)};for(auto v:p){float d=dot(v,axis);result[0]=std::min(result[0],d);result[1]=std::max(result[1],d);}return result;
    };
    auto axis_test=[&](Vec2 axis){if(std::abs(axis.x)<1e-5f&&std::abs(axis.y)<1e-5f)axis={0,1};auto aa=projection(a,pa,axis),bb=projection(b,pb,axis);bb[0]-=(aa[1]-aa[0])*.5f;bb[1]+=(aa[1]-aa[0])*.5f;float dmin=bb[0]-(aa[0]+aa[1])*.5f,dmax=bb[1]-(aa[0]+aa[1])*.5f;return !(dmin>0||dmax<0);};
    if(ca&&cb)return axis_test(normalized(sub(a.transform.origin,b.transform.origin)));
    auto faces=[&](const std::vector<Vec2>& p){for(size_t i=0;i<p.size();++i){auto edge=normalized(sub(p[(i+1)%p.size()],p[i]));if(!axis_test({-edge.y,edge.x}))return false;}return true;};
    if(!ca&&!faces(pa))return false;
    if(!cb&&!faces(pb))return false;
    if(ca){for(auto v:pb)if(!axis_test(normalized(sub(a.transform.origin,v))))return false;}
    if(cb){for(auto v:pa)if(!axis_test(normalized(sub(b.transform.origin,v))))return false;}
    return true;
}
bool ray_shape(const FieldGeometryActor& a,Vec2 from,Vec2 to,float& fraction,Vec2& normal){
    const auto begin=inverse(a.transform,from),end=inverse(a.transform,to),direction=sub(end,begin);float t=std::numeric_limits<float>::infinity();Vec2 local_normal{};
    if(a.kind==FieldGeometryKind::Circle){const float aa=dot(direction,direction),bb=2*dot(begin,direction),cc=dot(begin,begin)-a.radius*a.radius;float disc=bb*bb-4*aa*cc;if(disc<0||aa==0)return false;t=(-bb-std::sqrt(disc))/(2*aa);if(t<0||t>1+1e-5f)return false;local_normal=normalized(add(begin,mul(direction,t)));}
    else if(a.kind==FieldGeometryKind::Rectangle){float minimum=0,maximum=1;int axis=-1;float sign=0;for(unsigned i=0;i<2;++i){float b=i?begin.y:begin.x,e=i?end.y:end.x,extent=i?a.extents.y:a.extents.x,d=e-b;if(d==0){if(b<-extent||b>extent)return false;continue;}float low=(-extent-b)/d,high=(extent-b)/d,s=-1;if(low>high){std::swap(low,high);s=1;}if(low>minimum){minimum=low;axis=int(i);sign=s;}maximum=std::min(maximum,high);if(minimum>maximum)return false;}t=minimum;if(axis==0)local_normal={sign,0};else if(axis==1)local_normal={0,sign};}
    else{
        // Native convex intersects each original edge, including a ray starting
        // inside; equal fractions retain the first source edge.
        for(size_t i=0;i<a.points.size();++i){auto p=a.points[i],q=a.points[(i+1)%a.points.size()],edge=sub(q,p);float den=cross(direction,edge);if(den==0)continue;auto delta=sub(p,begin);float hit=cross(delta,edge)/den,u=cross(delta,direction)/den;if(hit<0||hit>1||u<0||u>1||hit>=t)continue;t=hit;auto n=normalized(Vec2{edge.y,-edge.x});if(dot(direction,n)>0)n=mul(n,-1);local_normal=n;}
        if(!std::isfinite(t))return false;
    }
    fraction=t;const float det=cross(a.transform.x,a.transform.y);normal=normalized(Vec2{(a.transform.y.y*local_normal.x-a.transform.x.y*local_normal.y)/det,(-a.transform.y.x*local_normal.x+a.transform.x.x*local_normal.y)/det});return true;
}
}
bool FieldGeometrySpace::keys(FieldGeometryBounds b,std::vector<int64_t>& result,std::string& error) const{
    if(!bounds_ok(b)||cell_size_<1){error="Field geometry spatial bounds rejected";return false;}const int32_t x0=int32_t(std::floor(b.minimum.x/cell_size_)),y0=int32_t(std::floor(b.minimum.y/cell_size_)),x1=int32_t(std::floor(b.maximum.x/cell_size_)),y1=int32_t(std::floor(b.maximum.y/cell_size_));
    if(uint64_t(int64_t(x1)-x0+1)*uint64_t(int64_t(y1)-y0+1)>65536){error="Field geometry spatial query capacity exceeded";return false;}result.clear();for(int64_t y=y0;y<=y1;++y)for(int64_t x=x0;x<=x1;++x)result.push_back(int64_t((uint64_t(uint32_t(x))<<32)|uint32_t(y)));return true;
}
bool FieldGeometrySpace::configure(const FieldGeometryView& source,float cell,std::string& error){
    if(!source.valid()||!std::isfinite(cell)||cell<1||cell>4096){error="Field geometry space source/grid rejected";return false;}FieldGeometrySpace c;c.source_=&source;c.cell_size_=cell;c.node_instances_.resize(source.node_count());c.children_.resize(source.node_count());c.owner_instances_.resize(source.owner_count());
    for(uint32_t i=0;i<source.node_count();++i){auto n=source.node(i);c.ids_[n.stable_id]=i;c.nodes_.push_back({n.local,n.world,false,source.string(n.script).empty(),0,0});if(n.parent!=none)c.children_[n.parent].push_back(i);}
    for(uint32_t i=0;i<source.owner_count();++i){auto o=source.owner(i);c.owners_.push_back({o.layer,o.mask,(o.flags&2)!=0,(o.flags&4)!=0});}
    for(uint32_t s=0;s<source.shape_count();++s){auto shape=source.shape(s);c.disabled_.push_back((shape.flags&1)!=0);for(uint32_t part=0;part<shape.part_count;++part){uint32_t i=uint32_t(c.instances_.size());auto o=source.owner(shape.owner);auto n=source.node(o.node);uint32_t native_index=part;for(uint32_t previous=o.shape_first;previous<s;++previous)native_index+=source.shape(previous).part_count;Instance item;item.contact={shape.owner,s,part,n.stable_id,native_index};item.geometry=shape.part_first+part;c.instances_.push_back(std::move(item));c.owner_instances_[shape.owner].push_back(i);uint32_t node=shape.node;while(node!=none){c.node_instances_[node].push_back(i);node=source.node(node).parent;}if(!c.rebuild_instance(i,error))return false;auto& entry=c.instances_[i];if(!entry.resolved)c.unresolved_.insert(i);for(auto key:entry.cells)c.grid_[key].push_back(i);}}
    *this=std::move(c);error.clear();return true;
}
bool FieldGeometrySpace::rebuild_instance(uint32_t i,std::string& error){
    auto& entry=instances_[i];auto shape=source_->shape(entry.contact.shape);auto primitive=source_->geometry(entry.geometry);entry.actor={};entry.actor.kind=primitive.kind;entry.actor.transform=nodes_[shape.node].world;entry.actor.radius=primitive.parameters[0];entry.actor.extents={primitive.parameters[0],primitive.parameters[1]};entry.actor.stable_id=entry.contact.stable_id;
    if(primitive.kind==FieldGeometryKind::Convex)for(uint32_t j=0;j<primitive.point_count;++j)entry.actor.points.push_back(source_->point(primitive.point_first+j));
    if(!actor_ok(entry.actor)){error="Field geometry live kind/transform unsupported";return false;}entry.bounds=actor_bounds(entry.actor);entry.disabled=disabled_[entry.contact.shape];entry.deleted=false;entry.resolved=true;uint32_t node=shape.node;while(node!=none){entry.deleted=entry.deleted||nodes_[node].deleted;entry.resolved=entry.resolved&&nodes_[node].bound;node=source_->node(node).parent;}return keys(entry.bounds,entry.cells,error);
}
bool FieldGeometrySpace::refresh(const std::vector<uint32_t>& changed,std::string& error){
    std::unordered_set<uint32_t> affected;for(auto n:changed)for(auto i:node_instances_[n])affected.insert(i);
    std::vector<uint32_t> indices(affected.begin(),affected.end());std::sort(indices.begin(),indices.end());std::vector<Instance> old;old.reserve(indices.size());
    for(auto i:indices){old.push_back(instances_[i]);if(!rebuild_instance(i,error)){for(size_t j=0;j<old.size();++j)instances_[indices[j]]=std::move(old[j]);return false;}}
    for(size_t j=0;j<indices.size();++j){auto i=indices[j];for(auto key:old[j].cells){auto found=grid_.find(key);if(found!=grid_.end()){auto& list=found->second;list.erase(std::remove(list.begin(),list.end(),i),list.end());if(list.empty())grid_.erase(found);}}for(auto key:instances_[i].cells)grid_[key].push_back(i);if(instances_[i].resolved)unresolved_.erase(i);else unresolved_.insert(i);}return true;
}
bool FieldGeometrySpace::bind_script(uint32_t id,const std::array<uint8_t,32>& hash,uint32_t family,uint32_t capability,std::string& error){
    auto it=ids_.find(id);if(!source_||it==ids_.end()||!family||!capability){error="Field geometry script adapter identity rejected";return false;}uint32_t i=it->second;auto n=source_->node(i);if(source_->string(n.script).empty()||n.script_sha256!=hash){error="Field geometry script source rejected";return false;}auto old=nodes_[i];if(old.bound&&(old.family!=family||old.capability!=capability)){error="Field geometry script adapter rebind rejected";return false;}nodes_[i].bound=true;nodes_[i].family=family;nodes_[i].capability=capability;if(!refresh({i},error)){nodes_[i]=old;return false;}error.clear();return true;
}
bool FieldGeometrySpace::apply_updates(const std::vector<FieldGeometryNodeUpdate>& updates,std::string& error){
    if(!source_){error="Field geometry space unavailable";return false;}auto old_nodes=nodes_;auto old_owners=owners_;auto old_disabled=disabled_;std::vector<uint32_t> changed;std::unordered_set<uint32_t> unique;
    auto fail=[&](const char* text){nodes_=std::move(old_nodes);owners_=std::move(old_owners);disabled_=std::move(old_disabled);error=text;return false;};
    for(const auto& u:updates){auto it=ids_.find(u.stable_id);if(it==ids_.end()||!u.fields||u.fields>31||!unique.insert(it->second).second)return fail("Field geometry dirty node update rejected");uint32_t i=it->second,p=i;while(p!=none){if(!nodes_[p].bound)return fail("Field geometry unbound scripted ancestor update rejected");if(nodes_[p].deleted)return fail("Field geometry freed source node update rejected");p=source_->node(p).parent;}changed.push_back(i);if(u.fields&1){if(!finite(u.local))return fail("Field geometry live local matrix rejected");nodes_[i].local=u.local;}if(u.fields&2){if(nodes_[i].deleted&&!u.deleted)return fail("Field geometry deleted source node resurrection rejected");nodes_[i].deleted=u.deleted;}
        if(u.fields&4){auto cls=source_->string(source_->node(i).class_name);if(cls!="CollisionShape2D"&&cls!="CollisionPolygon2D")return fail("Field geometry disabled update target rejected");bool found=false;for(auto instance:node_instances_[i]){auto shape=source_->shape(instances_[instance].contact.shape);if(shape.node==i){disabled_[instances_[instance].contact.shape]=u.disabled;found=true;}}if(!found&&cls=="CollisionPolygon2D")return fail("Field geometry disabled target has no native parts");}
        if(u.fields&24){bool found=false;for(uint32_t owner=0;owner<source_->owner_count();++owner)if(source_->owner(owner).node==i){found=true;if(u.fields&8){owners_[owner].layer=u.layer;owners_[owner].mask=u.mask;}if(u.fields&16){if(source_->owner(owner).kind!=4)return fail("Field geometry monitoring update target rejected");owners_[owner].monitoring=u.monitoring;owners_[owner].monitorable=u.monitorable;}}if(!found)return fail("Field geometry owner state target rejected");}
    }
    std::unordered_set<uint32_t> affected_nodes;std::vector<uint32_t> stack=changed;while(!stack.empty()){auto i=stack.back();stack.pop_back();if(!affected_nodes.insert(i).second)continue;for(auto c:children_[i])stack.push_back(c);}std::vector<uint32_t> ordered(affected_nodes.begin(),affected_nodes.end());std::sort(ordered.begin(),ordered.end());for(auto i:ordered){auto parent=source_->node(i).parent;nodes_[i].world=parent==none?nodes_[i].local:compose(nodes_[parent].world,nodes_[i].local);if(!finite(nodes_[i].world))return fail("Field geometry live world matrix rejected");}
    if(!refresh(changed,error)){nodes_=std::move(old_nodes);owners_=std::move(old_owners);disabled_=std::move(old_disabled);return false;}error.clear();return true;
}
bool FieldGeometrySpace::filter_owner(uint32_t i,const FieldGeometryFilter& f) const{auto o=source_->owner(i);auto state=owners_[i];if(o.kind==4?!f.areas:!f.bodies)return false;if(f.exclude_stable_id&&source_->node(o.node).stable_id==f.exclude_stable_id)return false;if(f.monitoring_only){if(o.kind!=4||!state.monitoring)return false;}else if(!(state.layer&f.layer_mask))return false;if(f.require_reciprocal_mask&&!(state.mask&f.reciprocal_layer))return false;return true;}
bool FieldGeometrySpace::query_instances(FieldGeometryBounds b,const FieldGeometryFilter& filter,size_t capacity,std::vector<uint32_t>& output,std::string& error) const{
    if(!source_){error="Field geometry space unavailable";return false;}
    // Unknown scripted motion cannot be discarded merely because its initial
    // source AABB lies outside this query. Resolve eligible owners first.
    for(auto i:unresolved_){const auto& e=instances_[i];const auto owner=source_->owner(e.contact.owner);const bool type=owner.kind==4?filter.areas:filter.bodies;const bool excluded=filter.exclude_stable_id&&e.contact.stable_id==filter.exclude_stable_id;if(type&&!excluded&&!e.deleted){error="Field geometry query requires actual scripted ancestor adapter before layer/disabled/monitoring filtering";return false;}}
    std::vector<int64_t> cells;if(!keys(b,cells,error))return false;std::unordered_set<uint32_t> seen;std::vector<uint32_t> hits;
    for(auto key:cells){auto it=grid_.find(key);if(it==grid_.end())continue;for(auto i:it->second){if(!seen.insert(i).second)continue;const auto& e=instances_[i];if(e.disabled||e.deleted||!filter_owner(e.contact.owner,filter)||!intersects(b,e.bounds))continue;if(hits.size()>=capacity){error="Field geometry query result capacity exceeded";return false;}hits.push_back(i);}}
    std::sort(hits.begin(),hits.end(),[&](uint32_t a,uint32_t c){auto aa=instances_[a].contact,bb=instances_[c].contact;auto ao=source_->node(source_->owner(aa.owner).node).order,bo=source_->node(source_->owner(bb.owner).node).order;if(ao!=bo)return ao<bo;auto as=source_->node(source_->shape(aa.shape).node).order,bs=source_->node(source_->shape(bb.shape).node).order;return as!=bs?as<bs:aa.part<bb.part;});output=std::move(hits);error.clear();return true;
}
bool FieldGeometrySpace::candidates(FieldGeometryBounds b,const FieldGeometryFilter& filter,size_t capacity,std::vector<FieldGeometryContact>& output,std::string& error) const{std::vector<uint32_t> indices;if(!query_instances(b,filter,capacity,indices,error))return false;std::vector<FieldGeometryContact> result;for(auto i:indices)result.push_back(instances_[i].contact);output=std::move(result);return true;}
bool FieldGeometrySpace::overlap_actor(const FieldGeometryActor& actor,const FieldGeometryFilter& filter,size_t capacity,std::vector<FieldGeometryContact>& output,std::string& error) const{
    if(!actor_ok(actor)){error="Field geometry actor kind/transform rejected";return false;}std::vector<uint32_t> indices;if(!query_instances(actor_bounds(actor),filter,instances_.size(),indices,error))return false;std::vector<FieldGeometryContact> result;for(auto i:indices)if(overlap(actor,instances_[i].actor)){if(result.size()>=capacity){error="Field geometry overlap capacity exceeded";return false;}result.push_back(instances_[i].contact);}output=std::move(result);error.clear();return true;
}
bool FieldGeometrySpace::monitoring_areas(const FieldGeometryActor& actor,size_t capacity,std::vector<FieldGeometryContact>& output,std::string& error) const{if(!source_||!actor_ok(actor)){error="Field geometry monitored actor/source rejected";return false;}if(actor.area&&!actor.monitorable){output.clear();error.clear();return true;}FieldGeometryFilter filter;filter.bodies=false;filter.areas=true;filter.monitoring_only=true;filter.require_reciprocal_mask=true;filter.reciprocal_layer=actor.layer;filter.exclude_stable_id=actor.stable_id;return overlap_actor(actor,filter,capacity,output,error);}
bool FieldGeometrySpace::ray(Vec2 from,Vec2 to,const FieldGeometryFilter& filter,size_t capacity,bool& collided,FieldGeometryRayHit& output,std::string& error) const{
    if(!finite(from)||!finite(to)||dot(sub(to,from),sub(to,from))==0){error="Field geometry ray endpoints rejected";return false;}FieldGeometryBounds b{{std::min(from.x,to.x),std::min(from.y,to.y)},{std::max(from.x,to.x),std::max(from.y,to.y)}};std::vector<uint32_t> indices;if(!query_instances(b,filter,capacity,indices,error))return false;bool found=false;FieldGeometryRayHit result;float best=std::numeric_limits<float>::infinity();for(auto i:indices){float t;Vec2 normal;if(ray_shape(instances_[i].actor,from,to,t,normal)&&t<best){best=t;found=true;static_cast<FieldGeometryContact&>(result)=instances_[i].contact;result.position=add(from,mul(sub(to,from),t));result.normal=normal;result.fraction=t;}}collided=found;if(found)output=result;error.clear();return true;
}
}
