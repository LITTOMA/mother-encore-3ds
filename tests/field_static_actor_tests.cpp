// Frozen output of the reviewed Mayor source-map candidate generator; no textures.
#include "encore/field_data.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
using namespace encore::upstream;
namespace {unsigned checks=0;std::string error;
void check(bool ok,const char*why){++checks;if(!ok){std::fprintf(stderr,"Static actors %u: %s (%s)\n",checks,why,error.c_str());std::exit(1);}}
#define CHECK(x) check(bool(x),#x)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(i*8));}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==3);std::vector<uint8_t>bytes,legacy;CHECK(encore::read_file(argv[1],bytes,1024*1024,error));CHECK(encore::read_file(argv[2],legacy,8*1024*1024,error));CHECK(get(bytes,20)==4&&get(legacy,20)==3);
 FieldMapData data;CHECK(data.load(bytes.data(),bytes.size(),error));const auto v=data.view();CHECK(v.scene_id()==3&&v.count(FieldMapSection::Doors)==3);unsigned root=0,sorted=0;size_t actor_at=0;
 for(unsigned g=0;g<v.count(FieldMapSection::Groups);++g){const auto group=v.group(g);for(unsigned k=0;k<group.count;++k){const auto index=group.first+k;const auto item=v.item(index);if(item.kind!=unsigned(FieldItemKind::Actor))continue;if(group.kind==unsigned(FieldGroupKind::Static)){++root;actor_at=get(bytes,64+13*16+4)+index*32;CHECK(item.first==1);CHECK(k>0&&k+1<group.count);CHECK(v.item(index-1).kind==unsigned(FieldItemKind::Sprites)&&v.item(index+1).kind==unsigned(FieldItemKind::Sprites));}else ++sorted;}}
 CHECK(root==1&&sorted==4&&actor_at);const auto identity=v.bytes_identity();
 auto reject=[&](std::vector<uint8_t>bad){sign(bad);CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.view().bytes_identity()==identity&&data.view().scene_id()==3);};
 for(auto edit:std::vector<std::pair<size_t,uint32_t>>{{20,1},{20,2},{20,3},{20,5},{actor_at,7},{actor_at+4,0},{actor_at+4,64},{actor_at+8,2},{actor_at+28,1}}){auto bad=bytes;put(bad,edit.first,edit.second);reject(bad);}
 auto bad=legacy;put(bad,20,4);reject(bad);reject(std::vector<uint8_t>(bytes.begin(),bytes.end()-1));
 FieldMapData prior;CHECK(prior.load(legacy.data(),legacy.size(),error)&&prior.view().scene_id()==2);
 std::printf("Static source actors PASS %u checks; root actor ordered between source door/desk sprites, sorted actors retained; cap1/2/3 downgrade, noncanonical4, unknown5, bad identity/count/reserved/truncation reject atomically; host metadata only\n",checks);
}
