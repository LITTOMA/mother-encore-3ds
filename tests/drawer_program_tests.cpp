#include "encore/drawer_program.hpp"
#include "encore/items_menu.hpp"
#include "encore/content.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace encore::upstream;
namespace {
void check(bool value,const char*why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
void put(std::vector<uint8_t>&b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(8*i));}
uint32_t crc(const std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=0;i<b.size();++i){c^=i>=16&&i<20?0:b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
struct Host:DrawerHost {
 bool got=false,space=true,reject_text=false,reject_sound=false,reject_grant=false;
 unsigned validation=0,effects=0,grants=0,sets=0,sounds=0;std::vector<uint32_t>text;std::vector<DrawerOpcode>events;
 bool validate_text(uint32_t,std::string&e)override{++validation;if(reject_text){e="missing text";return false;}return true;}
 bool validate_flag(std::string_view,std::string&)override{++validation;return true;}
 bool validate_item(DrawerItemTemplate,std::string_view,std::string&)override{++validation;return true;}
 bool validate_sound(std::string_view,std::string&e)override{++validation;if(reject_sound){e="missing sound";return false;}return true;}
 bool show_text(uint32_t id,std::string&)override{++effects;text.push_back(id);events.push_back(DrawerOpcode::ShowText);return true;}
 bool flag(std::string_view,bool&out,std::string&)override{out=got;return true;}
 bool inventory_space()const override{return space;}
 bool grant_item(DrawerItemTemplate,std::string_view,std::string&e)override{if(reject_grant){e="grant failed";return false;}++effects;++grants;events.push_back(DrawerOpcode::GrantItem);return true;}
 bool play_sound(std::string_view,std::string&)override{++effects;++sounds;events.push_back(DrawerOpcode::PlaySound);return true;}
 bool set_flag(std::string_view,bool value,std::string&)override{++effects;++sets;got=value;events.push_back(DrawerOpcode::SetFlag);return true;}
};
void finish(DrawerProgramRuntime&r,std::string&e){for(unsigned i=0;i<32&&r.state()==DrawerState::WaitingText;++i)check(r.advance_text(e),e.c_str());check(r.state()==DrawerState::Complete,"source path must terminate");}
}
int main(int argc,char**argv){
 check(argc==3,"actual drawer and Items binary paths required");std::string error;std::vector<uint8_t>bytes;DrawerProgramData data;
 check(encore::read_file(argv[1],bytes,1024*1024,error),error.c_str());check(data.load(bytes.data(),bytes.size(),error),error.c_str());
 auto v=data.view();const auto commands=v.count(DrawerSection::Commands);check(commands&&v.count(DrawerSection::Templates),"source program populated");
 auto reject=[&](std::vector<uint8_t>b,const char*why,bool rehash=true){if(rehash)put(b,16,crc(b));check(!data.load(b.data(),b.size(),error),why);check(data.view().count(DrawerSection::Commands)==commands,"failed reload preserves admitted program");};
 check(!data.load(nullptr,bytes.size(),error),"null rejected");
 for(size_t n:{size_t(0),size_t(8),size_t(127),bytes.size()-1})check(!data.load(bytes.data(),n,error),"truncated rejected");
 for(size_t p:{size_t(8),size_t(20),size_t(24),size_t(28)}){auto b=bytes;put(b,p,99);reject(b,"unknown version/caps/rules/directory rejected");}
 {auto b=bytes;b[0]^=1;reject(b,"magic rejected");}
 {auto b=bytes;b[52]=1;reject(b,"reserved bytes rejected");}
 {auto b=bytes;for(unsigned i=32;i<52;++i)b[i]=0;reject(b,"missing provenance rejected");}
 {auto b=bytes;b.back()^=1;reject(b,"CRC rejected",false);}
 {auto b=bytes;b.push_back(0);put(b,12,uint32_t(b.size()));reject(b,"trailing bytes rejected");}
 {auto b=bytes;put(b,84,128);reject(b,"overlapping sections rejected");}
 {auto b=bytes;b[66]=2;reject(b,"wrong string stride rejected");}
 const auto pool=get(bytes,68),templates=get(bytes,84),cmd=get(bytes,100);
 const auto binding=get(bytes,116);
 {auto b=bytes;put(b,binding,0);reject(b,"missing programme source rejected");}
 {auto b=bytes;put(b,binding+4,0);reject(b,"missing inspection owner rejected");}
 {auto b=bytes;b[pool+1]=0xff;reject(b,"invalid UTF8 rejected");}
 {auto b=bytes;put(b,templates,0);reject(b,"zero template id rejected");}
 {auto b=bytes;put(b,templates+4,v.item_template(0).source+1);reject(b,"interior string rejected");}
 {auto b=bytes;put(b,templates+8,0);reject(b,"zero doses rejected");}
 {auto b=bytes;put(b,templates+8,65536);reject(b,"doses above save domain rejected");}
 {auto b=bytes;put(b,templates+12,2);reject(b,"unknown key policy rejected");}
 {auto b=bytes;put(b,cmd,99);reject(b,"unknown opcode rejected");}
 {auto b=bytes;put(b,cmd+16,1);reject(b,"unused operand rejected");}
 {auto b=bytes;put(b,cmd,uint32_t(DrawerOpcode::Jump));put(b,cmd+4,0);put(b,cmd+8,0);put(b,cmd+12,0);put(b,cmd+16,0);reject(b,"self cycle rejected");}
 {auto b=bytes;put(b,cmd,uint32_t(DrawerOpcode::AwaitText));for(size_t p=4;p<20;p+=4)put(b,cmd+p,0);reject(b,"text gate without display rejected");}
 for(uint32_t i=0;i<commands;++i){const auto c=v.command(i);const size_t at=cmd+size_t(i)*20;
  if(c.opcode==uint32_t(DrawerOpcode::BranchFlag)){auto b=bytes;put(b,at+12,commands);reject(b,"branch target rejected");b=bytes;put(b,at+8,2);reject(b,"flag bool rejected");}
  if(c.opcode==uint32_t(DrawerOpcode::BranchSpace)){auto b=bytes;put(b,at+8,commands);reject(b,"capacity branch target rejected");b=bytes;put(b,at+4,2);reject(b,"capacity bool rejected");}
  if(c.opcode==uint32_t(DrawerOpcode::GrantItem)){auto b=bytes;put(b,at+4,v.count(DrawerSection::Templates));reject(b,"grant template rejected");}
  if(c.opcode==uint32_t(DrawerOpcode::SetFlag)){auto b=bytes;put(b,at+8,2);reject(b,"write flag boolean rejected");b=bytes;put(b,at+4,0);reject(b,"empty write flag rejected");}
  if(c.opcode==uint32_t(DrawerOpcode::PlaySound)){auto b=bytes;put(b,at+4,0);reject(b,"empty audio path rejected");}
  if(c.opcode==uint32_t(DrawerOpcode::ShowText)){auto b=bytes;put(b,at+4,0);reject(b,"zero text identity rejected");b=bytes;put(b,at,uint32_t(DrawerOpcode::AwaitText));for(size_t field=4;field<20;field+=4)put(b,at+field,0);reject(b,"each source phrase root needs a text display before acknowledgement");}
  if(c.opcode==uint32_t(DrawerOpcode::Jump)){auto b=bytes;put(b,at+4,i);reject(b,"cycle in source graph rejected");}
 }
 {Host h;h.reject_text=true;DrawerProgramRuntime r;check(!r.start(v,h,error),"missing text binding rejected");check(!h.effects&&r.state()==DrawerState::Idle,"preflight has no effects");}
 {Host h;h.reject_sound=true;DrawerProgramRuntime r;check(!r.start(v,h,error),"missing sound binding rejected before first text");check(!h.effects,"sound preflight has no effects");}
 {Host h;DrawerProgramRuntime r;check(r.start(v,h,error),error.c_str());check(r.state()==DrawerState::WaitingText&&h.text.size()==1&&!h.grants&&!h.sets,"initial phrase must await input before flag/capacity branches");check(!r.start(v,h,error),"active runtime cannot restart");finish(r,error);check(h.grants==1&&h.sets==1&&h.sounds==1&&h.got,"first acquisition effects once");const std::vector<DrawerOpcode>expected={DrawerOpcode::ShowText,DrawerOpcode::GrantItem,DrawerOpcode::ShowText,DrawerOpcode::PlaySound,DrawerOpcode::SetFlag};check(h.events==expected,"source item-before-text sound-before-flags order");check(!r.advance_text(error),"complete runtime rejects acknowledgement");}
 {Host h;h.got=true;DrawerProgramRuntime r;check(r.start(v,h,error),error.c_str());finish(r,error);check(!h.grants&&!h.sets&&!h.sounds&&h.text.size()==2,"repeat path has no item effects");}
 {Host h;h.space=false;DrawerProgramRuntime r;check(r.start(v,h,error),error.c_str());finish(r,error);check(!h.grants&&!h.sets&&!h.sounds&&h.text.size()==2,"full inventory path has no acquisition effects");}
 {Host h;h.reject_grant=true;DrawerProgramRuntime r;check(r.start(v,h,error),error.c_str());check(!r.advance_text(error)&&r.state()==DrawerState::Failed,"host grant failure stops execution");check(!h.grants&&!h.sets&&!h.sounds&&h.text.size()==1,"failed grant cannot print success/set flag/play sound");}
 ItemData items;check(items.load_file(argv[2],error),error.c_str());InventoryState inventory;check(inventory.initialize(items.view()),"initialize checked inventory");
 auto before=inventory.size();check(!inventory.append(UINT32_MAX,1,0,error)&&inventory.size()==before,"unknown grant definition atomic");check(!inventory.append(0,0,0,error)&&inventory.size()==before,"invalid grant doses atomic");
 if(before)check(!inventory.append(0,1,inventory.instance(0).id,error)&&inventory.size()==before,"duplicate grant UID atomic");
 uint32_t uid=0;auto unique=[&](){for(;;++uid){bool used=false;for(uint32_t i=0;i<inventory.size();++i)used|=inventory.instance(i).id==uid;if(!used)return uid++;}};
 while(inventory.has_space()){const uint32_t id=unique();check(inventory.can_append(0,1,error),"capacity preflight");check(inventory.append(0,1,id,error),error.c_str());check(inventory.instance(inventory.size()-1).id==id&&!inventory.instance(inventory.size()-1).equipped,"provided UID and unequipped source identity retained");}
 before=inventory.size();check(!inventory.can_append(0,1,error)&&!inventory.append(0,1,unique(),error)&&inventory.size()==before,"full grant atomic");
 std::cout<<"Drawer parser, branching, text gates, ordering and atomic inventory manual cases\n";
}
