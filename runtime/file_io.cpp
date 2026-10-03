#include "encore/content.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>
namespace encore {
bool read_file(const char* path,std::vector<uint8_t>& out,size_t limit,std::string& error) {
    FILE* f=std::fopen(path,"rb");if(!f){error=std::string("Cannot open: ")+path;return false;}
    if(std::fseek(f,0,SEEK_END)!=0) {std::fclose(f);error="Cannot seek file";return false;}
    long size=std::ftell(f);
    if(size<0||static_cast<size_t>(size)>limit||std::fseek(f,0,SEEK_SET)!=0) {std::fclose(f);error="File exceeds size limit";return false;}
    std::vector<uint8_t> t(static_cast<size_t>(size));
    bool ok=std::fread(t.data(),1,t.size(),f)==t.size();if(std::fclose(f)!=0)ok=false;
    if(!ok){error="File read failed";return false;}out=std::move(t);error.clear();return true;
}
bool write_file_atomic(const char* path,const std::vector<uint8_t>& data,std::string& error) {
    const std::string tmp=std::string(path)+".tmp",bak=std::string(path)+".bak";
    FILE* f=std::fopen(tmp.c_str(),"wb");if(!f){error="Cannot create temporary save; check directory/SD";return false;}
    bool ok=std::fwrite(data.data(),1,data.size(),f)==data.size();
    if(std::fflush(f)!=0)ok=false;
    if(std::fclose(f)!=0)ok=false;
    if(!ok){std::remove(tmp.c_str());error="Save write/flush failed";return false;}
    bool had_old=false;errno=0;FILE* old=std::fopen(path,"rb");
    if(!old&&errno!=ENOENT){std::remove(tmp.c_str());error="Cannot inspect existing save; refusing to overwrite it";return false;}
    if(old){std::fclose(old);had_old=true;}
    if(had_old){std::remove(bak.c_str());if(std::rename(path,bak.c_str())!=0){std::remove(tmp.c_str());error="Cannot back up old save";return false;}}
    if(std::rename(tmp.c_str(),path)!=0) {
        if(had_old)std::rename(bak.c_str(),path);
        error="Cannot commit new save (backup retained when possible)";return false;
    }
    // File close + backup + rename. No claim of power-loss durability on FAT/3DS.
    error.clear();return true;
}
}
