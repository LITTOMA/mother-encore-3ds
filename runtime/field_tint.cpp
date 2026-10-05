#include "encore/field_tint.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=i>=16&&i<20?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return~c;}
struct Reader {const uint8_t*p;size_t n,at=64;bool ok=true;
 uint32_t integer(){if(!ok||at>n||n-at<4){ok=false;return 0;}uint32_t v=u32(p+at);at+=4;return v;}
 float scalar(){uint32_t v=integer();float f;std::memcpy(&f,&v,4);if(!std::isfinite(f))ok=false;return f;}
 FieldTintColor color(){return{scalar(),scalar(),scalar(),scalar()};}
 std::string text(){uint32_t len=integer();if(!ok||len>8192||at>n||len>n-at){ok=false;return{};}std::string v(reinterpret_cast<const char*>(p+at),len);at+=len;size_t i=0;uint32_t cp=0;while(i<v.size())if(!encore::utf8_next(v,i,cp)||cp<32){ok=false;break;}return v;}
 bool hash(){if(!ok||at>n||n-at<32){ok=false;return false;}bool any=false;for(unsigned i=0;i<32;++i)any|=p[at+i]!=0;at+=32;ok&=any;return any;}
};
bool path(std::string_view p){return!p.empty()&&p.front()!='/'&&p.back()!='/'&&p.find("..") ==p.npos&&p.find(':')==p.npos&&p.find('\\')==p.npos;}
bool relative(std::string_view p){return!p.empty()&&p.front()!='/'&&p.find(':')==p.npos&&p.find('\\')==p.npos;}
bool canvas(std::string_view k){return k=="Sprite"||k=="Node2D"||k=="KinematicBody2D"||k=="StaticBody2D"||k=="Area2D"||k=="AnimatedSprite"||k=="Light2D"||k=="TileMap"||k=="Control";}
bool valid_color(FieldTintColor color){return std::all_of(color.begin(),color.end(),[](float v){return std::isfinite(v);});}
}
const FieldTintDescriptor*FieldTintData::record(uint32_t id)const{for(const auto&r:records_)if(r.id==id)return&r;return nullptr;}
const FieldTintDescriptor*FieldTintData::prototype(FieldTintKind kind)const{if(kind==FieldTintKind::Scene)return nullptr;for(const auto&r:records_)if(r.kind==kind)return&r;return nullptr;}
bool FieldTintData::load_file(const char*name,std::string&e){if(!name){e="Tint pack path absent";return false;}std::ifstream f(name,std::ios::binary);if(!f){e="Tint pack unavailable";return false;}f.seekg(0,std::ios::end);const auto n=f.tellg();if(n<64||n>2*1024*1024){e="Tint pack file size";return false;}f.seekg(0);std::vector<uint8_t>b(static_cast<size_t>(n));if(!f.read(reinterpret_cast<char*>(b.data()),n)){e="Tint pack file read";return false;}return load(b.data(),b.size(),e);}
bool FieldTintData::load(const uint8_t*p,size_t n,std::string&e){
 auto reject=[&](const char*t){e=t;return false;};if(!p||n<64||n>2*1024*1024)return reject("Tint pack size");if(std::memcmp(p,"ENCTINT1",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+24)!=1||!u32(p+28)||u32(p+28)>4096||!u32(p+52)||!u32(p+56)||u32(p+56)>512||u32(p+60)||u32(p+16)!=crc(p,n))return reject("Tint pack header/version/capabilities/rules/CRC");
 FieldTintData next;std::copy(p+32,p+52,next.pin_.begin());if(std::all_of(next.pin_.begin(),next.pin_.end(),[](uint8_t b){return!b;}))return reject("Tint source pin absent");next.scene_=u32(p+52);Reader r{p,n};next.default_=r.color();std::set<uint32_t>ids,prototypes;uint32_t last=0;bool had=false;
 for(uint32_t i=0;i<u32(p+28);++i){FieldTintDescriptor d;d.id=r.integer();d.scene_id=r.integer();d.ready_ordinal=r.integer();const auto kind=r.integer(),count=r.integer();d.kind=FieldTintKind(kind);d.scene=r.text();d.node=r.text();if(!r.ok||!d.id||d.id==UINT32_MAX||!ids.insert(d.id).second||!d.scene_id||kind<1||kind>3||count>32||!path(d.scene)||!path(d.node))return reject("Tint source descriptor");if(kind==1){if(d.scene_id!=next.scene_||(had&&d.ready_ordinal<=last)||d.ready_ordinal==UINT32_MAX)return reject("Tint static Ready order/source scene");last=d.ready_ordinal;had=true;}else if(!prototypes.insert(kind).second||d.ready_ordinal!=UINT32_MAX)return reject("Tint factory prototype namespace/order");
  for(uint32_t j=0;j<count;++j){FieldTintTarget t;t.source_id=r.integer();auto exists=r.integer();t.exists=exists;t.initial_self_modulate=r.color();t.node_path=r.text();t.node=r.text();t.kind=r.text();if(!r.ok||!t.source_id||t.source_id==UINT32_MAX||exists>1||!relative(t.node_path)||!path(t.node)||(t.exists?!canvas(t.kind):!t.kind.empty()))return reject("Tint source target/CanvasItem/color");d.targets.push_back(std::move(t));}next.records_.push_back(std::move(d));
 }
 const auto sources=r.integer();if(sources!=u32(p+56))return reject("Tint source receipt count");std::set<std::string>names;for(uint32_t i=0;i<sources;++i){const auto source=r.text();if(!r.ok||!path(source)||!names.insert(source).second||!r.hash())return reject("Tint source closure/hash");}if(!r.ok||r.at!=n)return reject("Tint trailing data");for(const auto&d:next.records_)if(!names.count(d.scene))return reject("Tint descriptor scene outside source closure");next.valid_=true;*this=std::move(next);e.clear();return true;
}
bool FieldTintRuntime::fail(const char*s){error_=s;return false;}
bool FieldTintRuntime::initialize(const FieldTintData&data,FieldTintHost host,std::string&e){if(!data.valid()||!host.resolve||!host.self_modulate){e="Tint runtime data/Host incomplete";return false;}data_=&data;host_=std::move(host);instances_.clear();events_.clear();active_signal_.clear();error_.clear();e.clear();return true;}
const FieldTintInstance*FieldTintRuntime::instance(uint32_t id)const{auto i=instances_.find(id);return i==instances_.end()?nullptr:&i->second;}
bool FieldTintRuntime::create(uint32_t descriptor_id,uint32_t id){if(!data_||!id||id==UINT32_MAX||instances_.count(id)||instances_.size()>=4096)return fail("Tint instance admission rejected");const auto*d=data_->record(descriptor_id);if(!d||(d->kind==FieldTintKind::Scene&&id!=d->id))return fail("Tint source/factory identity mismatch");FieldTintInstance s;s.id=id;s.descriptor_id=d->id;s.tint=data_->default_tint();instances_.emplace(id,std::move(s));return true;}
bool FieldTintRuntime::populate(FieldTintInstance&s){const auto*d=data_->record(s.descriptor_id);if(!d)return fail("Tint source descriptor lost");std::vector<uint32_t>targets;
 for(const auto&t:d->targets){if(!t.exists)continue;bool exists=false;uint32_t resolved=0;if(!host_.resolve(s.id,*d,t,exists,resolved,error_))return false;if(exists){if(!resolved||resolved==UINT32_MAX)return fail("Tint native target identity rejected");targets.push_back(resolved);}}
 s.targets.insert(s.targets.end(),targets.begin(),targets.end());return true;
}
bool FieldTintRuntime::ready(uint32_t id){auto i=instances_.find(id);if(i==instances_.end()||i->second.ready)return fail("Tint Ready missing/duplicate instance");if(!populate(i->second))return false;i->second.ready=true;events_.push_back({FieldTintEventKind::Ready,id,0,i->second.tint});return true;}
bool FieldTintRuntime::set_tint(uint32_t id,FieldTintColor color){auto i=instances_.find(id);if(i==instances_.end()||!valid_color(color))return fail("Tint set target/color rejected");if(std::find(active_signal_.begin(),active_signal_.end(),id)!=active_signal_.end())return fail("Unreviewed Tint signal cycle");auto&s=i->second;s.tint=color;if(s.targets.empty()&&!populate(s))return false;
 const auto targets=s.targets;for(uint32_t target:targets){if(!host_.self_modulate(target,color,error_))return false;events_.push_back({FieldTintEventKind::SelfModulate,id,target,color});}
 events_.push_back({FieldTintEventKind::ChangedTint,id,0,color});const auto receivers=s.connections;active_signal_.push_back(id);for(uint32_t receiver:receivers)if(!set_tint(receiver,color)){active_signal_.pop_back();return false;}active_signal_.pop_back();return true;
}
bool FieldTintRuntime::reachable(uint32_t from,uint32_t to,std::vector<uint32_t>&seen)const{if(from==to)return true;if(std::find(seen.begin(),seen.end(),from)!=seen.end())return false;seen.push_back(from);const auto*i=instance(from);if(!i)return false;for(auto id:i->connections)if(reachable(id,to,seen))return true;return false;}
bool FieldTintRuntime::connect_tint(uint32_t sender,uint32_t receiver){auto s=instances_.find(sender),r=instances_.find(receiver);if(s==instances_.end()||r==instances_.end())return fail("Tint connection requires actual admitted instances");std::vector<uint32_t>seen;if(reachable(receiver,sender,seen))return fail("Unreviewed cyclic Tint connection");
 auto&edges=s->second.connections;const bool duplicate=std::find(edges.begin(),edges.end(),receiver)!=edges.end();if(!duplicate)edges.push_back(receiver);
 events_.push_back({FieldTintEventKind::Connect,sender,receiver,s->second.tint});
 // Godot rejects a duplicate connect() return code; source does not branch on
 // that result and still immediately calls receiver.set_tint(current_tint).
 return set_tint(receiver,s->second.tint);
}
bool FieldTintRuntime::destroy(uint32_t id){if(!instances_.count(id)||!active_signal_.empty())return fail("Tint destroy unknown/reentrant instance");instances_.erase(id);for(auto&pair:instances_){auto&edges=pair.second.connections;edges.erase(std::remove(edges.begin(),edges.end(),id),edges.end());}return true;}
}
