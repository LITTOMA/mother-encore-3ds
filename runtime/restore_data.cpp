#include "encore/restore_data.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

namespace encore::upstream { namespace {
constexpr size_t max_bytes=1024*1024;
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
bool fail(std::string&e,const char*s){e=s;return false;}
uint32_t rotate(uint32_t v,unsigned n){return (v>>n)|(v<<(32-n));}
// SHA-256 algorithm constants are format integrity machinery, not game data.
std::array<uint8_t,32> sha256(const uint8_t*data,size_t size){
 static constexpr uint32_t constants[64]={
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 uint32_t hash[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 const size_t blocks=(size+9+63)/64;
 for(size_t block=0;block<blocks;++block){uint8_t raw[64]{};for(size_t j=0;j<64;++j){const size_t offset=block*64+j;if(offset<size)raw[j]=data[offset];else if(offset==size)raw[j]=0x80;}
  if(block+1==blocks){const uint64_t bits=uint64_t(size)*8;for(unsigned j=0;j<8;++j)raw[63-j]=uint8_t(bits>>(j*8));}
  uint32_t w[64];for(unsigned j=0;j<16;++j)w[j]=uint32_t(raw[j*4])<<24|uint32_t(raw[j*4+1])<<16|uint32_t(raw[j*4+2])<<8|raw[j*4+3];
  for(unsigned j=16;j<64;++j){const auto a=w[j-15],b=w[j-2];w[j]=w[j-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[j-7]+(rotate(b,17)^rotate(b,19)^(b>>10));}
  uint32_t a=hash[0],b=hash[1],c=hash[2],d=hash[3],e=hash[4],f=hash[5],g=hash[6],h=hash[7];
  for(unsigned j=0;j<64;++j){const auto t1=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[j]+w[j],t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
  hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;hash[4]+=e;hash[5]+=f;hash[6]+=g;hash[7]+=h;
 }
 std::array<uint8_t,32>out{};for(unsigned i=0;i<8;++i)for(unsigned j=0;j<4;++j)out[i*4+j]=uint8_t(hash[i]>>(24-j*8));return out;
}
struct Reader {
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t number(){if(n<4){ok=false;return 0;}auto value=u32(p);p+=4;n-=4;return value;}
 uint32_t count(uint32_t maximum){auto value=number();if(value>maximum){ok=false;return 0;}return value;}
 bool boolean(){auto value=number();if(value>1)ok=false;return value!=0;}
 float f32(){auto bits=number();float value;std::memcpy(&value,&bits,4);return value;}
 double f64(){const auto low=number(),high=number();uint64_t bits=uint64_t(low)|uint64_t(high)<<32;double value;std::memcpy(&value,&bits,8);return value;}
 Vec2 vec(){const float x=f32(),y=f32();return {x,y};}
 std::array<uint8_t,32>hash(){std::array<uint8_t,32>v{};if(n<32){ok=false;return v;}std::copy_n(p,32,v.begin());p+=32;n-=32;return v;}
 std::string text(){const auto size=count(4096);if(size>n){ok=false;return {};}std::string value(reinterpret_cast<const char*>(p),size);p+=size;n-=size;for(unsigned char c:value)if(c<32||c>126)ok=false;return value;}
 RestoreCondition condition(RoomView room){RestoreCondition c;c.flag_index=number();c.flag_id=number();c.expected_value=boolean();c.flag_name=text();if(c.flag_index>=room.flag_count()||room.flag(c.flag_index).stable_id!=c.flag_id||room.string(room.flag(c.flag_index).name_string)!=c.flag_name)ok=false;return c;}
};
bool finite(Vec2 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<=1000000&&std::abs(v.y)<=1000000;}
bool path(std::string_view p){if(p.empty()||p.front()=='/'||p.find(':')!=p.npos||p.find('\\')!=p.npos)return false;size_t at=0;while(at<=p.size()){auto end=p.find('/',at);if(end==p.npos)end=p.size();const auto part=p.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;if(end==p.size())break;at=end+1;}return true;}
bool identifier(std::string_view s){if(s.empty()||!((s[0]>='A'&&s[0]<='Z')||(s[0]>='a'&&s[0]<='z')))return false;for(auto c:s)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
}
bool RestoreData::matches(RoomView room,HouseView house)const{
 return valid_&&room.valid()&&house.valid()&&room_bytes_==room.byte_size()&&house_bytes_==house.byte_size()&&room_sha256_==sha256(room.bytes(),room.byte_size())&&house_sha256_==sha256(house.bytes(),house.byte_size());
}
bool RestoreData::load(const uint8_t*p,size_t n,RoomView room,HouseView house,std::string&e){
 if(!room.valid()||!house.valid())return fail(e,"Restore requires checked Room and House data");
 if(!p||n<24||n>max_bytes)return fail(e,"Restore pack size rejected");
 if(std::memcmp(p,"ENCREST1",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1)return fail(e,"Restore schema/size/capability rejected");
 if(crc(p+24,n-24)!=u32(p+16))return fail(e,"Restore CRC mismatch");
 Reader r{p+24,n-24};RestoreData next;
 const auto room_size=r.number();const auto room_sha=r.hash();const auto house_size=r.number();const auto house_sha=r.hash();
 if(room_size!=room.byte_size()||house_size!=house.byte_size()||room_sha!=sha256(room.bytes(),room.byte_size())||house_sha!=sha256(house.bytes(),house.byte_size()))return fail(e,"Restore Room/House fingerprint mismatch");
 next.room_bytes_=room_size;next.house_bytes_=house_size;next.room_sha256_=room_sha;next.house_sha256_=house_sha;
 const auto scene=r.number();const auto scene_path=r.text();if(scene!=room.scene().stable_id||scene_path!=room.string(room.scene().source_scene_string))return fail(e,"Restore scene identity mismatch");
 const auto npc_count=r.count(256);std::set<std::pair<uint32_t,uint32_t>>npc_flags;
 for(uint32_t i=0;i<npc_count;++i){RestoreNpcEventPosition v;v.npc_index=r.number();v.npc_id=r.number();v.actor_index=r.number();v.actor_id=r.number();v.body_id=r.number();v.source_path=r.text();v.condition=r.condition(room);v.position=r.vec();
  if(v.npc_index>=house.count(HouseSection::Npcs)||v.actor_index>=room.actor_instance_count()||!path(v.source_path)||!finite(v.position))return fail(e,"Restore NPC index/path/geometry rejected");
  const auto npc=house.npc(v.npc_index);bool body_found=false;for(uint32_t j=0;j<room.body_rule_count();++j){const auto b=room.body_rule(j);body_found|=b.body_id==v.body_id&&room.string(b.source_path_string)==v.source_path;}
  if(npc.id!=v.npc_id||npc.room_actor_index!=v.actor_index||npc.body_id!=v.body_id||house.string(npc.source_path)!=v.source_path||room.actor_instance(v.actor_index).stable_id!=v.actor_id||!body_found||!npc_flags.emplace(v.npc_index,v.condition.flag_index).second)return fail(e,"Restore NPC cross-pack binding rejected");
  next.npc_event_positions_.push_back(std::move(v));
 }
 const auto area_count=r.count(128);if(!area_count)return fail(e,"Restore music area coverage missing");std::set<uint32_t>area_ids;std::set<std::string>area_paths;
 for(uint32_t i=0;i<area_count;++i){RestoreMusicArea v;v.id=r.number();v.room_resource_index=r.number();v.room_resource_id=r.number();v.supported=r.boolean();v.source_path=r.text();v.resource_path=r.text();const auto hash=r.hash();v.source_sha256=hash;v.center=r.vec();v.extents=r.vec();v.volume_db=r.f64();v.fadein_seconds=r.f64();v.fadeout_seconds=r.f64();
  if(!v.id||!area_ids.insert(v.id).second||!area_paths.insert(v.source_path).second||!path(v.source_path)||v.resource_path.rfind("res://",0)!=0||!path(std::string_view(v.resource_path).substr(6))||!finite(v.center)||!finite(v.extents)||v.extents.x<=0||v.extents.y<=0)return fail(e,"Restore music identity/path/geometry rejected");
  if(!std::isfinite(v.volume_db)||v.volume_db < -100||v.volume_db>24||!std::isfinite(v.fadein_seconds)||v.fadein_seconds<0||v.fadein_seconds>120||!std::isfinite(v.fadeout_seconds)||v.fadeout_seconds<0||v.fadeout_seconds>120)return fail(e,"Restore music volume/fade rejected");
  if(std::all_of(hash.begin(),hash.end(),[](uint8_t b){return !b;}))return fail(e,"Restore music source fingerprint missing");
  if(v.supported){if(v.room_resource_index>=room.resource_count())return fail(e,"Restore music resource index rejected");const auto resource=room.resource(v.room_resource_index);if(resource.stable_id!=v.room_resource_id||resource.kind!=uint16_t(RoomResourceKind::AudioRequestOnly)||resource.sha256!=hash||room.string(resource.path_string)!=v.resource_path)return fail(e,"Restore music cross-pack binding rejected");}
  else {if(v.room_resource_index!=kRoomNoIndex||v.room_resource_id)return fail(e,"Restore unsupported music binding rejected");for(uint32_t j=0;j<room.resource_count();++j)if(room.string(room.resource(j).path_string)==v.resource_path)return fail(e,"Restore unsupported music is already mapped");}
  const auto count=r.count(16);if(!count)return fail(e,"Restore music conditions missing");std::set<uint32_t>unique;
  for(uint32_t j=0;j<count;++j){auto c=r.condition(room);if(!unique.insert(c.flag_index).second)return fail(e,"Restore duplicate music flag");v.conditions.push_back(std::move(c));}next.music_areas_.push_back(std::move(v));
 }
 const auto policy=r.number();if(policy!=uint32_t(RestoreUidPolicy::EagerFallbackBeforeSavedUid))return fail(e,"Restore UID policy rejected");next.uid_policy_=RestoreUidPolicy(policy);
 const auto inventory_count=r.count(128);if(inventory_count<3)return fail(e,"Restore inventory registry missing");std::set<std::string>characters;
 for(uint32_t i=0;i<inventory_count;++i){RestoreInventoryLoad v;v.order_id=r.number();const auto kind=r.number();v.rebuilds_inventory=r.boolean();v.character_id=r.text();
  if(v.order_id!=i+1||kind!=(i<2?i+1:3))return fail(e,"Restore inventory ordering rejected");
  v.kind=RestoreInventoryKind(kind);
  if(i<2?(!v.character_id.empty()||!v.rebuilds_inventory):(!identifier(v.character_id)||!characters.insert(v.character_id).second))return fail(e,"Restore inventory character identity rejected");
  const auto count=r.count(256);if(count&&!v.rebuilds_inventory)return fail(e,"Restore noninventory character has items");
  for(uint32_t j=0;j<count;++j){RestoreInventoryItem item;item.item_id=r.text();item.equipped=r.boolean();item.doses=r.number();if(!identifier(item.item_id)||!item.doses||item.doses>1000000)return fail(e,"Restore projected item rejected");v.projected_items.push_back(std::move(item));}next.inventory_load_order_.push_back(std::move(v));
 }
 if(!r.ok||r.n)return fail(e,"Restore malformed/trailing payload or flag binding mismatch");
 next.valid_=true;*this=std::move(next);e.clear();return true;
}
bool RestoreData::load_file(const char*path,RoomView room,HouseView house,std::string&e){
 if(!path)return fail(e,"Missing restore pack path");
 FILE*f=std::fopen(path,"rb");if(!f)return fail(e,"Cannot open restore pack");
 std::vector<uint8_t>bytes;uint8_t block[4096];bool good=true;while(true){const auto n=std::fread(block,1,sizeof(block),f);if(bytes.size()+n>max_bytes){good=false;break;}bytes.insert(bytes.end(),block,block+n);if(n<sizeof(block)){good=!std::ferror(f);break;}}if(std::fclose(f))good=false;if(!good)return fail(e,"Restore bounded file read failed");return load(bytes.data(),bytes.size(),room,house,e);
}
}
