// Manual-only actual resource cases. Not registered or run by this slice.
#include "encore/field_tint.hpp"
#include "encore/house_return_sources.hpp"
#include "manual_require.hpp"
#include <algorithm>

using namespace encore::upstream;
namespace {
void tint_word(std::vector<uint8_t>&b,size_t at,uint32_t value){
  for(unsigned i=0;i<4;++i)b.at(at+i)=uint8_t(value>>(8*i));
}
void tint_crc(std::vector<uint8_t>&b){
  tint_word(b,16,0);uint32_t crc=~0u;
  for(const auto byte:b){crc^=byte;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
  tint_word(b,16,~crc);
}
}
void house_return_tint_resource_manual(const std::vector<uint8_t>&bytes,
    const HouseReturnSources&sources){
  std::string error;FieldTintData data;
  MANUAL_REQUIRE(data.load(bytes.data(),bytes.size(),error));
  MANUAL_REQUIRE(data.records().size()==10&&data.scene_id()==sources.tree().identity().scene_id);
  MANUAL_REQUIRE(data.source_pin()==sources.tree().identity().upstream_commit);
  MANUAL_REQUIRE(!data.prototype(FieldTintKind::EnemyPrototype)&&!data.prototype(FieldTintKind::ActorPrototype));
  size_t targets=0;for(const auto&r:data.records()){
    const auto*n=sources.tree().record(r.id);
    MANUAL_REQUIRE(n&&n->path==r.node&&n->ready==r.ready_ordinal&&n->native_class=="Node");
    for(const auto&t:r.targets){++targets;MANUAL_REQUIRE(t.exists);const auto*actual=sources.tree().record(t.source_id);
      MANUAL_REQUIRE(actual&&actual->path==t.node&&actual->native_class==t.kind&&actual->self_modulate==t.initial_self_modulate);
    }
  }
  MANUAL_REQUIRE(targets==22);
  const auto*retained=data.record(data.records().front().id);
  auto rejected=[&](const std::vector<uint8_t>&bad){FieldTintData empty;
    MANUAL_REQUIRE(!empty.load(bad.data(),bad.size(),error)&&!empty.valid());
    MANUAL_REQUIRE(!data.load(bad.data(),bad.size(),error));
    MANUAL_REQUIRE(data.valid()&&data.record(retained->id)==retained);
  };
  for(size_t at:{size_t(8),size_t(20),size_t(24),size_t(28),size_t(52),size_t(56),size_t(60),size_t(92),size_t(96)}){
    auto bad=bytes;tint_word(bad,at,UINT32_MAX);tint_crc(bad);rejected(bad);
  }
  for(size_t end:{size_t(63),bytes.size()-1}){auto bad=bytes;bad.resize(end);rejected(bad);}
  auto invalid_color=bytes;tint_word(invalid_color,64,0x7fc00000);tint_crc(invalid_color);rejected(invalid_color);
  // Parseable foreign namespaces must remain visibly different; actual native
  // source admission, rather than parser success, is required by the next file.
  for(size_t at:{size_t(32)}){auto bad=bytes;bad.at(at)^=1;tint_crc(bad);FieldTintData foreign;
    MANUAL_REQUIRE(foreign.load(bad.data(),bad.size(),error));
    MANUAL_REQUIRE(foreign.scene_id()!=sources.tree().identity().scene_id||foreign.source_pin()!=sources.tree().identity().upstream_commit);
  }
  std::array<uint8_t,32>hash;
  MANUAL_REQUIRE(data.source_hash("Scripts/misc/character_tint.gd",hash));
  MANUAL_REQUIRE(!data.source_hash("unreviewed/tint.gd",hash));
  // The format remains the existing core's format, with missing concrete native
  // operations rejected. No mock target may be admitted as native House Ready.
  FieldTintRuntime runtime;FieldTintHost absent;
  MANUAL_REQUIRE(!runtime.initialize(data,absent,error));
}
