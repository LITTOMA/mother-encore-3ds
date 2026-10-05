// Manual source-core double only. No NDSP/audibility or hardware claim.
#include "encore/field_music_changer.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
using namespace encore::upstream;
int main(int argc,char**argv){
 assert(argc==3);FieldMusicChangerData d;std::string e;assert(d.load_file(argv[1],e)&&d.bindings().size()==13);AudioBank bank;assert(bank.load_file(argv[2],e));assert(d.matches_service(d.music(),bank,e));
 unsigned owners=0,parts=0;for(const auto&b:d.bindings()){owners+=b.shapes.size();for(const auto&s:b.shapes)parts+=s.parts.size();}assert(owners==16&&parts==38);
 MusicRegionController c;assert(c.initialize(d.music(),16,e)&&c.attach_scene(1,e));MusicRegionContext context;std::map<std::string,bool>flags;context.flag=[&](auto name){return flags[std::string(name)];};std::vector<uint32_t>shape_calls;unsigned source_idle=0;
 FieldMusicChangerHost h;
 h.admit_service=[&](const auto&data,auto epoch,auto&err){return epoch==1&&data.matches_service(d.music(),bank,err);};h.admit_ready=[](const auto&,const auto&,auto&){return true;};
 h.set_shape_disabled=[&](auto id,auto,auto&){shape_calls.push_back(id);return true;};h.context=[&](auto,auto&out,auto&){out=context;return true;};
 h.area_enter=[&](auto epoch,auto path,const auto&ct,auto&err){return c.enter(epoch,path,ct,err);};h.area_exit=[&](auto epoch,auto path,const auto&ct,auto&err){return c.exit(epoch,path,ct,err);};
 h.play_explicit=[&](auto epoch,auto path,auto&err){return c.play_explicit(epoch,path,err);};h.stop_explicit=[&](auto epoch,auto path,auto fade,auto&err){return c.stop_explicit(epoch,path,fade,err);};h.tree_exit=[&](auto epoch,auto path,auto&err){return c.tree_exit(epoch,path,err);};h.idle_frame=[&](auto epoch,auto&err){++source_idle;return c.idle_frame(epoch,err);};
 FieldMusicChangerRuntime r;assert(r.initialize(d,1,h,e));assert(!r.idle_frame(e));assert(!r.ready(d.bindings().back().id,e));for(const auto&b:d.bindings())assert(r.ready(b.id,e));assert(shape_calls.size()==13);
 const auto&b=d.bindings()[2];assert(b.shapes.size()==2);shape_calls.clear();assert(r.set_disabled(b.id,true,e)&&shape_calls==std::vector<uint32_t>{b.shapes.front().id});
 context.is_player=false;assert(r.body_enter(b.id,2,e)&&!c.regions()[2].inside);context.is_player=true;context.in_battle=true;assert(r.body_enter(b.id,1,e)&&!c.regions()[2].inside);context.in_battle=false;
 assert(r.body_enter(b.id,1,e)&&c.regions()[2].registered);assert(r.body_exit(b.id,1,e)&&c.regions()[2].pending_exit);assert(r.body_enter(b.id,1,e));assert(r.idle_frame(e)&&source_idle==1&&c.regions()[2].registered);
 context.in_cutscene=true;assert(r.body_exit(b.id,1,e)&&c.regions()[2].inside);assert(r.stop_music_immediately(b.id,e)&&!c.regions()[2].registered&&c.regions()[2].inside);assert(r.play_music(b.id,e)&&c.regions()[2].registered);context.in_cutscene=false;
 context.has_collisions=false;assert(r.body_exit(b.id,1,e)&&c.regions()[2].inside);context.has_collisions=true;assert(r.body_exit(b.id,1,e)&&!c.regions()[2].inside);assert(r.idle_frame(e)&&!c.regions()[2].registered);
 assert(!r.stop_music(b.id,std::numeric_limits<double>::quiet_NaN(),e));assert(!r.body_enter(b.id,0,e));for(const auto&v:d.bindings())assert(r.tree_exiting(v.id,e));assert(!r.play_music(b.id,e));assert(!FieldMusicChangerRuntime{}.initialize(d,1,{},e));
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>bytes((std::istreambuf_iterator<char>(f)),{});auto recalc=[](auto&b){uint32_t crc=~0u;for(size_t i=0;i<b.size();++i){crc^=i>=16&&i<20?0:b[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}crc=~crc;for(unsigned j=0;j<4;++j)b[16+j]=uint8_t(crc>>(j*8));};
 for(unsigned at:{8u,20u,24u,52u}){auto bad=bytes;bad[at]=99;recalc(bad);assert(!d.load(bad.data(),bad.size(),e)&&d.valid());}auto bad=bytes;bad.back()^=1;assert(!d.load(bad.data(),bad.size(),e));assert(!d.load(nullptr,0,e));
}
