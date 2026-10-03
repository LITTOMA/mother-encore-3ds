#include "encore/localization.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
constexpr size_t maximum_bytes=16*1024*1024;
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
void put(std::vector<uint8_t>&v,uint32_t n){for(unsigned i=0;i<4;++i)v.push_back(uint8_t(n>>(8*i)));}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return~c;}
bool fail(std::string&e,const char*s){e=s;return false;}
bool utf8(std::string_view s){size_t i=0;uint32_t cp=0;while(i<s.size())if(!utf8_next(s,i,cp)||!cp||(cp<32&&cp!=9&&cp!=10&&cp!=13))return false;return true;}
bool code(std::string_view s){if(s.empty()||s.size()>31)return false;for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'))return false;return true;}
struct Reader {const uint8_t*p;size_t n;bool ok=true;uint32_t integer(){if(n<4){ok=false;return 0;}uint32_t v=u32(p);p+=4;n-=4;return v;}uint32_t count(uint32_t max){const auto v=integer();if(v>max){ok=false;return 0;}return v;}std::string_view text(){uint32_t len=count(1024*1024);if(len>n){ok=false;return{};}std::string_view s(reinterpret_cast<const char*>(p),len);p+=len;n-=len;if(!utf8(s))ok=false;return s;}};
}
int LocaleCatalog::locale_index(std::string_view requested)const{
 // Alias names are source CSV identities; canonical codes are from global.gd.
 for(size_t i=0;i<locales_.size();++i)if(requested==locales_[i].code||requested==locales_[i].csv_code)return int(i);
 return -1;
}
Translation LocaleCatalog::lookup(std::string_view key,std::string_view requested)const{
 Translation result{key,key,{},{},TranslationStatus::MissingKey,0};
 auto it=std::upper_bound(records_.begin(),records_.end(),key,[](std::string_view k,const Record&r){return k<r.key;});
 if(it==records_.begin()||(--it)->key!=key)return result;
 result.source=it->source;result.line=it->line;
 int language=locale_index(requested);
 // Source Godot TranslationServer allows region -> language resolution.
 if(language<0){const auto at=requested.find('_');if(at!=std::string_view::npos)language=locale_index(requested.substr(0,at));}
 if(language>=0&&!it->values[size_t(language)].empty()){result.text=it->values[size_t(language)];result.locale=locales_[size_t(language)].code;result.status=TranslationStatus::Exact;return result;}
 language=locale_index(fallback_);
 if(language>=0&&!it->values[size_t(language)].empty()){result.text=it->values[size_t(language)];result.locale=locales_[size_t(language)].code;result.status=TranslationStatus::LanguageFallback;}
 return result;
}
const TextBinding*LocaleCatalog::binding(std::string_view id)const{auto it=std::lower_bound(bindings_.begin(),bindings_.end(),id,[](const TextBinding&a,std::string_view b){return a.identity<b;});return it!=bindings_.end()&&it->identity==id?&*it:nullptr;}
bool LocaleCatalog::bound(std::string_view id,std::string_view expected,std::string_view locale,std::string&out,std::string&e)const{
 const auto*b=binding(id);if(!b||b->expected!=expected)return fail(e,"Localized source binding does not match the loaded content");out=std::string(lookup(b->key,locale).text);e.clear();return true;
}
bool LocaleCatalog::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<24||n>maximum_bytes||std::memcmp(p,"ENCL10N1",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)||u32(p+16)!=crc(p+24,n-24))return fail(e,"Locale catalog version/size/CRC rejected");
 LocaleCatalog d;d.bytes_.assign(p,p+n);Reader r{d.bytes_.data()+24,n-24};d.fallback_=r.text();auto count=r.count(64);std::set<std::string_view>codes;
 for(uint32_t i=0;i<count;++i){LocaleInfo l{r.text(),r.text(),r.text(),r.text(),false,false,{}};auto enabled=r.integer();l.source_enabled=enabled!=0;const auto ready=r.integer();l.native_ready=ready!=0;l.native_blocker=r.text();if(!code(l.code)||!code(l.csv_code)||l.name.empty()||l.font.empty()||enabled>1||ready>1||!codes.insert(l.code).second)return fail(e,"Locale identity rejected");d.locales_.push_back(l);}
 if(d.locales_.empty()||d.locale_index(d.fallback_)<0)return fail(e,"Catalog fallback locale unavailable");
 count=r.count(100000);std::string_view previous;
 for(uint32_t i=0;i<count;++i){Record q;q.key=r.text();q.source=r.text();q.line=r.integer();if(q.key.empty()||q.source.empty()||!q.line||q.key<previous)return fail(e,"Catalog source order rejected");previous=q.key;for(size_t j=0;j<d.locales_.size();++j)q.values.push_back(r.text());d.records_.push_back(std::move(q));}
 count=r.count(100000);previous={};
 for(uint32_t i=0;i<count;++i){TextBinding b{r.text(),r.text(),r.text(),r.text()};if(b.identity.empty()||b.identity<=previous||b.key.empty()||b.origin.empty())return fail(e,"Catalog binding order rejected");previous=b.identity;d.bindings_.push_back(b);}
 if(!r.ok||r.n||d.records_.empty())return fail(e,"Malformed UTF-8 locale catalog");
 for(const auto&b:d.bindings_)if(d.lookup(b.key,d.fallback_).text!=b.expected)return fail(e,"Catalog expected source text rejected");
 *this=std::move(d);e.clear();return true;
}
bool LocaleCatalog::load_file(const char*path,std::string&e){if(!path)return fail(e,"No locale catalog path");FILE*f=std::fopen(path,"rb");if(!f)return fail(e,"Cannot open locale catalog");std::vector<uint8_t>b;uint8_t chunk[4096];bool ok=true;for(;;){const auto n=std::fread(chunk,1,sizeof chunk,f);if(b.size()+n>maximum_bytes){ok=false;break;}b.insert(b.end(),chunk,chunk+n);if(n<sizeof chunk){ok=!std::ferror(f);break;}}if(std::fclose(f))ok=false;return ok?load(b.data(),b.size(),e):fail(e,"Locale catalog bounded read failed");}
bool LocaleSelection::bind(const LocaleCatalog&c,std::string&e){if(!c.valid())return fail(e,"Locale catalog is invalid");catalog_=&c;code_=std::string(c.fallback());++revision_;e.clear();return true;}
bool LocaleSelection::select(std::string_view requested,std::string&e){if(!catalog_)return fail(e,"Locale selection has no catalog");int i=catalog_->locale_index(requested);if(i<0||!catalog_->locales()[size_t(i)].source_enabled)return fail(e,"Locale is not an enabled source language");if(!catalog_->locales()[size_t(i)].native_ready){e=std::string(catalog_->locales()[size_t(i)].native_blocker);return false;}std::string next(catalog_->locales()[size_t(i)].code);if(next!=code_){code_=std::move(next);++revision_;}e.clear();return true;}
bool encode_locale_preference(const LocaleSelection&s,std::vector<uint8_t>&out,std::string&e){if(!s.catalog()||!code(s.code()))return fail(e,"Locale preference is unbound");std::vector<uint8_t>b={'E','N','C','L','P','R','E','F'};put(b,1);put(b,uint32_t(s.code().size()));b.insert(b.end(),s.code().begin(),s.code().end());put(b,crc(b.data(),b.size()));out=std::move(b);e.clear();return true;}
bool decode_locale_preference(const LocaleCatalog&catalog,const uint8_t*p,size_t n,std::string&result,std::string&e){if(!p||n<21||n>51||std::memcmp(p,"ENCLPREF",8)||u32(p+8)!=1||u32(p+12)!=n-20||u32(p+n-4)!=crc(p,n-4))return fail(e,"Locale preference is corrupt or unsupported");std::string value(reinterpret_cast<const char*>(p+16),n-20);const int i=catalog.locale_index(value);if(i<0||!catalog.locales()[size_t(i)].source_enabled||!catalog.locales()[size_t(i)].native_ready)return fail(e,"Stored locale is unavailable");result=std::string(catalog.locales()[size_t(i)].code);e.clear();return true;}
}
