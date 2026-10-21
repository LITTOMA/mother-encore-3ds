#include "encore/dungeon_terrain.hpp"
#include "encore/content.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <tuple>
#include <unordered_map>
namespace encore::upstream {namespace {
constexpr uint32_t strides[]={1,1,32,80,32,72,32,8,4};
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
int32_t integer(const uint8_t*p){const auto raw=u(p);int32_t v;std::memcpy(&v,&raw,4);return v;}
uint16_t half(const uint8_t*p){return uint16_t(p[0]|uint16_t(p[1])<<8);}
int16_t small(const uint8_t*p){const auto raw=half(p);int16_t v;std::memcpy(&v,&raw,2);return v;}
float real(const uint8_t*p){const auto raw=u(p);float v;std::memcpy(&v,&raw,4);return v;}
Vec2 vec(const uint8_t*p){return {real(p),real(p+4)};}
bool finite(float v){return std::isfinite(v)&&std::abs(v)<=1000000.f;}
bool finite(Vec2 v){return finite(v.x)&&finite(v.y);}
bool fail(std::string&e,const char*s){e=s;return false;}
bool path(const std::string&s){if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;size_t start=0;for(size_t i=0;i<=s.size();++i){if(i==s.size()||s[i]=='/'){const auto part=s.substr(start,i-start);if(part.empty()||part=="."||part=="..")return false;start=i+1;}else if(uint8_t(s[i])<32||uint8_t(s[i])>=127||s[i]=='\\'||s[i]==':')return false;}return true;}
uint64_t key(uint32_t layer,int16_t x,int16_t y){return uint64_t(layer)<<32|uint32_t(uint16_t(x))<<16|uint16_t(y);}
}
struct DungeonTerrainData::State {TerrainInfo info;FieldMapData graphics;std::vector<TerrainLayer>layers;std::vector<TerrainCell>cells;std::vector<TerrainDefinition>definitions;std::vector<TerrainNavigation>navigation;std::vector<Vec2>points;std::vector<uint32_t>indices;};
FieldMapView DungeonTerrainData::graphics()const{return state_?state_->graphics.view():FieldMapView{};}
const TerrainInfo&DungeonTerrainData::info()const{static const TerrainInfo empty;return state_?state_->info:empty;}
const std::vector<TerrainLayer>&DungeonTerrainData::layers()const{static const std::vector<TerrainLayer>empty;return state_?state_->layers:empty;}
const std::vector<TerrainCell>&DungeonTerrainData::cells()const{static const std::vector<TerrainCell>empty;return state_?state_->cells:empty;}
const std::vector<TerrainDefinition>&DungeonTerrainData::definitions()const{static const std::vector<TerrainDefinition>empty;return state_?state_->definitions:empty;}
const std::vector<TerrainNavigation>&DungeonTerrainData::navigation()const{static const std::vector<TerrainNavigation>empty;return state_?state_->navigation:empty;}
const std::vector<Vec2>&DungeonTerrainData::navigation_points()const{static const std::vector<Vec2>empty;return state_?state_->points:empty;}
const std::vector<uint32_t>&DungeonTerrainData::navigation_indices()const{static const std::vector<uint32_t>empty;return state_?state_->indices:empty;}
const TerrainDefinition*DungeonTerrainData::definition(uint32_t id)const{const auto&rows=definitions();auto it=std::lower_bound(rows.begin(),rows.end(),id,[](const auto&r,uint32_t v){return r.id<v;});return it!=rows.end()&&it->id==id?&*it:nullptr;}
bool DungeonTerrainData::cell(uint32_t layer,int32_t x,int32_t y,bool&present,TerrainCell&out,std::string&e)const{
 if(!state_||x<std::numeric_limits<int16_t>::min()||x>std::numeric_limits<int16_t>::max()||y<std::numeric_limits<int16_t>::min()||y>std::numeric_limits<int16_t>::max())return fail(e,"Terrain cell owner/coordinate rejected");
 auto owner=std::find_if(layers().begin(),layers().end(),[&](const auto&l){return l.id==layer;});if(owner==layers().end())return fail(e,"Terrain cell layer rejected");
 const auto first=cells().begin()+owner->first,last=first+owner->count;auto it=std::lower_bound(first,last,std::make_pair(y,x),[](const auto&c,const auto&v){return std::make_pair(int32_t(c.y),int32_t(c.x))<v;});present=it!=last&&it->x==x&&it->y==y;if(present)out=*it;e.clear();return true;
}
bool DungeonTerrainData::load_file(const char*name,std::string&e){std::vector<uint8_t>bytes;return encore::read_file(name,bytes,8*1024*1024,e)&&load(bytes.data(),bytes.size(),e);}
bool DungeonTerrainData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<208||n>8*1024*1024||std::memcmp(p,"ENCTERR1",8)||u(p+8)!=1||u(p+12)!=n||u(p+20)!=1||!u(p+24)||u(p+28)||u(p+52)!=9||u(p+56)||u(p+60)||encore::crc32(p+32,n-32)!=u(p+16))return fail(e,"Terrain header/version/capability/CRC rejected");
 std::array<const uint8_t*,9>data{};std::array<uint32_t,9>counts{};size_t cursor=208;
 for(size_t i=0;i<9;++i){const auto*r=p+64+i*16;const auto at=u(r+4),count=u(r+8);if(u(r)!=i+1||u(r+12)!=strides[i])return fail(e,"Terrain directory kind/stride rejected");counts[i]=count;if(!count){if(at)return fail(e,"Terrain empty section offset rejected");continue;}if(at%4||at<cursor||at>n||count>(n-at)/strides[i])return fail(e,"Terrain section span rejected");for(size_t j=cursor;j<at;++j)if(p[j])return fail(e,"Terrain nonzero padding rejected");data[i]=p+at;cursor=size_t(at)+size_t(count)*strides[i];}
 if(cursor!=n||!counts[0]||counts[0]>1024*1024||counts[2]!=1||!counts[3]||counts[3]>256||!counts[4]||counts[4]>1048576||!counts[5]||counts[5]>65536||counts[6]>65536||counts[7]>1048576||counts[8]>1048576)return fail(e,"Terrain section capacity/trailing bytes rejected");
 auto next=std::make_shared<State>();if(!next->graphics.load(data[1],counts[1],e)||!next->graphics.view().terrain_only()||next->graphics.view().scene_id()!=u(p+24)||std::memcmp(p+32,data[1]+32,20))return fail(e,"Terrain nested graphics/pin/scene rejected");const auto g=next->graphics.view();
 std::string_view pool(reinterpret_cast<const char*>(data[0]),counts[0]);size_t utf_cursor=0;uint32_t cp=0;while(utf_cursor<pool.size())if(!encore::utf8_next(pool,utf_cursor,cp)||!cp)return fail(e,"Terrain text encoding rejected");
 std::set<std::pair<uint32_t,uint32_t>>text_spans;bool text_ok=true;
 auto text=[&](uint32_t off,uint32_t size,bool empty=false){if(!size){if(off||!empty)text_ok=false;return std::string{};}if(size>4096||off>pool.size()||size>pool.size()-off){text_ok=false;return std::string{};}std::string out(pool.substr(off,size));size_t at=0;uint32_t value=0;while(at<out.size())if(!encore::utf8_next(out,at,value)||!value){text_ok=false;break;}text_spans.emplace(off,size);return out;};
 const auto*m=data[2];next->info={u(p+24),u(m+16),u(m+24),u(m+28),text(u(m),u(m+4)),text(u(m+8),u(m+12))};if(!path(next->info.source)||!next->info.inactive_script||u(m+20)!=1||(!path(next->info.root_name)||next->info.root_name.find('/')!=std::string::npos)||counts[3]!=g.count(FieldMapSection::Layers))return fail(e,"Terrain source/script/layer identity rejected");
 auto definition_by_id=[&](uint32_t id)->const TerrainDefinition*{const auto&v=next->definitions;auto it=std::lower_bound(v.begin(),v.end(),id,[](const auto&d,uint32_t k){return d.id<k;});return it!=v.end()&&it->id==id?&*it:nullptr;};
 uint32_t nav_cursor=0;
 for(uint32_t i=0;i<counts[5];++i){const auto*r=data[5]+i*72;TerrainDefinition d;d.id=u(r);d.name=text(u(r+4),u(r+8),true);d.texture=text(u(r+12),u(r+16));for(size_t k=0;k<4;++k)d.region[k]=real(r+20+k*4);d.offset=vec(r+36);d.z=integer(r+44);d.mode=u(r+48);d.shape_count=u(r+52);d.nav_first=u(r+56);d.nav_count=u(r+60);
  if(d.id>=(1u<<29)||(i&&d.id<=next->definitions.back().id)||!path(d.texture)||d.mode>2||d.shape_count>1024||u(r+64)||u(r+68)||!finite(d.offset)||!std::all_of(d.region.begin(),d.region.end(),[](float v){return finite(v)&&v>=0;})||((d.region[2]==0)!=(d.region[3]==0))||d.nav_count>counts[6]-nav_cursor||(d.nav_count?d.nav_first!=nav_cursor:d.nav_first!=0))return fail(e,"Terrain tile definition/geometry/navigation span rejected");nav_cursor+=d.nav_count;next->definitions.push_back(std::move(d));}
 if(nav_cursor!=counts[6])return fail(e,"Terrain unowned navigation records rejected");
 std::unordered_map<uint64_t,std::pair<uint32_t,uint32_t>>draw_cells;uint32_t cell_cursor=0;
 for(uint32_t i=0;i<counts[3];++i){const auto*r=data[3]+i*80;TerrainLayer l;l.id=u(r);l.graphics_layer=u(r+4);l.name=text(u(r+8),u(r+12));l.position=vec(r+16);l.cell_size=vec(r+24);l.layer=u(r+32);l.mask=u(r+36);l.flags=u(r+40);l.origin=u(r+44);for(size_t k=0;k<4;++k)l.used[k]=integer(r+48+k*4);l.first=u(r+64);l.count=u(r+68);l.order=u(r+72);const auto layer=g.layer(i);
  // The nested ENCTMAP1 uses the existing field format's fixed16-pixel lattice.
  if(!l.id||(i&&l.id<=next->layers.back().id)||l.graphics_layer!=i||!path(l.name)||l.name!=g.text(layer.name)||!finite(l.position)||l.position.x!=layer.position.x||l.position.y!=layer.position.y||l.cell_size.x!=16||l.cell_size.y!=16||l.flags>3||(l.origin!=0&&l.origin!=2)||bool(l.flags&1)!=bool(layer.flags&1)||bool(l.flags&2)==bool(layer.flags&4)||bool(layer.flags&2)!=(bool(l.layer&next->info.player_mask)||bool(l.mask&next->info.player_layer))||layer.origin_y!=(l.origin==2?l.cell_size.y:0)||l.first!=cell_cursor||!l.count||l.count>counts[4]-cell_cursor||l.order!=i||u(r+76))return fail(e,"Terrain source/graphics layer relationship rejected");
  for(uint32_t k=0;k<layer.chunk_count;++k){const auto chunk=g.chunk(layer.chunk_first+k);for(uint32_t j=0;j<chunk.count;++j){const auto c=g.cell(chunk.first+j);if(!draw_cells.emplace(key(l.id,c.x,c.y),std::make_pair(uint32_t(c.tile),uint32_t(c.order))).second)return fail(e,"Terrain duplicate graphics cell rejected");}}
  cell_cursor+=l.count;next->layers.push_back(std::move(l));}
 if(cell_cursor!=counts[4])return fail(e,"Terrain unowned logical cells rejected");
 for(const auto&l:next->layers){int32_t minx=32767,miny=32767,maxx=-32768,maxy=-32768;std::vector<bool>order(l.count,false);std::pair<int16_t,int16_t>previous{};
  for(uint32_t i=0;i<l.count;++i){const auto*r=data[4]+size_t(l.first+i)*32;TerrainCell c;c.layer=u(r);c.x=small(r+4);c.y=small(r+6);c.tile=u(r+8);c.auto_x=half(r+12);c.auto_y=half(r+14);c.flags=u(r+16);c.graphics_tile=u(r+20);c.source_order=u(r+24);const auto position=std::make_pair(c.y,c.x);const auto draw=draw_cells.find(key(l.id,c.x,c.y));const bool resolved=definition_by_id(c.tile)!=nullptr;
   if(c.layer!=l.id||c.tile>=(1u<<29)||c.flags>3||(i&&position<=previous)||c.source_order>=l.count||order[c.source_order]||u(r+28)||resolved!=(c.graphics_tile!=kFieldNone)||resolved!=(draw!=draw_cells.end()))return fail(e,"Terrain logical cell identity/order/resolution rejected");
   if(resolved){if(c.graphics_tile>=g.count(FieldMapSection::Tiles)||draw->second.first!=c.graphics_tile||g.tile(c.graphics_tile).flags!=c.flags||((l.flags&1)&&draw->second.second!=c.source_order))return fail(e,"Terrain logical/graphics cell differs");draw_cells.erase(draw);}
   order[c.source_order]=true;previous=position;minx=std::min(minx,int32_t(c.x));miny=std::min(miny,int32_t(c.y));maxx=std::max(maxx,int32_t(c.x));maxy=std::max(maxy,int32_t(c.y));next->cells.push_back(c);}
  if(l.used!=std::array<int32_t,4>{{minx,miny,maxx-minx+1,maxy-miny+1}})return fail(e,"Terrain logical used rectangle differs");}
 if(!draw_cells.empty())return fail(e,"Terrain unowned graphics cells rejected");
 for(uint32_t i=0;i<counts[7];++i){auto v=vec(data[7]+i*8);if(!finite(v))return fail(e,"Terrain navigation vertex rejected");next->points.push_back(v);}
 for(uint32_t i=0;i<counts[8];++i)next->indices.push_back(u(data[8]+i*4));uint32_t point_cursor=0,index_cursor=0;
 for(const auto&d:next->definitions)for(uint32_t i=0;i<d.nav_count;++i){const auto*r=data[6]+size_t(d.nav_first+i)*32;TerrainNavigation v{u(r),u(r+4),u(r+8),u(r+12),u(r+16),vec(r+20)};
  if(v.tile!=d.id||v.point_first!=point_cursor||v.index_first!=index_cursor||v.point_count<3||v.point_count>256||v.index_count<3||v.index_count>256||v.point_count>counts[7]-point_cursor||v.index_count>counts[8]-index_cursor||!finite(v.offset)||u(r+28))return fail(e,"Terrain navigation polygon span rejected");std::set<uint32_t>unique;
  for(uint32_t j=0;j<v.index_count;++j){const auto id=next->indices[v.index_first+j];if(id>=v.point_count||!unique.insert(id).second)return fail(e,"Terrain navigation polygon index rejected");}
  point_cursor+=v.point_count;index_cursor+=v.index_count;next->navigation.push_back(v);}
 if(point_cursor!=counts[7]||index_cursor!=counts[8])return fail(e,"Terrain unowned navigation data rejected");
 size_t text_cursor=0;for(const auto&s:text_spans){if(s.first!=text_cursor)return fail(e,"Terrain text overlap/unowned bytes rejected");text_cursor+=s.second;}if(!text_ok||text_cursor!=pool.size())return fail(e,"Terrain unowned/invalid text rejected");state_=std::move(next);e.clear();return true;
}
}
