#include "encore/field_area_contacts.hpp"
namespace encore::upstream {namespace {
bool fail(std::string&e,const char*why){e=why;return false;}
bool bits(const uint8_t*p,size_t n){if(!p)return false;for(size_t i=0;i<n;++i)if(p[i]>1)return false;return true;}
}
bool FieldAreaContacts::initialize(const std::vector<uint32_t>&owners,std::string&e){
 if(ready()||owners.empty()||owners.size()>owners_.size())return fail(e,"Field Area contact owner count/rebind rejected");
 for(size_t i=0;i<owners.size();++i){if(!owners[i])return fail(e,"Field Area contact owner identity rejected");for(size_t j=0;j<i;++j)if(owners[j]==owners[i])return fail(e,"Field Area contact duplicate owner rejected");}
 for(size_t i=0;i<owners.size();++i)owners_[i]=owners[i];count_=owners.size();e.clear();return true;
}
bool FieldAreaContacts::begin(const uint8_t*enabled,size_t n,FieldAreaEvents&events,std::string&e){
 if(!ready()||sampling_||n!=count_||!bits(enabled,n))return fail(e,"Field Area physics begin rejected");
 FieldAreaEvents next;
 for(size_t i=0;i<n;++i){const bool entered=pending_[i]&&enabled[i];if(entered!=bool(inside_[i]))next.values[next.count++]={uint32_t(i),owners_[i],entered};}
 for(size_t i=0;i<n;++i)inside_[i]=uint8_t(pending_[i]&&enabled[i]);sampling_=true;events=next;e.clear();return true;
}
bool FieldAreaContacts::end(const uint8_t*enabled,const uint8_t*overlap,size_t n,std::string&e){
 if(!ready()||!sampling_||n!=count_||!bits(enabled,n)||!bits(overlap,n))return fail(e,"Field Area physics sample rejected");
 for(size_t i=0;i<n;++i)pending_[i]=uint8_t(enabled[i]&&overlap[i]);sampling_=false;e.clear();return true;
}
}
