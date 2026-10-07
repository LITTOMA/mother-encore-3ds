// Explicit manual negative cases only. Not registered in CMake/Make/CI and
// not executed during source preparation. Supply the real full House packs
// and their externally reviewed identity to this function when authorized.
#include "encore/house_return_controls.hpp"
#include <algorithm>
#include <cstring>
namespace encore::manual {
namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned j=0;j<4;++j)b[at+j]=uint8_t(v>>(j*8));}
void checksum(std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t j=128;j<b.size();++j){c^=b[j];for(unsigned k=0;k<8;++k)c=(c>>1)^((c&1)?0xedb88320u:0u);}put(b,16,uint32_t(b.size()));put(b,20,~c);}
bool replace(std::vector<uint8_t>&b,std::string_view from,std::string_view to){
 if(from.size()!=to.size())return false;auto at=std::search(b.begin()+128,b.end(),from.begin(),from.end());if(at==b.end())return false;std::copy(to.begin(),to.end(),at);checksum(b);return true;
}
}
bool house_controls_manual(const std::vector<uint8_t>&positive,
 const upstream::FieldIdentity&reviewed,const upstream::FieldNodeTreeData&tree,
 const upstream::FieldCanvasArtData&canvas,std::string&e){
 using upstream::HouseReturnControlsData;HouseReturnControlsData retained;
 if(!retained.load(positive.data(),positive.size(),reviewed,tree,canvas,e))return false;
 const auto ir=retained.ir_sha256();const auto count=retained.records().size();
 auto reject=[&](const std::vector<uint8_t>&bad,const char*case_name){
  std::string error;if(retained.load(bad.data(),bad.size(),reviewed,tree,canvas,error)||error.empty()||!retained.valid()||retained.ir_sha256()!=ir||retained.records().size()!=count||!retained.matches(tree,canvas,error)){
   e=std::string("House Control manual rejection/preserved Data failed: ")+case_name;return false;
  }return true;
 };
 if(positive.size()<128||word(positive.data()+16)!=positive.size()){e="House Control manual input is not a real complete pack";return false;}
 auto b=positive;put(b,8,2);if(!reject(b,"unknown format"))return false;
 b=positive;put(b,24,word(b.data()+24)^1);if(!reject(b,"foreign family"))return false;
 b=positive;put(b,28,2);if(!reject(b,"unknown capability"))return false;
 b=positive;put(b,32,2);if(!reject(b,"unknown rules"))return false;
 b=positive;b[36]^=1;if(!reject(b,"wrong actual House scene identity"))return false;
 b=positive;b[60]^=1;if(!reject(b,"wrong original source SHA"))return false;
 b=positive;if(!replace(b,"HBoxContainer","HBoxContaineX")||!reject(b,"unknown native class"))return false;
 b=positive;if(!replace(b,"TextureRect","TextureRecy")||!reject(b,"unknown actual TextureRect class"))return false;
 b=positive;if(!replace(b,"ColorRect","ColorRecy")||!reject(b,"unknown actual ColorRect class"))return false;
 b=positive;if(!replace(b,"anchor_left","anchor_lefx")||!reject(b,"unknown native property"))return false;
 b=positive;{
  std::string_view key="anchor_left";auto at=std::search(b.begin()+128,b.end(),key.begin(),key.end());
  if(at==b.end()||size_t(b.end()-at)<key.size()+4){e="House Control manual property fixture absent";return false;}
  put(b,size_t(at-b.begin())+key.size(),10);checksum(b);
  if(!reject(b,"unknown native property value type"))return false;
 }
 b=positive;if(!replace(b,"Fonts/BottleRocket.tres","Fonts/BottleRockex.tres")||!reject(b,"wrong source DynamicFont identity"))return false;
 // This corrupts a well-typed source native property and repairs CRC: the
 // canonical original property digest/Canvas crossbind must still reject.
 b=positive;if(!replace(b,"NodePath","NodePatz")||!reject(b,"wrong native NodePath type"))return false;
 b=positive;if(!replace(b,"_edit_lock_","_edit_locx_")||!reject(b,"unknown ColorRect editor metadata"))return false;
 b=positive;b.push_back(0);checksum(b);if(!reject(b,"trailing unknown payload"))return false;
 b=positive;b.resize(b.size()-1);checksum(b);if(!reject(b,"truncated source closure"))return false;
 auto wrong=reviewed;wrong.source_sha256[0]^=1;std::string error;
 if(retained.load(positive.data(),positive.size(),wrong,tree,canvas,error)||retained.ir_sha256()!=ir){e="House Control manual expected identity preservation failed";return false;}
 if(!retained.load(positive.data(),positive.size(),reviewed,tree,canvas,e))return false;
 e.clear();return true;
}
}
