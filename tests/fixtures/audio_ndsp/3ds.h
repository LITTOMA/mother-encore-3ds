#pragma once
// Deliberate host-only NDSP API double. Never link into console builds.
#include <cstdint>
#include <cstddef>
#include <array>
#include <cstdlib>
using Result=int32_t;
#define R_FAILED(r) ((r)<0)
struct ndspWaveBuf { int16_t* data_pcm16=nullptr;uint32_t nsamples=0;bool looping=false;uint8_t status=0;uint16_t sequence_id=0; };
enum {NDSP_WBUF_FREE=0,NDSP_WBUF_QUEUED=1,NDSP_WBUF_PLAYING=2,NDSP_WBUF_DONE=3};
enum {NDSP_INTERP_POLYPHASE=0,NDSP_FORMAT_MONO_PCM16=5,NDSP_FORMAT_STEREO_PCM16=6};
Result ndspInit();void ndspExit();void ndspSetMasterVol(float);void ndspChnReset(int);
void* linearAlloc(size_t);void linearFree(void*);uint32_t ndspGetDroppedFrames();
void ndspChnSetPaused(int,bool);
void ndspChnWaveBufClear(int);void ndspChnSetMix(int,float*);Result DSP_FlushDataCache(const void*,uint32_t);
void ndspChnWaveBufAdd(int,ndspWaveBuf*);void ndspChnSetInterp(int,int);void ndspChnSetRate(int,float);void ndspChnSetFormat(int,int);

// Model the documented libctru sequence assignment. Unused position APIs in
// older test doubles explicitly abort rather than returning invented positions.
inline std::array<uint16_t,24>ndsp_test_next_sequence{};
inline void ndsp_test_reset_sequence(int channel){if(channel<0||channel>=24)std::abort();ndsp_test_next_sequence[size_t(channel)]=0;}
inline void ndsp_test_assign_sequence(int channel,ndspWaveBuf*wave){if(channel<0||channel>=24||!wave)std::abort();auto&next=ndsp_test_next_sequence[size_t(channel)];if(!next)next=1;wave->sequence_id=next++;}
uint32_t ndspChnGetSamplePos(int);uint16_t ndspChnGetWaveBufSeq(int);
