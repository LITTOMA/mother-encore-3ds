#include "encore/field_tint.hpp"
#include "encore/content.hpp"
#include "encore/source_random.hpp"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <map>
using namespace encore::upstream;
namespace {
void check(bool ok,const char*why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
void fix(std::vector<uint8_t>&b){uint32_t c=~0u;for(size_t i=0;i<b.size();++i){c^=i>=16&&i<20?0:b[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}put(b,16,~c);}
}
int main(int argc,char**argv){check(argc==2,"Actual Podunk Tint resource required");std::string e;FieldTintData data;check(data.load_file(argv[1],e),e.c_str());std::vector<uint8_t>b;check(encore::read_file(argv[1],b,2*1024*1024,e),e.c_str());const auto pin=data.source_pin();
 for(size_t n=0;n<b.size();++n)check(!data.load(b.data(),n,e),"truncated Tint rejected");for(const auto row:std::vector<std::pair<size_t,uint32_t>>{{8,2},{20,2},{24,2},{60,1},{64,0x7fc00000},{80,0},{92,99},{96,1000}}){auto bad=b;put(bad,row.first,row.second);fix(bad);check(!data.load(bad.data(),bad.size(),e),"unknown Tint header/kind/color/count rejected");check(data.valid()&&data.source_pin()==pin,"failed binary replacement atomic");}
 unsigned static_count=0;for(const auto&r:data.records())static_count+=r.kind==FieldTintKind::Scene;check(static_count==87&&data.prototype(FieldTintKind::EnemyPrototype)&&data.prototype(FieldTintKind::ActorPrototype),"all actual Podunk Tint children and source factory templates");
 FieldTintHost host;std::vector<uint32_t>writes;std::map<uint32_t,FieldTintColor>colors;unsigned resolves=0;bool reject=false;
 host.resolve=[&](uint32_t owner,const FieldTintDescriptor&d,const FieldTintTarget&t,bool&exists,uint32_t&target,std::string&out){++resolves;if(reject){out="source child adapter absent";return false;}exists=t.exists;target=d.kind==FieldTintKind::Scene?t.source_id:owner^t.source_id;return true;};
 host.self_modulate=[&](uint32_t id,FieldTintColor color,std::string&){writes.push_back(id);colors[id]=color;return true;};
 FieldTintRuntime runtime;auto missing=host;missing.self_modulate={};check(!runtime.initialize(data,missing,e),"missing visual Host rejected");check(runtime.initialize(data,host,e),e.c_str());const auto&a=data.records()[0];const auto&c=data.records()[1];const auto&d=data.records()[2];check(runtime.create(a.id,a.id)&&runtime.create(c.id,c.id)&&runtime.create(d.id,d.id),runtime.error().c_str());check(!runtime.create(a.id,a.id)&&!runtime.create(a.id,123),"duplicate or mismatched source owner rejected");
 SourceRandom random(99);const auto raw=random.raw_draw_count();check(runtime.ready(a.id)&&runtime.ready(c.id),runtime.error().c_str());check(random.raw_draw_count()==raw,"Tint child Ready has no shared RNG draw");check(!runtime.ready(a.id),"duplicate Ready rejected");
 const FieldTintColor color{.2f,.3f,.4f,.5f};check(runtime.set_tint(a.id,color),runtime.error().c_str());check(writes.size()==a.targets.size(),"source writes each target before signal");check(runtime.connect_tint(a.id,c.id),runtime.error().c_str());check(runtime.instance(c.id)->tint==color,"connect immediately propagates stored tint");const auto edges=runtime.instance(a.id)->connections.size();const auto before_duplicate=writes.size();check(runtime.connect_tint(a.id,c.id)&&runtime.instance(a.id)->connections.size()==edges&&writes.size()==before_duplicate+c.targets.size(),"duplicate connect keeps one edge but immediate set still executes");
 check(!runtime.connect_tint(c.id,a.id),"source unreviewed cycle remains explicit rejected capability");
 // Source set_tint can populate before Ready, then _ready appends the same
 // target array again. Preserve actual source append semantics, not deduplication.
 const auto before=resolves;check(runtime.set_tint(d.id,color)&&runtime.ready(d.id),runtime.error().c_str());check(resolves==before+2*d.targets.size()&&runtime.instance(d.id)->targets.size()==2*d.targets.size(),"lazy set plus later Ready appends source duplicates");check(runtime.connect_tint(c.id,d.id),runtime.error().c_str());runtime.take_events();writes.clear();const FieldTintColor next{.8f,.7f,.6f,1};check(runtime.set_tint(a.id,next),runtime.error().c_str());check(writes.size()==a.targets.size()+c.targets.size()+2*d.targets.size(),"ordered synchronous cascade applies every source target");const auto events=runtime.take_events();std::vector<uint32_t>signals;for(const auto&x:events)if(x.kind==FieldTintEventKind::ChangedTint)signals.push_back(x.sender);check(signals==std::vector<uint32_t>{a.id,c.id,d.id},"source signal order is depth-first registration order");
 const auto*enemy=data.prototype(FieldTintKind::EnemyPrototype);check(runtime.create(enemy->id,9000001)&&runtime.ready(9000001),runtime.error().c_str());check(runtime.set_tint(9000001,next),runtime.error().c_str());check(enemy->targets.size()==2,"checked enemy shadow and sprite tint targets");
 check(runtime.destroy(c.id)&&runtime.instance(a.id)->connections.empty(),"scene exit removes signal connection");check(!runtime.set_tint(c.id,next)&&!runtime.destroy(c.id),"unknown/freed receiver fails closed");check(!runtime.set_tint(a.id,FieldTintColor{NAN,0,0,1}),"invalid color fails closed");
 FieldTintRuntime unsupported;check(unsupported.initialize(data,host,e)&&unsupported.create(a.id,a.id),e.c_str());reject=true;check(!unsupported.ready(a.id)&&!unsupported.instance(a.id)->ready,"missing adapter cannot become a fake Ready");
 std::cout<<"Manual original Tint schema/Ready/nullable source targets/order/cascade/duplicate connection/factory cases\n";
}
