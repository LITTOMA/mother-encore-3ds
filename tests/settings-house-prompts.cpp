#include "encore/house_button_prompts.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace encore::upstream;
namespace {
uint32_t integer(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void seal(std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=24;i<b.size();++i){c^=b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}put(b,16,~c);}
unsigned checks=0;void check(bool v,const char*m){++checks;if(!v){std::cerr<<"FAIL: "<<m<<'\n';std::exit(1);}}}
int main(int argc,char**argv){
 check(argc==5,"prompt/house/phone/inspection packs provided");std::string error;HouseButtonPromptData d;HouseData house;PhoneData phone;HouseInspectionData inspections;
 if(!d.load_file(argv[1],error)||!house.load_file(argv[2],error)||!phone.load_file(argv[3],error)||!inspections.load_file(argv[4],error)||!d.validate_bindings(house.view(),phone.view(),error,inspections.view())){std::cerr<<error<<'\n';return 1;}
 check(d.valid()&&d.choice_masks.size()==4&&d.resources.size()==3&&d.previews.size()==2,"checked source geometry/art modes loaded");
 HousePromptObservation o;o.paused=false;for(const auto&t:d.targets)o.targets.push_back({t.position,false,true,true});HousePromptPose pose;
 for(uint32_t i=0;i<d.targets.size();++i){const auto&t=d.targets[i];o.targets[i].visible=true;o.player={t.center.x-d.ray_origin.x,t.center.y+t.extents.y+d.ray_length/2-d.ray_origin.y};o.direction={0,-1};
  for(uint32_t choice=0;choice<d.choice_masks.size();++choice){check(evaluate_house_button_prompt(d,choice,o,pose,error),"choice observation accepted");check(pose.visible==bool(d.choice_masks[choice]&t.category),"Both/Objects/NPCs/None categories actually change visibility");if(pose.visible)check(pose.target==i&&pose.position.x==t.position.x+t.offset.x&&pose.position.y==t.position.y+t.offset.y,"source NPC/phone/door offset");}
  o.paused=true;check(evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"paused player hides prompt");o.paused=false;o.crouching=true;check(evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"unsupported telepathy hides prompt");o.crouching=false;
  o.targets[i].enabled=false;check(evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"disabled/unlocked door hides prompt");o.targets[i].enabled=true;o.targets[i].supported=false;check(evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"unsupported current interaction hides prompt");o.targets[i].supported=true;
  o.direction={0,1};check(evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"nearby target behind player is not selected");o.direction={0,-1};o.player.x+=t.extents.x+1;check(evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"ray misses lateral proximity");o.player.x-=t.extents.x+1;
  o.occlusion_distance=d.ray_length/4;check(evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"closer unrelated collider suppresses prompt");o.occlusion_distance=std::numeric_limits<float>::infinity();
  o.targets[i].position.x+=128;o.targets[i].position.y-=96;o.player.x+=128;o.player.y-=96;check(evaluate_house_button_prompt(d,0,o,pose,error)&&pose.visible&&pose.position.x==t.position.x+128+t.offset.x&&pose.position.y==t.position.y-96+t.offset.y,"moving NPC prompt/collider follows current position");o.targets[i].position=t.position;o.targets[i].visible=false;check(evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"hidden/replaced NPC has no hint");
 }
 // A collidable unsupported candidate occludes an otherwise supported one.
 o.targets[0]={d.targets[0].position,true,true,false};o.targets[1]={d.targets[0].position,true,true,true};o.player={d.targets[0].center.x-d.ray_origin.x,d.targets[0].center.y+d.targets[0].extents.y+d.ray_length/2-d.ray_origin.y};o.direction={0,-1};check(evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"no fallback through unsupported nearest collider");
 o.direction={0,0};check(!evaluate_house_button_prompt(d,0,o,pose,error)&&!pose.visible,"zero direction fails closed");o.direction={0,-1};check(!evaluate_house_button_prompt(d,99,o,pose,error),"unknown choice rejected");o.targets.pop_back();check(!evaluate_house_button_prompt(d,0,o,pose,error),"incomplete observation rejected");
 std::vector<uint8_t>bytes;check(encore::read_file(argv[1],bytes,1024*1024,error),"pack bytes available");for(auto offset:{0u,8u,12u,16u,20u}){auto bad=bytes;bad[offset]^=1;check(!d.load(bad.data(),bad.size(),error)&&d.valid(),"bad header/CRC fails without destroying checked data");}check(!d.load(bytes.data(),bytes.size()-1,error),"truncated pack rejected");
 // Malformed payloads retain a valid CRC, so they exercise semantic validation.
 const size_t resource_count=44+integer(bytes,40)*4;size_t at=resource_count+4;const size_t path=at+4,width=path+integer(bytes,at);for(uint32_t i=0;i<integer(bytes,resource_count);++i)at+=4+integer(bytes,at)+8;const size_t rectangle=at;at+=20;const size_t preview_count=at,preview=at+4;at+=4+integer(bytes,preview_count)*24;const size_t target=at+4,category=target+8,extents=target+16+integer(bytes,target+12)+16;
 const auto rejected=[&](size_t offset,uint32_t value){auto bad=bytes;put(bad,offset,value);seal(bad);check(!d.load(bad.data(),bad.size(),error)&&d.valid(),"valid-CRC malformed payload rejected atomically");};
 rejected(32,0);rejected(24,0x7fc00000u);rejected(36,0);rejected(44,4);rejected(width,0);rejected(rectangle+8,0);rejected(preview,UINT32_MAX);rejected(preview+4,3);rejected(target,5);rejected(category,1);rejected(extents,0);rejected(target+4,UINT32_MAX);
 auto bad_path=bytes;bad_path[path]='/';seal(bad_path);check(!d.load(bad_path.data(),bad_path.size(),error),"absolute resource path rejected");
 auto legacy_header=bytes;put(legacy_header,8,1);put(legacy_header,20,1);check(!d.load(legacy_header.data(),legacy_header.size(),error),"inspection kind requires explicitly admitted prompt capability 2");
 check(!d.validate_bindings(house.view(),phone.view(),error),"inspection prompts cannot borrow an absent inspection owner");
 check(!d.validate_bindings(HouseView{},phone.view(),error),"unvalidated cross-pack owner rejected");
 std::cout<<"settings-house-prompts: "<<checks<<" checks passed; "<<d.targets.size()<<" original house targets; static prompt scope\n";
}
