#include "encore/house_runtime.hpp"
#include "encore/house_presentation.hpp"
#include "encore/content.hpp"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char*why){++checks;if(!ok){std::cerr<<"Inspection runtime check "<<checks<<": "<<why<<'\n';std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
void put(std::vector<uint8_t>&b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(i*8));}
uint32_t crc(const std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=0;i<b.size();++i){c^=(i>=16&&i<20)?0:b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
}
int main(int argc,char**argv){
 check(argc==5,"inspection, House, Room and font packs required");std::string error;HouseInspectionData inspections;HouseData house;RoomData room;BattleData font;
 check(inspections.load_file(argv[1],error),error.c_str());check(house.load_file(argv[2],error),error.c_str());check(room.load_file(argv[3],error),error.c_str());check(font.load_file(argv[4],error),error.c_str());
 SourceRandom random(41);OpeningWorld world;HousePresentation presentation;HouseRuntime runtime;
 check(world.initialize(room.view()),world.error());world.attach_random(random);check(presentation.begin(house.view(),font.view(),random),presentation.error());check(runtime.initialize(house.view(),world,presentation),runtime.error());
 check(!runtime.inspection_visible(0)&&!runtime.inspection_interaction_supported(0),"unbound inspection is unavailable");
 check(runtime.bind_inspections(inspections.view()),runtime.error());check(!runtime.inspection_visible(UINT32_MAX),"unknown object unavailable");
 const auto base=inspections.view();const auto object=base.object(0);check(runtime.inspection_visible(0),"original object is visible");
 std::vector<uint8_t>bytes;check(encore::read_file(argv[1],bytes,1024*1024,error),error.c_str());const auto ob=get(bytes,84),ov=get(bytes,100);
 auto reject_binding=[&](std::vector<uint8_t>b,const char*why){put(b,16,crc(b));HouseInspectionData candidate;check(candidate.load(b.data(),b.size(),error),error.c_str());check(!runtime.bind_inspections(candidate.view()),why);check(runtime.phase()==HousePhase::Idle&&runtime.inspections().object(0).id==object.id&&world.stage()==OpeningStage::Walking&&!world.house_paused(),"failed binding preserves live world");};
 {auto b=bytes;b[32]^=1;reject_binding(b,"different reviewed source rejected");}
 {auto b=bytes;put(b,ob+12,object.source_path);reject_binding(b,"unimplemented appear lifecycle rejected");}
 {auto b=bytes;put(b,ob+16,object.source_path);reject_binding(b,"unimplemented disappear lifecycle rejected");}
 {auto b=bytes;put(b,ob+40,house.view().count(HouseSection::Dialogues));reject_binding(b,"missing House text index rejected");}
 {auto b=bytes;put(b,ob+8,object.source_path);reject_binding(b,"House text index with wrong source path rejected");}
 if(object.default_dialogue_index!=house_no_index){auto b=bytes;put(b,ob+40,house_no_index);reject_binding(b,"known text cannot become silent unsupported fallback");}
 if(base.count(HouseInspectionSection::Overrides)){auto b=bytes;put(b,ov+4,object.source_path);reject_binding(b,"unknown Room flag rejected");}
 // Runtime support follows every source override, with later entries winning.
 for(uint32_t i=0;i<base.count(HouseInspectionSection::Objects);++i){const auto o=base.object(i);uint32_t selected=o.default_dialogue_index;
  for(uint32_t j=0;j<o.override_count;++j){const auto r=base.override_dialogue(o.first_override+j);check(world.set_story_flag(base.string(r.flag),false,false),"clear source condition");}
  check(runtime.inspection_interaction_supported(i)==(selected!=house_no_index),"default capability is explicit");
  for(uint32_t j=0;j<o.override_count;++j){const auto r=base.override_dialogue(o.first_override+j);check(world.set_story_flag(base.string(r.flag),true,false),"set source condition");selected=r.dialogue_index;check(runtime.inspection_interaction_supported(i)==(selected!=house_no_index),"last matching source override controls capability");}
 }
 // Isolate one genuine admitted text on the nearest ray, leaving all other
 // inspectors far away. This changes binary geometry, never C++ game content.
 auto isolated=bytes;uint32_t text_object=house_no_index;
 for(uint32_t i=0;i<base.count(HouseInspectionSection::Objects);++i){const auto o=base.object(i);if(text_object==house_no_index&&o.default_dialogue_index!=house_no_index)text_object=i;for(unsigned field:{44u,48u,52u,56u})put(isolated,ob+size_t(i)*76+field,0x47c35000);}
 check(text_object!=house_no_index,"source contains complete text inspection");
 const auto p=world.player();auto float_bits=[](float f){uint32_t bits;std::memcpy(&bits,&f,4);return bits;};const auto ray=house.view().interaction();
 for(unsigned field:{44u,52u})put(isolated,ob+size_t(text_object)*76+field,float_bits(p.position.x+ray.ray_origin.x));
 for(unsigned field:{48u,56u})put(isolated,ob+size_t(text_object)*76+field,float_bits(p.position.y+ray.ray_origin.y));
 put(isolated,16,crc(isolated));HouseInspectionData isolated_data;check(isolated_data.load(isolated.data(),isolated.size(),error),error.c_str());check(runtime.bind_inspections(isolated_data.view()),runtime.error());
 const auto seen=runtime.seen_dialogue_keys();check(runtime.idle_frame(double(float(1./60)),true,false),runtime.error());
 check(runtime.phase()==HousePhase::Dialogue&&presentation.dialogue_active(),"actual House Accept opens inspection text");check(runtime.seen_dialogue_keys()==seen,"inspection leaves original NPC saved seen history unchanged");
 check(!runtime.bind_inspections(base)&&runtime.phase()==HousePhase::Dialogue&&presentation.dialogue_active(),"reload during active dialogue preserves interaction");
 // A nearer unsupported source object owns the ray even when a supported
 // object is behind it. This is a geometry fixture over real source text,
 // not replacement game rules or dialogue.
 uint32_t unsupported_object=house_no_index;
 for(uint32_t i=0;i<base.count(HouseInspectionSection::Objects);++i)if(base.object(i).default_dialogue_index==house_no_index){unsupported_object=i;break;}
 check(unsupported_object!=house_no_index,"source capability boundary exists");
 auto blocked=isolated;OpeningWorld occlusion_world;HousePresentation occlusion_presentation;HouseRuntime occlusion_runtime;SourceRandom occlusion_random(43);
 check(occlusion_world.initialize(room.view()),occlusion_world.error());occlusion_world.attach_random(occlusion_random);check(occlusion_presentation.begin(house.view(),font.view(),occlusion_random),occlusion_presentation.error());check(occlusion_runtime.initialize(house.view(),occlusion_world,occlusion_presentation),occlusion_runtime.error());
 const auto start=occlusion_world.player();const Vec2 origin{start.position.x+ray.ray_origin.x,start.position.y+ray.ray_origin.y};
 const float length=std::sqrt(start.direction.x*start.direction.x+start.direction.y*start.direction.y);
 check(length>0,"source initial ray direction valid");const Vec2 direction{start.direction.x/length,start.direction.y/length};
 auto place=[&](uint32_t index,float distance){const size_t at=ob+size_t(index)*76;const Vec2 center{origin.x+direction.x*distance,origin.y+direction.y*distance};
  for(unsigned field:{44u,52u})put(blocked,at+field,float_bits(center.x));for(unsigned field:{48u,56u})put(blocked,at+field,float_bits(center.y));for(unsigned field:{60u,64u})put(blocked,at+field,float_bits(1));};
 place(unsupported_object,2);place(text_object,7);put(blocked,16,crc(blocked));HouseInspectionData blocked_data;check(blocked_data.load(blocked.data(),blocked.size(),error),error.c_str());check(occlusion_runtime.bind_inspections(blocked_data.view()),occlusion_runtime.error());
 check(occlusion_runtime.idle_frame(double(float(1./60)),true,false),occlusion_runtime.error());
 check(occlusion_runtime.phase()==HousePhase::Unsupported&&!occlusion_presentation.dialogue_active()&&occlusion_world.house_paused(),"nearest unsupported inspection blocks text behind it");
 check(occlusion_runtime.idle_frame(double(float(1./60)),false,true),occlusion_runtime.error());
 check(occlusion_runtime.phase()==HousePhase::Idle&&!occlusion_world.house_paused()&&occlusion_world.player().position.x==start.position.x&&occlusion_world.player().position.y==start.position.y,"B restores control and source position at capability boundary");
 check(occlusion_runtime.seen_dialogue_keys().empty(),"unsupported inspection does not invent seen history");
 std::cout<<"House inspection runtime: "<<checks<<" checks\n";
}
