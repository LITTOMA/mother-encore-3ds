#include "encore/dungeon_keys.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
constexpr uint32_t strides[]={1,32,32};
constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
int32_t integer(const uint8_t*p){auto raw=u(p);int32_t value;std::memcpy(&value,&raw,4);return value;}
FieldText span(const uint8_t*p){return {u(p),u(p+4)};}
bool safe_path(std::string_view s){
 if(s.empty()||s.size()>512)return false;
 for(unsigned char c:s)if(c<32||c>=127||c==':'||c=='\\')return false;
 size_t begin=0;
 while(begin<=s.size()){
  auto end=s.find('/',begin);if(end==s.npos)end=s.size();auto part=s.substr(begin,end-begin);
  if(part.empty()||part=="."||part=="..")return false;
  if(end==s.size())break;begin=end+1;
 }
 return true;
}
}
uint32_t DungeonKeysView::count(uint32_t section)const{return p_&&section>=1&&section<=3?u(p_+64+(section-1)*16+8):0;}
const uint8_t*DungeonKeysView::record(uint32_t section,uint32_t index)const{return index<count(section)?p_+u(p_+64+(section-1)*16+4)+size_t(index)*strides[section-1]:nullptr;}
uint32_t DungeonKeysView::scene_id()const{return p_?u(p_+24):0;}
uint32_t DungeonKeysView::node_count()const{return count(3);}
std::string_view DungeonKeysView::text(FieldText t)const{return t.length&&t.offset<=count(1)&&t.length<=count(1)-t.offset?std::string_view(reinterpret_cast<const char*>(record(1,0))+t.offset,t.length):std::string_view{};}
DungeonKeyPolicy DungeonKeysView::policy()const{
 auto r=record(2,0);return r?DungeonKeyPolicy{span(r),span(r+24),integer(r+8),integer(r+12),integer(r+16)}:DungeonKeyPolicy{};
}
DungeonKeyNode DungeonKeysView::node(uint32_t index)const{
 auto r=record(3,index);return r?DungeonKeyNode{u(r),DungeonKeyKind(u(r+4)),span(r+8),span(r+16),integer(r+24),u(r+28)}:DungeonKeyNode{};
}
bool DungeonKeysData::load(const uint8_t*p,size_t n,std::string&e){
 auto fail=[&](const char*why){e=why;return false;};
 if(!p||n<112||n>65536)return fail("Dungeon key resource size rejected");
 if(std::memcmp(p,"ENCDKEY1",8)||u(p+8)!=1||u(p+12)!=n||u(p+20)!=1||!u(p+24)||u(p+24)>=65536||u(p+28)||u(p+52)!=3||u(p+56)||u(p+60))return fail("Dungeon key schema/capability/scene rejected");
 for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[2*i])*16+hex(pin[2*i+1])))return fail("Dungeon key source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u(p+16))return fail("Dungeon key checksum rejected");
 size_t cursor=112;
 for(uint32_t i=0;i<3;++i){
  const auto r=p+64+i*16;const auto offset=u(r+4),count=u(r+8);
  if(u(r)!=i+1||u(r+12)!=strides[i]||!count)return fail("Dungeon key section identity/stride/count rejected");
  const auto aligned=(cursor+3)&~size_t(3);
  if(offset!=aligned||offset>n||count>(n-offset)/strides[i])return fail("Dungeon key section bounds rejected");
  for(size_t at=cursor;at<aligned;++at)if(p[at])return fail("Dungeon key section padding rejected");
  cursor=offset+size_t(count)*strides[i];
 }
 if(cursor!=n)return fail("Dungeon key trailing data rejected");
 DungeonKeysView v;v.p_=p;
 if(v.count(2)!=1||v.node_count()>128)return fail("Dungeon key policy/node count rejected");
 const auto policy=v.policy();
 if(u(v.record(2,0)+20)||policy.minimum<0||policy.initial<policy.minimum||policy.maximum<policy.initial||policy.maximum>65535)return fail("Dungeon key counter policy rejected");
 uint32_t owned=0;std::vector<std::pair<std::string_view,FieldText>>strings;
 auto text=[&](FieldText t){
  const auto value=v.text(t);if(!safe_path(value))return false;
  for(const auto&known:strings)if(known.first==value)return known.second.offset==t.offset&&known.second.length==t.length;
  if(t.offset!=owned)return false;owned+=t.length;strings.push_back({value,t});return true;
 };
 if(!text(policy.region)||!text(policy.locked_dialogue))return fail("Dungeon key policy text rejected");
 std::set<uint32_t>ids;std::set<std::string_view>paths,flags;int64_t maximum=policy.initial;
 for(uint32_t i=0;i<v.node_count();++i){
  const auto row=v.node(i);
  if(!row.id||!ids.insert(row.id).second||row.flags!=3||!text(row.path)||!text(row.flag)||!paths.insert(v.text(row.path)).second||!flags.insert(v.text(row.flag)).second)return fail("Dungeon key node identity/text/flags rejected");
  if(row.kind==DungeonKeyKind::Pickup){if(row.delta<=0||row.delta>65535)return fail("Dungeon key pickup delta rejected");maximum+=row.delta;}
  else if(row.kind==DungeonKeyKind::Lock){if(row.delta>=0||row.delta< -65535)return fail("Dungeon key lock delta rejected");}
  else return fail("Dungeon key node kind rejected");
 }
 if(maximum!=policy.maximum||owned!=v.count(1))return fail("Dungeon key reachable counter/unused text rejected");
 std::vector<uint8_t>candidate(p,p+n);bytes_.swap(candidate);e.clear();return true;
}
bool DungeonKeysData::load_file(const char*path,std::string&e){std::vector<uint8_t>data;if(!encore::read_file(path,data,65536,e))return false;return load(data.data(),data.size(),e);}
}
