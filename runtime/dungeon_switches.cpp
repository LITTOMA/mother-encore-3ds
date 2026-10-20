#include "encore/dungeon_switches.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
constexpr uint32_t strides[]={1,48,64,48,8};
constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
int32_t integer(const uint8_t*p){auto bits=u(p);int32_t v;std::memcpy(&v,&bits,4);return v;}
float real(const uint8_t*p){auto bits=u(p);float v;std::memcpy(&v,&bits,4);return v;}
FieldText span(const uint8_t*p){return {u(p),u(p+4)};}
bool finite(float v){return std::isfinite(v)&&std::abs(v)<=1000000;}
bool path(std::string_view s){
 if(s.empty()||s.size()>512)return false;
 for(unsigned char c:s)if(c<32||c>=127||c==':'||c=='\\')return false;
 size_t at=0;
 while(at<=s.size()){auto end=s.find('/',at);if(end==s.npos)end=s.size();auto part=s.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;if(end==s.size())break;at=end+1;}
 return true;
}
}
uint32_t DungeonSwitchesView::count(uint32_t s)const{return p_&&s>=1&&s<=5?u(p_+64+(s-1)*16+8):0;}
const uint8_t*DungeonSwitchesView::record(uint32_t s,uint32_t i)const{return i<count(s)?p_+u(p_+64+(s-1)*16+4)+size_t(i)*strides[s-1]:nullptr;}
uint32_t DungeonSwitchesView::scene_id()const{return p_?u(p_+24):0;}
uint32_t DungeonSwitchesView::switch_count()const{return count(3);}
uint32_t DungeonSwitchesView::object_count()const{return count(4);}
uint32_t DungeonSwitchesView::edge_count()const{return count(5);}
std::string_view DungeonSwitchesView::text(FieldText t)const{return t.length&&t.offset<=count(1)&&t.length<=count(1)-t.offset?std::string_view(reinterpret_cast<const char*>(record(1,0))+t.offset,t.length):std::string_view{};}
DungeonSwitchPolicy DungeonSwitchesView::policy()const{auto p=record(2,0);return p?DungeonSwitchPolicy{span(p),real(p+8),u(p+12),u(p+16),real(p+20),real(p+24),span(p+32),span(p+40)}:DungeonSwitchPolicy{};}
DungeonSwitchNode DungeonSwitchesView::switch_node(uint32_t i)const{auto p=record(3,i);return p?DungeonSwitchNode{u(p),span(p+4),span(p+12),u(p+20),real(p+24),real(p+28),real(p+32),u(p+36),u(p+40),span(p+44),DungeonSwitchKind(u(p+52))}:DungeonSwitchNode{};}
DungeonControlledNode DungeonSwitchesView::object(uint32_t i)const{auto p=record(4,i);return p?DungeonControlledNode{u(p),span(p+4),DungeonControlledKind(u(p+12)),integer(p+16),u(p+20),real(p+24),real(p+28),real(p+32),real(p+36)}:DungeonControlledNode{};}
DungeonSwitchEdge DungeonSwitchesView::edge(uint32_t i)const{auto p=record(5,i);return p?DungeonSwitchEdge{u(p),u(p+4)}:DungeonSwitchEdge{};}
bool DungeonSwitchesData::load(const uint8_t*p,size_t n,std::string&e){
 auto fail=[&](const char*why){e=why;return false;};
 if(!p||n<144||n>65536)return fail("Dungeon switch resource size rejected");
 if(std::memcmp(p,"ENCDSW01",8)||u(p+8)!=1||u(p+12)!=n||u(p+20)!=1||!u(p+24)||u(p+24)>=65536||u(p+28)||u(p+52)!=5||u(p+56)||u(p+60))return fail("Dungeon switch schema/capability/scene rejected");
 for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[2*i])*16+hex(pin[2*i+1])))return fail("Dungeon switch source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u(p+16))return fail("Dungeon switch checksum rejected");
 size_t cursor=144;
 for(uint32_t i=0;i<5;++i){auto d=p+64+i*16;auto offset=u(d+4),count=u(d+8);auto aligned=(cursor+3)&~size_t(3);
  if(u(d)!=i+1||u(d+12)!=strides[i]||!count||offset!=aligned||offset>n||count>(n-offset)/strides[i])return fail("Dungeon switch section identity/bounds rejected");
  for(size_t at=cursor;at<aligned;++at)if(p[at])return fail("Dungeon switch padding rejected");cursor=offset+size_t(count)*strides[i];
 }
 if(cursor!=n)return fail("Dungeon switch trailing data rejected");
 DungeonSwitchesView v;v.p_=p;if(v.count(2)!=1||v.switch_count()>128||v.object_count()>128||v.edge_count()>1024)return fail("Dungeon switch section counts rejected");
 auto policy=v.policy();if(!finite(policy.ready_delay)||policy.ready_delay<=0||policy.ready_delay>60||!finite(policy.post_delay)||policy.post_delay<=0||policy.post_delay>60||!finite(policy.music_fade)||u(v.record(2,0)+28))return fail("Dungeon switch policy rejected");
 uint32_t owned=0;std::vector<std::pair<std::string_view,FieldText>>strings;
 auto text=[&](FieldText t){auto value=v.text(t);if(!path(value))return false;for(auto known:strings)if(known.first==value)return known.second.offset==t.offset&&known.second.length==t.length;if(t.offset!=owned)return false;owned+=t.length;strings.push_back({value,t});return true;};
 if(!text(policy.region)||!text(policy.denied_audio)||!text(policy.denied_channel))return fail("Dungeon switch policy text rejected");
 std::set<uint32_t>ids,object_ids,targets;std::set<std::string_view>paths,flags;uint32_t edge_cursor=0;
 for(uint32_t i=0;i<v.switch_count();++i){auto r=v.switch_node(i);auto raw=v.record(3,i);
  if(!r.id||!ids.insert(r.id).second||!text(r.path)||!paths.insert(v.text(r.path)).second||!text(r.flag)||!flags.insert(v.text(r.flag)).second||r.bits>127||!finite(r.camera_delay)||r.camera_delay<=0||r.camera_delay>60||!finite(r.x)||!finite(r.y)||r.first_edge!=edge_cursor||!r.edge_count||r.edge_count>v.edge_count()-edge_cursor||!text(r.audio)||(r.kind!=DungeonSwitchKind::Lever&&r.kind!=DungeonSwitchKind::Plate)||u(raw+56)||u(raw+60))return fail("Dungeon switch record rejected");
  edge_cursor+=r.edge_count;
 }
 if(edge_cursor!=v.edge_count())return fail("Dungeon switch unowned edges rejected");
 for(uint32_t i=0;i<v.object_count();++i){auto r=v.object(i);auto raw=v.record(4,i);
  if(!r.id||!ids.insert(r.id).second||!text(r.path)||!paths.insert(v.text(r.path)).second||(r.kind!=DungeonControlledKind::Gate&&r.kind!=DungeonControlledKind::Wall)||r.order< -32767||r.order>32767||r.bits>3||!finite(r.x)||!finite(r.y)||!finite(r.offset_x)||!finite(r.offset_y)||u(raw+40)||u(raw+44))return fail("Dungeon controlled object record rejected");
  object_ids.insert(r.id);
 }
 std::set<std::pair<uint32_t,uint32_t>>edges;
 for(uint32_t i=0;i<v.switch_count();++i){auto r=v.switch_node(i);for(uint32_t j=0;j<r.edge_count;++j){auto edge=v.edge(r.first_edge+j);if(edge.source!=r.id||!object_ids.count(edge.target)||!edges.insert({edge.source,edge.target}).second)return fail("Dungeon switch edge ownership/kind rejected");targets.insert(edge.target);}}
 if(targets!=object_ids)return fail("Dungeon switch uncontrolled object rejected");
 bool main=false,wall=false;for(uint32_t i=0;i<v.switch_count();++i){auto r=v.switch_node(i);if(r.id==policy.main_switch)main=(r.bits&16)!=0;}
 for(uint32_t i=0;i<v.object_count();++i){auto r=v.object(i);if(r.id==policy.wall)wall=r.kind==DungeonControlledKind::Wall;}
 if(!main||!wall||!edges.count({policy.main_switch,policy.wall})||owned!=v.count(1))return fail("Dungeon switch callback/string ownership rejected");
 std::vector<uint8_t>next(p,p+n);bytes_.swap(next);e.clear();return true;
}
bool DungeonSwitchesData::load_file(const char*path,std::string&e){std::vector<uint8_t>data;return encore::read_file(path,data,65536,e)&&load(data.data(),data.size(),e);}
}
