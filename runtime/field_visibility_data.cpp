#include "encore/field_visibility.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
#include <climits>
#include <tuple>
namespace encore::upstream {
namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}return ~c;}
bool reject(std::string&e){e="Native scene visibility format/source closure rejected";return false;}
struct Reader{
 const uint8_t*p;size_t n,pos=128;bool ok=true;
 uint32_t u(){if(!ok||pos+4>n){ok=false;return 0;}auto x=word(p+pos);pos+=4;return x;}
 float f(){auto b=u();float v=0;std::memcpy(&v,&b,4);if(!std::isfinite(v))ok=false;return v;}
 std::string t(){auto s=u();if(!ok||s>65536||s>n-pos){ok=false;return{};}std::string x(reinterpret_cast<const char*>(p+pos),s);pos+=s;if(x.find('\0')!=std::string::npos)ok=false;return x;}
 std::array<uint8_t,32>hash(){std::array<uint8_t,32>h{};if(!ok||pos+32>n){ok=false;return h;}std::copy_n(p+pos,32,h.begin());pos+=32;return h;}
};
bool nonzero(const std::array<uint8_t,32>&h){return std::any_of(h.begin(),h.end(),[](auto x){return x!=0;});}
}
bool FieldVisibilityData::load(const uint8_t*p,size_t n,const FieldNodeTreeData&tree,std::string&e){
 if(!tree.valid()||!p||n<128||n>8*1024*1024||std::memcmp(p,"ENCFVS01",8)||word(p+8)!=1||word(p+12)!=128||word(p+16)!=n||word(p+20)!=crc(p+128,n-128)||word(p+24)!=0x454e0069||word(p+28)!=1||word(p+32)!=1||word(p+36)!=tree.identity().scene_id||!std::equal(p+40,p+60,tree.identity().upstream_commit.begin())||!std::equal(p+60,p+92,tree.identity().source_sha256.begin())||word(p+124))return reject(e);
 Reader r{p,n};FieldVisibilityData next;next.identity_=tree.identity();std::copy_n(p+92,32,next.ir_sha_.begin());if(!nonzero(next.ir_sha_)||r.t()!=tree.source_scene())return reject(e);
 auto cell=r.u(),cutoff=r.u(),count=r.u();if(!cell||cell>uint32_t(INT32_MAX)||!cutoff||!count||count>tree.records().size())return reject(e);next.cell_=int32_t(cell);next.scan_cutoff_=cutoff;
 std::set<uint32_t>expected;for(const auto&d:tree.records())if(tree.classes()[d.class_index]=="VisibilityNotifier2D"||tree.classes()[d.class_index]=="VisibilityEnabler2D")expected.insert(d.id);
 if(expected.size()!=count)return reject(e);
 for(uint32_t i=0;i<count;++i){FieldVisibilityRecord v;v.id=r.u();v.parent=r.u();v.scope=r.u();v.kind=r.u();v.ready=r.u();v.flags=r.u();v.rect={r.f(),r.f(),r.f(),r.f()};v.path=r.t();auto tracks=r.u();const auto*d=tree.record(v.id);const auto*s=tree.record(v.scope);
  if(!r.ok||!d||!s||d->parent!=v.parent||d->ready!=v.ready||d->path!=v.path||!d->script.empty()||v.kind<1||v.kind>2||v.flags>63||(v.kind==1&&v.flags)||v.rect.width<0||v.rect.height<0||!expected.erase(v.id)||tree.classes()[d->class_index]!=(v.kind==1?"VisibilityNotifier2D":"VisibilityEnabler2D")||tracks>tree.records().size()||(v.kind==1&&tracks))return reject(e);
  std::set<uint32_t>seen;for(uint32_t j=0;j<tracks;++j){FieldVisibilityTracked q{r.u(),r.u()};const auto*t=tree.record(q.id);const char*classes[]={"","AnimationPlayer","AnimatedSprite","RigidBody2D","Particles2D"};if(!t||q.kind<1||q.kind>4||tree.classes()[t->class_index]!=classes[q.kind]||!seen.insert(q.id).second)return reject(e);auto ancestor=t;while(ancestor&&ancestor->id!=v.scope)ancestor=tree.record(ancestor->parent);if(!ancestor)return reject(e);v.tracked.push_back(q);}
  next.index_.emplace(v.id,next.records_.size());next.records_.push_back(std::move(v));
 }
 count=r.u();if(!r.ok||count>tree.records().size()*4)return reject(e);std::set<std::tuple<uint32_t,uint32_t,uint32_t,std::string>>keys;
 for(uint32_t i=0;i<count;++i){FieldVisibilityConnection c;c.emitter=r.u();c.target=r.u();c.adapter=r.u();c.signal=r.u();c.method=r.t();c.script=r.t();c.script_sha=r.hash();const auto*t=tree.record(c.target);if(!r.ok||!next.record(c.emitter)||!t||c.adapter<1||c.adapter>6||c.signal<1||c.signal>2||c.method.empty()||c.script!=t->script||c.script_sha!=t->script_sha||!keys.emplace(c.emitter,c.target,c.signal,c.method).second)return reject(e);next.connections_.push_back(std::move(c));}
 count=r.u();if(!r.ok||!count||count>100000)return reject(e);std::set<std::string>paths;for(uint32_t i=0;i<count;++i){auto path=r.t();auto h=r.hash();std::array<uint8_t,32>actual{};if(!r.ok||!paths.insert(path).second||!tree.source_hash(path,actual)||h!=actual)return reject(e);}
 if(!r.ok||r.pos!=n||!expected.empty())return reject(e);
 next.valid_=true;*this=std::move(next);return true;
}
bool FieldVisibilityData::load_file(const char*path,const FieldNodeTreeData&tree,std::string&e){if(!path)return reject(e);std::ifstream f(path,std::ios::binary|std::ios::ate);auto n=f.tellg();if(!f||n<128||n>8*1024*1024)return reject(e);std::vector<uint8_t>b(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),n))return reject(e);return load(b.data(),b.size(),tree,e);}
const FieldVisibilityRecord*FieldVisibilityData::record(uint32_t id)const{auto i=index_.find(id);return i==index_.end()?nullptr:&records_[i->second];}
}
