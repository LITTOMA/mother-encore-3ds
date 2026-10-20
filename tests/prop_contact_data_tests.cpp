#include "encore/field_props.hpp"
#include "encore/field_swing.hpp"
#include "encore/field_scene.hpp"
#include "encore/audio_data.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Prop contact %d: %s (%s)\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
uint32_t get(const std::vector<uint8_t>&b,size_t at){return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void real(std::vector<uint8_t>&b,size_t at,float v){uint32_t bits;std::memcpy(&bits,&v,4);put(b,at,bits);}
void sign(std::vector<uint8_t>&b){put(b,16,encore::crc32(b.data()+32,b.size()-32));}
}
int main(int argc,char**argv){
 CHECK(argc==3);const std::string dir=argv[1],root=argv[2];std::vector<uint8_t>bytes;CHECK(encore::read_file((dir+"/source.encprops").c_str(),bytes,128*1024,error));
 FieldPropsData data;CHECK(data.load(bytes.data(),bytes.size(),error)&&data.contact_bound()&&data.rendering_bound()&&data.capability()==3&&data.props().size()==13);
 const auto policy=get(bytes,84),rows=get(bytes,116);const auto&c=data.policy().contact;
 CHECK(c.target_layer==0&&c.target_mask==2&&c.target_flags==1&&c.emitter_layer==2&&c.emitter_mask==2&&c.emitter_flags==3&&c.gain_db==0&&c.pitch==1&&c.bus==1&&c.pause_mode==0);
 FieldSwingData action;CHECK(action.load_file((dir+"/source.encswing").c_str(),error));CHECK(data.matches_action(action,error)&&action.encoded_bytes()==c.swing_bytes&&action.encoded_crc()==c.swing_crc);
 FieldSwingData empty;CHECK(!data.matches_action(empty,error));AudioBank bank,looping,unrelated;AudioAsset out;out.stable_id=0x1234;
 CHECK(!data.matches_audio(bank,out,error)&&out.stable_id==0x1234);
 CHECK(bank.load_file((dir+"/matching.encaudio").c_str(),error)&&data.matches_audio(bank,out,error));CHECK(out.source_sha256==c.sound_sha256&&out.source_path=="res://"+data.policy().sound&&!out.loops());
 CHECK(looping.load_file((dir+"/looping.encaudio").c_str(),error));const auto selected=out.stable_id;CHECK(!data.matches_audio(looping,out,error)&&out.stable_id==selected);
 for(const auto name:{"unrelated.encaudio","wrong-path.encaudio","wrong-digest.encaudio"}){CHECK(unrelated.load_file((dir+"/"+name).c_str(),error));CHECK(!data.matches_audio(unrelated,out,error)&&out.stable_id==selected);}
 RoomData room;FieldMapData map;WorldLinksData links;CHECK(room.load_file((root+"/world/podunk/podunk.encroom").c_str(),error));CHECK(map.load_file((root+"/world/podunk/podunk.encmap").c_str(),error));CHECK(links.load_file((root+"/world/world.enclinks").c_str(),error));CHECK(data.bind(room.view(),map.view(),error));
 // Contact metadata cannot downgrade into a drawing-only scene owner when
 // the caller has not bound its prepared action, object and sound owners.
 FieldScene scene;CHECK(scene.prepare(room.view(),map.view(),links,std::vector<bool>(room.view().flag_count(),false),{1432,4719},{0,1},{400,240},error));CHECK(!scene.bind_props(data,error)&&!scene.props().ready());
 auto reject=[&](std::vector<uint8_t>bad){sign(bad);FieldPropsData probe;if(probe.load(bad.data(),bad.size(),error)){for(size_t i=20;i<bad.size();++i)if(bad[i]!=bytes[i])std::fprintf(stderr,"Unexpected accepted mutation byte %zu: %u -> %u\n",i,bytes[i],bad[i]);}CHECK(!data.load(bad.data(),bad.size(),error));CHECK(data.contact_bound()&&data.props().size()==13&&data.matches_action(action,error));};
 for(auto change:std::vector<std::pair<size_t,uint32_t>>{{20,4},{20,2},{92,96},{124,104},{policy+96,1u<<20},{policy+100,0},{policy+104,0},{policy+104,4},{policy+108,0},{policy+112,1u<<20},{policy+116,0},{policy+116,4},{policy+128,0},{policy+128,2},{policy+132,1},{policy+136,0},{policy+136,65537},{rows+104,kFieldNone},{rows+116+104,get(bytes,rows+104)},{rows+108,UINT32_MAX},{rows+112,0}}){auto bad=bytes;put(bad,change.first,change.second);reject(bad);}
 {auto bad=bytes;put(bad,rows+116+108,get(bytes,rows+108));put(bad,rows+116+112,get(bytes,rows+112));reject(bad);}
 for(float v:{-257.f,129.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){auto bad=bytes;real(bad,policy+120,v);reject(bad);}
 for(float v:{0.f,-1.f,17.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){auto bad=bytes;real(bad,policy+124,v);reject(bad);}
 {auto bad=bytes;std::fill(bad.begin()+policy+144,bad.begin()+policy+176,0);reject(bad);}
 for(size_t at:{size_t(policy+136),size_t(policy+140)}){auto bad=bytes;put(bad,at,get(bad,at)^1);sign(bad);FieldPropsData other;CHECK(other.load(bad.data(),bad.size(),error)&&!other.matches_action(action,error)&&data.matches_action(action,error));}
 {auto bad=bytes;bad[policy+144]^=1;sign(bad);FieldPropsData other;CHECK(other.load(bad.data(),bad.size(),error)&&!other.matches_audio(bank,out,error));}
 {auto bad=bytes;put(bad,rows+104,map.view().count(FieldMapSection::Notices)+1);sign(bad);FieldPropsData other;CHECK(other.load(bad.data(),bad.size(),error)&&!other.bind(room.view(),map.view(),error));}
 for(size_t i=0;i<bytes.size();++i){CHECK(!data.load(bytes.data(),i,error)&&data.contact_bound());auto bad=bytes;bad[i]^=0x80;CHECK(!data.load(bad.data(),bad.size(),error)&&data.contact_bound());}
 CHECK(!data.load(nullptr,bytes.size(),error));
 for(auto path:{root+"/../tests/fixtures/source-field-props-cap2.encprops",root+"/../tests/fixtures/source-field-props.encprops"}){FieldPropsData old;CHECK(old.load_file(path.c_str(),error)&&!old.contact_bound()&&!old.matches_action(action,error)&&!old.matches_audio(bank,out,error));}
 std::printf("Field prop contact metadata: %u checks; exact source action/audio/notice joins, legacy reads, malformed rollback and closed scene admission; no actual hit, PCM playback or emulator claim\n",checks);
}
