#pragma once
#include "encore/items_data.hpp"
#include <functional>

namespace encore::upstream {
enum class ItemDetailsSection:uint32_t {Strings=1,Definitions,Locales,Presentations,Tokens,Resources,Parameters};
enum class ItemDetailsTokenKind:uint32_t {Text=1,Nickname,Doses,InlineImage,Newline,ItemValue};
enum class ItemDetailsParameter:uint32_t {LineHeight=1,ImageBaseline,EnNameLimit,ZhNameLimit};
struct ItemDetailsDefinition {uint32_t definition=0,source=0,raw_description=0,max_doses=0,item_value=0;};
struct ItemDetailsLocale {uint32_t locale=0,separator=0,total=0,left_singular=0,left_plural=0,image_measure=0,base_color=0,hint_color=0;};
struct ItemDetailsPresentation {uint32_t definition=0,locale=0,first_token=0,token_count=0;};
struct ItemDetailsToken {ItemDetailsTokenKind kind=ItemDetailsTokenKind::Text;uint32_t value=0,color=0,reserved=0;};
struct ItemDetailsResource:BattleResource {uint32_t bytes=0,crc32=0;};
// Placed runs preserve source text/color and inline images. Positions are local
// to the description rectangle; clipping remains a platform renderer concern.
struct ItemDetailsAtom {
 ItemDetailsTokenKind kind=ItemDetailsTokenKind::Text;std::string text;
 uint32_t resource=item_no_index,color=0;float x=0,y=0,width=0,height=0;
};
struct ItemDetailsComposition {std::vector<ItemDetailsAtom> atoms;float height=0;};
using ItemDetailsMeasure=std::function<float(std::string_view)>;
class ItemDetailsView {
public:
 bool valid()const{return bytes_!=nullptr;}
 bool same_content(ItemDetailsView other)const{return bytes_==other.bytes_&&size_==other.size_;}
 uint32_t count(ItemDetailsSection)const;std::string_view string(uint32_t)const;
 ItemDetailsDefinition definition(uint32_t)const;ItemDetailsLocale locale(uint32_t)const;
 ItemDetailsPresentation presentation(uint32_t)const;ItemDetailsToken token(uint32_t)const;
 ItemDetailsResource resource(uint32_t)const;float parameter(ItemDetailsParameter)const;
 std::string reviewed_commit()const;bool bind_items(ItemView,std::string&)const;
 bool verify_resources(const char* romfs_root,std::string&)const;
 // Uses the caller's checked source font metrics and actual panel width.
 // No inventory, doses, nickname, equipment or saved identity is mutated.
 bool compose(uint32_t definition,uint32_t doses,std::string_view nickname,
              std::string_view locale,float width,const ItemDetailsMeasure&,
              ItemDetailsComposition&,std::string&)const;
private:
 friend class ItemDetailsData;const uint8_t*bytes_=nullptr;size_t size_=0;
 const uint8_t*record(ItemDetailsSection,uint32_t)const;
};
class ItemDetailsData {
public:
 ItemDetailsData()=default;ItemDetailsData(const ItemDetailsData&)=delete;
 ItemDetailsData&operator=(const ItemDetailsData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 ItemDetailsView view()const{ItemDetailsView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}
