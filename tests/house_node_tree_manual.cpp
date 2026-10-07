#include "encore/field_node_tree.hpp"
#include "manual_require.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <utility>

using namespace encore::upstream;
namespace {
using Bytes=std::vector<uint8_t>;
uint32_t word(const Bytes&b,size_t at){
  MANUAL_REQUIRE(at<=b.size()&&b.size()-at>=4);
  return uint32_t(b[at])|uint32_t(b[at+1])<<8|
         uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;
}
void put(Bytes&b,size_t at,uint32_t value){
  MANUAL_REQUIRE(at<=b.size()&&b.size()-at>=4);
  for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(8*i));
}
void seal(Bytes&b){
  uint32_t crc=~0u;
  for(size_t i=128;i<b.size();++i){
    crc^=b[i];
    for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);
  }
  put(b,16,uint32_t(b.size()));put(b,20,~crc);
}
Bytes read(const char*path){
  MANUAL_REQUIRE(path&&*path);
  std::ifstream stream(path,std::ios::binary);
  MANUAL_REQUIRE(stream.good());
  Bytes out(std::istreambuf_iterator<char>{stream},std::istreambuf_iterator<char>{});
  MANUAL_REQUIRE(out.size()>=128);return out;
}
struct Cursor {
  const Bytes&bytes;size_t at=128;
  void skip(size_t n){MANUAL_REQUIRE(at<=bytes.size()&&n<=bytes.size()-at);at+=n;}
  uint32_t integer(){const auto value=word(bytes,at);skip(4);return value;}
  std::string text(){
    const auto size=integer();const auto start=at;skip(size);
    return std::string(reinterpret_cast<const char*>(bytes.data()+start),size);
  }
};
// Locate fields in a previously accepted fixture. The mutations below reseal
// the payload CRC, so their failures exercise the schema rather than checksum.
struct Layout {
  size_t class_count=0,records_begin=0;
  std::vector<size_t>class_names,records;
  std::vector<std::string>sources;
};
Layout locate(const Bytes&b){
  Cursor cursor{b};Layout out;
  cursor.text();out.class_count=cursor.at;
  const auto classes=cursor.integer();
  for(uint32_t i=0;i<classes;++i){out.class_names.push_back(cursor.at+4);cursor.text();}
  out.records_begin=cursor.at;
  for(uint32_t i=0;i<word(b,32);++i){
    out.records.push_back(cursor.at);
    cursor.skip(10*4+3*4+20*4);
    cursor.text();cursor.text();cursor.text();cursor.skip(32);
    const auto groups=cursor.integer();for(uint32_t g=0;g<groups;++g)cursor.text();
  }
  const auto sources=cursor.integer();
  for(uint32_t i=0;i<sources;++i){out.sources.push_back(cursor.text());cursor.skip(32);}
  MANUAL_REQUIRE(cursor.at==b.size());return out;
}
bool identity_equal(const FieldIdentity&a,const FieldIdentity&b){
  return a.scene_id==b.scene_id&&a.upstream_commit==b.upstream_commit&&a.source_sha256==b.source_sha256;
}
bool record_equal(const FieldNodeDescriptor&a,const FieldNodeDescriptor&b){
  const auto transform=[](const FieldTransform&a,const FieldTransform&b){
    for(size_t i=0;i<a.size();++i)if(a[i].x!=b[i].x||a[i].y!=b[i].y)return false;
    return true;
  };
  return a.id==b.id&&a.parent==b.parent&&a.owner==b.owner&&a.canvas_parent==b.canvas_parent&&
    a.class_index==b.class_index&&a.ready==b.ready&&a.pause==b.pause&&a.flags==b.flags&&
    a.light_mask==b.light_mask&&a.script_methods==b.script_methods&&a.index==b.index&&
    a.priority==b.priority&&a.z==b.z&&transform(a.local,b.local)&&transform(a.world,b.world)&&
    a.modulate==b.modulate&&a.self_modulate==b.self_modulate&&a.path==b.path&&a.name==b.name&&
    a.script==b.script&&a.native_class==b.native_class&&a.native_generated==b.native_generated&&
    a.script_sha==b.script_sha&&a.groups==b.groups;
}
struct Snapshot {
  FieldIdentity identity;
  std::string scene;
  std::vector<std::string>classes;
  std::vector<FieldNodeDescriptor>records;
  const FieldNodeDescriptor*address=nullptr;
  std::map<std::string,std::array<uint8_t,32>>sources;
  Snapshot(const FieldNodeTreeData&d,const Layout&layout):identity(d.identity()),
      scene(d.source_scene()),classes(d.classes()),records(d.records()),address(d.records().data()){
    for(const auto&source:layout.sources){std::array<uint8_t,32>hash{};
      MANUAL_REQUIRE(d.source_hash(source,hash));sources.emplace(source,hash);}
  }
  void unchanged(const FieldNodeTreeData&d)const{
    MANUAL_REQUIRE(d.valid()&&!d.scene_admitted()&&identity_equal(d.identity(),identity));
    MANUAL_REQUIRE(d.source_scene()==scene&&d.classes()==classes&&d.records().size()==records.size());
    MANUAL_REQUIRE(d.records().data()==address);
    for(size_t i=0;i<records.size();++i){
      MANUAL_REQUIRE(record_equal(d.records()[i],records[i]));
      MANUAL_REQUIRE(d.record(records[i].id)==address+i);
    }
    for(const auto&source:sources){std::array<uint8_t,32>hash{};
      MANUAL_REQUIRE(d.source_hash(source.first,hash)&&hash==source.second);}
  }
};
void reject_house_mutations(FieldNodeTreeData&existing,const Snapshot&before,
    const Bytes&house,const Layout&layout,const FieldIdentity&identity){
  const auto reject=[&](Bytes broken,const char*expected_error){
    seal(broken);std::string error;
    MANUAL_REQUIRE(!existing.load(broken.data(),broken.size(),identity,error));
    MANUAL_REQUIRE(error==expected_error);before.unchanged(existing);
  };
  auto wrong_name=house;
  MANUAL_REQUIRE(layout.class_names.size()==27);
  wrong_name[layout.class_names[25]]='X';
  reject(std::move(wrong_name),"NodeTree class opcode rejected");

  auto unknown_class=house;
  put(unknown_class,layout.class_count,28);
  const std::string unknown="UnsupportedNative";
  Bytes extra(4);put(extra,0,uint32_t(unknown.size()));extra.insert(extra.end(),unknown.begin(),unknown.end());
  unknown_class.insert(unknown_class.begin()+layout.records_begin,extra.begin(),extra.end());
  reject(std::move(unknown_class),"NodeTree native class schema rejected");

  for(uint32_t opcode:{25u,26u}){
    const auto found=std::find_if(layout.records.begin(),layout.records.end(),
      [&](size_t record){return word(house,record+16)==opcode;});
    MANUAL_REQUIRE(found!=layout.records.end());
    auto not_canvas=house;const auto flags=word(not_canvas,*found+28);
    MANUAL_REQUIRE(flags&1u);put(not_canvas,*found+28,flags&~1u);
    reject(std::move(not_canvas),"NodeTree node fields rejected");
  }
  auto incompatible=house;put(incompatible,8,2);
  reject(std::move(incompatible),"NodeTree identity/version/capability rejected");
}
}
// Manual only: an external manual driver supplies the actual positive files
// and their independently checked identities. This file has no main, automatic
// registration, CI hook or source gameplay execution. Checks survive NDEBUG.
void house_node_tree_manual(const char*house_path,const FieldIdentity&house_identity,
    const char*podunk_path,const FieldIdentity&podunk_identity){
  const auto house=read(house_path),podunk=read(podunk_path);
  FieldNodeTreeData data;std::string error;
  MANUAL_REQUIRE(data.load(house.data(),house.size(),house_identity,error));
  MANUAL_REQUIRE(data.records().size()==497&&data.classes().size()==27&&!data.scene_admitted());
  MANUAL_REQUIRE(data.classes()[25]=="Control"&&data.classes()[26]=="ColorRect");
  const auto house_layout=locate(house);
  const Snapshot house_before(data,house_layout);
  reject_house_mutations(data,house_before,house,house_layout,house_identity);

  MANUAL_REQUIRE(data.load(podunk.data(),podunk.size(),podunk_identity,error));
  MANUAL_REQUIRE(data.records().size()==8686&&data.classes().size()==25&&!data.scene_admitted());
  const auto podunk_layout=locate(podunk);
  const Snapshot podunk_before(data,podunk_layout);
  // A rejected House replacement must retain an already loaded legacy tree,
  // including every record address, class, source fingerprint and identity.
  reject_house_mutations(data,podunk_before,house,house_layout,house_identity);
}
