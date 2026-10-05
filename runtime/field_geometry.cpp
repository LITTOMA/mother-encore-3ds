#include "encore/field_geometry.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_set>

namespace encore::upstream {
namespace {
constexpr uint32_t none=0xffffffffu;
constexpr uint32_t strides[]={0,8,1,112,84,112,44,8};
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t* p){const uint32_t n=u32(p);float v;std::memcpy(&v,&n,4);return v;}
Vec2 vec(const uint8_t* p){return {real(p),real(p+4)};}
FieldGeometryTransform tr(const uint8_t* p){return {vec(p),vec(p+8),vec(p+16)};}
bool range(uint32_t i,uint32_t n,uint32_t total){return i<=total&&n<=total-i;}
bool finite(float n){return std::isfinite(n)&&std::abs(n)<1000000;}
bool finite(Vec2 p){return finite(p.x)&&finite(p.y);}
bool finite(FieldGeometryTransform t){return finite(t.x)&&finite(t.y)&&finite(t.origin)&&t.x.x*t.y.y-t.x.y*t.y.x!=0;}
bool same(Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;}
bool same(FieldGeometryTransform a,FieldGeometryTransform b){return same(a.x,b.x)&&same(a.y,b.y)&&same(a.origin,b.origin);}
Vec2 basis(FieldGeometryTransform t,Vec2 p){return {t.x.x*p.x+t.y.x*p.y,t.x.y*p.x+t.y.y*p.y};}
FieldGeometryTransform compose(FieldGeometryTransform a,FieldGeometryTransform b){auto origin=basis(a,b.origin);origin.x+=a.origin.x;origin.y+=a.origin.y;return {basis(a,b.x),basis(a,b.y),origin};}
uint32_t crc(const uint8_t* p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320u:0u);}return ~c;}
bool canonical(std::string_view p,bool scene){
    if(p.empty()||p.front()=='/'||p.back()=='/'||p.find('\\')!=p.npos||p.find('\0')!=p.npos)return false;
    if(!scene&&p==".")return true;
    size_t first=0;
    while(first<p.size()){auto end=p.find('/',first);if(end==p.npos)end=p.size();auto part=p.substr(first,end-first);if(part.empty()||part=="."||part=="..")return false;first=end+1;}
    return !scene||(p.size()>5&&p.substr(p.size()-5)==".tscn"&&p.find(':')==p.npos);
}
}
uint32_t FieldGeometryView::count(uint32_t k) const{return valid()&&k>=1&&k<=7?u32(bytes_.data()+128+(k-1)*24+4):0;}
const uint8_t* FieldGeometryView::record(uint32_t k,uint32_t i) const{return i<count(k)?bytes_.data()+u32(bytes_.data()+128+(k-1)*24+12)+size_t(i)*strides[k]:nullptr;}
FieldIdentity FieldGeometryView::identity() const{FieldIdentity id;if(valid()){id.scene_id=u32(bytes_.data()+36);std::copy_n(bytes_.data()+40,20,id.upstream_commit.begin());std::copy_n(bytes_.data()+60,32,id.source_sha256.begin());}return id;}
std::string_view FieldGeometryView::source_scene() const{return valid()?string(u32(bytes_.data()+124)):std::string_view{};}
uint32_t FieldGeometryView::node_count() const{return count(3);}uint32_t FieldGeometryView::owner_count() const{return count(4);}uint32_t FieldGeometryView::shape_count() const{return count(5);}uint32_t FieldGeometryView::geometry_count() const{return count(6);}
std::string_view FieldGeometryView::string(uint32_t i) const{auto p=record(1,i);if(!p)return {};return {reinterpret_cast<const char*>(record(2,u32(p))),u32(p+4)};}
FieldGeometryNode FieldGeometryView::node(uint32_t i) const{FieldGeometryNode n;auto p=record(3,i);if(!p)return n;uint32_t* a[]={&n.stable_id,&n.path,&n.parent,&n.order,&n.ready,&n.flags,&n.class_name,&n.script};for(unsigned j=0;j<8;++j)*a[j]=u32(p+j*4);std::copy_n(p+32,32,n.script_sha256.begin());n.local=tr(p+64);n.world=tr(p+88);return n;}
FieldGeometryOwner FieldGeometryView::owner(uint32_t i) const{FieldGeometryOwner o;auto p=record(4,i);if(!p)return o;uint32_t* a[]={&o.node,&o.kind,&o.layer,&o.mask,&o.flags,&o.shape_first,&o.shape_count,&o.audio_bus,&o.space_override,&o.platform_leave};for(unsigned j=0;j<10;++j)*a[j]=u32(p+j*4);o.safe_margin=real(p+40);o.priority=real(p+44);o.gravity=real(p+48);o.gravity_distance_scale=real(p+52);o.gravity_vector=vec(p+56);o.linear_damp=real(p+64);o.angular_damp=real(p+68);o.constant_linear_velocity=vec(p+72);o.constant_angular_velocity=real(p+80);return o;}
FieldGeometryShape FieldGeometryView::shape(uint32_t i) const{FieldGeometryShape s;auto p=record(5,i);if(!p)return s;uint32_t* a[]={&s.node,&s.owner,&s.kind,&s.flags,&s.geometry,&s.part_first,&s.part_count};for(unsigned j=0;j<7;++j)*a[j]=u32(p+j*4);s.margin=real(p+32);s.owner_margin=real(p+36);s.world=tr(p+40);s.owner_transform=tr(p+64);s.cached_before_enter_tree=tr(p+88);return s;}
FieldGeometryPrimitive FieldGeometryView::geometry(uint32_t i) const{FieldGeometryPrimitive g;auto p=record(6,i);if(!p)return g;g.kind=static_cast<FieldGeometryKind>(u32(p));g.point_first=u32(p+4);g.point_count=u32(p+8);g.resource=u32(p+12);for(unsigned j=0;j<6;++j)g.parameters[j]=real(p+20+j*4);return g;}
Vec2 FieldGeometryView::point(uint32_t i) const{auto p=record(7,i);return p?vec(p):Vec2{};}
bool FieldGeometryView::load(const uint8_t* p,size_t n,const FieldIdentity& id,std::string& error){if(!p||n<296||n>8*1024*1024){error="Field geometry size rejected";return false;}return admit(std::vector<uint8_t>(p,p+n),id,error);}
bool FieldGeometryView::load_file(const char* path,const FieldIdentity& id,std::string& error){
    if(!path||!*path){error="Field geometry path rejected";return false;}
    FILE* file=std::fopen(path,"rb");if(!file){error="Cannot open field geometry";return false;}
    if(std::fseek(file,0,SEEK_END)){std::fclose(file);error="Field geometry seek rejected";return false;}
    const long size=std::ftell(file);if(size<296||size>8*1024*1024||std::fseek(file,0,SEEK_SET)){std::fclose(file);error="Field geometry file size rejected";return false;}
    std::vector<uint8_t> bytes(static_cast<size_t>(size));const size_t got=std::fread(bytes.data(),1,bytes.size(),file);const bool closed=std::fclose(file)==0;
    if(got!=bytes.size()||!closed){error="Field geometry read rejected";return false;}return admit(std::move(bytes),id,error);
}
bool FieldGeometryView::admit(std::vector<uint8_t>&& bytes,const FieldIdentity& expected,std::string& error){
    auto reject=[&](const char* why){error=why;return false;};const auto p=bytes.data();const size_t size=bytes.size();
    if(size<296||std::memcmp(p,"ENCFGEO1",8)||u32(p+8)!=1||u32(p+12)!=128||u32(p+16)!=size||u32(p+20)!=7||u32(p+28)!=0x454e001b||u32(p+32)!=1||u32(p+36)!=expected.scene_id||!expected.scene_id||std::memcmp(p+40,expected.upstream_commit.data(),20)||std::memcmp(p+60,expected.source_sha256.data(),32))return reject("Field geometry identity/version rejected");
    if(crc(p+296,size-296)!=u32(p+24))return reject("Field geometry checksum rejected");
    uint64_t offset=296;
    for(uint32_t k=1;k<=7;++k){auto d=p+128+(k-1)*24;const uint32_t n=u32(d+4);if(u32(d)!=k||u32(d+8)!=strides[k]||u32(d+12)!=offset||u32(d+16)!=uint64_t(n)*strides[k]||u32(d+20)||uint64_t(n)*strides[k]>size-offset)return reject("Field geometry directory rejected");offset+=uint64_t(n)*strides[k];}
    if(offset!=size)return reject("Field geometry trailing bytes rejected");
    FieldGeometryView candidate;candidate.bytes_=std::move(bytes);
    if(!candidate.count(1)||!candidate.count(2)||!candidate.node_count()||candidate.node_count()>10000||candidate.owner_count()>10000||candidate.shape_count()>20000||candidate.geometry_count()>100000||candidate.count(7)>200000)return reject("Field geometry capacity rejected");
    uint32_t string_end=0;
    for(uint32_t i=0;i<candidate.count(1);++i){auto r=candidate.record(1,i);uint32_t begin=u32(r),len=u32(r+4);if(begin!=string_end||len>=candidate.count(2)||!range(begin,len+1,candidate.count(2)))return reject("Field geometry string span rejected");const auto q=candidate.record(2,begin);if(q[len]||std::memchr(q,0,len))return reject("Field geometry string bytes rejected");string_end+=len+1;}
    if(string_end!=candidate.count(2)||u32(p+124)>=candidate.count(1)||!canonical(candidate.source_scene(),true))return reject("Field geometry source scene rejected");
    std::unordered_set<uint32_t> ids,ready,order;std::unordered_set<std::string_view> paths;
    for(uint32_t i=0;i<candidate.node_count();++i){const auto n=candidate.node(i);if(!n.stable_id||!ids.insert(n.stable_id).second||!ready.insert(n.ready).second||!order.insert(n.order).second||n.path>=candidate.count(1)||n.class_name>=candidate.count(1)||n.script>=candidate.count(1)||!canonical(candidate.string(n.path),false)||!paths.insert(candidate.string(n.path)).second||candidate.string(n.class_name).empty()||n.flags>7||!finite(n.local)||!finite(n.world)||(i==0?n.parent!=none:n.parent>=i))return reject("Field geometry node hierarchy rejected");
        if(i==0){if(candidate.string(n.path)!=".")return reject("Field geometry root rejected");}
        else{auto path=candidate.string(n.path);auto slash=path.rfind('/');auto expected_parent=slash==path.npos?std::string_view("."):path.substr(0,slash);if(candidate.string(candidate.node(n.parent).path)!=expected_parent||candidate.node(n.parent).order>=n.order||candidate.node(n.parent).ready<=n.ready)return reject("Field geometry parent/order rejected");}
        if(i>0&&!same(compose(candidate.node(n.parent).world,n.local),n.world))return reject("Field geometry hierarchy transform correspondence rejected");
        const bool zero=std::all_of(n.script_sha256.begin(),n.script_sha256.end(),[](uint8_t v){return v==0;});if(candidate.string(n.script).empty()!=zero)return reject("Field geometry script receipt rejected");
    }
    uint32_t point_end=0;
    for(uint32_t i=0;i<candidate.geometry_count();++i){auto g=candidate.geometry(i);auto r=candidate.record(6,i);const auto kind=static_cast<uint32_t>(g.kind);if(kind>10||u32(r+16)||g.resource>=candidate.count(1)||g.point_first!=point_end||!range(g.point_first,g.point_count,candidate.count(7)))return reject("Field geometry primitive span rejected");point_end+=g.point_count;for(float v:g.parameters)if(!finite(v))return reject("Field geometry primitive parameter rejected");for(uint32_t j=0;j<g.point_count;++j)if(!finite(candidate.point(g.point_first+j)))return reject("Field geometry primitive point rejected");
        if((kind<=3||kind==7||kind==8)&&g.point_count)return reject("Field geometry analytic shape points rejected");
        if(kind==0&&std::any_of(g.parameters.begin(),g.parameters.end(),[](float v){return v!=0;}))return reject("Field geometry empty primitive rejected");
        if(kind==1&&(g.parameters[0]<=0||g.parameters[1]<=0))return reject("Field geometry rectangle rejected");
        if(kind==2&&g.parameters[0]<=0)return reject("Field geometry circle rejected");
        if(kind==3&&(g.parameters[0]<=0||g.parameters[1]<0))return reject("Field geometry capsule rejected");
        if(kind==4&&g.point_count<3)return reject("Field geometry convex rejected");
        if(kind==4){int winding=0;for(uint32_t j=0;j<g.point_count;++j){const auto a=candidate.point(g.point_first+j),b=candidate.point(g.point_first+(j+1)%g.point_count),c=candidate.point(g.point_first+(j+2)%g.point_count);const double cross=(double(b.x)-a.x)*(double(c.y)-b.y)-(double(b.y)-a.y)*(double(c.x)-b.x);if(cross==0)return reject("Field geometry degenerate convex rejected");const int sign=cross>0?1:-1;if(winding&&winding!=sign)return reject("Field geometry nonconvex primitive rejected");winding=sign;}}
        if(kind==5&&(g.point_count%2))return reject("Field geometry concave segment array rejected");
        if(kind==6&&g.point_count!=2)return reject("Field geometry segment rejected");
        if(kind==7&&(g.parameters[0]<=0||(g.parameters[1]!=0&&g.parameters[1]!=1)))return reject("Field geometry ray rejected");
        if(kind==8&&(g.parameters[0]==0&&g.parameters[1]==0))return reject("Field geometry line rejected");
    }
    if(point_end!=candidate.count(7))return reject("Field geometry orphan points rejected");
    std::unordered_set<uint32_t> owner_nodes,shape_nodes;uint32_t shape_end=0;
    for(uint32_t i=0;i<candidate.owner_count();++i){auto o=candidate.owner(i);auto r=candidate.record(4,i);if(o.node>=candidate.node_count()||!owner_nodes.insert(o.node).second||o.kind<1||o.kind>4||o.flags>63||o.audio_bus>=candidate.count(1)||o.space_override>4||o.platform_leave>2||o.shape_first!=shape_end||!range(o.shape_first,o.shape_count,candidate.shape_count())||o.safe_margin<0)return reject("Field geometry owner rejected");shape_end+=o.shape_count;for(unsigned j=0;j<11;++j)if(!finite(real(r+40+j*4)))return reject("Field geometry owner real rejected");const char* classes[]={"StaticBody2D","KinematicBody2D","RigidBody2D","Area2D"};if(candidate.string(candidate.node(o.node).class_name)!=classes[o.kind-1])return reject("Field geometry owner class rejected");
        for(uint32_t j=0;j<o.shape_count;++j){const auto s=candidate.shape(o.shape_first+j);auto sr=candidate.record(5,o.shape_first+j);if(s.node>=candidate.node_count()||!shape_nodes.insert(s.node).second||candidate.node(s.node).parent!=o.node||s.owner!=i||s.kind>10||s.flags>3||u32(sr+28)||s.geometry>=candidate.geometry_count()||static_cast<uint32_t>(candidate.geometry(s.geometry).kind)!=s.kind||!finite(s.margin)||!finite(s.owner_margin)||s.margin<0||s.owner_margin<0||!finite(s.world)||!finite(s.owner_transform)||!finite(s.cached_before_enter_tree)||!same(s.world,candidate.node(s.node).world)||(s.part_count?!range(s.part_first,s.part_count,candidate.geometry_count()):s.part_first!=none))return reject("Field geometry shape owner/transform rejected");
            const auto cls=candidate.string(candidate.node(s.node).class_name);if(cls!=(s.kind>=9?"CollisionPolygon2D":"CollisionShape2D"))return reject("Field geometry shape class rejected");
            if(!same(s.owner_transform,candidate.node(s.node).local))return reject("Field geometry native owner/local transform rejected");
            if(s.kind==0&&s.part_count)return reject("Field geometry empty shape parts rejected");
            if(s.kind>0&&s.kind<9&&(s.part_count!=1||s.part_first!=s.geometry))return reject("Field geometry analytic shape part rejected");
            for(uint32_t k=0;k<s.part_count;++k){auto kind=static_cast<uint32_t>(candidate.geometry(s.part_first+k).kind);if((s.kind==9&&kind!=4)||(s.kind==10&&kind!=5))return reject("Field geometry polygon native part rejected");}
        }
    }
    if(shape_end!=candidate.shape_count())return reject("Field geometry orphan shape rejected");
    bytes_=std::move(candidate.bytes_);error.clear();return true;
}
bool FieldGeometryView::owners_for_layer(uint32_t mask,size_t capacity,std::vector<uint32_t>& output,std::string& error) const{
    if(!valid()){error="Field geometry unavailable";return false;}std::vector<uint32_t> result;
    for(uint32_t i=0;i<owner_count();++i)if(owner(i).layer&mask){if(result.size()>=capacity){error="Field geometry owner query capacity exceeded";return false;}result.push_back(i);}
    output=std::move(result);error.clear();return true;
}
}
