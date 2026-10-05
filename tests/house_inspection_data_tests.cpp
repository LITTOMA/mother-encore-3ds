#include "encore/house_inspection_data.hpp"
#include "encore/content.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool ok,const char*why){++checks;if(!ok){std::cerr<<"Inspection pack check "<<checks<<": "<<why<<'\n';std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);}
void put(std::vector<uint8_t>&b,size_t p,uint32_t n){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(n>>(i*8));}
void put_float(std::vector<uint8_t>&b,size_t p,float value){uint32_t bits;std::memcpy(&bits,&value,4);put(b,p,bits);}
uint32_t crc(const std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=0;i<b.size();++i){c^=(i>=16&&i<20)?0:b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
size_t offset(const std::vector<uint8_t>&b,HouseInspectionSection s){return get(b,64+(uint32_t(s)-1)*16+4);}
}
int main(int argc,char**argv){
 check(argc==2,"actual opening.encinspect path required");std::vector<uint8_t>bytes;std::string error;HouseInspectionData data;
 check(encore::read_file(argv[1],bytes,1024*1024,error),error.c_str());check(data.load(bytes.data(),bytes.size(),error),error.c_str());
 const auto view=data.view();const auto first=view.object(0);const uint32_t count=view.count(HouseInspectionSection::Objects);
 check(count>=2&&first.id&&first.interact_extents.x>0&&first.interact_extents.y>0,"bounded admitted source objects");
 check(view.count(static_cast<HouseInspectionSection>(0))==0&&view.count(static_cast<HouseInspectionSection>(99))==0,"unknown section lookup");
 check(view.object(UINT32_MAX).default_dialogue_index==house_inspection_no_index&&view.string(UINT32_MAX).empty(),"unknown record lookup");
 auto reject=[&](std::vector<uint8_t>b,const char*why,bool rehash=true){if(rehash)put(b,16,crc(b));check(!data.load(b.data(),b.size(),error),why);check(data.view().object(0).id==first.id&&data.view().count(HouseInspectionSection::Objects)==count,"failed admission preserves previous bytes");};
 for(size_t size:{size_t(0),size_t(8),size_t(111),bytes.size()-1})check(!data.load(bytes.data(),size,error),"truncated pack rejected");
 check(!data.load(nullptr,bytes.size(),error),"null input rejected");
 for(size_t field:{size_t(8),size_t(20),size_t(24),size_t(28)}){auto b=bytes;put(b,field,99);reject(b,"unknown schema/directory count/capability/rules");}
 {auto b=bytes;b[0]^=1;reject(b,"unknown magic");}
 {auto b=bytes;b[52]=1;reject(b,"reserved header rejected");}
 {auto b=bytes;for(size_t i=32;i<52;++i)b[i]=0;reject(b,"empty provenance rejected");}
 {auto b=bytes;b.back()^=1;reject(b,"CRC mismatch",false);}
 {auto b=bytes;b.push_back(0);put(b,12,uint32_t(b.size()));reject(b,"trailing bytes rejected");}
 {auto b=bytes;put(b,84,112);reject(b,"overlapping directory span rejected");}
 {auto b=bytes;b[66]=2;reject(b,"wrong string stride rejected");}
 const size_t ob=offset(bytes,HouseInspectionSection::Objects),ov=offset(bytes,HouseInspectionSection::Overrides),pool=offset(bytes,HouseInspectionSection::Strings);
 {auto b=bytes;put(b,ob,0);reject(b,"zero identity rejected");}
 {auto b=bytes;put(b,ob+76,first.id);reject(b,"duplicate identity rejected");}
 {auto b=bytes;put(b,ob+76+4,first.source_path);reject(b,"duplicate object source rejected");}
 {auto b=bytes;put(b,ob+4,first.source_path+1);reject(b,"non-start string rejected");}
 {auto b=bytes;b[pool+first.source_path]=':';reject(b,"unsafe source path rejected");}
 {auto b=bytes;b[pool+1]=0xff;reject(b,"invalid UTF-8 rejected");}
 {auto b=bytes;put(b,ob+20,4);reject(b,"unknown player turn bit rejected");}
 {auto b=bytes;put(b,ob+32,0);reject(b,"missing collision binding rejected");}
 {auto b=bytes;put(b,ob+36,first.source_path);reject(b,"invented inspection seen identity rejected");}
 {auto b=bytes;put(b,ob+40,1024);reject(b,"out of schema text index rejected");}
 {auto b=bytes;put(b,ob+44,0x7fc00000);reject(b,"nonfinite position rejected");}
 for(size_t field=44;field<76;field+=4){auto b=bytes;put_float(b,ob+field,10001.0f);reject(b,"geometry above source schema bound rejected");}
 for(size_t field:{size_t(44),size_t(48),size_t(52),size_t(56),size_t(68),size_t(72)}){auto b=bytes;put_float(b,ob+field,-10001.0f);reject(b,"negative geometry below source schema bound rejected");}
 {auto b=bytes;put(b,ob+60,0);reject(b,"empty ray geometry rejected");}
 {auto b=bytes;put(b,ob+24,UINT32_MAX);reject(b,"out of range override span rejected");}
 if(view.count(HouseInspectionSection::Overrides)){
  {auto b=bytes;put(b,ov,count);reject(b,"wrong override owner rejected");}
  {auto b=bytes;put(b,ov+4,0);reject(b,"empty condition flag rejected");}
  {auto b=bytes;put(b,ov+8,0);reject(b,"empty override source rejected");}
  {auto b=bytes;put(b,ov+12,1024);reject(b,"out of schema override text index rejected");}
  check(first.override_count>=2,"actual source object supplies two ordered override conditions");
  {auto b=bytes;const size_t at=ov+size_t(first.first_override)*16;put(b,at+16+4,get(b,at+4));reject(b,"duplicate condition flags within one object rejected");}
 }
 std::cout<<"House inspection pack: "<<checks<<" checks\n";
}
