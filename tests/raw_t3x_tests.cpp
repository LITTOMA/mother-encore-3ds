#include "encore/raw_t3x.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
#include <vector>
int main(int argc,char**argv){size_t total=0;for(int i=1;i<argc;++i){std::ifstream f(argv[i],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});encore::RawT3x layout;std::string error;assert(encore::parse_raw_t3x(bytes.data(),bytes.size(),layout,error));assert(layout.size+21==bytes.size());total+=layout.size;
 for(size_t n: {size_t(0),size_t(5),size_t(20),bytes.size()-1})assert(!encore::parse_raw_t3x(bytes.data(),n,layout,error));
 for(auto patch:std::vector<std::pair<size_t,uint8_t>>{{0,0},{2,0xc0},{3,1},{4,1},{5,0},{11,0},{17,0x10},{20,0xff}}){auto bad=bytes;bad[patch.first]=patch.second;if(patch.first==5)bad[6]=0;if(patch.first==11)bad[12]=0;assert(!encore::parse_raw_t3x(bad.data(),bad.size(),layout,error));}
 }std::cout<<"PASS raw T3X exact metadata/payload bounds and rejection cases: "<<argc-1<<" assets, "<<total<<" pixel bytes\n";}
