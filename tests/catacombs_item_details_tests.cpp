#include "encore/item_details.hpp"
#include "encore/content.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Extended details failed at %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
static std::string composed(const ItemDetailsComposition&value){std::string result;for(const auto&atom:value.atoms){if(atom.kind==ItemDetailsTokenKind::Text)result+=atom.text;else if(atom.kind==ItemDetailsTokenKind::InlineImage)result+="<image"+std::to_string(atom.resource)+">";}return result;}
int main(int argc,char**argv){
 std::string error;CHECK(argc==2);const std::string root=argv[1];
 ItemData items,legacy_items;ItemDetailsData data,legacy;std::vector<uint8_t>bytes;
 CHECK(items.load_file((root+"/candidate.encitems").c_str(),error));
 CHECK(legacy_items.load_file((root+"/legacy.encitems").c_str(),error));
 CHECK(legacy.load_file((root+"/legacy.encdetails").c_str(),error));
 CHECK(legacy.view().bind_items(legacy_items.view(),error));
 CHECK(legacy.view().count(ItemDetailsSection::Definitions)==4);
 CHECK(data.load_file((root+"/candidate.encdetails").c_str(),error));
 const auto view=data.view();CHECK(view.bind_items(items.view(),error));CHECK(!view.bind_items(legacy_items.view(),error));
 CHECK(!legacy.view().bind_items(items.view(),error));CHECK(view.verify_resources(root.c_str(),error));
 CHECK(!view.verify_resources((root+"/corrupt-textures").c_str(),error));CHECK(!view.verify_resources((root+"/missing-textures").c_str(),error));
 CHECK(view.count(ItemDetailsSection::Definitions)==7);CHECK(view.count(ItemDetailsSection::Resources)==2);
 CHECK(view.resource(0).id==1&&view.resource(1).id==2);CHECK(view.resource(0).width==14&&view.resource(1).width==15);
 const ItemDetailsMeasure metric=[](std::string_view s){float n=0;for(unsigned char c:s)if((c&0xc0)!=0x80)n+=6;return n;};
 for(uint32_t definition=4;definition<7;++definition)for(const std::string language:{"en","zh_Hans_CN"}){
  std::vector<uint8_t>expected;CHECK(encore::read_file((root+"/expected-"+std::to_string(definition)+"-"+language+".txt").c_str(),expected,8192,error));
  ItemDetailsComposition output;CHECK(view.compose(definition,1,"CUSTOM",language,8192,metric,output,error));
  CHECK(composed(output)==std::string(expected.begin(),expected.end()));
  unsigned images=0;for(const auto&atom:output.atoms)if(atom.kind==ItemDetailsTokenKind::InlineImage){++images;CHECK(atom.resource==1);CHECK(atom.width==15&&atom.height==12);}
  CHECK(images==(definition==6?1u:0u));
  if(definition<6)CHECK(output.height==2*view.parameter(ItemDetailsParameter::LineHeight));
  if(definition==4)CHECK(composed(output).find("99%")!=std::string::npos);
  CHECK(view.compose(definition,1,"CUSTOM",language,48,metric,output,error));CHECK(output.height>view.parameter(ItemDetailsParameter::LineHeight));
 }
 CHECK(encore::read_file((root+"/candidate.encdetails").c_str(),bytes,1024*1024,error));
 const auto retained=data.view();for(size_t size=0;size<bytes.size();++size){CHECK(!data.load(bytes.data(),size,error));CHECK(data.view().same_content(retained));}
 std::ifstream count_file(root+"/negative-count.txt");unsigned count=0;CHECK(bool(count_file>>count)&&count>=10);
 for(unsigned i=0;i<count;++i){std::vector<uint8_t>bad;CHECK(encore::read_file((root+"/bad-"+std::to_string(i)+".encdetails").c_str(),bad,1024*1024,error));CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.view().same_content(retained));}
 CHECK(data.load(bytes.data(),bytes.size(),error));
 std::printf("Extended item details: %u checks; seven source definitions, bilingual percentage/HP/icon composition, exact legacy fixture, %u negative variants\n",checks,count);
}
