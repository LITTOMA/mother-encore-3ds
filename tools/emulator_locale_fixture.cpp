// An artificial external language setting for a disposable emulator profile.
#include "encore/localization.hpp"
#include <fstream>
#include <cstdio>
using namespace encore::upstream;
int main(int argc,char**argv){
 if(argc!=3)return 1;LocaleCatalog catalog;LocaleSelection selected;std::string error,decoded;std::vector<uint8_t>bytes;
 if(!catalog.load_file("romfs/localization/localization.enclocale",error)||!selected.bind(catalog,error)||!selected.select(argv[1],error)||!encode_locale_preference(selected,bytes,error)||!decode_locale_preference(catalog,bytes.data(),bytes.size(),decoded,error)||decoded!=selected.code()){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
 std::ofstream file(argv[2],std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.flush();if(!file.good())return 1;
 std::printf("Checked artificial language preference: %s; %zu bytes; no gameplay progress\n",decoded.c_str(),bytes.size());
}
