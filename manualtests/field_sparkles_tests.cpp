#include "encore/field_sparkles.hpp"
#include "encore/field_present.hpp"
#include "encore/field_dropped.hpp"
#include <cassert>
#include <functional>
// Manual-only regression cases. This translation unit is compiled during
// development but the cases run only when a human explicitly requests tests.
namespace encore::upstream::manual {
namespace {
uint32_t word(const std::vector<uint8_t>&b,size_t p){return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(i*8));}
void crc(std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t j=128;j<b.size();++j){c^=b[j];for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320u:0u);}put(b,20,~c);}
size_t records_offset(const std::vector<uint8_t>&b){size_t p=128;auto text=[&](){auto n=word(b,p);p+=4+n;assert(p<=b.size());};text();text();p+=40;text();text();p+=72;auto count=word(b,p);p+=4;for(uint32_t i=0;i<count;++i){text();p+=8;auto frames=word(b,p);p+=4+frames*16;}assert(p<b.size());return p;}
}
void field_sparkles_parser_cases(const std::vector<uint8_t>&bytes,const FieldIdentity&id){
 std::string e;FieldSparklesData valid;assert(valid.load(bytes.data(),bytes.size(),id,e));auto before=valid.records().front().id;
 auto reject=[&](std::vector<uint8_t>b,bool repair){if(repair)crc(b);assert(!valid.load(b.data(),b.size(),id,e));assert(valid.valid()&&valid.records().front().id==before);};
 auto altered=[&](size_t p,uint32_t value,bool repair=true){auto b=bytes;put(b,p,value);reject(std::move(b),repair);};
 altered(8,2);altered(28,4);altered(24,0);altered(32,0);altered(124,1);auto b=bytes;b[40]^=1;reject(b,true);b=bytes;b[60]^=1;reject(b,true);b=bytes;b[20]^=1;reject(b,false);b=bytes;b.pop_back();put(b,16,uint32_t(b.size()));reject(b,true);b=bytes;b.push_back(1);put(b,16,uint32_t(b.size()));reject(b,true);
 const auto record=records_offset(bytes);altered(record+8,3);altered(record+20,0xffffffffu);altered(record+24,0xffffffffu);altered(record,0);altered(record+4,word(bytes,record));
}
void field_sparkles_owner_cases(const FieldSparklesData&s,const FieldPresentData&p,const FieldDroppedData&d){std::string e;assert(validate_sparkles_owner_bridge(s,p,d,e));FieldPresentData absent;assert(!validate_sparkles_owner_bridge(s,absent,d,e));}
}
