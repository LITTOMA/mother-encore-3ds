#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace encore::upstream {
enum class TranslationStatus {Exact,LanguageFallback,MissingKey};
struct Translation {std::string_view text,key,source,locale;TranslationStatus status=TranslationStatus::MissingKey;uint32_t line=0;};
struct LocaleInfo {std::string_view code,csv_code,name,font;bool source_enabled=false,native_ready=false;std::string_view native_blocker;};
struct TextBinding {std::string_view identity,key,expected,origin;};
// App-owned, immutable catalog. Locale is a presentation preference and never a
// save-domain field. Views borrow this owner; keep it at a stable lifetime.
class LocaleCatalog {
public:
 LocaleCatalog()=default;LocaleCatalog(const LocaleCatalog&)=delete;LocaleCatalog&operator=(const LocaleCatalog&)=delete;
 LocaleCatalog(LocaleCatalog&&)=default;LocaleCatalog&operator=(LocaleCatalog&&)=default;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return !bytes_.empty();}
 const std::vector<LocaleInfo>&locales()const{return locales_;}
 std::string_view fallback()const{return fallback_;}
 int locale_index(std::string_view)const;
 Translation lookup(std::string_view key,std::string_view locale)const;
 const TextBinding*binding(std::string_view identity)const;
 bool bound(std::string_view identity,std::string_view expected,std::string_view locale,std::string&out,std::string&error)const;
 size_t key_count()const{return records_.size();}
private:
 struct Record {std::string_view key,source;uint32_t line=0;std::vector<std::string_view>values;};
 std::vector<uint8_t>bytes_;std::vector<LocaleInfo>locales_;std::vector<Record>records_;std::vector<TextBinding>bindings_;std::string_view fallback_;
};
class LocaleSelection {
public:
 bool bind(const LocaleCatalog&,std::string&error);bool select(std::string_view,std::string&error);
 const LocaleCatalog*catalog()const{return catalog_;}std::string_view code()const{return code_;}uint32_t revision()const{return revision_;}
 Translation text(std::string_view key)const{return catalog_?catalog_->lookup(key,code_):Translation{key,key,{},{},TranslationStatus::MissingKey,0};}
private:const LocaleCatalog*catalog_=nullptr;std::string code_;uint32_t revision_=0;
};
// Separate application-preference encoding; never changes a SessionSnapshot.
// The stored value is a stable locale code, not a mutable array index.
bool encode_locale_preference(const LocaleSelection&,std::vector<uint8_t>&,std::string&);
bool decode_locale_preference(const LocaleCatalog&,const uint8_t*,size_t,std::string&code,std::string&error);
enum class LocalePreferenceRead {Missing,Loaded,Rejected};
LocalePreferenceRead read_locale_preference(const LocaleCatalog&,const char*path,std::string&code,std::string&error);
bool write_locale_preference(const LocaleSelection&,const char*path,std::string&error);

}
