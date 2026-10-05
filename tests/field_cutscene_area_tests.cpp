// Manual-only source lifecycle and negative resource cases; never auto-registered.
#include "encore/field_cutscene_area.hpp"
#include <cassert>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
using namespace encore::upstream;
int main(int argc,char**argv){
 assert(argc==2);FieldCutsceneAreaData d;std::string e;assert(d.load_file(argv[1],e)&&d.bindings().size()==15);
 FieldCutsceneAreaUi ui;bool admission=true,player=true;std::map<std::string,bool>flags;std::vector<std::string>trace;
 FieldCutsceneAreaHost h;
 h.admit_ready=[](const auto&,const auto&,auto&){return true;};
 h.connect_battle_to_overworld=[&](auto,std::string_view signal,auto&){assert(signal==d.policy().battle_signal);trace.push_back("connect");return true;};
 h.body_is_current_player=[&](auto,bool&v,auto&){v=player;return true;};h.query_ui=[&](auto&v,auto&){v=ui;return true;};
 h.read_flag=[&](auto name,bool&v,auto&){trace.push_back(std::string(name));v=flags[std::string(name)];return true;};
 h.admit_programme=[&](const auto&b,auto&){assert(!b.programme.empty());trace.push_back("admit");return admission;};
 h.close_commands=[&](const auto&p,auto&){assert(p==d.policy().close);trace.push_back("close");return true;};
 h.pause_player=[&](const auto&p,auto&){assert(p==d.policy().pause);trace.push_back("pause");return true;};
 h.open_room_and_unpause=[&](const auto&,auto method,auto&){assert(method==d.policy().completion);trace.push_back("room");ui.cutscene=true;return true;};
 FieldCutsceneAreaRuntime r;assert(r.initialize(d,h,e));assert(!r.ready(d.bindings().back().id,e));for(const auto&b:d.bindings())assert(r.ready(b.id,e));assert(!r.ready(d.bindings().front().id,e));
 const auto*b=&d.bindings().front();for(const auto&v:d.bindings())if(!v.appear.empty()){b=&v;break;}assert(!b->appear.empty());
 // check_start invokes source flags despite enabling either result. A false
 // appear must not read disappear and an unblocked idle false disables once.
 trace.clear();assert(r.body_enter(b->id,1,e)&&r.state(b->id)->processing);assert(trace==std::vector<std::string>{b->appear});
 assert(r.idle_process(b->id,true,e)&&!r.state(b->id)->processing);flags[b->appear]=true;assert(r.idle_process(b->id,true,e)&&!r.state(b->id)->processing);
 assert(r.body_enter(b->id,1,e));ui.pause=true;trace.clear();assert(r.idle_process(b->id,true,e)&&r.state(b->id)->processing&&trace.empty());ui.pause=false;
 admission=false;assert(!r.idle_process(b->id,true,e)&&r.state(b->id)->processing);assert(trace.back()=="admit");assert(std::find(trace.begin(),trace.end(),"pause")==trace.end());
 admission=true;trace.clear();assert(r.idle_process(b->id,true,e)&&!r.state(b->id)->processing);assert(trace[trace.size()-3]=="close"&&trace[trace.size()-2]=="pause"&&trace.back()=="room");
 // Source blocked frame preserves enabled process; battle exit does not clear,
 // but battle_to_ov does, even while the actual body is still overlapping.
 assert(r.check_start(b->id,e));ui.battle=true;assert(r.body_exit(b->id,1,e)&&r.state(b->id)->processing);assert(r.battle_to_overworld(b->id,e)&&!r.state(b->id)->processing);ui.battle=false;ui.cutscene=false;
 assert(r.check_start(b->id,e));player=false;assert(r.body_exit(b->id,2,e)&&r.state(b->id)->processing);player=true;assert(r.body_exit(b->id,1,e)&&!r.state(b->id)->processing);
 assert(!r.body_enter(b->id,0,e));assert(r.exit_tree(b->id,e)&&!r.state(b->id)->alive);assert(!r.check_start(b->id,e));assert(!r.battle_to_overworld(0,e));assert(!FieldCutsceneAreaRuntime{}.initialize(d,{},e));
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>bytes((std::istreambuf_iterator<char>(f)),{});
 auto recalc=[](auto&b){uint32_t crc=~0u;for(size_t i=0;i<b.size();++i){crc^=i>=16&&i<20?0:b[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}crc=~crc;for(unsigned j=0;j<4;++j)b[16+j]=uint8_t(crc>>(j*8));};
 for(unsigned at:{8u,20u,24u,52u}){auto bad=bytes;bad[at]=99;recalc(bad);assert(!d.load(bad.data(),bad.size(),e)&&d.valid());}
 auto bad=bytes;bad.back()^=1;assert(!d.load(bad.data(),bad.size(),e));assert(!d.load(nullptr,0,e));
}
