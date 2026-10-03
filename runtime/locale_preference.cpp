#include "encore/localization.hpp"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
namespace encore::upstream {namespace {
bool path_ok(const char*path){if(!path||!*path)return false;const std::string s(path);return s.size()<1000&&s.back()!='/'&&s.find("..") == std::string::npos;}
bool read_bytes(const std::string&path,std::vector<uint8_t>&out,bool&missing,std::string&e){missing=false;errno=0;FILE*f=std::fopen(path.c_str(),"rb");if(!f){missing=errno==ENOENT;if(!missing)e="Cannot read locale preference";return missing;}uint8_t bytes[52];const size_t n=std::fread(bytes,1,sizeof bytes,f);bool ok=!std::ferror(f)&&n<sizeof bytes;if(std::fclose(f))ok=false;if(!ok){e="Locale preference read/bounds failed";return false;}out.assign(bytes,bytes+n);return true;}
bool write_new(const std::string&path,const std::vector<uint8_t>&bytes,std::string&e){int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0){e="Cannot exclusively create locale preference temporary file";return false;}FILE*f=::fdopen(fd,"wb");if(!f){::close(fd);std::remove(path.c_str());e="Cannot open locale preference stream";return false;}bool ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size();if(std::fflush(f))ok=false;if(std::fclose(f))ok=false;if(!ok){std::remove(path.c_str());e="Locale preference write/flush failed";}return ok;}
}
LocalePreferenceRead read_locale_preference(const LocaleCatalog&catalog,const char*path,std::string&code,std::string&e){if(!path_ok(path)){e="Invalid locale preference path";return LocalePreferenceRead::Rejected;}std::vector<uint8_t>bytes;bool missing=false;if(!read_bytes(path,bytes,missing,e))return LocalePreferenceRead::Rejected;if(missing){e.clear();return LocalePreferenceRead::Missing;}return decode_locale_preference(catalog,bytes.data(),bytes.size(),code,e)?LocalePreferenceRead::Loaded:LocalePreferenceRead::Rejected;}
bool write_locale_preference(const LocaleSelection&selection,const char*path,std::string&e){
 if(!path_ok(path)){e="Invalid locale preference path";return false;}
 std::vector<uint8_t>next,prior;bool missing=false;std::string prior_code;
 if(!encode_locale_preference(selection,next,e)||!read_bytes(path,prior,missing,e))return false;
 if(!missing&&!decode_locale_preference(*selection.catalog(),prior.data(),prior.size(),prior_code,e))return false;
 const std::string primary(path),temporary=primary+".tmp",backup=primary+".bak";
 if(!write_new(temporary,next,e))return false;
 std::vector<uint8_t>check;bool absent=false;if(!read_bytes(temporary,check,absent,e)||absent||check!=next){std::remove(temporary.c_str());e="Locale preference read-back mismatch";return false;}
 if(!missing){if(std::remove(backup.c_str())&&errno!=ENOENT){std::remove(temporary.c_str());e="Cannot rotate locale backup";return false;}if(std::rename(primary.c_str(),backup.c_str())){std::remove(temporary.c_str());e="Cannot preserve previous locale preference";return false;}}
 if(std::rename(temporary.c_str(),primary.c_str())){
  e="Cannot commit locale preference";std::remove(temporary.c_str());
  if(!missing){std::vector<uint8_t>current;bool gone=false;std::string detail;if(read_bytes(primary,current,gone,detail)&&gone&&write_new(primary,prior,detail))e+="; previous preference restored";else e+="; previous preference retained in .bak";}
  return false;
 }
 if(!read_bytes(primary,check,absent,e)||absent||check!=next){e="Committed locale preference failed read-back; prior retained in .bak";return false;}
 e.clear();return true;
}
}
