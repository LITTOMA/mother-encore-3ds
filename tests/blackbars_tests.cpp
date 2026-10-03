#include "encore/blackbars.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
using encore::upstream::Blackbars;
int main(int argc,char**argv){
 if(argc<2)return 2;
 std::string error;Blackbars bars;if(!bars.load_file(argv[1],error)){std::cerr<<error;return 1;}
 int failures=0;auto check=[&](bool ok,const char*s){if(!ok){std::cerr<<s<<'\n';++failures;}};
 auto p=bars.pose(400,240);check(p.bars[0].h==0&&p.bars[1].h==0,"Reset is hidden");
 check(bars.update(true,.5),"Open");p=bars.pose(400,240);check(p.bars[0].h==18&&p.bars[1].y==212&&p.bars[1].h==28,"Native source-sized bars");
 p=bars.pose(320,180);check(p.bars[1].y==152&&p.bars[1].h==28,"Reference exact bars");
 check(bars.update(true,0)&&bars.pose(320,180).bars[1].y==152,"Repeated open preserves animation");
 check(bars.update(false,.15),"Close");p=bars.pose(400,240);check(p.bars[0].h==0&&p.bars[1].h<.0001,"Close reaches hidden");
 bars.update(true,.05);bars.update(false,0);check(bars.pose(320,180).bars[1].y==152,"Interrupted close uses authored first key");bars.update(true,0);check(bars.pose(320,180).bars[0].h==0,"Interrupted open uses authored first key");
 check(!bars.update(true,-1)&&!bars.update(true,2)&&!bars.update(true,NAN),"Invalid delta rejected");
 std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t>bytes((std::istreambuf_iterator<char>(in)),{});
 for(size_t n=0;n<bytes.size();++n){Blackbars b;check(!b.load(bytes.data(),n,error),"Truncated pack accepted");}
 auto repair=[](std::vector<uint8_t>&v){uint32_t crc=~0u;for(size_t i=24;i<v.size();++i){crc^=v[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}crc=~crc;for(int i=0;i<4;++i)v[16+i]=uint8_t(crc>>(8*i));};
 for(size_t offset:{size_t(8),size_t(12),size_t(20),size_t(24),size_t(28),size_t(32),size_t(36),size_t(44),size_t(48),size_t(52),size_t(56),size_t(60)}){auto b=bytes;b[offset]=0;b[offset+1]=0;b[offset+2]=0xc0;b[offset+3]=0x7f;repair(b);Blackbars bad;check(!bad.load(b.data(),b.size(),error),"Invalid field accepted");}
 double maximum=0;unsigned samples=0;
 if(argc>2){std::ifstream source(argv[2]);std::string row;while(std::getline(source,row)){std::istringstream s(row);std::string clip;double time,top,bottom;s>>clip>>time>>top>>bottom;bars.reset();if(clip=="Close")bars.update(true,.5);bars.update(clip=="Open",time);const auto pose=bars.pose(320,180);maximum=std::max(maximum,std::abs(double(pose.bars[0].h-18)-top));maximum=std::max(maximum,std::abs(double(pose.bars[1].y)-bottom));++samples;}check(samples==602&&maximum<.0001,"Dense native Godot interpolation mismatch");}
 std::cout<<"Blackbars failures="<<failures<<" native_samples="<<samples<<" max_error="<<maximum<<'\n';return failures?1:0;
}
