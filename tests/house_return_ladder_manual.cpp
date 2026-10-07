#include "encore/house_return_ladder.hpp"
#include "encore/player_motion.hpp"
#include "manual_require.hpp"
#include <fstream>
#include <iterator>
#include <utility>

using namespace encore::upstream;
namespace {
using Bytes=std::vector<uint8_t>;
uint32_t word(const Bytes&b,size_t p){
 MANUAL_REQUIRE(p<=b.size()&&b.size()-p>=4);
 return uint32_t(b[p])|uint32_t(b[p+1])<<8|uint32_t(b[p+2])<<16|uint32_t(b[p+3])<<24;
}
void put(Bytes&b,size_t p,uint32_t v){
 MANUAL_REQUIRE(p<=b.size()&&b.size()-p>=4);
 for(unsigned k=0;k<4;++k)b[p+k]=uint8_t(v>>(k*8));
}
void seal(Bytes&b){
 uint32_t crc=~0u;
 for(size_t p=128;p<b.size();++p){crc^=b[p];for(unsigned k=0;k<8;++k)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}
 put(b,16,uint32_t(b.size()));put(b,20,~crc);
}
struct Cursor {
 const Bytes&b;size_t at=128;
 void skip(size_t n){MANUAL_REQUIRE(at<=b.size()&&n<=b.size()-at);at+=n;}
 uint32_t integer(){auto v=word(b,at);skip(4);return v;}
 void text(){auto n=integer();skip(n);}
};
struct Layout {
 size_t id=0,owner=0,speed=0,dependency=0,text_count=0,first_text=0;
 size_t connection_flags=0,connection_args=0,body_first=0,source_hash=0;
 explicit Layout(const Bytes&b){
  Cursor c{b};c.text();c.text();id=c.at;owner=id+8;c.skip(8*4+12*4);
  speed=c.at;c.skip(2*4);dependency=c.at;c.skip(3*32);
  text_count=c.at;const auto texts=c.integer();first_text=c.at+4;
  for(uint32_t k=0;k<texts;++k)c.text();
  MANUAL_REQUIRE(c.integer()==2);
  for(unsigned k=0;k<2;++k){c.text();c.text();if(!k){connection_flags=c.at;connection_args=c.at+4;}c.skip(8);}
  const auto bodies=c.integer();MANUAL_REQUIRE(bodies>=2);body_first=c.at;c.skip(size_t(bodies)*4);
  MANUAL_REQUIRE(c.integer()>0);c.text();source_hash=c.at;
 }
};
struct Snapshot {
 HouseReturnLadderData copy;
 const std::string*text_address=nullptr;
 const uint32_t*body_address=nullptr;
 explicit Snapshot(const HouseReturnLadderData&d):copy(d),
  text_address(&d.text(HouseLadderText::Script)),body_address(d.non_party_bodies().data()){}
 void unchanged(const HouseReturnLadderData&d)const{
  MANUAL_REQUIRE(d.valid()&&d.identity().scene_id==copy.identity().scene_id&&
   d.identity().upstream_commit==copy.identity().upstream_commit&&
   d.identity().source_sha256==copy.identity().source_sha256&&d.ir_sha256()==copy.ir_sha256());
  MANUAL_REQUIRE(d.scene()==copy.scene()&&d.node()==copy.node()&&d.id()==copy.id()&&
   d.shape_id()==copy.shape_id()&&d.owner_index()==copy.owner_index()&&d.shape_index()==copy.shape_index()&&
   d.stopped_speed()==copy.stopped_speed()&&d.position_y()==copy.position_y());
  MANUAL_REQUIRE(&d.text(HouseLadderText::Script)==text_address&&
   d.non_party_bodies().data()==body_address&&d.non_party_bodies()==copy.non_party_bodies());
  for(size_t k=0;k<size_t(HouseLadderText::Count);++k)
   MANUAL_REQUIRE(d.text(HouseLadderText(k))==copy.text(HouseLadderText(k)));
  for(size_t k=0;k<d.connections().size();++k){const auto&a=d.connections()[k],&b=copy.connections()[k];
   MANUAL_REQUIRE(a.signal==b.signal&&a.method==b.method&&a.flags==b.flags&&a.arguments==b.arguments);}
  for(auto role:{HouseLadderText::Script,HouseLadderText::PartyBase,HouseLadderText::PlayerScript,HouseLadderText::FollowerScript}){
   std::array<uint8_t,32>a{},b{};MANUAL_REQUIRE(d.source_hash(d.text(role),a)&&copy.source_hash(copy.text(role),b)&&a==b);}
 }
};
}
// Authored manual cases only. An explicit external driver must supply the
// actual independently checked resource identity and loaded source resources.
// No main, registration, automatic workflow or gameplay execution is added.
void house_return_ladder_manual(const char*file,const FieldIdentity&identity,
 const FieldNodeTreeData&tree,const FieldGeometryView&geometry,
 const PlayerInitializationData&initialization,const PlayerReadyData&ready,const PlayerMotionData&motion){
 MANUAL_REQUIRE(file&&*file&&tree.valid()&&tree.records().size()==497);
 std::ifstream stream(file,std::ios::binary);MANUAL_REQUIRE(stream.good());
 const Bytes bytes(std::istreambuf_iterator<char>{stream},std::istreambuf_iterator<char>{});
 MANUAL_REQUIRE(bytes.size()>=128);std::string error;HouseReturnLadderData data;
 MANUAL_REQUIRE(data.load(bytes.data(),bytes.size(),identity,tree,geometry,initialization,ready,motion,error));
 MANUAL_REQUIRE(data.matches(tree,geometry,initialization,ready,motion,error));
 MANUAL_REQUIRE(data.connections().size()==2&&!data.non_party_bodies().empty());
 const Snapshot before(data);const Layout layout(bytes);
 const auto reject=[&](Bytes broken,bool reseal=true){
  if(reseal)seal(broken);error.clear();
  MANUAL_REQUIRE(!data.load(broken.data(),broken.size(),identity,tree,geometry,initialization,ready,motion,error));
  MANUAL_REQUIRE(!error.empty());before.unchanged(data);
 };
 for(size_t at:{size_t(8),size_t(24),size_t(28),size_t(32)}){auto b=bytes;put(b,at,word(b,at)+1);reject(std::move(b));}
 auto b=bytes;b[0]^=1;reject(std::move(b));
 b=bytes;put(b,124,1);reject(std::move(b));
 for(size_t at:{size_t(36),size_t(40),size_t(60),layout.source_hash,layout.dependency}){b=bytes;b[at]^=1;reject(std::move(b));}
 b=bytes;for(size_t k=92;k<124;++k)b[k]=0;reject(std::move(b));
 b=bytes;put(b,layout.text_count,uint32_t(HouseLadderText::Count)+1);reject(std::move(b));
 b=bytes;b[layout.first_text]=0xff;reject(std::move(b));
 b=bytes;b[layout.first_text]=0;reject(std::move(b));
 b=bytes;put(b,layout.connection_flags,4);reject(std::move(b));
 b=bytes;put(b,layout.connection_args,0);reject(std::move(b));
 b=bytes;put(b,layout.body_first+4,word(b,layout.body_first));reject(std::move(b));
 b=bytes;put(b,layout.id,0);reject(std::move(b));
 b=bytes;put(b,layout.owner,0xffffffffu);reject(std::move(b));
 b=bytes;put(b,layout.speed,0x3f800000u);reject(std::move(b));
 b=bytes;put(b,layout.speed,0x7fc00000u);reject(std::move(b));
 b=bytes;b.back()^=1;reject(std::move(b),false); // CRC corruption.
 b=bytes;b.pop_back();reject(std::move(b));      // Resealed truncated closure.
 b=bytes;b.push_back(0);reject(std::move(b));    // Resealed unknown trailing payload.
 error.clear();auto wrong=identity;wrong.scene_id^=1;
 MANUAL_REQUIRE(!data.load(bytes.data(),bytes.size(),wrong,tree,geometry,initialization,ready,motion,error));
 before.unchanged(data);
}
