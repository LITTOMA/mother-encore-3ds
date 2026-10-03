#pragma once
// Deliberate host-only NDSP API double. Never link into console builds.
#include <cstdint>
#include <cstddef>
using Result=int32_t;
#define R_FAILED(r) ((r)<0)
struct ndspWaveBuf { int16_t* data_pcm16=nullptr;uint32_t nsamples=0;bool looping=false;uint8_t status=0; };
enum {NDSP_WBUF_FREE=0,NDSP_WBUF_QUEUED=1,NDSP_WBUF_PLAYING=2,NDSP_WBUF_DONE=3};
enum {NDSP_INTERP_POLYPHASE=0,NDSP_FORMAT_MONO_PCM16=5,NDSP_FORMAT_STEREO_PCM16=6};
Result ndspInit();void ndspExit();void ndspSetMasterVol(float);void ndspChnReset(int);
void* linearAlloc(size_t);void linearFree(void*);uint32_t ndspGetDroppedFrames();
void ndspChnWaveBufClear(int);void ndspChnSetMix(int,float*);Result DSP_FlushDataCache(const void*,uint32_t);
void ndspChnWaveBufAdd(int,ndspWaveBuf*);void ndspChnSetInterp(int,int);void ndspChnSetRate(int,float);void ndspChnSetFormat(int,int);
