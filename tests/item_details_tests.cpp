#include "encore/item_details.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Item details failed at %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
static uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
static void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
static void fix(std::vector<uint8_t>&b){put(b,16,0);put(b,16,encore::crc32(b.data(),b.size()));}
static uint32_t section(const std::vector<uint8_t>&b,ItemDetailsSection s){return get(b,64+(uint32_t(s)-1)*16+4);}
static std::string text(const ItemDetailsComposition&c){std::string s;for(const auto&a:c.atoms)if(a.kind==ItemDetailsTokenKind::Text)s+=a.text;return s;}
int main(int argc,char**argv){
 const std::string root=argc>1?argv[1]:"romfs";std::string error;ItemData items;ItemDetailsData data;std::vector<uint8_t>bytes;
 CHECK(items.load_file((root+"/data/opening.encitems").c_str(),error));CHECK(encore::read_file((root+"/data/opening.encdetails").c_str(),bytes,1024*1024,error));CHECK(data.load(bytes.data(),bytes.size(),error));const auto v=data.view();CHECK(v.bind_items(items.view(),error));CHECK(v.verify_resources(root.c_str(),error));CHECK(v.count(ItemDetailsSection::Definitions)==2);CHECK(v.count(ItemDetailsSection::Locales)==2);
 // This deliberately simple metric double tests semantic substitutions and
 // source word assembly. Device rendering uses the checked source font.
 const ItemDetailsMeasure measure=[](std::string_view s){float n=0;for(unsigned char c:s)if((c&0xc0)!=0x80)n+=6;return n;};
 ItemDetailsComposition cap,spray;CHECK(v.compose(0,1,"CUSTOM","en",512,measure,cap,error));CHECK(text(cap).find("Defense +5")!=std::string::npos);CHECK(cap.height>=2*v.parameter(ItemDetailsParameter::LineHeight));
 CHECK(v.compose(1,3,"CUSTOM","en",512,measure,spray,error));CHECK(text(spray).find("CUSTOM")!=std::string::npos);CHECK(text(spray).find("3\xc2\xa0uses.")!=std::string::npos);CHECK(text(spray).find("[Ninten]")==std::string::npos);CHECK(text(spray).find("%s")==std::string::npos);
 unsigned images=0;for(const auto&a:spray.atoms)if(a.kind==ItemDetailsTokenKind::InlineImage){++images;CHECK(a.resource<v.count(ItemDetailsSection::Resources));CHECK(a.width==v.resource(a.resource).width&&a.height==v.resource(a.resource).height);}CHECK(images==1);
 CHECK(v.compose(1,1,"ABCDEFGH","en",512,measure,spray,error));CHECK(text(spray).find("ABCDEFG")!=std::string::npos);CHECK(text(spray).find("ABCDEFGH")==std::string::npos);CHECK(text(spray).find("1\xc2\xa0use\xc2\xa0left.")!=std::string::npos);
 CHECK(v.compose(1,2,"CUSTOM","en",512,measure,spray,error));CHECK(text(spray).find("2\xc2\xa0uses\xc2\xa0left.")!=std::string::npos);CHECK(v.compose(1,0,"CUSTOM","en",512,measure,spray,error));CHECK(text(spray).find("0\xc2\xa0uses\xc2\xa0left.")!=std::string::npos);
 CHECK(v.compose(1,3,u8"自定义名字很长","zh_Hans_CN",512,measure,spray,error));CHECK(text(spray).find(u8"自定义名字很")!=std::string::npos);CHECK(text(spray).find(u8"可用3次。")!=std::string::npos);CHECK(v.compose(1,2,u8"小明","zh_Hans_CN",24,measure,spray,error));CHECK(spray.height>v.parameter(ItemDetailsParameter::LineHeight));
 const auto retained_text=text(spray);const auto retained_height=spray.height;auto reject_compose=[&](uint32_t id,uint32_t dose,std::string_view name,std::string_view lang,float width,const ItemDetailsMeasure&m){CHECK(!v.compose(id,dose,name,lang,width,m,spray,error));CHECK(text(spray)==retained_text&&spray.height==retained_height);};
 reject_compose(99,1,"X","en",100,measure);reject_compose(1,4,"X","en",100,measure);reject_compose(1,1,"X","fr",100,measure);reject_compose(1,1,"X","en",0,measure);reject_compose(1,1,"X","en",std::numeric_limits<float>::infinity(),measure);reject_compose(1,1,std::string("\xff"),"en",100,measure);reject_compose(1,1,"X","en",100,ItemDetailsMeasure{});reject_compose(1,1,"X","en",100,[](std::string_view){return std::numeric_limits<float>::quiet_NaN();});
 const auto retained=data.view();for(size_t n=0;n<bytes.size();++n){CHECK(!data.load(bytes.data(),n,error));CHECK(data.view().same_content(retained));}
 auto reject_edit=[&](size_t at,uint32_t value){auto b=bytes;put(b,at,value);fix(b);CHECK(!data.load(b.data(),b.size(),error));CHECK(data.view().same_content(retained));};
 for(auto x:std::vector<std::pair<size_t,uint32_t>>{{0,0},{8,2},{12,0},{20,8},{24,2},{28,2},{52,1},{64,0},{68,0},{72,UINT32_MAX},{76,0}})reject_edit(x.first,x.second);
 const auto defs=section(bytes,ItemDetailsSection::Definitions),locs=section(bytes,ItemDetailsSection::Locales),pres=section(bytes,ItemDetailsSection::Presentations),tokens=section(bytes,ItemDetailsSection::Tokens),resources=section(bytes,ItemDetailsSection::Resources),params=section(bytes,ItemDetailsSection::Parameters),pool=section(bytes,ItemDetailsSection::Strings);
 for(auto x:std::vector<std::pair<size_t,uint32_t>>{{defs+20,get(bytes,defs)},{defs+4,UINT32_MAX},{defs+12,0},{locs+32,get(bytes,locs)},{locs+4,UINT32_MAX},{pres+4,2},{pres+8,1},{pres+12,UINT32_MAX},{tokens,7},{tokens+8,2},{tokens+12,1},{resources+8,2},{resources+12,0},{resources+20,2},{resources+60,0},{params,99},{params+4,0x7fc00000},{params+4,0}})reject_edit(x.first,x.second);
 auto b=bytes;b[pool]=0xff;fix(b);CHECK(!data.load(b.data(),b.size(),error));b=bytes;b.back()^=1;CHECK(!data.load(b.data(),b.size(),error));b=bytes;b.push_back(0);put(b,12,uint32_t(b.size()));fix(b);CHECK(!data.load(b.data(),b.size(),error));
 b=bytes;std::fill(b.begin()+32,b.begin()+52,0);fix(b);CHECK(!data.load(b.data(),b.size(),error));
 for(uint32_t i=0;i<v.count(ItemDetailsSection::Tokens);++i)if(v.token(i).kind==ItemDetailsTokenKind::InlineImage)reject_edit(tokens+i*16+4,99);
 ItemDetailsData foreign;b=bytes;b[32]^=1;fix(b);CHECK(foreign.load(b.data(),b.size(),error));CHECK(!foreign.view().bind_items(items.view(),error));
 b=bytes;put(b,resources+64,get(b,resources+64)^1);fix(b);CHECK(foreign.load(b.data(),b.size(),error));CHECK(!foreign.view().verify_resources(root.c_str(),error));CHECK(!v.verify_resources("nonexistent-item-details-root",error));
 CHECK(ItemDetailsView{}.count(ItemDetailsSection::Strings)==0);CHECK(v.count(ItemDetailsSection(UINT32_MAX))==0);CHECK(v.string(UINT32_MAX).empty());CHECK(v.resource(UINT32_MAX).bytes==0);CHECK(v.parameter(ItemDetailsParameter(UINT32_MAX))==0);
 std::printf("Item details: %u manual checks; source substitution, UTF-8, typed image, malformed formats and transactional load\n",checks);
}
