// Actual unchanged production renderer, explicit Tex3DS/C2D public API doubles.
// Byte admission is real. The double does not decode textures or render pixels.
#include "dungeon_key_renderer.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Key renderer line%d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
bool near(float a,float b){return std::abs(a-b)<.0001f;}
std::vector<DungeonKeyFlagValue>flags(const DungeonKeyMediaData&data,bool saved=false){std::vector<DungeonKeyFlagValue>out;for(uint32_t i=0;i<data.clocks().metadata().node_count();++i)out.push_back({data.clocks().metadata().node(i).id,saved});return out;}
void write(const std::string&path,const std::vector<uint8_t>&bytes){std::filesystem::create_directories(std::filesystem::path(path).parent_path());std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(out.good());}
}
int main(int argc,char**argv){
 CHECK(argc==5);DungeonKeyMediaData data,foreign;CHECK(data.load_file(argv[1],error)&&foreign.load_file(argv[1],error));const std::string source_root=argv[2],scratch=argv[3];CHECK(!std::filesystem::exists(scratch));std::filesystem::create_directories(scratch);
 using namespace key_graphics_test;for(const auto&s:data.sprites()){std::vector<uint8_t>bytes;CHECK(encore::read_file((source_root+"/"+s.texture).c_str(),bytes,8*1024*1024,error));CHECK(data.texture_matches(s.id,bytes.data(),bytes.size(),error));specs.push_back({s.id,s.width,s.height,bytes});write(scratch+"/"+s.texture,bytes);}
 std::map<std::tuple<uint32_t,uint32_t,uint32_t>,std::array<float,4>>reference;std::ifstream native(argv[4]);CHECK(native.good());std::string line;while(std::getline(native,line)){std::istringstream in(line);char tag;CHECK(bool(in>>tag));if(tag=='S'){uint32_t owner,profile,frame;std::array<float,4>r;CHECK(bool(in>>owner>>profile>>frame>>r[0]>>r[1]>>r[2]>>r[3]));CHECK(reference.emplace(std::make_tuple(owner,profile,frame),r).second);}}CHECK(reference.size()==69);
 DungeonKeyClocks world,other;CHECK(world.prepare(data.clocks(),101,3,flags(data),error)&&world.advance(101,0,error));CHECK(other.prepare(foreign.clocks(),101,0,flags(foreign),error));
 DungeonKeyRenderer renderer;CHECK(!renderer.ready()&&renderer.frame()==0&&renderer.commands().empty());CHECK(!renderer.compose(world,error)&&!renderer.draw(101,1,0,0,0,error)&&!renderer.retire(101,error));CHECK(!renderer.prepare({},world,scratch.c_str(),error)&&!renderer.prepare(data,other,scratch.c_str(),error));CHECK(!renderer.prepare(data,world,nullptr,error)&&!renderer.prepare(data,world,"",error));CHECK(live==0);
 // Byte failure after earlier profiles were admitted must free every candidate.
 for(size_t i=0;i<specs.size();++i){const auto path=scratch+"/"+data.sprites()[i].texture;auto bad=specs[i].bytes;bad.back()^=1;write(path,bad);CHECK(!renderer.prepare(data,world,scratch.c_str(),error)&&!renderer.ready()&&live==0);bad.pop_back();write(path,bad);CHECK(!renderer.prepare(data,world,scratch.c_str(),error)&&live==0);CHECK(std::filesystem::remove(path));CHECK(!renderer.prepare(data,world,scratch.c_str(),error)&&live==0);write(path,specs[i].bytes);}
 // Every public decode/layout failure cleans the partially prepared batch.
 for(unsigned slot=0;slot<specs.size();++slot)for(unsigned fault_mode=1;fault_mode<=8;++fault_mode){target=imports+slot+1;mode=fault_mode;CHECK(!renderer.prepare(data,world,scratch.c_str(),error)&&!renderer.ready()&&renderer.commands().empty()&&live==0);target=mode=0;}
 CHECK(renderer.prepare(data,world,scratch.c_str(),error)&&renderer.ready()&&renderer.epoch()==101&&live==3);CHECK(!renderer.prepare(data,world,scratch.c_str(),error));
 // Delete only this test's copies. Every subsequent compose/draw uses owners
 // prepared above, never a path lookup, read or hidden global texture cache.
 for(const auto&s:data.sprites())CHECK(std::filesystem::remove(scratch+"/"+s.texture));const unsigned prepared_imports=imports;
 std::set<std::tuple<uint32_t,uint32_t,uint32_t>>visited;auto drawing=[&](){const auto old=renderer.frame();CHECK(renderer.compose(world,error)&&renderer.frame()>old);std::vector<KeyDraw>expected;CHECK(data.draw(world,expected,error)&&expected.size()==renderer.commands().size());calls.clear();const auto count=renderer.commands().size();const float cx=123.25f,cy=-45.75f;
  const auto frame=renderer.frame();CHECK(!renderer.draw(100,frame,0,cx,cy,error)&&!renderer.draw(101,frame-1,0,cx,cy,error)&&!renderer.draw(101,frame,count,cx,cy,error));CHECK(!renderer.draw(101,frame,0,std::numeric_limits<float>::infinity(),cy,error)&&calls.empty());
  // Reverse submission proves the caller retains ordering control. There is
  // no renderer-side sorting which would separate a Lock from its parent.
  for(size_t k=count;k>0;--k){const size_t i=k-1;const auto&d=renderer.commands()[i];const auto&p=*data.sprite(d.profile);const auto&o=*data.owner(d.owner);DungeonKeyClockView state;CHECK(world.state(101,d.owner,state,error));const auto&pose=d.profile==o.profile?state.root:state.child;const auto*clock=data.clocks().profile(d.profile);visited.emplace(d.owner,d.profile,pose.frame);const auto it=reference.find({d.owner,d.profile,pose.frame});CHECK(it!=reference.end());const auto&r=it->second;CHECK(near(d.top_left.x,r[0]+pose.position.x-clock->position.x)&&near(d.top_left.y,r[1]+pose.position.y-clock->position.y)&&d.width==r[2]&&d.height==r[3]);CHECK(d.parent_order==o.parent_order&&d.parent_y_sort==bool(o.bits&1)&&d.sort_y==o.position.y&&d.order==o.order*2+unsigned(d.profile!=o.profile)&&d.z_index==((p.bits&16)?o.z_base+p.z_index:p.z_index));
   CHECK(renderer.draw(101,frame,i,cx,cy,error));const auto&c=calls.back();unsigned w=8,h=8;while(w<p.width*2)w*=2;while(h<p.height*2)h*=2;CHECK(c.profile==d.profile&&near(c.x,std::floor(d.top_left.x-cx+.5f))&&near(c.y,std::floor(d.top_left.y-cy+.5f))&&c.sub.width==d.width&&c.sub.height==d.height);CHECK(near(c.sub.left,.125f+float(d.x)/w)&&near(c.sub.right,.125f+float(d.x+d.width)/w)&&near(c.sub.top,.875f-float(d.y)/h)&&near(c.sub.bottom,.875f-float(d.y+d.height)/h));CHECK(c.filter==GPU_NEAREST&&c.wrap==GPU_CLAMP_TO_BORDER&&c.border==0);
  }CHECK(calls.size()==count&&imports==prepared_imports&&live==3);};
 drawing();CHECK(renderer.commands().size()==9);
 // Exercise all key image cells on a separate candidate of the current
 // world, then restore its untouched starting state. Do not play RESET here:
 // the original key queues deletion when ANY assigned animation finishes.
 // This is visual component sampling, not a gameplay pickup path.
 const auto visual_start=world;
 for(const auto&o:data.owners())if(data.clocks().profile(o.profile)->kind==KeyClockKind::Key)CHECK(world.play_animation(101,o.owner,KeyClockRole::Use,error));
 for(unsigned i=0;i<80;++i){CHECK(world.advance(101,1.f/128,error)&&world.idle_frame(101,error));drawing();world.clear_events();}
 world=visual_start;
 for(const auto&o:data.owners())if(data.clocks().profile(o.profile)->kind==KeyClockKind::Door)CHECK(world.interact(101,o.owner,error));
 for(unsigned i=0;i<300;++i){CHECK(world.advance(101,1.f/120,error)&&world.idle_frame(101,error));drawing();world.clear_events();}
 // Source Lock.tscn ends its blink at visible=false; three opened doors plus
 // three keys remain. Picking one key hides one of these six immediately.
 if(visited.size()!=69){std::fprintf(stderr,"Key renderer visited=%zu visible=%zu; absent source image cells:\n",visited.size(),renderer.commands().size());for(const auto&r:reference)if(!visited.count(r.first))std::fprintf(stderr,"owner%u/profile%u/frame%u\n",std::get<0>(r.first),std::get<1>(r.first),std::get<2>(r.first));}
 CHECK(renderer.commands().size()==6&&visited.size()==69);uint32_t key=0;for(const auto&o:data.owners())if(data.clocks().profile(o.profile)->kind==KeyClockKind::Key)key=o.owner;CHECK(key&&world.pickup(101,key,true,false,error)&&world.idle_frame(101,error));drawing();CHECK(renderer.commands().size()==5);CHECK(world.audio_finished(101,key,error)&&world.idle_frame(101,error));drawing();
 auto frame=renderer.frame();draw_failure=true;const auto before=calls.size();CHECK(!renderer.draw(101,frame,0,0,0,error)&&calls.size()==before);draw_failure=false;CHECK(!renderer.compose(other,error)&&renderer.frame()==0&&renderer.commands().empty()&&!renderer.draw(101,frame,0,0,0,error));CHECK(renderer.compose(world,error)&&renderer.frame()>frame);
 CHECK(!renderer.retire(102,error)&&renderer.ready()&&live==3);frame=renderer.frame();CHECK(renderer.retire(101,error)&&!renderer.ready()&&live==0&&renderer.commands().empty()&&!renderer.draw(101,frame,0,0,0,error));CHECK(!renderer.retire(101,error)&&!renderer.prepare(data,world,source_root.c_str(),error));
 // Ready plays Opened; its first animation evaluation sets Lock.visible=false
 // and frame9. Saved keys queue-free at the explicit idle boundary.
 DungeonKeyClocks saved;CHECK(saved.prepare(data.clocks(),102,0,flags(data,true),error)&&saved.advance(102,0,error)&&saved.idle_frame(102,error));CHECK(renderer.prepare(data,saved,source_root.c_str(),error)&&renderer.compose(saved,error));CHECK(renderer.commands().size()==3&&live==3);CHECK(renderer.retire(102,error)&&live==0);CHECK(frees==imports-3); // Three deliberate import failures allocate nothing.
 {DungeonKeyRenderer scoped;DungeonKeyClocks third;CHECK(third.prepare(data.clocks(),103,0,flags(data),error));CHECK(scoped.prepare(data,third,source_root.c_str(),error)&&live==3);}CHECK(live==0);
 // Remove only the empty directory chain created by this fresh scratch owner.
 // All three known files were individually removed immediately after prepare.
 for(const auto&s:data.sprites()){auto dir=std::filesystem::path(scratch+"/"+s.texture).parent_path();while(dir!=std::filesystem::path(scratch)){if(std::filesystem::exists(dir))CHECK(std::filesystem::remove(dir));dir=dir.parent_path();}}CHECK(std::filesystem::remove(scratch));
 std::printf("Key renderer:%u checks;production header,3 admitted byte owners,source69-frame rectangle oracle,doors/locks/pickup/hide/delete/saved states,caller ordering,UVs,camera,24 decode failures,9 byte failures,retirement and no draw-time IO;explicit Tex3DS/C2D doubles,no GPU/pixel/emulator claim\n",checks);
}
