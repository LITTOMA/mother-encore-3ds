#include "encore/music_library_bindings.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace encore::upstream;
namespace {
unsigned checks=0;std::string error;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"Music bindings %u: %s (%s)\n",checks,#x,error.c_str());std::exit(1);}}while(0)
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void store(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void fix(std::vector<uint8_t>&b){store(b,12,uint32_t(b.size()));store(b,16,0);store(b,16,encore::crc32(b.data(),b.size()));}
std::vector<uint8_t>read(const char*p){std::ifstream f(p,std::ios::binary);CHECK(bool(f));return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),{});}
std::string digest(const MusicLibraryBindings&b){std::ostringstream out;out<<b.valid()<<b.admitted()<<b.asset_root()<<b.index_file().path<<b.index_file().bytes<<b.index_file().crc<<b.index().valid()<<b.index().tracks().size();for(const auto&r:b.bindings())out<<r.track<<r.metadata.path<<r.metadata.bytes<<r.metadata.crc<<r.audio.path<<r.audio.bytes<<r.audio.crc;return out.str();}
std::string media(const MusicRegionData&d,const AudioBank&b){std::ostringstream out;out<<d.valid()<<d.format_version()<<d.empty_player_gain_db()<<d.silence_db()<<d.fade_to_seconds()<<b.count()<<b.master_db();for(const auto&t:d.tracks()){out<<t.id<<t.source_path;for(auto n:t.source_sha)out<<unsigned(n);for(auto n:t.import_sha)out<<unsigned(n);}for(uint32_t i=0;i<b.count();++i){const auto a=b.asset(i);out<<a.stable_id<<a.source_path<<a.pcm_path<<a.frames<<a.pcm_crc<<a.gain_db;}return out.str();}
void negative(const std::vector<uint8_t>&raw,const char*root,const AudioBank&source){
 MusicLibraryBindings b;CHECK(b.load(raw.data(),raw.size(),error));const auto before=digest(b);
 auto reject=[&](const std::vector<uint8_t>&v){CHECK(!b.load(v.data(),v.size(),error)&&digest(b)==before);};
 CHECK(!b.load(nullptr,raw.size(),error)&&digest(b)==before);CHECK(!b.load_file(nullptr,error)&&digest(b)==before);
 for(size_t size=0;size<raw.size();++size){std::vector<uint8_t>v(raw.begin(),raw.begin()+size);reject(v);}
 for(const auto mutation:std::vector<std::pair<size_t,uint32_t>>{{8,0},{8,2},{20,0},{20,2},{24,0},{24,65},{28,1},{32,0},{32,65537},{40,0},{40,257}}){auto v=raw;store(v,mutation.first,mutation.second);fix(v);reject(v);}
 auto v=raw;v[0]^=1;fix(v);reject(v);v=raw;v[60]^=1;reject(v);v=raw;v.push_back(0);fix(v);reject(v);
 const auto index_end=44+word(raw.data()+40);const auto meta_length=word(raw.data()+index_end+12);const auto audio_start=index_end+16+meta_length;const auto first_end=audio_start+12+word(raw.data()+audio_start+8);
 v=raw;store(v,index_end,0);fix(v);reject(v);v=raw;store(v,first_end,word(raw.data()+index_end));fix(v);reject(v);
 for(uint8_t byte:{uint8_t(':'),uint8_t('\\'),uint8_t(' '),uint8_t(255)}){v=raw;v[44]=byte;fix(v);reject(v);}
 MusicRegionData output;AudioBank audio;const auto old=media(output,audio);CHECK(!b.read_track(1,output,audio,error)&&media(output,audio)==old);
 CHECK(!b.admit(nullptr,source,error)&&digest(b)==before);AudioBank empty;CHECK(!b.admit(root,empty,error)&&digest(b)==before);CHECK(!b.admit("missing-unpublished-music-root",source,error)&&digest(b)==before);
 // Descriptor CRC is valid but its bound index fingerprint is wrong.
 v=raw;store(v,36,word(v.data()+36)^1);fix(v);MusicLibraryBindings wrong;CHECK(wrong.load(v.data(),v.size(),error));auto wrong_before=digest(wrong);CHECK(!wrong.admit(root,source,error)&&digest(wrong)==wrong_before);
 // Selected bank admission is lazy. A bad selected-file fingerprint must not
 // prevent unrelated metadata admission, nor publish either output on read.
 v=raw;store(v,index_end+8,word(v.data()+index_end+8)^1);fix(v);CHECK(wrong.load(v.data(),v.size(),error)&&wrong.admit(root,source,error));wrong_before=digest(wrong);CHECK(!wrong.read_track(word(v.data()+index_end),output,audio,error)&&media(output,audio)==old&&digest(wrong)==wrong_before);
 // Swap complete file references between two distinct identities, preserving
 // every path, actual file size/CRC and uniqueness. Semantic joins must reject.
 const auto second_meta_length=word(raw.data()+first_end+12);const auto second_audio=first_end+16+second_meta_length;const auto second_end=second_audio+12+word(raw.data()+second_audio+8);
 std::vector<uint8_t>swapped(raw.begin(),raw.begin()+index_end+4);swapped.insert(swapped.end(),raw.begin()+first_end+4,raw.begin()+second_end);swapped.insert(swapped.end(),raw.begin()+first_end,raw.begin()+first_end+4);swapped.insert(swapped.end(),raw.begin()+index_end+4,raw.begin()+first_end);swapped.insert(swapped.end(),raw.begin()+second_end,raw.end());fix(swapped);
 CHECK(wrong.load(swapped.data(),swapped.size(),error)&&wrong.admit(root,source,error));CHECK(!wrong.read_track(word(raw.data()+index_end),output,audio,error)&&media(output,audio)==old);
}
}
int main(int argc,char**argv){
 CHECK(argc==4);const auto raw=read(argv[1]);AudioBank source;CHECK(source.load_file(argv[3],error));MusicLibraryBindings bindings;CHECK(bindings.load_file(argv[1],error)&&bindings.valid()&&!bindings.admitted());CHECK(bindings.admit(argv[2],source,error)&&bindings.admitted()&&bindings.index().library()&&bindings.bindings().size()==13);
 const auto admitted=digest(bindings);CHECK(!bindings.admit(argv[2],source,error)&&digest(bindings)==admitted);
 MusicRegionData data;AudioBank bank;
 for(const auto&b:bindings.bindings()){CHECK(bindings.find(b.track)==&b);CHECK(bindings.read_track(b.track,data,bank,error)&&data.tracks().size()==1&&bank.count()==1&&bank.asset(0).stable_id==b.track);}
 const auto old=media(data,bank);CHECK(!bindings.read_track(0,data,bank,error)&&media(data,bank)==old);CHECK(!bindings.read_track(1,data,bank,error)&&media(data,bank)==old);CHECK(!bindings.find(0));
 auto retained=bindings;CHECK(bindings.load(raw.data(),raw.size(),error)&&!bindings.admitted()&&bindings.index().tracks().empty()&&bindings.asset_root().empty());CHECK(retained.admitted()&&retained.read_track(retained.bindings()[0].track,data,bank,error));negative(raw,argv[2],source);
 std::printf("Music library bindings: %u checks; explicit 13-track metadata joins, lazy checked bank reads, source/import/PCM ownership, truncation/schema/path/CRC/identity rollback; no PCM validation or live frontend claim\n",checks);
}
