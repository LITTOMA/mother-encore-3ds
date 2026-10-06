#include "encore/field_map.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unordered_set>

namespace encore::upstream {
namespace {
constexpr uint32_t none=0xffffffffu;
constexpr uint32_t strides[]={0,8,1,64,26,28,56,8,104,36,20,44,52,32,28,8,32,4,24};
uint16_t u16(const uint8_t* p) {return uint16_t(p[0])|uint16_t(p[1])<<8;}
int16_t i16(const uint8_t* p) {const auto n=u16(p);int16_t v;std::memcpy(&v,&n,2);return v;}
uint32_t u32(const uint8_t* p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
int32_t i32(const uint8_t* p) { const uint32_t n=u32(p);int32_t v;std::memcpy(&v,&n,4);return v; }
float real(const uint8_t* p) { const auto n=u32(p);float v;std::memcpy(&v,&n,4);return v; }
Vec2 vec(const uint8_t* p) { return {real(p),real(p+4)}; }
uint32_t crc(const uint8_t* p,size_t n) { uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320u:0u);}return ~c; }
bool finite(Vec2 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<1000000&&std::abs(v.y)<1000000; }
bool range(uint32_t first,uint32_t n,uint32_t total) { return first<=total&&n<=total-first; }
bool intersects(FieldMapRect a,FieldMapRect b) { return a.minimum.x<=b.maximum.x&&a.maximum.x>=b.minimum.x&&a.minimum.y<=b.maximum.y&&a.maximum.y>=b.minimum.y; }
bool bounds(FieldMapRect b) { return finite(b.minimum)&&finite(b.maximum)&&b.minimum.x<=b.maximum.x&&b.minimum.y<=b.maximum.y; }
bool encloses(FieldMapRect outer,FieldMapRect inner) {return outer.minimum.x<=inner.minimum.x&&outer.minimum.y<=inner.minimum.y&&outer.maximum.x>=inner.maximum.x&&outer.maximum.y>=inner.maximum.y;}
bool texture_path(std::string_view path) {
    const std::string_view prefix="graphics/world/",suffix=".t3x";
    if(path.size()<=prefix.size()+suffix.size()||path.substr(0,prefix.size())!=prefix||path.substr(path.size()-suffix.size())!=suffix)return false;
    bool separator=true;
    for(char c:path.substr(prefix.size(),path.size()-prefix.size()-suffix.size())){
        if(c=='/'){if(separator)return false;separator=true;continue;}
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;
        separator=false;
    }
    if(separator)return false;
    return true;
}
FieldMapRect draw_bounds(FieldMapDraw d) { Vec2 s{std::abs(d.signed_size.x),std::abs(d.signed_size.y)};if(d.flags&4)std::swap(s.x,s.y);return {d.position,{d.position.x+s.x,d.position.y+s.y}}; }
Vec2 transformed(FieldMapShapeTransform t,Vec2 p) {return {t.x.x*p.x+t.y.x*p.y+t.origin.x,t.x.y*p.x+t.y.y*p.y+t.origin.y};}
bool native_intersects(FieldMapRect a,FieldMapRect b) {return a.minimum.x<b.maximum.x&&a.maximum.x>b.minimum.x&&a.minimum.y<b.maximum.y&&a.maximum.y>b.minimum.y;}
}
uint32_t FieldMapView::count(uint32_t k) const { return valid()&&k>=1&&k<=18?u32(bytes_.data()+128+(k-1)*24+8):0; }
const uint8_t* FieldMapView::record(uint32_t k,uint32_t i) const { if(i>=count(k))return nullptr;return bytes_.data()+u32(bytes_.data()+128+(k-1)*24+4)+size_t(i)*strides[k]; }
FieldIdentity FieldMapView::identity() const {FieldIdentity id;if(!valid())return id;id.scene_id=u32(bytes_.data()+36);std::copy_n(bytes_.data()+40,20,id.upstream_commit.begin());std::copy_n(bytes_.data()+60,32,id.source_sha256.begin());return id;}
std::string_view FieldMapView::source_scene() const {return valid()?string(u32(bytes_.data()+124)):std::string_view{};}
FieldMapLocalGeometry FieldMapView::local_geometry(uint32_t i) const {FieldMapLocalGeometry v;auto p=record(14,i);if(!p)return v;v.kind=u32(p);v.point_first=u32(p+4);v.point_count=u32(p+8);v.bvh_first=u32(p+12);v.bvh_count=u32(p+16);v.leaf_first=u32(p+20);v.leaf_count=u32(p+24);return v;}
Vec2 FieldMapView::local_point(uint32_t i) const {auto p=record(15,i);return p?vec(p):Vec2{};}
FieldMapShapeTransform FieldMapView::shape_transform(uint32_t i) const {auto p=record(18,i);return p?FieldMapShapeTransform{vec(p),vec(p+8),vec(p+16)}:FieldMapShapeTransform{};}
FieldMapConcaveNode FieldMapView::concave_node(uint32_t i) const {FieldMapConcaveNode v;auto p=record(16,i);if(!p)return v;v.left=u32(p);v.right=u32(p+4);v.segment=u32(p+8);v.bounds={vec(p+16),vec(p+24)};return v;}
uint32_t FieldMapView::concave_leaf(uint32_t i) const {auto p=record(17,i);return p?u32(p):none;}
uint32_t FieldMapView::map_count() const{return count(3);} uint32_t FieldMapView::cell_count() const{return count(4);}
uint32_t FieldMapView::draw_count() const{return count(5);} uint32_t FieldMapView::polygon_count() const{return count(6);}
uint32_t FieldMapView::texture_count() const{return count(8);} uint32_t FieldMapView::canvas_count() const{return count(9);} uint32_t FieldMapView::gate_count() const{return count(10);}
std::string_view FieldMapView::string(uint32_t i) const { auto p=record(1,i);if(!p)return {};auto b=record(2,u32(p));return {reinterpret_cast<const char*>(b),u32(p+4)}; }
FieldMapLayer FieldMapView::map(uint32_t i) const { FieldMapLayer v;auto p=record(3,i);if(!p)return v;uint32_t* a[]={&v.stable_id,&v.node,&v.canvas,&v.cell_first,&v.cell_count,&v.draw_first,&v.draw_count,&v.polygon_first,&v.polygon_count,&v.flags,&v.layer,&v.mask};for(unsigned j=0;j<12;++j)*a[j]=u32(p+j*4);v.position=vec(p+48);v.cell_size=vec(p+56);return v; }
FieldMapCell FieldMapView::cell(uint32_t i) const { FieldMapCell v;auto p=record(4,i);if(!p)return v;v.x=i16(p);v.y=i16(p+2);v.tile=i16(p+4);v.atlas_x=u16(p+6);v.atlas_y=u16(p+8);v.quadrant_ordinal=u16(p+10);v.flags=u16(p+12);v.draw_first=u32(p+14);v.draw_count=u16(p+18);v.polygon_first=u32(p+20);v.polygon_count=u16(p+24);return v; }
FieldMapDraw FieldMapView::draw(uint32_t i) const { FieldMapDraw v;auto p=record(5,i);if(!p)return v;auto t=record(12,u32(p+8));if(!t)return v;v.map=u32(p);v.cell=u32(p+4);v.texture=u32(t);v.flags=u32(t+4);v.order=u32(p+12);v.gate=u32(p+16);v.z=i32(t+8);v.position=vec(p+20);v.signed_size=vec(t+12);v.uv_position=vec(t+20);v.uv_size=vec(t+28);for(unsigned j=0;j<4;++j)v.color[j]=real(t+36+j*4);return v; }
FieldMapPolygon FieldMapView::polygon(uint32_t i) const { FieldMapPolygon v;auto p=record(6,i);if(!p)return v;v.map=u32(p);v.cell=u32(p+4);v.kind=u32(p+8);v.point_first=u32(p+12);v.point_count=u32(p+16);v.layer=u32(p+20);v.order=u32(p+24);v.gate=u32(p+28);v.geometry=u32(p+32);v.transform=u32(p+36);v.bounds={vec(p+40),vec(p+48)};return v; }
Vec2 FieldMapView::point(uint32_t i) const { auto p=record(7,i);return p?vec(p):Vec2{}; }
FieldMapTexture FieldMapView::texture(uint32_t i) const { FieldMapTexture v;auto p=record(8,i);if(!p)return v;v.stable_id=u32(p);v.source=u32(p+4);v.path=u32(p+8);v.width=u32(p+12);v.height=u32(p+16);v.group=u32(p+20);v.frame=u32(p+24);v.frames=u32(p+28);v.fps=real(p+32);v.delay=real(p+36);std::copy(p+40,p+72,v.source_sha256.begin());std::copy(p+72,p+104,v.output_sha256.begin());return v; }
FieldMapCanvas FieldMapView::canvas(uint32_t i) const { FieldMapCanvas v;auto p=record(9,i);if(!p)return v;v.stable_id=u32(p);v.node=u32(p+4);v.parent=u32(p+8);v.order=u32(p+12);v.flags=u32(p+16);v.gate=u32(p+20);v.z=i32(p+24);v.position=vec(p+28);return v; }
FieldMapGate FieldMapView::gate(uint32_t i) const { FieldMapGate v;auto p=record(10,i);if(!p)return v;v.stable_id=u32(p);v.node=u32(p+4);v.appear=u32(p+8);v.disappear=u32(p+12);v.delete_if_hidden=u32(p+16)!=0;return v; }
bool FieldMapView::load_file(const char* path,const FieldIdentity& id,std::string& error) {
    auto f=std::fopen(path,"rb");if(!f){error="Cannot open field map";return false;}
    if(std::fseek(f,0,SEEK_END)!=0){std::fclose(f);error="Cannot size field map";return false;}
    const long n=std::ftell(f);if(n<560||n>32*1024*1024||std::fseek(f,0,SEEK_SET)!=0){std::fclose(f);error="Field map size rejected";return false;}
    std::vector<uint8_t> b(static_cast<size_t>(n));const size_t got=std::fread(b.data(),1,b.size(),f);std::fclose(f);
    if(got!=b.size()){error="Truncated field map";return false;}return admit(std::move(b),id,error);
}
bool FieldMapView::load(const uint8_t* p,size_t size,const FieldIdentity& id,std::string& error) {
    if(!p||size<560||size>32*1024*1024){error="Field map input size rejected";return false;}
    return admit(std::vector<uint8_t>(p,p+size),id,error);
}
bool FieldMapView::admit(std::vector<uint8_t>&& candidate,const FieldIdentity& id,std::string& error) {
    const uint8_t* p=candidate.data();const size_t size=candidate.size();
    const auto reject=[&](const char* s){error=s;return false;};
    if(!p||size<560||size>32*1024*1024||std::memcmp(p,"ENCFMAP1",8)||u32(p+8)!=1||u32(p+12)!=128||u32(p+16)!=size||u32(p+20)!=18||u32(p+28)!=0x454e0019||u32(p+32)!=1||u32(p+36)!=id.scene_id||!id.scene_id)return reject("Field map header/version/source rejected");
    if(!std::equal(id.upstream_commit.begin(),id.upstream_commit.end(),p+40)||!std::equal(id.source_sha256.begin(),id.source_sha256.end(),p+60)||std::all_of(id.source_sha256.begin(),id.source_sha256.end(),[](uint8_t v){return v==0;}))return reject("Field map source identity rejected");
    if(std::all_of(p+92,p+124,[](uint8_t v){return v==0;})||crc(p+128,size-128)!=u32(p+24))return reject("Field map integrity rejected");
    size_t end=560;for(uint32_t k=1;k<=18;++k){auto d=p+128+(k-1)*24;const uint64_t bytes=uint64_t(u32(d+8))*strides[k];if(u32(d)!=k||u32(d+4)!=end||u32(d+12)!=strides[k]||u32(d+16)!=bytes||u32(d+20)||bytes>size-end)return reject("Field map section bounds/stride rejected");end+=static_cast<size_t>(bytes);}if(end!=size)return reject("Field map trailing data rejected");
    FieldMapView v;v.bytes_=std::move(candidate);std::unordered_set<uint32_t> ids;
    for(uint32_t i=0;i<v.count(1);++i){auto r=v.record(1,i);const uint32_t a=u32(r),n=u32(r+4);if(!range(a,n+1,v.count(2))||n==none||v.record(2,a+n)[0]||std::memchr(v.record(2,a),0,n))return reject("Field map string rejected");}
    const auto str=[&](uint32_t i){return i<v.count(1);};
    if(!str(u32(v.bytes_.data()+124))||v.source_scene().empty()||v.source_scene().front()=='/'||v.source_scene().find(':')!=std::string_view::npos||v.source_scene().find("..")!=std::string_view::npos||v.source_scene().find('\\')!=std::string_view::npos||v.source_scene().size()<5||v.source_scene().substr(v.source_scene().size()-5)!=".tscn")return reject("Field map source scene reference rejected");
    const auto gateidx=[&](uint32_t i){return i==none||i<v.gate_count();};
    for(uint32_t i=0;i<v.gate_count();++i){auto g=v.gate(i);if(!g.stable_id||!ids.insert(g.stable_id).second||!str(g.node)||!str(g.appear)||!str(g.disappear)||u32(v.record(10,i)+16)>1)return reject("Field map flag gate rejected");}
    ids.clear();for(uint32_t i=0;i<v.canvas_count();++i){auto c=v.canvas(i);if(!c.stable_id||!ids.insert(c.stable_id).second||!str(c.node)||(i==0?c.parent!=none:c.parent>=i)||(i==0?c.order!=0:c.order<=v.canvas(i-1).order)||c.flags&~15u||!gateidx(c.gate)||!finite(c.position)||c.z!=0)return reject("Field map canvas hierarchy rejected");}
    ids.clear();uint32_t nextcell=0;for(uint32_t i=0;i<v.map_count();++i){auto m=v.map(i);if(!m.stable_id||!ids.insert(m.stable_id).second||!str(m.node)||m.canvas>=v.canvas_count()||m.cell_first!=nextcell||!range(m.cell_first,m.cell_count,v.cell_count())||!range(m.draw_first,m.draw_count,v.draw_count())||!range(m.polygon_first,m.polygon_count,v.polygon_count())||m.flags&~3u||!finite(m.position)||!finite(m.cell_size)||m.cell_size.x<=0||m.cell_size.y<=0)return reject("Field map layer rejected");nextcell+=m.cell_count;for(uint32_t j=m.cell_first;j<nextcell;++j){auto c=v.cell(j);if(c.x<-32768||c.x>32767||c.y<-32768||c.y>32767||c.atlas_x<0||c.atlas_y<0||c.quadrant_ordinal<0||c.flags&~31u||!range(c.draw_first,c.draw_count,v.draw_count())||!range(c.polygon_first,c.polygon_count,v.polygon_count())||((c.flags&24)&&(c.draw_count||c.polygon_count)))return reject("Field map cell rejected");for(uint32_t n=c.draw_first;n<c.draw_first+c.draw_count;++n)if(v.draw(n).map!=i||v.draw(n).cell!=j)return reject("Field map cell draw owner rejected");for(uint32_t n=c.polygon_first;n<c.polygon_first+c.polygon_count;++n)if(v.polygon(n).map!=i||v.polygon(n).cell!=j)return reject("Field map cell collision owner rejected");}}
    if(nextcell!=v.cell_count()||!v.map_count())return reject("Field map incomplete cell ownership");
    uint32_t owned_draw=0,owned_polygon=0;for(uint32_t i=0;i<v.cell_count();++i){auto c=v.cell(i);if(c.draw_first!=owned_draw||c.polygon_first!=owned_polygon)return reject("Field map cell range gap/overlap rejected");owned_draw+=c.draw_count;owned_polygon+=c.polygon_count;}if(owned_draw!=v.draw_count()||owned_polygon!=v.polygon_count())return reject("Field map unowned primitive rejected");
    ids.clear();for(uint32_t i=0;i<v.texture_count();++i){auto t=v.texture(i);if(!t.stable_id||!ids.insert(t.stable_id).second||!str(t.source)||!str(t.path)||!t.width||t.width>1024||!t.height||t.height>1024||!t.frames||t.frames>64||t.frame>=t.frames||!range(t.group,t.frames,v.texture_count())||!std::isfinite(t.fps)||!std::isfinite(t.delay)||t.delay<0||(t.frames>1&&t.fps<=0)||!texture_path(v.string(t.path))||std::all_of(t.source_sha256.begin(),t.source_sha256.end(),[](uint8_t x){return !x;})||std::all_of(t.output_sha256.begin(),t.output_sha256.end(),[](uint8_t x){return !x;}))return reject("Field map texture/page rejected");if(t.frames>1){auto first=v.texture(t.group);if(first.group!=t.group||first.frame||first.frames!=t.frames||t.group+t.frame!=i||first.width!=t.width||first.height!=t.height||first.fps!=t.fps)return reject("Field map animated page sequence rejected");}}
    for(uint32_t i=0;i<v.draw_count();++i){auto d=v.draw(i);if(u32(v.record(5,i)+8)>=v.count(12)||d.map>=v.map_count()||d.cell>=v.cell_count()||d.texture>=v.texture_count()||d.flags&~7u||d.order!=i||d.gate!=none||d.z<-4096||d.z>4096||!finite(d.position)||!finite(d.signed_size)||d.signed_size.x==0||d.signed_size.y==0||!finite(d.uv_position)||!finite(d.uv_size)||d.uv_position.x<0||d.uv_position.y<0||d.uv_size.x<=0||d.uv_size.y<=0||d.uv_position.x+d.uv_size.x>v.texture(d.texture).width||d.uv_position.y+d.uv_size.y>v.texture(d.texture).height)return reject("Field map draw rejected");for(float x:d.color)if(!std::isfinite(x)||x<0||x>1)return reject("Field map color rejected");}
    uint32_t pointend=0;for(uint32_t i=0;i<v.polygon_count();++i){auto poly=v.polygon(i);if(poly.map>=v.map_count()||poly.cell>=v.cell_count()||(poly.kind!=1&&poly.kind!=2&&poly.kind!=3)||poly.point_first!=pointend||!range(poly.point_first,poly.point_count,v.count(7))||(poly.kind!=2?poly.point_count<3:poly.point_count%2||!poly.point_count)||poly.point_count>4096||poly.order!=i||poly.gate!=none||!bounds(poly.bounds)||poly.layer!=v.map(poly.map).layer)return reject("Field map collision record rejected");float sign=0;for(uint32_t j=0;j<poly.point_count;++j){auto q=v.point(pointend+j);if(!finite(q)||q.x<poly.bounds.minimum.x||q.y<poly.bounds.minimum.y||q.x>poly.bounds.maximum.x||q.y>poly.bounds.maximum.y)return reject("Field map collision point rejected");if(poly.kind!=2){auto a=v.point(pointend+(j+poly.point_count-2)%poly.point_count),b=v.point(pointend+(j+poly.point_count-1)%poly.point_count);const float cross=(b.x-a.x)*(q.y-b.y)-(b.y-a.y)*(q.x-b.x);if(cross){if(sign&&std::signbit(sign)!=std::signbit(cross))return reject("Field map nonconvex polygon rejected");sign=cross;}}}if(poly.kind==1&&!sign)return reject("Field map degenerate polygon rejected");if(poly.kind==3&&(sign||(poly.bounds.minimum.x==poly.bounds.maximum.x&&poly.bounds.minimum.y==poly.bounds.maximum.y)))return reject("Field map mislabeled degenerate polygon rejected");pointend+=poly.point_count;}if(pointend!=v.count(7))return reject("Field map point ownership rejected");
    uint32_t chunkcell=0;for(uint32_t i=0;i<v.count(11);++i){auto r=v.record(11,i);auto m=u32(r);const uint32_t first=u32(r+4),n=u32(r+8),df=u32(r+12),dn=u32(r+16),pf=u32(r+20),pn=u32(r+24);if(m>=v.map_count()||first!=chunkcell||!n||!range(first,n,v.cell_count())||!range(df,dn,v.draw_count())||!range(pf,pn,v.polygon_count())||!bounds({vec(r+28),vec(r+36)})||!range(first,n,v.map(m).cell_first+v.map(m).cell_count)||first<v.map(m).cell_first)return reject("Field map quadrant rejected");const auto a=v.cell(first),b=v.cell(first+n-1);if(df!=a.draw_first||dn!=b.draw_first+b.draw_count-df||pf!=a.polygon_first||pn!=b.polygon_first+b.polygon_count-pf)return reject("Field map quadrant primitive ownership rejected");const FieldMapRect box{vec(r+28),vec(r+36)};for(uint32_t j=df;j<df+dn;++j)if(!encloses(box,draw_bounds(v.draw(j))))return reject("Field map quadrant draw bounds rejected");for(uint32_t j=pf;j<pf+pn;++j)if(!encloses(box,v.polygon(j).bounds))return reject("Field map quadrant collision bounds rejected");chunkcell+=n;}if(chunkcell!=v.cell_count())return reject("Field map quadrant coverage rejected");
    if(v.count(13)!=uint64_t(v.count(11))*2-1)return reject("Field map spatial index coverage rejected");
    std::vector<uint8_t> parents(v.count(13)),leaves(v.count(11)),depth(v.count(13));depth[0]=1;
    for(uint32_t i=0;i<v.count(13);++i){auto r=v.record(13,i);const uint32_t left=u32(r),right=u32(r+4),leaf=u32(r+8);const FieldMapRect box{vec(r+16),vec(r+24)};if(u32(r+12)||!bounds(box)||!depth[i])return reject("Field map spatial node rejected");if(leaf!=none){if(left!=none||right!=none||leaf>=v.count(11)||leaves[leaf]++)return reject("Field map duplicate spatial leaf rejected");auto c=v.record(11,leaf);if(!encloses(box,{vec(c+28),vec(c+36)}))return reject("Field map spatial leaf bounds rejected");}else{if(left<=i||right<=i||left>=v.count(13)||right>=v.count(13)||left==right||depth[i]>=63||parents[left]++||parents[right]++)return reject("Field map spatial parent/cycle rejected");depth[left]=depth[right]=depth[i]+1;for(auto child:{left,right}){auto c=v.record(13,child);if(!encloses(box,{vec(c+16),vec(c+24)}))return reject("Field map spatial child bounds rejected");}}}
    for(uint32_t i=1;i<v.count(13);++i)if(parents[i]!=1)return reject("Field map disconnected spatial index rejected");
    for(auto leaf:leaves)if(leaf!=1)return reject("Field map missing spatial leaf rejected");
    uint32_t localend=0,bvhend=0,leafend=0;
    for(uint32_t i=0;i<v.count(14);++i){auto g=v.local_geometry(i);if((g.kind!=1&&g.kind!=2&&g.kind!=3)||g.point_first!=localend||g.bvh_first!=bvhend||g.leaf_first!=leafend||!range(g.point_first,g.point_count,v.count(15))||!range(g.bvh_first,g.bvh_count,v.count(16))||!range(g.leaf_first,g.leaf_count,v.count(17))||(g.kind==2?g.point_count<2||g.point_count%2:g.point_count<3))return reject("Field map local geometry rejected");
        for(uint32_t j=0;j<g.point_count;++j)if(!finite(v.local_point(g.point_first+j)))return reject("Field map local point rejected");
        if(g.kind!=2){if(g.bvh_count||g.leaf_count)return reject("Field map convex BVH rejected");}
        else{const uint32_t pairs=g.point_count/2;if(g.bvh_count!=pairs*2-1||g.leaf_count!=pairs)return reject("Field map concave BVH coverage rejected");std::vector<uint8_t> parents(g.bvh_count),leaves(pairs),depth(g.bvh_count);depth[0]=1;
            for(uint32_t j=0;j<g.bvh_count;++j){auto n=v.concave_node(g.bvh_first+j);if(!bounds(n.bounds)||u32(v.record(16,g.bvh_first+j)+12)||!depth[j])return reject("Field map native BVH node rejected");if(n.segment!=none){if(n.left!=none||n.right!=none||n.segment>=pairs||leaves[n.segment]++)return reject("Field map native BVH pair identity rejected");auto a=v.local_point(g.point_first+n.segment*2),b=v.local_point(g.point_first+n.segment*2+1);const Vec2 lo{std::min(a.x,b.x),std::min(a.y,b.y)},hi{std::max(a.x,b.x),std::max(a.y,b.y)};const Vec2 extent{hi.x-lo.x,hi.y-lo.y};if(n.bounds.minimum.x!=lo.x||n.bounds.minimum.y!=lo.y||n.bounds.maximum.x!=lo.x+extent.x||n.bounds.maximum.y!=lo.y+extent.y)return reject("Field map native leaf/source endpoints rejected");}
                else{if(n.left<=g.bvh_first+j||n.right<=g.bvh_first+j||n.left>=g.bvh_first+g.bvh_count||n.right>=g.bvh_first+g.bvh_count||n.left==n.right||depth[j]>=63||parents[n.left-g.bvh_first]++||parents[n.right-g.bvh_first]++)return reject("Field map native BVH parent/cycle rejected");depth[n.left-g.bvh_first]=depth[n.right-g.bvh_first]=depth[j]+1;for(auto child:{n.left,n.right})if(!encloses(n.bounds,v.concave_node(child).bounds))return reject("Field map native BVH child bounds rejected");}}
            for(uint32_t j=1;j<g.bvh_count;++j){if(parents[j]!=1)return reject("Field map disconnected native BVH rejected");}
            for(auto leaf:leaves){if(leaf!=1)return reject("Field map missing native segment rejected");}
            std::array<uint32_t,64> stack{};size_t used=1;stack[0]=g.bvh_first;uint32_t ordinal=0;while(used){auto n=v.concave_node(stack[--used]);if(n.segment!=none){if(v.concave_leaf(g.leaf_first+ordinal++)!=n.segment)return reject("Field map native leaf traversal order rejected");}else{if(used+2>stack.size())return reject("Field map native traversal depth rejected");stack[used++]=n.right;stack[used++]=n.left;}}if(ordinal!=g.leaf_count)return reject("Field map native traversal coverage rejected");
        }
        localend+=g.point_count;bvhend+=g.bvh_count;leafend+=g.leaf_count;
    }
    if(localend!=v.count(15)||bvhend!=v.count(16)||leafend!=v.count(17)||v.count(18)!=v.polygon_count())return reject("Field map local geometry ownership rejected");
    for(uint32_t i=0;i<v.polygon_count();++i){auto p=v.polygon(i);if(p.geometry>=v.count(14)||p.transform!=i)return reject("Field map shape transform owner rejected");auto g=v.local_geometry(p.geometry);auto t=v.shape_transform(p.transform);const float det=t.x.x*t.y.y-t.x.y*t.y.x;if(g.kind!=p.kind||g.point_count!=p.point_count||!finite(t.x)||!finite(t.y)||!finite(t.origin)||!std::isfinite(det)||det==0)return reject("Field map local/world shape identity rejected");for(uint32_t j=0;j<p.point_count;++j){auto a=transformed(t,v.local_point(g.point_first+j)),b=v.point(p.point_first+j);if(a.x!=b.x||a.y!=b.y)return reject("Field map local/world point correspondence rejected");}}
    bytes_.swap(v.bytes_);error.clear();return true;
}
bool FieldMapView::map_active(uint32_t m,const FieldMapGateQuery& query,bool drawing,bool& active,std::string& error) const {
    if(m>=map_count()){error="Unknown field map";return false;}active=true;uint32_t ci=map(m).canvas;
    while(ci!=none){auto c=canvas(ci);if(drawing&&!(c.flags&1))active=false;if(c.gate!=none){if(!query){error="Field map flag lifecycle requires consumer";return false;}const auto state=query(gate(c.gate).stable_id);if(state==FieldMapGateState::Pending){error="Field map flag Ready not executed";return false;}if(state==FieldMapGateState::Deleted||(drawing&&state==FieldMapGateState::Hidden))active=false;}ci=c.parent;}return true;
}
bool FieldMapView::spatial_chunks(FieldMapRect area,std::vector<uint32_t>& output,std::string& error) const {
    std::array<uint32_t,64> stack{};size_t used=1;stack[0]=0;
    while(used){auto r=record(13,stack[--used]);if(!r){error="Field spatial index unavailable";return false;}if(!intersects(area,{vec(r+16),vec(r+24)}))continue;const auto chunk=u32(r+8);if(chunk!=none)output.push_back(chunk);else{if(used+2>stack.size()){error="Field spatial traversal capacity exceeded";return false;}stack[used++]=u32(r+4);stack[used++]=u32(r);}}
    std::sort(output.begin(),output.end());return true;
}
bool FieldMapView::collect_draws(FieldMapRect area,const FieldMapGateQuery& gates,size_t capacity,std::vector<uint32_t>& output,std::string& error) const {
    if(!valid()||!bounds(area)){error="Invalid field map draw query";return false;}std::vector<uint32_t> found;std::vector<uint8_t> enabled(map_count());
    for(uint32_t i=0;i<map_count();++i){bool active;if(!map_active(i,gates,true,active,error))return false;enabled[i]=active;}
    return collect_draws_masked(area,enabled,capacity,output,error);
}
bool FieldMapView::collect_draws_masked(FieldMapRect area,const std::vector<uint8_t>&enabled,size_t capacity,std::vector<uint32_t>&output,std::string&error)const{
 if(!valid()||!bounds(area)||enabled.size()!=map_count()||std::any_of(enabled.begin(),enabled.end(),[](uint8_t v){return v>1;})){error="Field live map query/mask rejected";return false;}std::vector<uint32_t>found;
    std::vector<uint32_t> candidates;if(!spatial_chunks(area,candidates,error))return false;
    for(uint32_t i:candidates){auto r=record(11,i);if(!enabled[u32(r)]||!intersects(area,{vec(r+28),vec(r+36)}))continue;for(uint32_t j=u32(r+12),end=j+u32(r+16);j<end;++j)if(intersects(area,draw_bounds(draw(j)))){if(found.size()==capacity){error="Field map visible draw capacity exceeded";return false;}found.push_back(j);}}
    output.swap(found);error.clear();return true;
}
bool FieldMapView::collision_polygons(FieldMapRect area,uint32_t mask,const FieldMapGateQuery& gates,size_t capacity,std::vector<uint32_t>& output,std::string& error) const {
    if(!valid()||!bounds(area)){error="Invalid field map collision query";return false;}std::vector<uint32_t> found;std::vector<uint8_t> enabled(map_count());for(uint32_t i=0;i<map_count();++i){bool active;if(!map_active(i,gates,false,active,error))return false;enabled[i]=active&&(map(i).layer&mask);}
    return collision_polygons_masked(area,enabled,capacity,output,error);
}
bool FieldMapView::collision_polygons_masked(FieldMapRect area,const std::vector<uint8_t>&enabled,size_t capacity,std::vector<uint32_t>&output,std::string&error)const{
 if(!valid()||!bounds(area)||enabled.size()!=map_count()||std::any_of(enabled.begin(),enabled.end(),[](uint8_t v){return v>1;})){error="Field live map query/mask rejected";return false;}std::vector<uint32_t>found;
    std::vector<uint32_t> candidates;if(!spatial_chunks(area,candidates,error))return false;
    for(uint32_t i:candidates){auto r=record(11,i);if(!enabled[u32(r)]||!intersects(area,{vec(r+28),vec(r+36)}))continue;for(uint32_t j=u32(r+20),end=j+u32(r+24);j<end;++j)if(intersects(area,polygon(j).bounds)){if(found.size()==capacity){error="Field map collision capacity exceeded";return false;}found.push_back(j);}}
    output.swap(found);error.clear();return true;
}
Vec2 FieldMapView::sort_anchor(uint32_t i) const { auto d=draw(i);auto m=map(d.map);auto c=cell(d.cell);return m.flags&1?Vec2{m.position.x+c.x*m.cell_size.x,m.position.y+c.y*m.cell_size.y+((m.flags&2)?m.cell_size.y:0)}:m.position; }
bool FieldMapView::concave_segments(uint32_t pi,FieldMapRect area,size_t capacity,std::vector<uint32_t>& output,std::string& error) const {
    if(pi>=polygon_count()||polygon(pi).kind!=2||!bounds(area)){error="Invalid field native concave query";return false;}auto g=local_geometry(polygon(pi).geometry);std::array<uint32_t,64> stack{};size_t used=1;stack[0]=g.bvh_first;std::vector<uint32_t> found;
    while(used){auto n=concave_node(stack[--used]);if(!native_intersects(area,n.bounds))continue;if(n.segment!=none){if(found.size()==capacity){error="Field native concave capacity exceeded";return false;}found.push_back(n.segment);}else{if(used+2>stack.size()){error="Field native concave depth exceeded";return false;}stack[used++]=n.right;stack[used++]=n.left;}}
    output.swap(found);error.clear();return true;
}
bool FieldMapView::start_animation(uint32_t i,FieldMapAnimationState& state,std::string& error) const {
    if(i>=texture_count()){error="Unknown field texture animation";return false;}auto t=texture(i);state={t.frames==1?i:t.group,0,0,true};error.clear();return true;
}
bool FieldMapView::advance_animation(FieldMapAnimationState& state,float delta,std::string& error) const {
    if(state.group>=texture_count()||!std::isfinite(delta)||delta<0||!std::isfinite(state.time)||state.time<0){error="Invalid field animation tick";return false;}auto t=texture(state.group);if(state.frame>=t.frames||(t.frames>1&&t.group!=state.group)){error="Field animation frame rejected";return false;}
    auto next=state;
    if(next.first_draw){next.first_draw=false;state=next;error.clear();return true;}
    if(t.frames==1){error.clear();return true;}
    next.time+=delta;if(!std::isfinite(next.time)){error="Field animation time overflow rejected";return false;}
    for(uint32_t i=0;i<t.frames;++i){const float limit=(t.fps==0?0:1.0f/t.fps)+texture(next.group+next.frame).delay;if(!std::isfinite(limit)||limit<0){error="Field animation limit rejected";return false;}if(next.time<=limit)break;next.frame=(next.frame+1)%t.frames;next.time-=limit;}
    state=next;error.clear();return true;
}
}
