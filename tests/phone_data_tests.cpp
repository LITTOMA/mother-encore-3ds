#include "encore/phone_data.hpp"
#include "encore/content.hpp"
#include <cstring>
#include <iostream>
#include <vector>
using namespace encore::upstream;
namespace {
int failures=0;
void check(bool v,const char*m){if(!v){std::cerr<<"FAIL: "<<m<<'\n';++failures;}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
void put(std::vector<uint8_t>&b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(i*8));}
uint32_t crc(const std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=0;i<b.size();++i){c^=(i>=16&&i<20)?0:b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
size_t offset(const std::vector<uint8_t>&b,PhoneSection s){return get(b,64+(uint32_t(s)-1)*16+4);}
}
int main(int argc,char**argv){
 if(argc!=2){std::cerr<<"usage: phone_data_tests opening.encphone\n";return 2;}
 std::vector<uint8_t>bytes;std::string error;PhoneData data;
 check(encore::read_file(argv[1],bytes,1024*1024,error),"read pack");
 if(bytes.empty())return 1;
 check(data.load(bytes.data(),bytes.size(),error),error.c_str());if(!data.view())return 1;
 auto view=data.view();check(view.count(PhoneSection::Objects)==1,"bounded phone object");
 auto object=view.object(0);auto resource=view.resource(object.resource);
 check(resource.width/resource.columns==19&&resource.height/resource.rows==22,"original atlas frame geometry");
 check(object.position.x==148&&object.position.y==677,"instance override position");
 check(object.sprite_center.x==148&&object.sprite_center.y==680,"sprite center");
 check(object.interact_center.x==148&&object.interact_center.y==692,"interaction center");
 check(object.collider_center.x==149&&object.collider_center.y==685.5f,"collider center");
 check(object.audio_center.x==148&&object.audio_center.y==686,"positional sound center");
 check(view.clip(object.idle_clip).role==uint32_t(PhoneClipRole::Idle)&&view.clip(object.ring_clip).role==uint32_t(PhoneClipRole::Ring),"typed clips");
 check(view.count(PhoneSection::FlagRefs)==2&&object.dispatch_count==2,"ordered flag dispatch");
 check(view.count(static_cast<PhoneSection>(0))==0&&view.count(static_cast<PhoneSection>(99))==0,"unknown section lookup");
 check(view.string(UINT32_MAX).empty()&&view.object(UINT32_MAX).resource==phone_no_index,"out of bounds read-only views");
 for(size_t size:{size_t(0),size_t(8),size_t(191),bytes.size()-1})check(!data.load(bytes.data(),size,error),"truncated pack rejected");
 check(!data.load(nullptr,bytes.size(),error),"null pack rejected");
 auto reject=[&](std::vector<uint8_t>changed,const char*message,bool rehash=true){if(rehash)put(changed,16,crc(changed));check(!data.load(changed.data(),changed.size(),error),message);check(data.view().object(0).id==object.id,"failed load preserves existing pack");};
 for(size_t field:{size_t(8),size_t(24),size_t(28)}){auto b=bytes;put(b,field,2);reject(b,"unknown schema/capability/rules");}
 {auto b=bytes;b[52]=1;reject(b,"unknown reserved header");}
 {auto b=bytes;b.back()^=1;reject(b,"CRC mismatch",false);}
 {auto b=bytes;b.push_back(0);put(b,12,b.size());reject(b,"trailing bytes");}
 {auto b=bytes;put(b,64+16+4,192);reject(b,"overlapping sections");}
 const size_t ob=offset(bytes,PhoneSection::Objects),cl=offset(bytes,PhoneSection::Clips);
 for(auto field:{2u,6u,7u}){auto b=bytes;put(b,ob+field*4,UINT32_MAX);reject(b,"invalid object binding");}
 {auto b=bytes;put(b,ob+48,object.policy|32);reject(b,"unknown policy bit");}
 {auto b=bytes;put(b,ob+48,object.policy&~16u);reject(b,"payphone not supported");}
 {auto b=bytes;put(b,ob+68,resource.columns*resource.rows);reject(b,"invalid initial frame");}
 {auto b=bytes;put(b,ob+72,0x7fc00000);reject(b,"nonfinite geometry");}
 {auto b=bytes;put(b,ob+96,0);reject(b,"zero interaction extent");}
 {auto b=bytes;put(b,ob+4,object.source_path+1);reject(b,"non-start string pointer");}
 {auto b=bytes;put(b,cl+4,99);reject(b,"unknown clip role");}
 {auto b=bytes;put(b,cl+24,1);reject(b,"idle loop rejected");}
 {auto b=bytes;put(b,cl+44+28,1);reject(b,"unreviewed source track order");}
 {auto b=bytes;put(b,offset(b,PhoneSection::FrameKeys)+8,UINT32_MAX);reject(b,"out of atlas frame");}
 {auto b=bytes;put(b,offset(b,PhoneSection::Dispatch),UINT32_MAX);reject(b,"unresolved flag reference");}
 {auto b=bytes;auto start=offset(b,PhoneSection::Strings);b[start+1]=0xff;reject(b,"invalid UTF-8");}
 std::cout<<"Phone pack checks: "<<failures<<" failures\n";return failures?1:0;
}
