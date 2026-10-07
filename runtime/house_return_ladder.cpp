#include "encore/house_return_ladder.hpp"
#include "encore/player_motion.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int k=0;k<8;++k)c=(c>>1)^((c&1)?0xedb88320u:0u);}return ~c;}
bool nz(const std::array<uint8_t,32>&h){return std::any_of(h.begin(),h.end(),[](auto b){return b!=0;});}
bool path(std::string_view s){return !s.empty()&&s.front()!='/'&&s.find_first_of("\\:\0") == s.npos&&s.find("..") == s.npos;}
struct Reader {
 const uint8_t*p;size_t n,at=128;bool ok=true;
 uint32_t u(){if(!ok||at>n||n-at<4){ok=false;return 0;}auto v=word(p+at);at+=4;return v;}
 float f(){auto v=u();float x;std::memcpy(&x,&v,4);if(!std::isfinite(x))ok=false;return x;}
 std::array<uint8_t,32>hash(){std::array<uint8_t,32>h{};if(!ok||at>n||n-at<32){ok=false;return h;}std::copy_n(p+at,32,h.begin());at+=32;return h;}
 std::string text(){auto count=u();if(!ok||count>65536||at>n||n-at<count){ok=false;return {};}std::string v(reinterpret_cast<const char*>(p+at),count);at+=count;size_t chars=0;if(v.find('\0')!=v.npos||!utf8_count(v,chars))ok=false;return v;}
 FieldTransform transform(){FieldTransform t;for(auto&v:t){v.x=f();v.y=f();}return t;}
};
bool equal(FieldTransform a,FieldTransform b){for(size_t k=0;k<3;++k)if(a[k].x!=b[k].x||a[k].y!=b[k].y)return false;return true;}
}
bool HouseReturnLadderData::source_hash(std::string_view source,std::array<uint8_t,32>&out)const{
 auto i=sources_.find(std::string(source));if(i==sources_.end())return false;out=i->second;return true;
}
bool HouseReturnLadderData::player_matches(const PlayerInitializationData&i,const PlayerReadyData&r,const PlayerMotionData&m,std::string&e)const{
 std::array<uint8_t,32>a{},b{};
 if(!valid_||!i.valid()||!r.valid()||!m.valid()||i.identity().upstream_commit!=identity_.upstream_commit||
    i.ir_sha256()!=initialization_ir_||r.ir_sha256()!=ready_ir_||m.ir_sha256()!=motion_ir_||
    m.field(PlayerMotionField::Climbing)!=text(HouseLadderText::ClimbingField)||
    m.text(PlayerMotionText::RunVoice)!=text(HouseLadderText::RunVoice)||
    m.text(PlayerMotionText::IdleAnimation)!=text(HouseLadderText::IdleAnimation))
  return fail(e,"House Ladder actual Player source dependency differs");
 for(auto role:{HouseLadderText::PartyBase,HouseLadderText::PlayerScript})
  if(!source_hash(text(role),a)||!i.source_hash(text(role),b)||a!=b)
   return fail(e,"House Ladder Player inherited source proof differs");
 e.clear();return true;
}
bool HouseReturnLadderData::matches(const FieldNodeTreeData&t,const FieldGeometryView&g,
 const PlayerInitializationData&i,const PlayerReadyData&r,const PlayerMotionData&m,std::string&e)const{
 if(!player_matches(i,r,m,e))return false;
 std::array<uint8_t,32>sha{};const auto*n=t.record(id_);const auto*s=t.record(shape_);
 if(!t.valid()||!g.valid()||t.source_scene()!=scene_||g.source_scene()!=scene_||
    t.identity().upstream_commit!=identity_.upstream_commit||g.identity().upstream_commit!=identity_.upstream_commit||
    t.identity().source_sha256!=identity_.source_sha256||g.identity().source_sha256!=identity_.source_sha256||
    !n||!s||n->path!=node_||n->native_class!="Area2D"||n->script!=text(HouseLadderText::Script)||
    !source_hash(n->script,sha)||sha!=n->script_sha||n->script_methods||s->parent!=id_||
    s->native_class!="CollisionShape2D"||!equal(n->local,local_)||!equal(n->world,world_)||
    owner_>=g.owner_count()||shape_index_>=g.shape_count())
  return fail(e,"House Ladder complete native tree/geometry proof differs");
 const auto o=g.owner(owner_);const auto shape=g.shape(shape_index_);
 if(o.node>=g.node_count()||shape.node>=g.node_count()||g.node(o.node).stable_id!=id_||
    g.node(shape.node).stable_id!=shape_||o.kind!=4||o.layer!=layer_||o.mask!=mask_||o.flags!=owner_flags_||
    o.shape_first!=shape_index_||o.shape_count!=1||shape.owner!=owner_||shape.flags!=shape_flags_||shape.part_count!=1||
    g.node(o.node).script_sha256!=n->script_sha)
  return fail(e,"House Ladder native collision owner/shape differs");
 for(const auto &body:t.records()) {
  if(body.native_class!="StaticBody2D"&&body.native_class!="KinematicBody2D"&&body.native_class!="RigidBody2D")continue;
  if(std::count(nonparty_.begin(),nonparty_.end(),body.id)!=1 ||
     (!body.script.empty()&&(!source_hash(body.script,sha)||sha!=body.script_sha)))
   return fail(e,"House Ladder non-party native body source proof differs");
 }
 for(auto id:nonparty_){const auto*body=t.record(id);if(!body||
    (body->native_class!="StaticBody2D"&&body->native_class!="KinematicBody2D"&&body->native_class!="RigidBody2D"))
   return fail(e,"House Ladder unsupported non-party body class");}
 e.clear();return true;
}
bool HouseReturnLadderData::load(const uint8_t*p,size_t n,const FieldIdentity&expected,
 const FieldNodeTreeData&t,const FieldGeometryView&g,const PlayerInitializationData&i,
 const PlayerReadyData&r,const PlayerMotionData&m,std::string&e){
 if(!p||n<128||n>1024*1024||std::memcmp(p,"ENCHLDR1",8)||word(p+8)!=1||word(p+12)!=128||
    word(p+16)!=n||word(p+20)!=crc(p+128,n-128)||word(p+24)!=0x454e0078||word(p+28)!=1||
    word(p+32)!=1||word(p+124))return fail(e,"House Ladder header/format/capability/rules rejected");
 HouseReturnLadderData d;d.identity_.scene_id=word(p+36);std::copy_n(p+40,20,d.identity_.upstream_commit.begin());
 std::copy_n(p+60,32,d.identity_.source_sha256.begin());std::copy_n(p+92,32,d.ir_.begin());
 if(d.identity_.scene_id!=expected.scene_id||d.identity_.upstream_commit!=expected.upstream_commit||
    d.identity_.source_sha256!=expected.source_sha256||!nz(d.ir_))return fail(e,"House Ladder source identity rejected");
 Reader rd{p,n};d.scene_=rd.text();d.node_=rd.text();d.id_=rd.u();d.shape_=rd.u();d.owner_=rd.u();d.shape_index_=rd.u();
 d.layer_=rd.u();d.mask_=rd.u();d.owner_flags_=rd.u();d.shape_flags_=rd.u();d.local_=rd.transform();d.world_=rd.transform();
 d.stopped_speed_=rd.f();d.position_y_=rd.f();d.initialization_ir_=rd.hash();d.ready_ir_=rd.hash();d.motion_ir_=rd.hash();
 if(!path(d.scene_)||!path(d.node_)||!d.id_||!d.shape_||d.id_==d.shape_||d.owner_flags_&~63u||d.shape_flags_&~3u||
    d.stopped_speed_!=0||d.position_y_!=0||rd.u()!=size_t(HouseLadderText::Count))
  return fail(e,"House Ladder source schema/unsupported policy rejected");
 for(auto&v:d.texts_){v=rd.text();if(v.empty())return fail(e,"House Ladder empty source symbol rejected");}
 if(rd.u()!=d.connections_.size())return fail(e,"House Ladder source connection count rejected");
 for(size_t k=0;k<d.connections_.size();++k){auto&c=d.connections_[k];c.signal=rd.text();c.method=rd.text();c.flags=rd.u();c.arguments=rd.u();
  if(c.signal!=d.text(k?HouseLadderText::ExitSignal:HouseLadderText::EnterSignal)||
     c.method!=d.text(k?HouseLadderText::ExitMethod:HouseLadderText::EnterMethod)||c.flags!=2||c.arguments!=1)
   return fail(e,"House Ladder source connection signature/flags rejected");}
 const auto body_count=rd.u();if(!body_count||body_count>4096)return fail(e,"House Ladder non-party body count rejected");
 for(uint32_t k=0;k<body_count;++k){auto id=rd.u();if(!id||std::find(d.nonparty_.begin(),d.nonparty_.end(),id)!=d.nonparty_.end())return fail(e,"House Ladder non-party body ID duplicate");d.nonparty_.push_back(id);}
 const auto count=rd.u();if(!count||count>4096)return fail(e,"House Ladder source closure count rejected");
 for(uint32_t k=0;k<count;++k){auto source=rd.text();auto hash=rd.hash();if(!path(source)||!nz(hash)||!d.sources_.emplace(source,hash).second)
  return fail(e,"House Ladder duplicate/malformed source proof rejected");}
 std::array<uint8_t,32>sha{};if(!rd.ok||rd.at!=n||!d.source_hash(d.scene_,sha)||sha!=d.identity_.source_sha256||
    d.connections_[0].signal==d.connections_[1].signal||d.connections_[0].method==d.connections_[1].method)
  return fail(e,"House Ladder truncated/unknown source payload rejected");
 for(auto role:{HouseLadderText::Script,HouseLadderText::PartyBase,HouseLadderText::PlayerScript,HouseLadderText::FollowerScript})
  if(!path(d.text(role))||!d.source_hash(d.text(role),sha))return fail(e,"House Ladder receiver source closure incomplete");
 d.valid_=true;if(!d.matches(t,g,i,r,m,e))return false;*this=std::move(d);e.clear();return true;
}
bool HouseReturnLadderData::load_file(const char*file,const FieldIdentity&expected,const FieldNodeTreeData&t,
 const FieldGeometryView&g,const PlayerInitializationData&i,const PlayerReadyData&r,const PlayerMotionData&m,std::string&e){
 if(!file)return fail(e,"House Ladder resource path missing");FILE*f=std::fopen(file,"rb");if(!f)return fail(e,"House Ladder resource open failed");
 if(std::fseek(f,0,SEEK_END)){std::fclose(f);return fail(e,"House Ladder seek failed");}auto n=std::ftell(f);
 if(n<128||n>1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);return fail(e,"House Ladder file size unsupported");}
 std::vector<uint8_t>b(static_cast<size_t>(n));bool ok=std::fread(b.data(),1,b.size(),f)==b.size();std::fclose(f);
 return ok?load(b.data(),b.size(),expected,t,g,i,r,m,e):fail(e,"House Ladder incomplete file");
}
}
