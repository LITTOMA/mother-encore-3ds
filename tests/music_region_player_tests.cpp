#include "music_region_player.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <fstream>
#include <iterator>
using namespace encore;
namespace {unsigned checks=0,mutations=0;int alloc_left=-1;size_t allocated=0;Result flush_result=0;
std::array<std::vector<ndspWaveBuf*>,20>queues;std::array<float,20>gains{};std::array<bool,20>paused{};
void check(bool x,const char*m){++checks;if(!x){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
size_t ch(int c){check(c>=4&&c<20,"existing music/SFX/jingle/dialogue channels never touched");++mutations;return size_t(c);}
void drain(int c){auto&q=queues[ch(c)];for(auto*b:q)b->status=NDSP_WBUF_DONE;q.clear();}
uint32_t checksum(const int16_t*p,size_t n){return upstream::audio_crc32(reinterpret_cast<const uint8_t*>(p),n*2);}
}
Result ndspInit(){check(false,"adapter must not initialize owner DSP");return 0;}
void ndspExit(){check(false,"adapter must not exit owner DSP");}
void ndspSetMasterVol(float){check(false,"adapter must not change master gain");}
uint32_t ndspGetDroppedFrames(){return 0;}
void*linearAlloc(size_t n){if(alloc_left==0)return nullptr;if(alloc_left>0)--alloc_left;allocated+=n;return std::malloc(n);}
void linearFree(void*p){check(p!=nullptr,"valid release");allocated-=3*2048*4;std::free(p);}
void ndspChnReset(int c){drain(c);}void ndspChnWaveBufClear(int c){drain(c);}
void ndspChnSetMix(int c,float*v){gains[ch(c)]=v[0];check(v[0]==v[1],"equal stereo gain");}
void ndspChnSetPaused(int c,bool value){paused[ch(c)]=value;}
Result DSP_FlushDataCache(const void*p,uint32_t n){check(p&&n>0&&n<=2048*4,"bounded streaming flush");return flush_result;}
void ndspChnWaveBufAdd(int c,ndspWaveBuf*b){auto&q=queues[ch(c)];check(b->data_pcm16&&b->nsamples==2048&&!b->looping,"source streamed frame buffer");b->status=NDSP_WBUF_QUEUED;q.push_back(b);}
void ndspChnSetInterp(int c,int){ch(c);}void ndspChnSetRate(int c,float){ch(c);}void ndspChnSetFormat(int c,int){ch(c);}
// Test-only completion driver retains each original adapter case while
// exercising the production incremental API; this is not a runtime wrapper.
bool prepare_all(ctr::MusicRegionPlayer& player,const char* bank,const char* root,
                 const upstream::MusicRegionData& data,uint32_t capacity,
                 bool available,float master,std::string& error){
 if(!player.begin_prepare(bank,root,data,capacity,available,master,error))return false;
 for(unsigned i=0;i<20000;++i){
  const auto step=player.prepare_step(8192,error);
  if(step==ctr::MusicPreparationStep::Ready)return true;
  if(step==ctr::MusicPreparationStep::Failed)return false;
 }
 check(false,"bounded preparation must terminate for frozen fixture");return false;
}
int main(int argc,char**argv){check(argc==4,"region+audio+root arguments");upstream::MusicRegionData data;std::string e;check(data.load_file(argv[1],e),e.c_str());upstream::AudioBank bank;check(bank.load_file(argv[2],e),e.c_str());
 ctr::MusicRegionPlayer unavailable;check(!prepare_all(unavailable,argv[2],argv[3],data,8,false,bank.master_db(),e)&&mutations==0&&allocated==0,"unavailable preserves existing backend without init retry");
 ctr::MusicRegionPlayer failed;alloc_left=2;check(!prepare_all(failed,argv[2],argv[3],data,8,true,bank.master_db(),e)&&mutations==0&&allocated==0,"partial allocation rollback never touches existing channels");alloc_left=-1;
 ctr::MusicRegionPlayer audio;check(prepare_all(audio,argv[2],argv[3],data,8,true,bank.master_db(),e),e.c_str());check(mutations==0&&allocated==8*24576&&audio.buffer_bytes()==allocated,"prepare only checks streams and reserves192KiB, silent");
 upstream::MusicRegionController c;upstream::MusicRegionContext context;context.flag=[](std::string_view){return false;};check(c.initialize(data,8,e)&&c.attach_scene(1,e)&&c.enter(1,"Music/MusicArea",context,e)&&audio.sync(c,e),e.c_str());check(audio.submitted_voices()==1&&queues[4].size()==3,"region starts streaming");const auto first=checksum(queues[4][0]->data_pcm16,4096);auto generation=c.voices()[0].generation;
 check(c.enter(1,"Music/MusicArea3",context,e)&&c.advance(.5,false,e)&&audio.sync(c,e),"same track lower-gain area");check(audio.submitted_voices()==1&&c.voices()[0].generation==generation&&std::abs(gains[4]-upstream::audio_linear_gain(-4))<1e-5,"fadeto keeps queued voice and cursor");
 check(c.exit(1,"Music/MusicArea",context,e)&&c.exit(1,"Music/MusicArea3",context,e)&&c.idle_frame(1,e)&&c.enter(1,"Music/MusicArea6",context,e)&&audio.sync(c,e),"overlap crossfade retains old track");check(queues[4].size()==3&&queues[5].size()==3,"both music voices concurrently queued");
 check(c.exit(1,"Music/MusicArea6",context,e)&&c.idle_frame(1,e)&&c.enter(1,"Music/MusicArea",context,e)&&audio.sync(c,e),"rapid return duplicate original loop");check(audio.submitted_voices()==3&&queues[6].size()==3,"independent same-track duplicate voice supported");check(checksum(queues[6][0]->data_pcm16,4096)==first,"new duplicate starts at source frame zero");drain(4);check(audio.sync(c,e),"old same-track cursor refilled");check(checksum(queues[4][0]->data_pcm16,4096)!=first,"old voice phase continues independently");
 // Mainline menu gain and field-battle pause must coexist on the same voice.
 auto retained=queues[6];check(audio.set_paused(true,e)&&paused[6],"battle pause reaches live region voice");
 check(audio.set_user_music_db(-12)&&audio.sync(c,e)&&paused[6]&&queues[6]==retained,"menu volume update preserves battle pause and queued cursor");
 check(std::abs(gains[6]-upstream::audio_linear_gain(c.voices()[2].gain_db-12))<1e-5,"menu music gain remains applied while paused");
 check(!audio.set_user_music_db(25)&&!audio.set_user_music_db(NAN),"invalid menu music gain rejected");
 check(audio.set_paused(false,e)&&!paused[6]&&queues[6]==retained,"resume preserves queued audio");
 check(audio.set_user_music_db(0)&&audio.sync(c,e),"restore menu gain without restarting track");
 upstream::MusicRegionController wrong;check(wrong.initialize(data,1,e)&&wrong.attach_scene(2,e),"wrong capacity fixture");auto old=queues;check(!audio.sync(wrong,e)&&queues==old,"bad snapshot capacity cannot stop old audio");
 check(c.advance(2,false,e)&&audio.sync(c,e),"completed fades clear only old region channels");check(queues[4].empty()&&queues[5].empty()&&queues[6].size()==3,"current duplicate survives old fade cleanup");
 // A detached candidate does not claim a range. Once explicitly synchronized,
 // its new voice leases a free channel without touching the old scene voice.
 ctr::MusicRegionPlayer candidate;auto old_queues=queues;auto old_mutations=mutations;
 check(prepare_all(candidate,argv[2],argv[3],data,1,true,bank.master_db(),e)&&mutations==old_mutations,"detached replacement preparation leaves live region audio intact");
 check(candidate.sync(wrong,e)&&queues==old_queues&&mutations==old_mutations,"silent candidate claims no active channel");
 check(wrong.enter(2,"Music/MusicArea6",context,e)&&candidate.sync(wrong,e)&&queues[4].size()==3&&queues[6]==old_queues[6],"second explicitly committed scene leases a free channel and preserves old queue");
 old_queues=queues;const auto pump_gains=gains;const auto submitted=candidate.submitted_voices();
 drain(4);check(candidate.pump_streams(e)&&queues[4].size()==3&&queues[6]==old_queues[6]&&candidate.submitted_voices()==submitted&&gains==pump_gains,"refill preserves identities and other scene queues");
 const auto no_op_mutations=mutations;check(candidate.set_paused(true,e),"candidate pause admitted");const auto paused_mutations=mutations;check(candidate.pump_streams(e)&&mutations==paused_mutations,"paused regional refill has zero IO or channel work");check(candidate.set_paused(false,e)&&mutations>no_op_mutations,"candidate resume");
 candidate.shutdown();check(queues[4].empty()&&queues[6]==old_queues[6],"discarding scene releases only its own channel");
 flush_result=-1;drain(6);check(!audio.sync(c,e)&&e=="Region DSP cache flush failed"&&queues[6].empty(),"stream failure explicit and limited to affected voice");flush_result=0;check(audio.sync(c,e)&&queues[6].size()==3,"authorized retry refills affected region");
 audio.shutdown();check(allocated==0,"first scenes released");
 // Fill the physical pool with source-owned fade tails, then prove rejection
 // is atomic across every queue. No fabricated controller voice snapshots.
 ctr::MusicRegionPlayer full,overflow;
 upstream::MusicRegionController filled,extra;
 check(prepare_all(full,argv[2],argv[3],data,16,true,bank.master_db(),e)&&prepare_all(overflow,argv[2],argv[3],data,1,true,bank.master_db(),e),"physical pool fixtures prepared");
 check(filled.initialize(data,16,e)&&filled.attach_scene(3,e)&&extra.initialize(data,1,e)&&extra.attach_scene(4,e),"physical pool controllers");
 for(uint64_t n=1;n<=16;++n){
  check(filled.observe_external_player({n,true,true},e)&&filled.enter(3,"Music/MusicArea",context,e)&&full.sync(filled,e),"new external source order allocates distinct voice");
  check(filled.tree_exit(3,"Music/MusicArea",e),"source removes registration but retains fade tail");
 }
 check(extra.enter(4,"Music/MusicArea6",context,e),"overflow core fixture queued");
 const auto all_queues=queues;const auto exhausted_mutations=mutations;
 check(!overflow.sync(extra,e)&&e=="Regional channel capacity exhausted; existing audio preserved"&&queues==all_queues&&mutations==exhausted_mutations&&overflow.submitted_voices()==0,"physical exhaustion preserves all sixteen other queues and identities");
 full.shutdown();check(overflow.sync(extra,e)&&queues[4].size()==3,"released channel is reusable after explicit old owner teardown");overflow.shutdown();
// Global node routes use the same checked PCM transport, with two detached
 // providers. These bank instances intentionally share the original Podunk
 // files; distinct Podunk/Mayor metadata ownership is tested in the core.
 {
  upstream::MusicPlayers graph;upstream::MusicBankRoutes routes;
  check(graph.initialize({data.silence_db(),0,data.fade_to_seconds(),32},e),"global graph prepared");
  check(routes.initialize({8,16,bank.master_db(),bank.silence_db()},e)&&routes.add_provider(1,data,bank,e)&&routes.add_provider(2,data,bank,e),"independent checked providers");
  ctr::MusicRegionPlayer first_bank,second_bank;
  auto before_prepare=mutations;
  check(prepare_all(first_bank,argv[2],argv[3],data,4,true,bank.master_db(),e)&&prepare_all(second_bank,argv[2],argv[3],data,4,true,bank.master_db(),e)&&mutations==before_prepare,"global providers prepare without touching NDSP");
  check(!first_bank.check_routes(routes,1,e)&&mutations==before_prepare,"unpublished graph cannot acquire a transport");
  const auto source1=data.tracks()[0].source_path,source2=data.tracks()[1].source_path;
  check(graph.play_loop(0,source1,e)&&routes.reconcile(graph,{{0,1,1}},e),"first explicitly routed start");
  check(first_bank.check_routes(routes,1,e)&&second_bank.check_routes(routes,2,e)&&mutations==before_prepare,"complete route preflight is zero NDSP");
  check(first_bank.sync_routes(routes,1,e)&&queues[4].size()==3&&first_bank.submitted_voices()==1,"first routed stream queued");
  const auto initial_crc=checksum(queues[4][0]->data_pcm16,4096);
  auto retained_first=queues[4];
  check(graph.set_volume(0,-4,e)&&routes.reconcile(graph,{},e)&&first_bank.set_user_music_db(-12)&&first_bank.sync_routes(routes,1,e),"routed property and menu volume");
  check(queues[4]==retained_first&&first_bank.submitted_voices()==1&&std::abs(gains[4]-upstream::audio_linear_gain(-16))<.00001,"routed gain preserves the original queue and cursor");
  drain(4);check(first_bank.sync_routes(routes,1,e)&&checksum(queues[4][0]->data_pcm16,4096)!=initial_crc,"routed refill advances existing source phase");
  uint64_t second;check(graph.add_player(second,e)&&second==1&&graph.play_loop(second,source2,e)&&routes.reconcile(graph,{{second,1,2}},e),"second provider start admitted");
  retained_first=queues[4];check(second_bank.sync_routes(routes,2,e)&&queues[5].size()==3&&queues[4]==retained_first,"second provider retains first bank fade tail");
  const auto second_crc=checksum(queues[5][0]->data_pcm16,4096);auto retained_second=queues[5];auto before_reject=mutations;
  upstream::MusicRegionController legacy;check(legacy.initialize(data,4,e)&&legacy.attach_scene(1,e),"legacy owner fixture");
  check(!first_bank.sync(legacy,e)&&!first_bank.sync_routes(routes,2,e)&&queues[4]==retained_first&&queues[5]==retained_second&&mutations==before_reject,"wrong graph mode/provider cannot touch either bank");
  // Same numeric provider/player identities with changed checked metadata must
  // not redirect an already prepared stream. Only in-memory negative bytes change.
  for(unsigned kind=0;kind<2;++kind){
   std::ifstream mf(argv[1],std::ios::binary),af(argv[2],std::ios::binary);
   std::vector<uint8_t>mb{std::istreambuf_iterator<char>(mf),{}},ab{std::istreambuf_iterator<char>(af),{}};
   auto fix=[](std::vector<uint8_t>&b){std::fill(b.begin()+16,b.begin()+20,0);auto crc=upstream::audio_crc32(b.data(),b.size());for(unsigned i=0;i<4;++i)b[16+i]=uint8_t(crc>>(8*i));};
   if(kind==0){mb[100]^=1;fix(mb);}else{ab[120]^=1;fix(ab);}
   upstream::MusicRegionData changed;upstream::AudioBank altered;upstream::MusicBankRoutes wrong;
   check(changed.load(mb.data(),mb.size(),e)&&altered.load(ab.data(),ab.size(),e)&&wrong.initialize({8,16,bank.master_db(),bank.silence_db()},e)&&wrong.add_provider(1,changed,altered,e)&&wrong.reconcile(graph,{{0,1,1},{1,1,1}},e),"structurally valid changed metadata fixture");
   check(!first_bank.check_routes(wrong,1,e)&&!first_bank.sync_routes(wrong,1,e)&&mutations==before_reject&&queues[4]==retained_first&&queues[5]==retained_second,"import/PCM disagreement rejects before any NDSP call");
  }
  upstream::MusicPlayers other_graph;upstream::MusicBankRoutes other_routes;
  check(other_graph.initialize({data.silence_db(),0,data.fade_to_seconds(),16},e)&&other_graph.play_loop(0,source1,e)&&other_routes.initialize({8,16,bank.master_db(),bank.silence_db()},e)&&other_routes.add_provider(1,data,bank,e)&&other_routes.reconcile(other_graph,{{0,1,1}},e),"independent new-game numeric identities");
  check(!first_bank.sync_routes(other_routes,1,e)&&mutations==before_reject,"owner token prevents new-game alias without transport teardown");
  // Delete node0 and create node2 before publication: both playback generations
  // equal 1, so checking only generation would leave node0's old phase playing.
  check(graph.queue_remove(0,e)&&graph.flush_deletions(e),"source deferred removal boundary");
  uint64_t third;check(graph.add_player(third,e)&&third==2&&graph.play_loop(third,source1,e)&&routes.reconcile(graph,{{third,1,1}},e),"new node reuses released slot with same numeric playback generation");
  check(first_bank.check_routes(routes,1,e)&&second_bank.check_routes(routes,2,e)&&first_bank.release_routes(routes,1,e)&&second_bank.release_routes(routes,2,e),"validate all then release obsolete routes");
  check(queues[4].empty()&&queues[5]==retained_second,"release phase clears only obsolete node");
  check(first_bank.sync_routes(routes,1,e)&&second_bank.sync_routes(routes,2,e)&&first_bank.submitted_voices()==2&&second_bank.submitted_voices()==1,"new logical identity starts once");
  check(checksum(queues[4][0]->data_pcm16,4096)==initial_crc&&queues[5]==retained_second,"new node restarts at source frame zero, other provider keeps phase");
  // Transfer one slot from provider1 to provider2 without touching its other node.
  check(graph.play_loop(third,source2,e)&&routes.reconcile(graph,{{third,2,2}},e),"node changes prepared bank");
  check(first_bank.release_routes(routes,1,e)&&second_bank.release_routes(routes,2,e)&&first_bank.sync_routes(routes,1,e)&&second_bank.sync_routes(routes,2,e),"ordered cross-provider physical handoff");
  check(queues[5]==retained_second&&second_bank.submitted_voices()==2&&checksum(queues[4][0]->data_pcm16,4096)==second_crc,"transferred node resets source and preserves unrelated playback");
  check(routes.remove_provider(1,e)&&first_bank.sync_routes(routes,1,e),"bound removed provider may drain its empty transport");first_bank.shutdown();
  auto current_queues=queues;check(second_bank.set_paused(true,e)&&second_bank.set_user_music_db(-12)&&second_bank.sync_routes(routes,2,e)&&queues==current_queues&&paused[4]&&paused[5],"global routes retain pause, user volume and queues");
  auto paused_calls=mutations;check(second_bank.pump_streams(e)&&mutations==paused_calls,"paused routed pump is silent");check(second_bank.set_paused(false,e),"routed resume");
  flush_result=-1;drain(4);retained_second=queues[5];check(!second_bank.sync_routes(routes,2,e)&&queues[4].empty()&&queues[5]==retained_second,"routed flush failure affects only its node");flush_result=0;check(second_bank.sync_routes(routes,2,e)&&queues[4].size()==3&&queues[5]==retained_second,"explicit routed retry");
  second_bank.shutdown();check(allocated==0,"global provider buffers released");
 }
 check(allocated==0,"region streaming buffers all released");for(size_t i=4;i<queues.size();++i)check(queues[i].empty(),"region shutdown clear queues before buffers");check(mutations>0,"double exercised adapter");std::cout<<"Music region NDSP-double PASS "<<checks<<" checks; no real audio output claim\n";
}
