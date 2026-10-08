#include "encore/field_data.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>

namespace encore::upstream {
namespace {
constexpr size_t header_bytes=64,max_bytes=8u*1024u*1024u;
constexpr uint32_t strides[kFieldMapSectionCount]={1,16,12,16,12,32,40,8,12,48,32,8,32,32,16,56,48,48,40,56,16};
constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint16_t u16(const uint8_t*p){return uint16_t(p[0]|p[1]<<8);}
int16_t i16(const uint8_t*p){return int16_t(u16(p));}
float f32(const uint8_t*p){const uint32_t v=u32(p);float f;std::memcpy(&f,&v,4);return f;}
Vec2 vec(const uint8_t*p){return {f32(p),f32(p+4)};}
FieldText text_at(const uint8_t*p){return {u32(p),u32(p+4)};}
FieldSpan span_at(const uint8_t*p){return {u32(p),u32(p+4)};}
bool finite(Vec2 v,float limit=1000000.f){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::fabs(v.x)<=limit&&std::fabs(v.y)<=limit;}
bool utf8(const uint8_t*p,size_t n){for(size_t i=0;i<n;){uint32_t c=p[i++];if(!c)return false;if(c<0x80)continue;unsigned extra;uint32_t low;
    if(c>=0xc2&&c<=0xdf){extra=1;low=0x80;c&=31;}else if(c>=0xe0&&c<=0xef){extra=2;low=0x800;c&=15;}else if(c>=0xf0&&c<=0xf4){extra=3;low=0x10000;c&=7;}else return false;
    if(extra>n-i)return false;while(extra--){const auto b=p[i++];if((b&0xc0)!=0x80)return false;c=(c<<6)|(b&63);}
    if(c<low||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;}return true;}
bool safe_path(std::string_view p){if(p.empty()||p.size()>256||p.front()=='/'||p.find(':')!=p.npos||p.find('\\')!=p.npos)return false;
    size_t a=0;while(a<=p.size()){auto b=p.find('/',a);if(b==p.npos)b=p.size();const auto s=p.substr(a,b-a);if(s.empty()||s=="."||s=="..")return false;
    for(unsigned char ch:s)if(ch<33||ch>126)return false;if(b==p.size())break;a=b+1;}return true;}
bool pow2(uint32_t v){return v>=8&&v<=1024&&(v&(v-1))==0;}
}
uint32_t FieldMapView::scene_id()const{return bytes_?u32(bytes_+24):0;}
uint32_t FieldMapView::count(FieldMapSection s)const{const auto k=uint32_t(s);return bytes_&&k>=1&&k<=kFieldMapSectionCount?u32(bytes_+header_bytes+(k-1)*16+8):0;}
const uint8_t*FieldMapView::record(FieldMapSection s,uint32_t i)const{const auto k=uint32_t(s);if(!bytes_||k<1||k>kFieldMapSectionCount||i>=count(s))return nullptr;
    return bytes_+u32(bytes_+header_bytes+(k-1)*16+4)+size_t(i)*strides[k-1];}
std::string_view FieldMapView::text(FieldText t)const{const auto pool=count(FieldMapSection::Bytes);if(!t.length||t.offset>pool||t.length>pool-t.offset)return {};
    return std::string_view(reinterpret_cast<const char*>(record(FieldMapSection::Bytes,0))+t.offset,t.length);}
FieldAtlas FieldMapView::atlas(uint32_t i)const{auto*p=record(FieldMapSection::Atlases,i);return p?FieldAtlas{text_at(p),u16(p+8),u16(p+10)}:FieldAtlas{};}
FieldFrame FieldMapView::frame(uint32_t i)const{auto*p=record(FieldMapSection::Frames,i);return p?FieldFrame{u16(p),u16(p+2),u16(p+4),u16(p+6),u16(p+8)}:FieldFrame{};}
FieldPicture FieldMapView::picture(uint32_t i)const{auto*p=record(FieldMapSection::Pictures,i);return p?FieldPicture{u32(p),u16(p+4),u16(p+6),f32(p+8),u32(p+12)}:FieldPicture{};}
FieldVariant FieldMapView::variant(uint32_t i)const{auto*p=record(FieldMapSection::Variants,i);return p?FieldVariant{text_at(p),u32(p+8)}:FieldVariant{};}
FieldTile FieldMapView::tile(uint32_t i)const{auto*p=record(FieldMapSection::Tiles,i);return p?FieldTile{u32(p),u16(p+4),u16(p+6),vec(p+8),int32_t(u32(p+16)),u32(p+20),u32(p+24),u32(p+28)}:FieldTile{};}
FieldShape FieldMapView::shape(uint32_t i)const{auto*p=record(FieldMapSection::Shapes,i);return p?FieldShape{u16(p),u16(p+2),u32(p+4),vec(p+8),vec(p+16),vec(p+24)}:FieldShape{};}
Vec2 FieldMapView::point(uint32_t i)const{auto*p=record(FieldMapSection::Points,i);return p?vec(p):Vec2{};}
FieldCondition FieldMapView::condition(uint32_t i)const{auto*p=record(FieldMapSection::Conditions,i);return p?FieldCondition{text_at(p),p[8]!=0}:FieldCondition{};}
FieldLayer FieldMapView::layer(uint32_t i)const{auto*p=record(FieldMapSection::Layers,i);return p?FieldLayer{text_at(p),u32(p+8),span_at(p+12),vec(p+20),u32(p+28),u32(p+32),f32(p+36)}:FieldLayer{};}
FieldChunk FieldMapView::chunk(uint32_t i)const{auto*p=record(FieldMapSection::Chunks,i);return p?FieldChunk{i16(p),i16(p+2),u32(p+4),u32(p+8),vec(p+12),vec(p+20)}:FieldChunk{};}
FieldCell FieldMapView::cell(uint32_t i)const{auto*p=record(FieldMapSection::Cells,i);return p?FieldCell{i16(p),i16(p+2),u16(p+4),u16(p+6)}:FieldCell{};}
FieldSprite FieldMapView::sprite(uint32_t i)const{auto*p=record(FieldMapSection::Sprites,i);return p?FieldSprite{u32(p),vec(p+4),u16(p+12),u16(p+14),u16(p+16),u16(p+18),span_at(p+20)}:FieldSprite{};}
FieldItem FieldMapView::item(uint32_t i)const{auto*p=record(FieldMapSection::Items,i);return p?FieldItem{u16(p),u32(p+4),u32(p+8),f32(p+12),u32(p+16),span_at(p+20)}:FieldItem{};}
FieldGroup FieldMapView::group(uint32_t i)const{auto*p=record(FieldMapSection::Groups,i);return p?FieldGroup{u32(p),u32(p+4),u32(p+8)}:FieldGroup{};}
FieldOpenable FieldMapView::openable(uint32_t i)const{auto*p=record(FieldMapSection::Openables,i);return p?FieldOpenable{text_at(p),vec(p+8),vec(p+16),text_at(p+24),text_at(p+32),f32(p+40),u32(p+44),span_at(p+48)}:FieldOpenable{};}
FieldDoor FieldMapView::door(uint32_t i)const{auto*p=record(FieldMapSection::Doors,i);return p?FieldDoor{text_at(p),text_at(p+8),u32(p+16),vec(p+20),vec(p+28),span_at(p+36)}:FieldDoor{};}
FieldBoundary FieldMapView::boundary(uint32_t i)const{auto*p=record(FieldMapSection::Boundaries,i);return p?FieldBoundary{u16(p),text_at(p+4),text_at(p+12),vec(p+20),vec(p+28),span_at(p+36)}:FieldBoundary{};}
FieldNotice FieldMapView::notice(uint32_t i)const{auto*p=record(FieldMapSection::Notices,i);return p?FieldNotice{u16(p),text_at(p+4),text_at(p+12),vec(p+20),span_at(p+28)}:FieldNotice{};}
FieldCamera FieldMapView::camera(uint32_t i)const{auto*p=record(FieldMapSection::Cameras,i);return p?FieldCamera{vec(p),vec(p+8),f32(p+16),f32(p+20),f32(p+24),f32(p+28),vec(p+32),span_at(p+40),u32(p+48)!=0}:FieldCamera{};}
FieldBody FieldMapView::body(uint32_t i)const{auto*p=record(FieldMapSection::Bodies,i);return p?FieldBody{u32(p),u32(p+4),span_at(p+8)}:FieldBody{};}
uint32_t FieldMapView::localized_picture(uint32_t index,std::string_view locale)const{
    const auto pic=picture(index);
    for(uint32_t i=0;i<pic.variant_count;++i){const auto v=variant(pic.variant_first+i);if(text(v.locale)==locale)return v.picture;}
    return index;
}

bool FieldMapData::load(const uint8_t*p,size_t n,std::string&error){
    auto fail=[&](const char*m){error=m;return false;};
    if(!p||n<header_bytes+kFieldMapSectionCount*16||n>max_bytes)return fail("Field map size rejected");
    if(std::memcmp(p,"ENCMAP01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+28)||u32(p+52)!=kFieldMapSectionCount||u32(p+56)||u32(p+60))
        return fail("Field map schema/capability/reserved rejected");
    if(!u32(p+24))return fail("Field map scene identity rejected");
    for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail("Field map source pin rejected");}
    if(encore::crc32(p+32,n-32)!=u32(p+16))return fail("Field map checksum rejected");
    size_t cursor=header_bytes+kFieldMapSectionCount*16;
    for(uint32_t i=0;i<kFieldMapSectionCount;++i){const auto*d=p+header_bytes+i*16;const auto off=u32(d+4),count=u32(d+8);
        if(u32(d)!=i+1||u32(d+12)!=strides[i])return fail("Field map directory rejected");
        if(!count){if(off)return fail("Field map empty section rejected");continue;}
        if(off%4||off<cursor||off>n||count>(n-off)/strides[i])return fail("Field map section span rejected");
        for(size_t j=cursor;j<off;++j)if(p[j])return fail("Field map padding rejected");
        cursor=size_t(off)+size_t(count)*strides[i];}
    if(cursor!=n)return fail("Field map trailing bytes rejected");
    FieldMapView v;v.bytes_=p;v.size_=n;
    using S=FieldMapSection;
    const uint32_t pool=v.count(S::Bytes);
    auto text_ok=[&](FieldText t,bool empty,size_t limit=4096){if(!t.length)return empty&&!t.offset;
        return t.offset<=pool&&t.length<=pool-t.offset&&t.length<=limit&&utf8(v.record(S::Bytes,0)+t.offset,t.length);};
    auto span_ok=[&](FieldSpan s,S section){return s.first<=v.count(section)&&s.count<=v.count(section)-s.first;};
    auto cond_ok=[&](FieldSpan s){return s.count<=16&&span_ok(s,S::Conditions);};
    const auto atlases=v.count(S::Atlases),frames=v.count(S::Frames),pictures=v.count(S::Pictures),variants=v.count(S::Variants);
    const auto tiles=v.count(S::Tiles),shapes=v.count(S::Shapes),layers=v.count(S::Layers),chunks=v.count(S::Chunks),cells=v.count(S::Cells);
    const auto sprites=v.count(S::Sprites),items=v.count(S::Items),groups=v.count(S::Groups),openables=v.count(S::Openables);
    if(!pool||pool>1024u*1024u||!atlases||atlases>16||!frames||frames>65536||!pictures||pictures>65536||variants>65536||!tiles||tiles>65535||
       shapes>65536||!layers||layers>256||!chunks||chunks>65536||!cells||cells>1048576||sprites>65535||!items||items>65536||!groups||groups>4096||openables>256)
        return fail("Field map capacity rejected");
    for(uint32_t i=0;i<atlases;++i){const auto a=v.atlas(i);const auto path=v.text(a.path);
        if(!text_ok(a.path,false,256)||!safe_path(path)||path.size()<5||path.substr(path.size()-4)!=".t3x"||!pow2(a.width)||!pow2(a.height)||u32(v.record(S::Atlases,i)+12))
            return fail("Field map atlas rejected");}
    for(uint32_t i=0;i<frames;++i){const auto f=v.frame(i);const auto a=v.atlas(f.atlas);
        if(f.atlas>=atlases||!f.w||!f.h||f.x+f.w>a.width||f.y+f.h>a.height||u16(v.record(S::Frames,i)+10))return fail("Field map frame rejected");}
    for(uint32_t i=0;i<pictures;++i){const auto pic=v.picture(i);
        if(!pic.frame_count||pic.frame_count>64||pic.first_frame>frames||pic.frame_count>frames-pic.first_frame||!std::isfinite(pic.fps)||pic.fps<0||pic.fps>120||(pic.frame_count>1&&pic.fps<=0))
            return fail("Field map picture frames rejected");
        const auto base=v.frame(pic.first_frame);
        for(uint32_t f=1;f<pic.frame_count;++f){const auto fr=v.frame(pic.first_frame+f);if(fr.w!=base.w||fr.h!=base.h)return fail("Field map picture frame size rejected");}
        if(pic.variant_count>8||(!pic.variant_count&&pic.variant_first!=kFieldNone)||(pic.variant_count&&(pic.variant_first>variants||pic.variant_count>variants-pic.variant_first)))
            return fail("Field map picture variants rejected");
        for(uint32_t k=0;k<pic.variant_count;++k){const auto var=v.variant(pic.variant_first+k);
            if(!text_ok(var.locale,false,32)||var.picture>=pictures||var.picture==i)return fail("Field map variant rejected");
            const auto other=v.picture(var.picture);if(other.variant_count)return fail("Field map nested variant rejected");
            const auto of=v.frame(other.first_frame);if(other.first_frame>=frames||of.w!=base.w||of.h!=base.h)return fail("Field map variant size rejected");}
    }
    for(uint32_t i=0;i<shapes;++i){const auto s=v.shape(i);const auto*raw=v.record(S::Shapes,i);
        if(u32(raw+32)||u32(raw+36)||s.first>v.count(S::Points)||s.count>v.count(S::Points)-s.first)return fail("Field map shape span rejected");
        if(s.kind==uint16_t(FieldShapeKind::Convex)){if(s.count<2||s.count>64||s.normal.x!=0||s.normal.y!=0)return fail("Field map convex shape rejected");}
        else if(s.kind==uint16_t(FieldShapeKind::Segment)){const float len=std::sqrt(s.normal.x*s.normal.x+s.normal.y*s.normal.y);if(s.count!=2||!finite(s.normal,2)||std::fabs(len-1)>1e-3f)return fail("Field map segment shape rejected");}
        else return fail("Field map shape kind rejected");
        Vec2 lo{INFINITY,INFINITY},hi{-INFINITY,-INFINITY};
        for(uint32_t k=0;k<s.count;++k){const auto q=v.point(s.first+k);if(!finite(q))return fail("Field map point rejected");
            lo.x=std::fmin(lo.x,q.x);lo.y=std::fmin(lo.y,q.y);hi.x=std::fmax(hi.x,q.x);hi.y=std::fmax(hi.y,q.y);}
        if(lo.x!=s.minimum.x||lo.y!=s.minimum.y||hi.x!=s.maximum.x||hi.y!=s.maximum.y)return fail("Field map shape bounds rejected");}
    for(uint32_t i=0;i<tiles;++i){const auto t=v.tile(i);const auto pic=v.picture(t.picture);const auto f=v.frame(pic.first_frame);
        if(t.picture>=pictures||f.w!=t.w||f.h!=t.h||!finite(t.offset,4096)||t.z<-4096||t.z>4096||t.flags>3||t.shape_first>shapes||t.shape_count>shapes-t.shape_first)
            return fail("Field map tile rejected");}
    for(uint32_t i=0;i<v.count(S::Conditions);++i){const auto c=v.condition(i);const auto*raw=v.record(S::Conditions,i);
        if(!text_ok(c.flag,false,64)||raw[8]>1||raw[9]||raw[10]||raw[11])return fail("Field map condition rejected");}
    uint32_t chunk_cursor=0,cell_cursor=0;
    for(uint32_t i=0;i<layers;++i){const auto l=v.layer(i);const auto*raw=v.record(S::Layers,i);
        if(!text_ok(l.name,false,512)||l.flags>7||!cond_ok(l.conditions)||!finite(l.position)||l.chunk_first!=chunk_cursor||l.chunk_count>chunks-chunk_cursor||
           !std::isfinite(l.origin_y)||l.origin_y<0||l.origin_y>64||u32(raw+40)||u32(raw+44))return fail("Field map layer rejected");
        const bool ysort=l.flags&uint32_t(FieldLayerFlag::YSort);
        std::vector<bool> orders(ysort?65536:0,false);
        for(uint32_t c=0;c<l.chunk_count;++c){const auto ch=v.chunk(l.chunk_first+c);const auto*craw=v.record(S::Chunks,l.chunk_first+c);
            if(c){const auto prev=v.chunk(l.chunk_first+c-1);if(ch.cy<prev.cy||(ch.cy==prev.cy&&ch.cx<=prev.cx))return fail("Field map chunk order rejected");}
            if(ch.first!=cell_cursor||!ch.count||ch.count>256||ch.count>cells-cell_cursor||u32(craw+28)||!finite(ch.minimum)||!finite(ch.maximum)||ch.minimum.x>ch.maximum.x||ch.minimum.y>ch.maximum.y)
                return fail("Field map chunk rejected");
            for(uint32_t k=0;k<ch.count;++k){const auto cell=v.cell(ch.first+k);
                if(cell.tile>=tiles||int(cell.x)<ch.cx*16||int(cell.x)>=ch.cx*16+16||int(cell.y)<ch.cy*16||int(cell.y)>=ch.cy*16+16)return fail("Field map cell rejected");
                if(k){const auto prev=v.cell(ch.first+k-1);if(cell.y<prev.y||(cell.y==prev.y&&cell.x<=prev.x))return fail("Field map cell order rejected");}
                if(ysort){if(orders[cell.order])return fail("Field map duplicate sort order rejected");orders[cell.order]=true;}else if(cell.order)return fail("Field map unsorted cell order rejected");
                const auto t=v.tile(cell.tile);
                for(uint32_t s=0;s<t.shape_count;++s){const auto sh=v.shape(t.shape_first+s);const float bx=l.position.x+float(cell.x)*16,by=l.position.y+float(cell.y)*16;
                    if(bx+sh.minimum.x<ch.minimum.x-.01f||by+sh.minimum.y<ch.minimum.y-.01f||bx+sh.maximum.x>ch.maximum.x+.01f||by+sh.maximum.y>ch.maximum.y+.01f)return fail("Field map chunk bounds rejected");}
            }
            cell_cursor+=ch.count;}
        chunk_cursor+=l.chunk_count;}
    if(chunk_cursor!=chunks||cell_cursor!=cells)return fail("Field map unowned chunks/cells rejected");
    for(uint32_t i=0;i<sprites;++i){const auto s=v.sprite(i);const auto pic=v.picture(s.picture);const auto f=v.frame(pic.first_frame);
        if(s.picture>=pictures||f.w!=s.w||f.h!=s.h||!finite(s.position)||s.flags>3||(s.openable!=kFieldNoOpenable&&s.openable>=openables)||!cond_ok(s.conditions)||u32(v.record(S::Sprites,i)+28))
            return fail("Field map sprite rejected");}
    uint32_t players=0;
    for(uint32_t i=0;i<items;++i){const auto it=v.item(i);const auto*raw=v.record(S::Items,i);
        if(u16(raw+2)||u32(raw+28)||!std::isfinite(it.sort_y)||std::fabs(it.sort_y)>1000000||!cond_ok(it.conditions))return fail("Field map item rejected");
        switch(FieldItemKind(it.kind)){
        case FieldItemKind::LayerCells:case FieldItemKind::SortedLayer:
            if(it.first>=layers||it.count!=1||(v.layer(it.first).flags&uint32_t(FieldLayerFlag::YSort)))return fail("Field map layer item rejected");break;
        case FieldItemKind::SortedLayerCells:
            if(it.first>=layers||it.count!=1||!(v.layer(it.first).flags&uint32_t(FieldLayerFlag::YSort)))return fail("Field map sorted layer item rejected");break;
        case FieldItemKind::Sprites:
            if(!it.count||it.first>sprites||it.count>sprites-it.first)return fail("Field map sprite item rejected");break;
        case FieldItemKind::Player:
            if(it.first||it.count)return fail("Field map player item rejected");++players;break;
        default:return fail("Field map item kind rejected");}
    }
    uint32_t item_cursor=0;bool player_sorted=false;
    for(uint32_t i=0;i<groups;++i){const auto g=v.group(i);
        if((g.kind!=uint32_t(FieldGroupKind::Static)&&g.kind!=uint32_t(FieldGroupKind::YSort))||g.first!=item_cursor||!g.count||g.count>items-item_cursor||u32(v.record(S::Groups,i)+12))
            return fail("Field map group rejected");
        for(uint32_t k=0;k<g.count;++k){const auto kind=FieldItemKind(v.item(g.first+k).kind);
            if(g.kind==uint32_t(FieldGroupKind::Static)&&kind!=FieldItemKind::LayerCells&&kind!=FieldItemKind::Sprites)return fail("Field map static group item rejected");
            if(g.kind==uint32_t(FieldGroupKind::YSort)&&kind==FieldItemKind::LayerCells)return fail("Field map sort group item rejected");
            if(kind==FieldItemKind::Player)player_sorted=true;}
        item_cursor+=g.count;}
    if(item_cursor!=items||players!=1||!player_sorted)return fail("Field map draw program rejected");
    for(uint32_t i=0;i<openables;++i){const auto o=v.openable(i);
        auto audio=[&](FieldText t){return text_ok(t,true,256)&&(!t.length||v.text(t).substr(0,6)=="res://");};
        if(!text_ok(o.path,false,512)||!finite(o.center)||!finite(o.extents)||o.extents.x<=0||o.extents.y<=0||!audio(o.sound)||!audio(o.end_sound)||
           !std::isfinite(o.timer)||o.timer<=0||o.timer>60||o.sprite>=sprites||v.sprite(o.sprite).openable!=i||!cond_ok(o.conditions))return fail("Field map openable door rejected");}
    for(uint32_t i=0;i<v.count(S::Doors);++i){const auto d=v.door(i);
        if(!text_ok(d.path,false,512)||!text_ok(d.target,true,512)||(d.target.length&&v.text(d.target).substr(0,6)!="res://")||!finite(d.center)||!finite(d.extents)||
           d.extents.x<=0||d.extents.y<=0||!cond_ok(d.conditions)||u32(v.record(S::Doors,i)+44))return fail("Field map door rejected");}
    for(uint32_t i=0;i<v.count(S::Boundaries);++i){const auto b=v.boundary(i);const auto*raw=v.record(S::Boundaries,i);
        if((b.kind!=1&&b.kind!=2)||u16(raw+2)||!text_ok(b.path,false,512)||!text_ok(b.label,false,128)||!finite(b.center)||!finite(b.extents)||b.extents.x<=0||b.extents.y<=0||
           !cond_ok(b.conditions)||u32(raw+44))return fail("Field map boundary rejected");}
    for(uint32_t i=0;i<v.count(S::Notices);++i){const auto b=v.notice(i);const auto*raw=v.record(S::Notices,i);
        if((b.kind!=1&&b.kind!=2)||u16(raw+2)||!text_ok(b.path,false,512)||!text_ok(b.label,false,128)||!finite(b.position)||!cond_ok(b.conditions)||u32(raw+36))
            return fail("Field map notice rejected");}
    for(uint32_t i=0;i<v.count(S::Cameras);++i){const auto c=v.camera(i);const auto*raw=v.record(S::Cameras,i);
        if(!finite(c.center)||!finite(c.extents)||c.extents.x<=0||c.extents.y<=0||!finite({c.left,c.top})||!finite({c.width,c.height})||c.width<=0||c.height<=0||
           !finite(c.offset)||!cond_ok(c.conditions)||u32(raw+48)>1||u32(raw+52))return fail("Field map camera area rejected");}
    for(uint32_t i=0;i<v.count(S::Bodies);++i){const auto b=v.body(i);
        if(!b.shape_count||b.shape_first>shapes||b.shape_count>shapes-b.shape_first||!cond_ok(b.conditions))return fail("Field map body rejected");
        for(uint32_t k=0;k<b.shape_count;++k)if(v.shape(b.shape_first+k).kind!=uint16_t(FieldShapeKind::Convex)||v.shape(b.shape_first+k).count<3)return fail("Field map body shape rejected");}
    bytes_.assign(p,p+n);error.clear();return true;
}
bool FieldMapData::load_file(const char*path,std::string&error){
    if(!path){error="Missing field map path";return false;}
    std::vector<uint8_t> bytes;return encore::read_file(path,bytes,max_bytes,error)&&load(bytes.data(),bytes.size(),error);
}
}
