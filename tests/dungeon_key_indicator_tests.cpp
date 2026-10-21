#include "encore/dungeon_key_indicator.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Key HUD line%d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
bool near(float a,double b){return std::abs(double(a)-b)<.0001;}
uint32_t word(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
void put_real(std::vector<uint8_t>&b,size_t at,double v){uint64_t bits;std::memcpy(&bits,&v,8);for(size_t i=0;i<8;++i)b[at+i]=uint8_t(bits>>(i*8));}
bool same(const KeyIndicatorView&a,const KeyIndicatorView&b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.text==b.text&&a.count==b.count&&a.visible==b.visible&&a.open==b.open&&a.running==b.running;}
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>bytes;CHECK(encore::read_file(argv[1],bytes,512*1024,error));DungeonKeyIndicatorData data;CHECK(!data.valid()&&!data.keys().valid()&&data.config().initial_text.empty());CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(data.config().parameters[4]==.2&&data.config().parameters[7]==.1&&data.config().diagnostics==1&&data.config().threshold==0);
 const auto identity=data.keys().bytes_identity();auto reject=[&](std::vector<uint8_t>b){sign(b);CHECK(!data.load(b.data(),b.size(),error)&&data.keys().bytes_identity()==identity);DungeonKeyIndicatorData empty;CHECK(!empty.load(b.data(),b.size(),error)&&!empty.valid());};
 for(size_t n=0;n<bytes.size();++n)CHECK(!data.load(bytes.data(),n,error)&&data.keys().bytes_identity()==identity);CHECK(!data.load(nullptr,bytes.size(),error)&&!data.load_file("build/absent-key-indicator.enckui",error));
 for(size_t at:{8u,20u,24u,28u,32u,36u,40u,44u,48u,52u,56u,60u,136u,140u,144u,148u,152u,156u,160u,168u,172u}){auto b=bytes;put(b,at,0xffffffffu);reject(b);} {auto b=bytes;put(b,164,65536);reject(b);}
 for(size_t i=0;i<9;++i){auto b=bytes;put(b,64+i*8+4,0x7ff80000u);reject(b);}
 for(size_t i:{4u,7u,8u}){auto b=bytes;put(b,64+i*8,0);put(b,64+i*8+4,0);reject(b);}
 for(size_t i:{4u,7u,8u}){auto b=bytes;put_real(b,64+i*8,1e-300);reject(b);} {auto b=bytes;put(b,8,2);reject(b);b=bytes;put(b,20,2);reject(b);b=bytes;put(b,168,2);reject(b);b=bytes;put(b,164,0xffffffffu);sign(b);DungeonKeyIndicatorData known;CHECK(known.load(b.data(),b.size(),error)&&known.config().threshold== -1);}
 {auto b=bytes;for(size_t j=0;j<8;++j)b[64+3*8+j]=b[64+2*8+j];reject(b);b=bytes;b[word(b,44)]=0;reject(b);b=bytes;b[176]^=1;reject(b);b=bytes;b.back()^=1;CHECK(!data.load(b.data(),b.size(),error));}
 DungeonKeyIndicator hud;CHECK(!hud.prepare({},1,error)&&!hud.prepare(data,0,error)&&!hud.close(1,error)&&!hud.advance(1,0,error)&&!hud.retire(1,error));
 std::ifstream input(argv[2]);CHECK(input.good());std::string line;int previous=-1;unsigned rows=0,cases=0,diag_total=0;
 while(std::getline(input,line)){std::istringstream in(line);int ci,pi,count,visible,open,running,diags;char operation;double delta,x,y;std::string text;CHECK(bool(in>>ci>>pi>>operation>>delta>>count>>x>>y>>visible>>open>>running>>diags>>std::quoted(text)));std::string tail;CHECK(!(in>>tail));
  const uint64_t epoch=uint64_t(ci)+1;if(ci!=previous){CHECK(pi== -1&&operation=='I');previous=ci;++cases;hud={};CHECK(hud.prepare(data,epoch,error)&&!hud.prepare(data,epoch+1,error));}
  const auto before=hud.view();CHECK(!hud.update_counter(epoch+1,count,error)&&!hud.close(epoch+1,error)&&!hud.advance(epoch+1,delta,error)&&same(before,hud.view()));CHECK(!hud.update_counter(epoch,-1,error)&&!hud.update_counter(epoch,data.keys().policy().maximum+1,error)&&same(before,hud.view()));for(double bad:{-1.,61.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})CHECK(!hud.advance(epoch,bad,error)&&same(before,hud.view()));
  if(operation=='U')CHECK(hud.update_counter(epoch,count,error));else if(operation=='C')CHECK(hud.close(epoch,error));else if(operation=='T')CHECK(hud.advance(epoch,delta,error));else CHECK(operation=='I');const auto&v=hud.view();
  if(!near(v.position.x,x)||!near(v.position.y,y)||v.running!=bool(running))std::fprintf(stderr,"case%d/phase%d actual x%.9g y%.9g running%d expected %.9g %.9g %d\n",ci,pi,v.position.x,v.position.y,int(v.running),x,y,running);
  CHECK(near(v.position.x,x)&&near(v.position.y,y)&&v.visible==bool(visible)&&v.open==bool(open)&&v.running==bool(running)&&v.count==count&&v.text==text);CHECK(hud.diagnostics().size()==size_t(diags));for(const auto&d:hud.diagnostics())CHECK(d.epoch==epoch&&d.kind==KeyIndicatorDiagnosticKind::AppendAfterStarted&&d.count==count);diag_total+=diags;hud.clear_diagnostics();++rows;
 }
 CHECK(input.eof()&&rows==91&&cases==8&&diag_total==3);
 // A resource without the reviewed diagnostic cannot partially publish the
 // label/count change when the original update hits the invalid append path.
 {auto b=bytes;put(b,60,0);sign(b);DungeonKeyIndicatorData strict;CHECK(strict.load(b.data(),b.size(),error));DungeonKeyIndicator refused;CHECK(refused.prepare(strict,100,error)&&refused.update_counter(100,1,error)&&refused.advance(100,.2,error)&&refused.update_counter(100,2,error)&&refused.advance(100,.05,error));const auto before=refused.view();CHECK(!refused.update_counter(100,3,error)&&same(before,refused.view())&&refused.diagnostics().empty());}
 // Explicit component capacity probes, not a claim that the current player
 // can collect more than the source policy's three physical keys.
 hud={};CHECK(hud.prepare(data,200,error)&&hud.update_counter(200,1,error)&&hud.advance(200,.2,error));for(unsigned i=0;i<128;++i)CHECK(hud.update_counter(200,2,error));auto before=hud.view();CHECK(!hud.update_counter(200,3,error)&&same(before,hud.view())&&hud.diagnostics().empty());CHECK(hud.close(200,error)&&hud.advance(200,.2,error)&&!hud.view().visible);
 for(unsigned i=0;i<256;++i){CHECK(hud.update_counter(200,1,error)&&hud.advance(200,.2,error)&&hud.update_counter(200,2,error)&&hud.advance(200,.05,error)&&hud.update_counter(200,3,error));CHECK(hud.close(200,error)&&hud.advance(200,.2,error));}CHECK(hud.diagnostics().size()==256);CHECK(hud.update_counter(200,1,error)&&hud.advance(200,.2,error)&&hud.update_counter(200,2,error)&&hud.advance(200,.05,error));before=hud.view();CHECK(!hud.update_counter(200,3,error)&&same(before,hud.view())&&hud.diagnostics().size()==256);hud.clear_diagnostics();CHECK(hud.update_counter(200,3,error)&&hud.diagnostics().size()==1);
 CHECK(!hud.retire(201,error)&&hud.epoch()==200);CHECK(hud.retire(200,error)&&hud.epoch()==0&&hud.diagnostics().empty()&&!hud.close(200,error)&&!hud.advance(200,.2,error));CHECK(!hud.prepare(data,200,error)&&hud.prepare(data,201,error));CHECK(hud.view().visible&&!hud.view().open&&!hud.view().running&&hud.view().text==data.config().initial_text);CHECK(hud.close(201,error)&&hud.advance(201,.2,error)&&hud.view().visible);
 std::printf("Key HUD:%u checks;91 original native states/8 cases,3 typed source append failures,open/close/reopen cancellation,prestart sequences and inherited linear/Sine curves,source f32 time/remainder,zero-delta lifecycle,all truncations and unknown headers/nested identities/doubles/text rejected,atomic capacity and epoch retirement;no glyph/icon/fullscene/save claim\n",checks);
}
