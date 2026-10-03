#pragma once
#include "encore/localization.hpp"
#include "encore/house_presentation.hpp"
#include "encore/battle_round_data.hpp"
namespace encore::upstream {
class LocalizedPresentation {
public:
 explicit LocalizedPresentation(const LocaleSelection&s):selection_(&s){}
 const LocaleSelection&selection()const{return *selection_;}
 bool house(HouseView,uint32_t first,uint32_t count,std::string_view name,LocalizedHouseSpan&,std::string&error)const;
 static bool resolve_house(void*context,HouseView v,uint32_t first,uint32_t count,std::string_view name,LocalizedHouseSpan&out,std::string&error){return static_cast<LocalizedPresentation*>(context)->house(v,first,count,name,out,error);}
 bool battle(RoundView,uint32_t index,std::string_view name,std::string&out,std::string&error)const;
 bool plain_tags(std::string_view,std::string_view name,std::string&out,std::string&error)const;
private:const LocaleSelection*selection_;
};
}
