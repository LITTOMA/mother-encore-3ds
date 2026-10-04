#include "audio_player.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace encore;
namespace {
unsigned checks=0;Result init_result=0,flush_result=0;bool ndsp_started=false;int exits=0;size_t allocated=0;bool fail_alloc=false;
constexpr size_t channel_count=24;
std::array<std::vector<ndspWaveBuf*>,channel_count> queues;std::array<float,channel_count> gains{},rates{};float master=0;
void check(bool ok,const char* why){++checks;if(!ok){std::cerr<<"FAIL: "<<why<<"\n";std::exit(1);}}
size_t channel(int lane){check(lane>=0&&size_t(lane)<queues.size(),"NDSP double channel in bounds");return size_t(lane);}
void drain(int lane){auto& queue=queues[channel(lane)];for(auto* b:queue)b->status=NDSP_WBUF_DONE;queue.clear();}
uint32_t room_resource(const upstream::RoomView& room,uint32_t id){
    for(uint32_t i=0;i<room.resource_count();++i)if(room.resource(i).stable_id==id)return i;
    check(false,"audio test resource exists in room");return upstream::kRoomNoIndex;
}
}
Result ndspInit(){if(init_result>=0)ndsp_started=true;return init_result;}
void ndspExit(){for(size_t i=0;i<queues.size();++i)drain(int(i));ndsp_started=false;++exits;}
void ndspSetMasterVol(float v){master=v;}
void ndspChnReset(int lane){drain(lane);}
void* linearAlloc(size_t n){if(fail_alloc)return nullptr;allocated+=n;return std::malloc(n);}
void linearFree(void* p){check(!ndsp_started,"buffer freed only after NDSP exit");std::free(p);}
uint32_t ndspGetDroppedFrames(){return 7;}
void ndspChnWaveBufClear(int lane){drain(lane);}
void ndspChnSetMix(int lane,float* mix){gains[channel(lane)]=mix[0];check(mix[0]==mix[1],"stereo equal gain");}
Result DSP_FlushDataCache(const void* p,uint32_t size){check(p&&size>0&&size<=2048*4,"bounded flush");return flush_result;}
void ndspChnWaveBufAdd(int lane,ndspWaveBuf* wave){auto& queue=queues[channel(lane)];check(ndsp_started&&wave->data_pcm16&&wave->nsamples>0&&wave->nsamples<=2048&&!wave->looping,"valid frame queue");wave->status=NDSP_WBUF_QUEUED;queue.push_back(wave);}
void ndspChnSetInterp(int lane,int){channel(lane);}void ndspChnSetRate(int lane,float rate){rates[channel(lane)]=rate;}void ndspChnSetFormat(int lane,int){channel(lane);}
int main(int argc,char** argv){
    check(argc==4,"provide audio bank, asset root, room pack");ctr::AudioPlayer player;std::string error;
    check(!player.initialize(argv[1],nullptr,error)&&error=="Audio asset root is missing","missing root rejected");
    const auto missing_root=std::string(argv[1])+"/not-a-directory/";
    init_result=-42;check(!player.initialize(argv[1],missing_root.c_str(),error)&&!player.available(),"NDSP failure explicit before inaccessible PCM assets");check(error.find("NDSP init")!=std::string::npos&&player.dsp_result()==-42,"NDSP failure retained");check(player.submitted_voices()==0&&allocated==0&&exits==0,"unavailable never scans PCM, queues or claims audio");check(!player.play(21,ctr::AudioLane::Music,error),"unavailable playback rejected");
    player.reset_scene();check(!player.available()&&player.dsp_result()==-42&&exits==0&&allocated==0,"unavailable scene reset preserves DSP failure without retry");
    init_result=0;check(player.initialize(argv[1],missing_root.c_str(),error)&&player.available()&&!player.prepared(21),"metadata initialization does not scan unused PCM");
    check(!player.prepare(21,error)&&error=="Cannot open PCM asset"&&!player.prepared(21),"missing PCM rejected at first preparation");
    check(!player.play(21,ctr::AudioLane::Music,error)&&player.submitted_voices()==0&&queues[0].empty(),"unvalidated PCM never reaches NDSP");
    player.shutdown();check(exits==1&&allocated==0&&!ndsp_started,"deferred preparation failure can be cleaned up");
    fail_alloc=true;check(!player.initialize(argv[1],argv[2],error)&&!player.available()&&exits==2,"allocation failure shuts down NDSP");fail_alloc=false;
    check(player.initialize(argv[1],argv[2],error),error.c_str());check(allocated==4*3*2048*2*2,"96KiB four-lane streaming allocation");check(std::abs(master-upstream::audio_linear_gain(-5.93075f))<0.00001,"master gain loaded from bank");
    check(!player.prepare(12345,error)&&!player.prepared(12345),"unknown preparation identity rejected");
    check(!player.prepared(21)&&player.prepare(21,error)&&player.prepared(21),"full PCM admission is deferred until requested");
    check(player.prepare(21,error)&&player.submitted_voices()==0&&queues[0].empty(),"cached preparation has no playback side effects");
    check(!player.play(12345,ctr::AudioLane::Music,error),"unknown asset rejected");check(!player.play(21,static_cast<ctr::AudioLane>(6),error)&&!player.play(21,static_cast<ctr::AudioLane>(255),error),"unknown lanes rejected");
    upstream::RoomData room;check(room.load_file(argv[3],error),error.c_str());
    std::vector<upstream::OpeningAudioRequest> requests={{upstream::AudioRequestKind::FadeMusic,upstream::kRoomNoIndex,2,0},{upstream::AudioRequestKind::PlayMusic,20,0,2}};
    check(player.consume(room.view(),requests,error),error.c_str());check(player.consumed_requests()==2&&player.submitted_voices()==1&&queues[0].size()==3,"typed requests consumed and music queued");
    check(player.consume(room.view(),requests,error)&&player.submitted_voices()==1,"same request history not replayed");
    check(!player.play(21,ctr::AudioLane::Jingle,error),"concurrent same asset unsupported fails closed");
    for(int phrase:{5,7,9}){requests.push_back({upstream::AudioRequestKind::PlayEffect,21,0,uint32_t(phrase)});check(player.consume(room.view(),requests,error),"dialogue SFX retrigger");check(queues[1].size()==3,"retrigger replaces old voice");}
    check(player.play(1001,ctr::AudioLane::Jingle,error),"battle entry jingle queued");check(queues[0].size()==3&&queues[2].size()==3,"jingle preserves existing overworld music");
    for(int n=0;n<50;++n){drain(1);drain(2);check(player.update(0,error),"effect and jingle stream drain");}
    check(player.completed_voices()==2&&queues[1].empty()&&queues[2].empty()&&!queues[0].empty(),"oneshots complete while music remains");
    check(player.fade_music(2,error)&&player.update(1,error),"quartic fade first half");check(std::abs(gains[0]-upstream::audio_linear_gain(-5))<0.00001,"fade matches source quartic dB");check(player.update(1,error)&&queues[0].empty(),"fade completes and stops");
    const auto house_resource=room_resource(room.view(),34),melody_resource=room_resource(room.view(),32);
    requests.push_back({upstream::AudioRequestKind::PlayMusic,house_resource,0,0});
    check(player.consume(room.view(),requests,error)&&player.play(22,ctr::AudioLane::Effect,error)&&player.play(1001,ctr::AudioLane::Jingle,error),"area music, effect and jingle active before dialogue music");
    const auto existing_queues=queues;
    requests.push_back({upstream::AudioRequestKind::PlayDialogueMusic,melody_resource,0,0});
    check(player.consume(room.view(),requests,error),"fourth-lane dialogue music request accepted");
    check(queues[3].size()==3&&queues[0]==existing_queues[0]&&queues[1]==existing_queues[1]&&queues[2]==existing_queues[2],"dialogue music preserves all three concurrent voices");
    const auto concurrent_queues=queues;const auto submissions=player.submitted_voices();
    check(!player.play(32,ctr::AudioLane::Music,error)&&queues==concurrent_queues,"fourth-lane same-asset collision leaves all voices intact");
    check(player.consume(room.view(),requests,error)&&player.submitted_voices()==submissions,"dialogue music history is not replayed");
    requests.push_back({upstream::AudioRequestKind::StopMusicResource,room_resource(room.view(),21),0,0});
    check(player.consume(room.view(),requests,error)&&queues==concurrent_queues,"unattached resource stop leaves all voices intact");
    requests.push_back({upstream::AudioRequestKind::StopMusicResource,house_resource,0,0});
    check(player.consume(room.view(),requests,error)&&player.update(0,error),"targeted area music stop consumed");
    check(queues[0].empty()&&queues[1]==concurrent_queues[1]&&queues[2]==concurrent_queues[2]&&queues[3]==concurrent_queues[3],"area resource stop preserves dialogue melody, effect and jingle");
    check(player.play(34,ctr::AudioLane::Music,error),"area music restarts independently");const auto restarted_music=queues[0];
    requests.push_back({upstream::AudioRequestKind::StopMusicResource,melody_resource,0,0});
    check(player.consume(room.view(),requests,error)&&player.update(0,error),"targeted dialogue music stop consumed");
    check(queues[3].empty()&&queues[0]==restarted_music&&queues[1]==concurrent_queues[1]&&queues[2]==concurrent_queues[2],"dialogue resource stop preserves area music, effect and jingle");
    requests.push_back({upstream::AudioRequestKind::StopMusicResource,room_resource(room.view(),22),0,0});
    check(player.consume(room.view(),requests,error)&&queues[1]==concurrent_queues[1],"targeted music stop cannot stop matching effect asset");
    check(player.play(32,ctr::AudioLane::DialogueMusic,error),"dialogue music restarts after targeted stop");
    check(!player.update(-1,error)&&!player.update(INFINITY,error)&&!player.fade_music(-1,error),"invalid frame time rejected");
    requests.push_back({upstream::AudioRequestKind::PlayEffect,9999,0,99});check(!player.consume(room.view(),requests,error),"invalid room resource rejected");requests.pop_back();
    requests.push_back({upstream::AudioRequestKind::PlayEffect,21,1,99});check(!player.consume(room.view(),requests,error),"unsupported playback duration rejected");requests.pop_back();
    requests.push_back({static_cast<upstream::AudioRequestKind>(99),21,0,99});check(!player.consume(room.view(),requests,error),"unknown request kind rejected");requests.pop_back();
    requests.push_back({upstream::AudioRequestKind::FadeMusic,21,1,99});check(!player.consume(room.view(),requests,error),"targeted fade outside schema rejected");requests.pop_back();
    const auto valid_queues=queues;
    requests.push_back({upstream::AudioRequestKind::PlayDialogueMusic,melody_resource,1,99});check(!player.consume(room.view(),requests,error)&&queues==valid_queues,"timed dialogue music rejected without disturbing voices");requests.pop_back();
    requests.push_back({upstream::AudioRequestKind::StopMusicResource,house_resource,1,99});check(!player.consume(room.view(),requests,error)&&queues==valid_queues,"timed resource stop rejected without disturbing voices");requests.pop_back();
    requests.push_back({upstream::AudioRequestKind::StopMusicResource,9999,0,99});check(!player.consume(room.view(),requests,error)&&queues==valid_queues,"invalid stop resource rejected without disturbing voices");requests.pop_back();
    flush_result=-1;check(!player.play(22,ctr::AudioLane::Effect,error)&&queues[1].empty(),"DSP cache failure aborts playback");flush_result=0;
    check(player.play(34,ctr::AudioLane::Music,error)&&player.play(22,ctr::AudioLane::Effect,error)&&player.play(1001,ctr::AudioLane::Jingle,error)&&player.play(32,ctr::AudioLane::DialogueMusic,error),"all lanes active before scene reset");
    check(player.fade_music(2,error)&&player.update(.25,error),"scene reset includes an active fade");
    std::array<int16_t*,channel_count> prior_buffers{};for(size_t lane=0;lane<4;++lane)prior_buffers[lane]=queues[lane].front()->data_pcm16;
    const auto allocations_before_reset=allocated;const auto exits_before_reset=exits;
    player.reset_scene();player.reset_scene();
    check(player.available()&&ndsp_started&&allocated==allocations_before_reset&&exits==exits_before_reset,"repeated scene reset retains DSP and streaming allocations");
    check(player.consumed_requests()==0&&player.submitted_voices()==0&&player.completed_voices()==0&&player.queued_frames()==0,"scene request and voice counters restart together");
    for(const auto&queue:queues)check(queue.empty(),"scene reset drains all lanes");
    check(player.update(1,error),"reset voices and fades do not resume");
    for(const auto&queue:queues)check(queue.empty(),"no stale voice requeued after reset");
    const std::vector<upstream::OpeningAudioRequest> fresh_requests={{upstream::AudioRequestKind::PlayMusic,house_resource,0,0},{upstream::AudioRequestKind::PlayEffect,room_resource(room.view(),22),0,0},{upstream::AudioRequestKind::PlayDialogueMusic,melody_resource,0,0}};
    check(player.consume(room.view(),fresh_requests,error)&&player.play(1001,ctr::AudioLane::Jingle,error),"fresh scene can replay checked PCM assets on every lane");
    check(player.consumed_requests()==3&&player.submitted_voices()==4,"fresh request history consumed from its beginning");
    for(size_t lane=0;lane<4;++lane)check(!queues[lane].empty()&&queues[lane].front()->data_pcm16==prior_buffers[lane],"same streaming buffer reused after reset");
    upstream::AudioBank checked_bank;upstream::AudioAsset house_music;check(checked_bank.load_file(argv[1],error)&&checked_bank.find(34,house_music),"checked music gain available");
    check(std::abs(gains[0]-upstream::audio_linear_gain(house_music.gain_db))<.00001f,"fresh music has no stale fade gain");
    const auto before_aux=queues;const auto before_aux_bytes=allocated;
    for(float pitch:{0.f,-1.f,5.f,NAN,INFINITY})check(!player.play(1101,ctr::AudioLane::AuxiliaryEffect0,error,0,0,pitch)&&queues==before_aux&&allocated==before_aux_bytes,"invalid pitch preserves all voices and allocations");
    fail_alloc=true;check(!player.play(1101,ctr::AudioLane::AuxiliaryEffect0,error)&&queues==before_aux&&allocated==before_aux_bytes,"auxiliary allocation failure preserves live voices");fail_alloc=false;
    check(player.play(1101,ctr::AudioLane::AuxiliaryEffect0,error,0,0,.85f)&&player.play(1102,ctr::AudioLane::AuxiliaryEffect1,error,0,0,1.f),"independent auxiliary effects can overlap text and music");
    upstream::AudioAsset pitched;check(checked_bank.find(1101,pitched)&&std::abs(rates[22]-float(pitched.sample_rate)*.85f)<.01f,"checked pitch reaches NDSP sample rate");
    check(allocated==before_aux_bytes+2*3*2048*2*2&&!queues[22].empty()&&!queues[23].empty(),"auxiliary buffers allocated only on demand on reserved channels");
    for(size_t i=4;i<20;++i)check(queues[i].empty(),"area music channel reservation remains untouched");
    const auto with_aux=queues;check(!player.stop_lane(static_cast<ctr::AudioLane>(255),error)&&queues==with_aux,"unknown stop target fails without mutation");
    check(player.stop_lane(ctr::AudioLane::AuxiliaryEffect0,error)&&queues[22].empty()&&queues[23]==with_aux[23]&&queues[1]==with_aux[1],"targeted effect stop preserves thunder and text");
    check(!player.fade_all_music(NAN,error)&&!player.fade_all_music(-1,error)&&!player.fade_all_music(61,error),"invalid global music fades rejected");
    check(player.fade_all_music(0,error)&&queues[0].empty()&&queues[3].empty()&&queues[23]==with_aux[23]&&queues[1]==with_aux[1]&&queues[2]==with_aux[2],"global music fade affects both music voices and preserves every effect");
    check(player.dropped_frames()==7,"platform drop count reported");player.shutdown();check(!player.available()&&player.dropped_frames()==0,"shutdown explicit");
    player.reset_scene();check(!player.available()&&!ndsp_started,"scene reset after shutdown stays unavailable");
    for(const auto& queue:queues)check(queue.empty(),"shutdown drains all four channels before freeing buffers");
    std::cout<<"Audio adapter: "<<checks<<" host-double checks passed; this is NOT NDSP audibility verification\n";
}
