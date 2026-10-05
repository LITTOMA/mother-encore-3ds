#include "encore/drawer_program.hpp"
#include "encore/content.hpp"
#include <cstring>
#include <set>

namespace encore::upstream {
namespace {
constexpr uint32_t header=128,sections=4,max_bytes=1024*1024,strides[]={1,16,20,8};
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
uint16_t u16(const uint8_t*p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=(i>=16&&i<20)?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
bool utf8(const uint8_t*p,size_t n){for(size_t i=0;i<n;){uint32_t c=p[i++];if(c<0x80)continue;unsigned extra;uint32_t low;if(c>=0xc2&&c<=0xdf){extra=1;low=0x80;c&=31;}else if(c>=0xe0&&c<=0xef){extra=2;low=0x800;c&=15;}else if(c>=0xf0&&c<=0xf4){extra=3;low=0x10000;c&=7;}else return false;if(extra>n-i)return false;while(extra--){auto b=p[i++];if((b&0xc0)!=0x80)return false;c=(c<<6)|(b&63);}if(c<low||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;}return true;}
bool safe(std::string_view p){if(p.empty()||p.front()=='/'||p.find(':')!=p.npos||p.find('\\')!=p.npos)return false;size_t a=0;while(a<=p.size()){auto b=p.find('/',a);if(b==p.npos)b=p.size();auto s=p.substr(a,b-a);if(s.empty()||s=="."||s=="..")return false;for(unsigned char ch:s)if(ch<32||ch==127)return false;if(b==p.size())break;a=b+1;}return true;}
std::vector<uint32_t> edges(DrawerCommand c,uint32_t pc){switch(DrawerOpcode(c.opcode)){case DrawerOpcode::End:return {};case DrawerOpcode::Jump:return {c.a};case DrawerOpcode::BranchFlag:return {pc+1,c.c};case DrawerOpcode::BranchSpace:return {pc+1,c.b};default:return {pc+1};}}
}
uint32_t DrawerProgramView::count(DrawerSection s)const{auto k=uint32_t(s);return bytes_&&k>=1&&k<=sections?u32(bytes_+64+(k-1)*16+8):0;}
const uint8_t*DrawerProgramView::record(DrawerSection s,uint32_t i)const{auto k=uint32_t(s);if(!bytes_||k<1||k>sections||i>=count(s))return nullptr;return bytes_+u32(bytes_+64+(k-1)*16+4)+size_t(i)*strides[k-1];}
std::string_view DrawerProgramView::string(uint32_t o)const{auto*p=record(DrawerSection::Strings,o);if(!p)return {};auto*e=static_cast<const uint8_t*>(std::memchr(p,0,count(DrawerSection::Strings)-o));return e?std::string_view(reinterpret_cast<const char*>(p),size_t(e-p)):std::string_view{};}
DrawerItemTemplate DrawerProgramView::item_template(uint32_t i)const{auto*p=record(DrawerSection::Templates,i);return p?DrawerItemTemplate{u32(p),u32(p+4),u32(p+8),u32(p+12)}:DrawerItemTemplate{};}
DrawerCommand DrawerProgramView::command(uint32_t i)const{auto*p=record(DrawerSection::Commands,i);return p?DrawerCommand{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16)}:DrawerCommand{};}
DrawerBinding DrawerProgramView::binding()const{auto*p=record(DrawerSection::Binding,0);return p?DrawerBinding{u32(p),u32(p+4)}:DrawerBinding{};}
bool DrawerProgramData::load(const uint8_t*input,size_t size,std::string&error){
 auto fail=[&](const char*s){error=s;return false;};
 if(!input||size<header||size>max_bytes)return fail("Drawer program size rejected");
 if(std::memcmp(input,"ENCDRP01",8)||u32(input+8)!=1||u32(input+12)!=size||u32(input+20)!=sections||u32(input+24)!=1||u32(input+28)!=1)return fail("Drawer program schema/capabilities/rules rejected");
 for(unsigned i=52;i<64;++i)if(input[i])return fail("Drawer program reserved bytes rejected");
 bool pin=false;for(unsigned i=32;i<52;++i)pin|=input[i]!=0;if(!pin)return fail("Drawer program provenance rejected");
 if(crc(input,size)!=u32(input+16))return fail("Drawer program CRC mismatch");
 size_t end=header;
 for(uint32_t i=0;i<sections;++i){auto*d=input+64+i*16;const auto off=u32(d+4),n=u32(d+8),bytes=u32(d+12);
  if(u16(d)!=i+1||u16(d+2)!=strides[i]||uint64_t(n)*strides[i]!=bytes)return fail("Drawer program directory rejected");
  if(!n){if(off||bytes)return fail("Drawer program empty section rejected");continue;}
  if(off%4||off<end||off>size||bytes>size-off)return fail("Drawer program section span rejected");
  for(size_t j=end;j<off;++j)if(input[j])return fail("Drawer program padding rejected");end=size_t(off)+bytes;
 }
 if(end!=size)return fail("Drawer program trailing bytes rejected");
 DrawerProgramView v;v.bytes_=input;v.size_=size;
 const auto pool_size=v.count(DrawerSection::Strings),templates=v.count(DrawerSection::Templates),commands=v.count(DrawerSection::Commands);
 if(!pool_size||pool_size>65536||templates>64||!commands||commands>1024)return fail("Drawer program capacity rejected");
 const auto*pool=v.record(DrawerSection::Strings,0);
 if(pool[0]||pool[pool_size-1]||!utf8(pool,pool_size))return fail("Drawer program strings rejected");
 auto str=[&](uint32_t o){return o<pool_size&&(o==0||pool[o-1]==0)&&v.string(o).size()<=4096&&safe(v.string(o));};
 const auto binding=v.binding();const auto source_path=v.string(binding.source_path);
 if(v.count(DrawerSection::Binding)!=1||!str(binding.source_path)||!str(binding.inspection_source)||source_path.substr(0,14)!="Data/Dialogue/"||source_path.size()<5||source_path.substr(source_path.size()-5)!=".yaml")return fail("Drawer program source binding rejected");
 std::set<uint32_t>ids;std::set<std::string_view>sources;
 for(uint32_t i=0;i<templates;++i){const auto t=v.item_template(i);if(!t.id||!ids.insert(t.id).second||!str(t.source)||v.string(t.source).find('/')!=std::string_view::npos||!sources.insert(v.string(t.source)).second||!t.doses||t.doses>65535||t.key_item>1)return fail("Drawer program item template rejected");}
 for(uint32_t i=0;i<commands;++i){const auto c=v.command(i);bool ok=false;
  switch(DrawerOpcode(c.opcode)){
   case DrawerOpcode::ShowText:ok=c.a&& !c.b&&!c.c&&!c.d;break;
   case DrawerOpcode::AwaitText:case DrawerOpcode::End:ok=!c.a&&!c.b&&!c.c&&!c.d;break;
   case DrawerOpcode::BranchFlag:ok=str(c.a)&&c.b<=1&&c.c<commands&&!c.d;break;
   case DrawerOpcode::BranchSpace:ok=c.a<=1&&c.b<commands&&!c.c&&!c.d;break;
   case DrawerOpcode::GrantItem:ok=c.a<templates&&!c.b&&!c.c&&!c.d;break;
   case DrawerOpcode::PlaySound:ok=str(c.a)&&!c.b&&!c.c&&!c.d;break;
   case DrawerOpcode::SetFlag:ok=str(c.a)&&c.b<=1&&!c.c&&!c.d;break;
   case DrawerOpcode::Jump:ok=c.a<commands&&!c.b&&!c.c&&!c.d;break;
  }
  if(!ok)return fail("Drawer program opcode/operand rejected");
  for(auto target:edges(c,i))if(target>=commands)return fail("Drawer program fallthrough rejected");
 }
 // No graph cycle is admitted, including unreachable source phrases. Every path
 // therefore terminates in End and cannot consume an unbounded frame budget.
 // Iterative topological admission avoids recursion on the console's stack.
 std::vector<uint32_t>incoming(commands),ready;
 for(uint32_t i=0;i<commands;++i)for(auto next:edges(v.command(i),i))++incoming[next];
 for(uint32_t i=0;i<commands;++i)if(!incoming[i])ready.push_back(i);
 const auto roots=ready;
 for(size_t i=0;i<ready.size();++i)for(auto next:edges(v.command(ready[i]),ready[i]))if(--incoming[next]==0)ready.push_back(next);
 if(ready.size()!=commands)return fail("Drawer program cycle rejected");
 // Validate both sides of conditionals: exactly one outstanding text span must
 // exist at every acknowledgement and none may be dropped by a later phrase.
 std::vector<uint8_t>seen(commands*2);std::vector<uint32_t>gates{0};seen[0]=1;
 for(auto root:roots)if(!seen[root*2]){seen[root*2]=1;gates.push_back(root*2);}
 for(size_t i=0;i<gates.size();++i){const uint32_t pc=gates[i]/2;bool pending=(gates[i]&1)!=0;const auto c=v.command(pc);
  switch(DrawerOpcode(c.opcode)){
   case DrawerOpcode::ShowText:if(pending)return fail("Drawer program overlapping text rejected");pending=true;break;
   case DrawerOpcode::AwaitText:if(!pending)return fail("Drawer program acknowledgement without text rejected");pending=false;break;
   case DrawerOpcode::End:if(pending)return fail("Drawer program unacknowledged text rejected");break;
   default:break;
  }
  for(auto next:edges(c,pc)){const uint32_t key=next*2+uint32_t(pending);if(!seen[key]){seen[key]=1;gates.push_back(key);}}
 }
 std::vector<uint8_t>next(input,input+size);bytes_.swap(next);error.clear();return true;
}
bool DrawerProgramData::load_file(const char*path,std::string&error){std::vector<uint8_t>bytes;return encore::read_file(path,bytes,max_bytes,error)&&load(bytes.data(),bytes.size(),error);}
bool DrawerProgramRuntime::start(DrawerProgramView v,DrawerHost&host,std::string&error){
 if(state_==DrawerState::WaitingText){error="Drawer program already active";return false;}
 if(!v.valid()){error="Drawer program requires checked content";return false;}
 for(uint32_t i=0;i<v.count(DrawerSection::Templates);++i){const auto t=v.item_template(i);if(!host.validate_item(t,v.string(t.source),error))return false;}
 for(uint32_t i=0;i<v.count(DrawerSection::Commands);++i){const auto c=v.command(i);switch(DrawerOpcode(c.opcode)){case DrawerOpcode::ShowText:if(!host.validate_text(c.a,error))return false;break;case DrawerOpcode::BranchFlag:case DrawerOpcode::SetFlag:if(!host.validate_flag(v.string(c.a),error))return false;break;case DrawerOpcode::PlaySound:if(!host.validate_sound(v.string(c.a),error))return false;break;default:break;}}
 view_=v;host_=&host;pc_=0;state_=DrawerState::Idle;error.clear();return step(error);
}
bool DrawerProgramRuntime::advance_text(std::string&error){if(state_!=DrawerState::WaitingText){error="Drawer program has no pending text";return false;}state_=DrawerState::Idle;return step(error);}
bool DrawerProgramRuntime::step(std::string&error){
 auto fail=[&](){state_=DrawerState::Failed;if(error.empty())error="Drawer host rejected execution";return false;};
 error.clear();
 for(uint32_t budget=0;budget<view_.count(DrawerSection::Commands);++budget){if(pc_>=view_.count(DrawerSection::Commands)){error="Drawer program PC rejected";return fail();}const auto c=view_.command(pc_++);
  switch(DrawerOpcode(c.opcode)){
   case DrawerOpcode::ShowText:if(!host_->show_text(c.a,error))return fail();break;
   case DrawerOpcode::AwaitText:state_=DrawerState::WaitingText;return true;
   case DrawerOpcode::BranchFlag:{bool value=false;if(!host_->flag(view_.string(c.a),value,error))return fail();if(value==bool(c.b))pc_=c.c;break;}
   case DrawerOpcode::BranchSpace:if(host_->inventory_space()==bool(c.a))pc_=c.b;break;
   case DrawerOpcode::GrantItem:{const auto t=view_.item_template(c.a);if(!host_->grant_item(t,view_.string(t.source),error))return fail();break;}
   case DrawerOpcode::PlaySound:if(!host_->play_sound(view_.string(c.a),error))return fail();break;
   case DrawerOpcode::SetFlag:if(!host_->set_flag(view_.string(c.a),bool(c.b),error))return fail();break;
   case DrawerOpcode::Jump:pc_=c.a;break;
   case DrawerOpcode::End:state_=DrawerState::Complete;return true;
   default:error="Drawer program opcode rejected";return fail();
  }
 }
 error="Drawer program execution budget exceeded";return fail();
}
}
