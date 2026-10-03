#include "encore/region_background_kernel.hpp"
#include "encore/row_linear_background_kernel.hpp"
#include "encore/encounter_residency.hpp"
#include "encore/raw_t3x.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace encore;const uint32_t w=512,h=512;std::vector<uint8_t>a(w*h),b(w*h);std::vector<uint32_t>palette(8);
 for(size_t i=0;i<palette.size();++i)palette[i]=0xff000000u+uint32_t(i)*0x182430u;
 for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){a[y*w+x]=uint8_t((x+y)%8);b[y*w+x]=uint8_t((3*x+y)%8);}
 std::vector<BackgroundKernel::Layer> layers(2);for(auto&l:layers){l.repeat=true;l.width=w;l.height=h;l.compression_amplitude_y=.1f;l.compression_frequency_y=1;l.source={w,h,nullptr,palette.data(),8};}
 layers[0].source.pixels=a.data();layers[1].source.pixels=b.data();layers[0].opacity=1;layers[1].opacity=.5f;layers[0].amplitude_x=.1f;layers[0].frequency_x=1;
 const size_t bound=RegionBackgroundKernel::preparation_upper_bound(layers,400,240,true);const size_t tables=2*size_t(513)*513*8*4;assert(bound>tables+2*400*240*16);
 EncounterResidency admission;std::string error;assert(admission.declare({1,{{"stress","a.encbattle","a.encround"}}},{20*1024*1024,2*1024*1024},{},error));assert(!admission.reserve(0,{bound,512*1024},error));
 RegionBackgroundKernel actual;assert(actual.prepare(layers,400,240,error));assert(actual.allocated_bytes()<=bound);
 assert(admission.declare({2,{{"one","a.encbattle","a.encround"}}},{1000,1000},{200,200},error));assert(admission.reserve(0,{100,100},error));assert(admission.revise({2,0},{700,700},error));assert(!admission.revise({1,0},{800,800},error));assert(!admission.revise({2,0},{801,801},error));assert(admission.retire_other_live({200,200}));assert(!admission.retire_other_live({1,0}));assert(admission.revise({2,0},{900,900},error));assert(admission.publish({2,0},{850,850},error));
 std::vector<uint8_t> header(21);auto put16=[&](size_t i,uint32_t n){header[i]=uint8_t(n);header[i+1]=uint8_t(n>>8);};put16(0,1);header[2]=6|(6<<3);put16(5,64);put16(7,64);put16(11,1024);put16(13,128);put16(15,896);const size_t backing=512*512*4;header[18]=uint8_t(backing);header[19]=uint8_t(backing>>8);header[20]=uint8_t(backing>>16);RawT3x raw;assert(inspect_raw_t3x(header.data(),header.size(),backing+21,raw,error));assert(raw.size==backing&&raw.size>64*64*4);
 std::cout<<"PASS fresh-kernel bound "<<bound<<" >= actual "<<actual.allocated_bytes()<<"; distinct large homogeneity tables rejected before allocation; padded backing, reservation revision, stale epoch, old-owner retirement\n";
}
