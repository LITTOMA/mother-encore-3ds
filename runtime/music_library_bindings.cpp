#include "encore/music_library_bindings.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cstring>
namespace encore::upstream {namespace {
constexpr size_t limit=65536;
bool bad(std::string&e,const char*why){e=why;return false;}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool path_ok(const std::string&p,const char*suffix){
 if(p.empty()||p.size()>256||p.size()<=std::strlen(suffix)||p.compare(p.size()-std::strlen(suffix),std::strlen(suffix),suffix))return false;
 for(unsigned char c:p)if(c<33||c>126||c=='\\'||c==':'||c=='?'||c=='#')return false;
 size_t first=0;while(first<=p.size()){auto end=p.find('/',first);if(end==p.npos)end=p.size();const auto part=p.substr(first,end-first);if(part.empty()||part=="."||part=="..")return false;if(end==p.size())break;first=end+1;}return true;
}
struct Reader{
 const uint8_t*p;size_t n;bool okay=true;
 uint32_t integer(){if(n<4){okay=false;return 0;}const auto v=word(p);p+=4;n-=4;return v;}
 MusicLibraryFile file(const char*suffix){MusicLibraryFile f;f.bytes=integer();f.crc=integer();const auto size=integer();if(!okay||!f.bytes||f.bytes>limit||!size||size>256||size>n){okay=false;return f;}f.path.assign(reinterpret_cast<const char*>(p),size);p+=size;n-=size;okay=path_ok(f.path,suffix);return f;}
};
bool read(const std::string&root,const MusicLibraryFile&f,std::vector<uint8_t>&bytes,std::string&e){
 std::vector<uint8_t>next;if(!encore::read_file((root+f.path).c_str(),next,limit,e))return false;
 if(next.size()!=f.bytes||encore::crc32(next.data(),next.size())!=f.crc)return bad(e,"Music library file size/checksum differs from checked descriptor");
 bytes=std::move(next);e.clear();return true;
}
bool same(const AudioAsset&a,const AudioAsset&b){return a.stable_id==b.stable_id&&a.sample_rate==b.sample_rate&&a.channels==b.channels&&a.flags==b.flags&&a.frames==b.frames&&a.loop_start==b.loop_start&&a.gain_db==b.gain_db&&a.pcm_bytes==b.pcm_bytes&&a.pcm_crc==b.pcm_crc&&a.source_sha256==b.source_sha256&&a.source_path==b.source_path&&a.pcm_path==b.pcm_path;}
}
const MusicLibraryBinding*MusicLibraryBindings::find(uint32_t id)const{if(valid_)for(const auto&b:bindings_)if(b.track==id)return &b;return nullptr;}
bool MusicLibraryBindings::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<32||n>limit)return bad(e,"Music library descriptor size is invalid");
 if(std::memcmp(p,"ENCLIB01",8)||word(p+8)!=1||word(p+12)!=n||word(p+20)!=1||!word(p+24)||word(p+24)>64||word(p+28))return bad(e,"Music library descriptor schema/count/reserved is invalid");
 const uint8_t zero[4]{};auto crc=encore::crc32_update(~0u,p,16);crc=encore::crc32_update(crc,zero,4);crc=encore::crc32_update(crc,p+20,n-20);
 if((crc^~0u)!=word(p+16))return bad(e,"Music library descriptor checksum is invalid");
 MusicLibraryBindings next;Reader r{p+32,n-32};next.index_file_=r.file(".encmusic");
 for(uint32_t i=0;i<word(p+24);++i){MusicLibraryBinding b;b.track=r.integer();b.metadata=r.file(".encmusic");b.audio=r.file(".encaudio");if(!r.okay||!b.track)return bad(e,"Music library descriptor binding is invalid");
  if(b.metadata.path==next.index_file_.path||b.metadata.path==b.audio.path)return bad(e,"Music library descriptor file aliases its index");
  for(const auto&prior:next.bindings_)if(prior.track==b.track||prior.metadata.path==b.metadata.path||prior.audio.path==b.audio.path)return bad(e,"Music library descriptor duplicate identity/path");
  next.bindings_.push_back(std::move(b));
 }
 if(!r.okay||r.n)return bad(e,"Music library descriptor truncated/trailing records");
 next.valid_=true;*this=std::move(next);e.clear();return true;
}
bool MusicLibraryBindings::load_file(const char*path,std::string&e){if(!path)return bad(e,"Music library descriptor path is absent");std::vector<uint8_t>bytes;return encore::read_file(path,bytes,limit,e)&&load(bytes.data(),bytes.size(),e);}
bool MusicLibraryBindings::admit(const char*root,const AudioBank&bank,std::string&e){
 if(!valid_||admitted_||!root||!bank.count())return bad(e,"Music library admission owner/root/source bank is invalid");
 std::string prefix(root);if(!prefix.empty()&&prefix.back()!='/'&&prefix.back()!='\\')prefix+='/';
 std::vector<uint8_t>bytes;MusicRegionData index;
 if(!read(prefix,index_file_,bytes,e)||!index.load(bytes.data(),bytes.size(),e))return false;
 if(!index.library()||index.tracks().size()!=bindings_.size()||!index.matches(bank,e))return bad(e,"Music library index/source bank coverage is invalid");
 for(const auto&track:index.tracks())if(!find(track.id))return bad(e,"Music library index has no explicit file binding");
 index_=std::move(index);source_bank_=bank;root_=std::move(prefix);admitted_=true;e.clear();return true;
}
bool MusicLibraryBindings::read_track(uint32_t id,MusicRegionData&metadata,AudioBank&audio,std::string&e)const{
 const auto*b=find(id);if(!admitted_||!b)return bad(e,"Music library track has no admitted file binding");
 std::vector<uint8_t>music_bytes,audio_bytes;MusicRegionData music;AudioBank bank;
 if(!read(root_,b->metadata,music_bytes,e)||!read(root_,b->audio,audio_bytes,e)||!music.load(music_bytes.data(),music_bytes.size(),e)||!bank.load(audio_bytes.data(),audio_bytes.size(),e))return false;
 if(!music.library()||music.tracks().size()!=1||music.tracks()[0].id!=id||bank.count()!=1||!music.matches(bank,e)||music.silence_db()!=index_.silence_db()||music.fade_to_seconds()!=index_.fade_to_seconds()||music.empty_player_gain_db()!=index_.empty_player_gain_db()||bank.master_db()!=source_bank_.master_db())return bad(e,"Music selected bank/index policy or coverage differs");
 const auto&one=music.tracks()[0];const MusicRegionTrack*declared=nullptr;for(const auto&t:index_.tracks())if(t.id==id)declared=&t;AudioAsset original,selected;
 if(!declared||declared->source_path!=one.source_path||declared->source_sha!=one.source_sha||declared->import_sha!=one.import_sha||!source_bank_.find(id,original)||!bank.find(id,selected)||!same(original,selected))return bad(e,"Music selected bank/import/PCM differs from admitted source");
 metadata=std::move(music);audio=std::move(bank);e.clear();return true;
}
}
