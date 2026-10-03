#include "encore/session_migration.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

namespace encore::upstream { namespace {
constexpr size_t migration_max_bytes=1024*1024;
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool same(const SessionSaveCompatibility&a,const SessionSaveCompatibility&b){return a.content_family==b.content_family&&a.content_revision==b.content_revision&&a.rules_revision==b.rules_revision;}
struct Reader {
 const uint8_t*p;size_t n;bool ok=true;
 uint32_t number(){if(n<4){ok=false;return 0;}auto v=u32(p);p+=4;n-=4;return v;}
 SessionSaveCompatibility identity(){const auto family=number(),content=number(),rules=number();return {family,content,rules};}
 template<class Data>bool pack(Data&data,std::string&e){const auto size=number();if(!ok||size>n){ok=false;return fail(e,"Session migration nested pack bounds rejected");}if(!data.load(p,size,e))return false;p+=size;n-=size;return true;}
};
bool read_bytes(const char*path,size_t limit,std::vector<uint8_t>&bytes,std::string&e,SessionSaveFileOps*ops=nullptr){
 if(!path||!*path)return fail(e,"Missing session migration/read path");
 if(ops){auto result=ops->read(path,limit,bytes,e);if(result==SessionFileReadResult::Ok){if(bytes.size()>limit)return fail(e,"Session read exceeds file bounds");return true;}if(e.empty())e=result==SessionFileReadResult::Missing?"Session save missing":"Session save read failed";return false;}
 FILE*f=std::fopen(path,"rb");if(!f)return fail(e,"Cannot open session save or migration pack");
 std::vector<uint8_t>candidate;uint8_t block[4096];bool good=true;
 while(true){const auto n=std::fread(block,1,sizeof(block),f);if(n>limit-candidate.size()){good=false;break;}candidate.insert(candidate.end(),block,block+n);if(n<sizeof(block)){good=!std::ferror(f);break;}}
 if(std::fclose(f))good=false;
 if(!good)return fail(e,"Session bounded read failed");
 bytes=std::move(candidate);e.clear();return true;
}
bool prepare_current(const NativeSessionData&data,RoomView room,HouseView house,
 RoundView round,ItemView items,BattleView font,const SessionSnapshot&snapshot,
 PreparedSessionRestore&out,std::string&e,bool* migrated,bool legacy){
 PreparedSessionRestore next;
 if(!prepare_session_restore(data,room,house,round,items,font,snapshot,next,e)){
  if(legacy)e="Legacy save cannot restore losslessly under current rules: "+e;
  return false;
 }
 out=std::move(next);if(migrated)*migrated=legacy;e.clear();return true;
}
}

bool SessionMigrationData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<24||n>migration_max_bytes)return fail(e,"Session migration pack size rejected");
 if(std::memcmp(p,"ENCMIG01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1)return fail(e,"Session migration schema/size/capability rejected");
 if(crc(p+24,n-24)!=u32(p+16))return fail(e,"Session migration CRC mismatch");
 Reader r{p+24,n-24};SessionMigrationData next;const auto from=r.identity();next.target_=r.identity();
 // Revision numbers describe this one migration mechanism. Content identities
 // and all game rules/defaults remain in the externally reviewed bundle.
 if(!from.content_family||!from.content_revision||from.rules_revision!=6||next.target_.rules_revision!=7||from.content_family!=next.target_.content_family||from.content_revision!=next.target_.content_revision)return fail(e,"Session migration transition is not reviewed rules6 to rules7");
 next.house_=std::make_unique<HouseData>();next.round_=std::make_unique<BattleRoundData>();next.items_=std::make_unique<ItemData>();
 if(!r.pack(next.legacy_,e)||!r.pack(next.room_,e)||!r.pack(*next.house_,e)||!r.pack(*next.round_,e)||!r.pack(*next.items_,e))return false;
 if(!r.ok||r.n)return fail(e,"Session migration malformed/trailing payload");
 if(!same(from,next.legacy_.compatibility()))return fail(e,"Session migration legacy identity mismatch");
 if(!validate_native_session_snapshot(next.legacy_,next.room_.view(),next.house_->view(),next.round_->view(),next.items_->view(),next.legacy_.defaults(),e))return false;
 next.valid_=true;*this=std::move(next);e.clear();return true;
}
bool SessionMigrationData::load_file(const char*path,std::string&e){std::vector<uint8_t>bytes;return read_bytes(path,migration_max_bytes,bytes,e)&&load(bytes.data(),bytes.size(),e);}
bool SessionMigrationData::validate_legacy(const SessionSnapshot&s,std::string&e)const{
 if(!valid())return fail(e,"Session migration requires a checked legacy bundle");
 if(!validate_native_session_snapshot(legacy_,room_.view(),house_->view(),round_->view(),items_->view(),s,e)){e="Legacy save outside reviewed rules6 scope: "+e;return false;}return true;
}
bool prepare_compatible_session_restore(const uint8_t*p,size_t n,
 const SessionMigrationData&migration,const NativeSessionData&data,RoomView room,
 HouseView house,RoundView round,ItemView items,BattleView font,
 PreparedSessionRestore&out,std::string&e,bool*migrated){
 SessionSnapshot snapshot;
 if(decode_session_save(p,n,data.compatibility(),snapshot,e))
  return prepare_current(data,room,house,round,items,font,snapshot,out,e,migrated,false);
 if(!migration.valid()||!same(migration.target_compatibility(),data.compatibility()))return fail(e,"Session migration target compatibility unavailable");
 if(!decode_session_save(p,n,migration.legacy_compatibility(),snapshot,e)){e="Save rejected by current and exact legacy decoders: "+e;return false;}
 if(!migration.validate_legacy(snapshot,e))return false;
 return prepare_current(data,room,house,round,items,font,snapshot,out,e,migrated,true);
}
bool read_compatible_session_restore(const char*path,const char*migration_path,
 const NativeSessionData&data,RoomView room,HouseView house,RoundView round,
 ItemView items,BattleView font,SessionSnapshot&snapshot,PreparedSessionRestore&out,
 std::string&e,SessionSaveFileOps*ops,bool*migrated){
 std::vector<uint8_t>bytes;if(!read_bytes(path,session_save_max_bytes,bytes,e,ops))return false;
 PreparedSessionRestore next;SessionSnapshot decoded;bool did_migrate=false;
 if(decode_session_save(bytes.data(),bytes.size(),data.compatibility(),decoded,e)){
  if(!prepare_current(data,room,house,round,items,font,decoded,next,e,&did_migrate,false))return false;
 }else{
  SessionMigrationData migration;
  if(!migration.load_file(migration_path,e))return false;
  if(!prepare_compatible_session_restore(bytes.data(),bytes.size(),migration,data,room,house,round,items,font,next,e,&did_migrate))return false;
 }
 snapshot=next.state;out=std::move(next);if(migrated)*migrated=did_migrate;e.clear();return true;
}
}
