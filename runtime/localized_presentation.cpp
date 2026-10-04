#include "encore/localized_presentation.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
namespace encore::upstream {namespace {
bool fail(std::string&e,const std::string&s){e=s;return false;}
std::string lower(std::string value){for(char&c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return value;}
void replace(std::string&s,std::string_view from,std::string_view to){size_t at=0;while((at=s.find(from,at))!=std::string::npos){s.replace(at,from.size(),to);at+=to.size();}}
bool condition(std::string&s,std::string&e){size_t at=0;while((at=s.find("[if ",at))!=std::string::npos){auto end=s.find(']',at),close=s.find("[/if]",end);if(end==std::string::npos||close==std::string::npos||s.substr(at,end-at+1)!="[if input:gamepad]")return fail(e,"Unreviewed locale conditional tag");auto alternate=s.find("[else]",end);if(alternate==std::string::npos||alternate>close)alternate=close;s.replace(at,close+5-at,s.substr(end+1,alternate-end-1));}return true;}
}
bool LocalizedPresentation::plain_tags(std::string_view input,std::string_view name,std::string&out,std::string&e)const{
 std::string s(input);if(!condition(s,e))return false;out.clear();size_t at=0;
 while(at<s.size()){auto open=s.find('[',at);if(open==std::string::npos){out+=s.substr(at);break;}out+=s.substr(at,open-at);auto close=s.find(']',open);if(close==std::string::npos)return fail(e,"Unclosed locale tag");const auto tag=lower(s.substr(open+1,close-open-1));
 if(tag=="ninten"||tag=="partylead")out+=name;
 else if(tag.rfind("elision:",0)==0||tag.rfind("genitive:",0)==0){
  const bool elision=tag.rfind("elision:",0)==0;const auto who=tag.substr(elision?8:9);if(who!="ninten"&&who!="partylead")return fail(e,"Unreviewed affix target");
  if(!selection_||!selection_->catalog())return fail(e,"Missing source affix catalog");const auto*rule=selection_->catalog()->affix(elision?LocaleAffix::Elision:LocaleAffix::Genitive);if(!rule)return fail(e,"Missing source affix profile");
  std::u32string word,matches,pairs;if(!encore::utf8_decode(name,word)||!encore::utf8_decode(rule->matching,matches)||!encore::utf8_decode(rule->lower_pairs,pairs))return fail(e,"Malformed affix word/codepoints");
  char32_t cp=word.empty()?0:(elision?word.front():word.back());for(size_t i=0;i<pairs.size();i+=2)if(cp==pairs[i]){cp=pairs[i+1];break;}
  const bool match=!word.empty()&&matches.find(cp)!=std::u32string::npos;if(!elision)out+=name;out+=match?rule->when_match:rule->otherwise;
 }
 else if(tag.rfind("plur_num:",0)==0||tag.rfind("slav_num:",0)==0||tag.rfind("slav_num_strict:",0)==0){std::vector<std::string>parts;size_t start=0;for(;;){auto next=tag.find(':',start);parts.push_back(tag.substr(start,next==std::string::npos?tag.size()-start:next-start));if(next==std::string::npos)break;start=next+1;}if(parts.size()<4)return fail(e,"Malformed number suffix");char*end=nullptr;const long n=std::strtol(parts[1].c_str(),&end,10);if(end!=parts[1].c_str()+parts[1].size())return fail(e,"Noninteger number suffix");if(parts[0]=="plur_num")out+=parts[n>=2?3:2];else{if(parts.size()!=5)return fail(e,"Malformed Slavic suffix");long units=n%10,tens=(n%100)/10;out+=parts[(units==1&&tens!=1&&(parts[0]!="slav_num_strict"||n==1))?2:((units>=2&&units<=4&&tens!=1)?3:4)];}}
 else if(tag.rfind("particle:",0)==0){auto split=tag.rfind(':');const std::string who=tag.substr(9,split-9);if(split==std::string::npos||split+2!=tag.size()||tag.back()<'0'||tag.back()>'4')return fail(e,"Malformed Korean particle");const unsigned type=unsigned(tag.back()-'0');std::string word=(who=="ninten"||who=="partylead")?std::string(name):who;std::u32string cps;if(!encore::utf8_decode(word,cps))return fail(e,"Malformed particle word");uint32_t cp=cps.empty()?0:uint32_t(cps.back());if(cp>='A'&&cp<='Z')cp+=32;const auto&catalog=*selection_->catalog();auto contains=[&](unsigned group){const auto*b=catalog.binding("text.korean.endings/"+std::to_string(group));std::u32string values;return b&&encore::utf8_decode(b->expected,values)&&values.find(char32_t(cp))!=std::u32string::npos;};bool vowel=false;if(cp>=0xac00&&cp<0xac00+11172)vowel=(cp-0xac00)%28==0||(type==4&&(cp-0xac00)%28==8);else if(cp>=0x314f&&cp<=0x3163)vowel=true;else if(type==4&&cp==0x3139)vowel=true;else if(cp>='0'&&cp<='9')vowel=contains(0)||(type==4&&contains(1));else if(cp>='a'&&cp<='z')vowel=!contains(3)||(type==4&&contains(4));else vowel=contains(5)||contains(6)||contains(7);const auto*b=catalog.binding(std::string("text.particle.")+(vowel?"vowel/":"consonant/")+std::to_string(type));if(!b)return fail(e,"Missing source particle data");out+=b->expected;}
 else return fail(e,"Unimplemented locale tag: ["+tag+"]");
 at=close+1;
 }return true;
}
bool LocalizedPresentation::battle(RoundView view,uint32_t index,std::string_view name,std::string&out,std::string&e)const{
 if(!view.valid()||index>=view.count(RoundSection::Texts))return fail(e,"Invalid battle localized record");const auto row=view.text(index);if(row.role==0){out.clear();return true;}
 const auto id="battle/"+std::to_string(view.binding().battle_id)+"/"+std::to_string(index);std::string format;
 if(!selection_->catalog()->bound(id,view.string(row.text),selection_->code(),format,e))return false;
 if(!plain_tags(format,name,out,e))return false;
 if(out.find('{')!=std::string::npos||out.find('}')!=std::string::npos)return fail(e,"Unsupported source battle format field: "+id);
 return true;
}
bool LocalizedPresentation::house(HouseView view,uint32_t first,uint32_t count,std::string_view name,LocalizedHouseSpan&out,std::string&e)const{
 out={};if(selection_->code()==selection_->catalog()->fallback()){e.clear();return true;}const auto&catalog=*selection_->catalog();const auto id=std::to_string(first)+"/"+std::to_string(count);const auto*b=catalog.binding("house/"+id);if(!b)return fail(e,"House locale source binding unavailable: "+id);
 if(!view.valid()||first>=view.count(HouseSection::Segments)||count>view.count(HouseSection::Segments)-first)return fail(e,"House locale span outside loaded data");
 std::string signature;auto sized=[&](std::string_view s){signature+=std::to_string(s.size())+":";signature+=s;};
 for(uint32_t n=0;n<count;++n){const auto segment=view.segment(first+n);signature+=std::to_string(segment.flags)+"/";sized(view.string(segment.speaker));sized(view.string(segment.voice));signature+=std::to_string(segment.token_count)+"/";for(uint32_t i=0;i<segment.token_count;++i){const auto token=view.token(segment.first_token+i);signature+=std::to_string(token.kind)+"/";sized(view.string(token.text));}}
 const auto*expected=catalog.binding("house.expected/"+id);if(!expected||expected->expected!=signature)return fail(e,"House locale binding differs from loaded source tokens: "+id);
 // Original voice remains an asset identity. No translated string is ever
 // interpreted as an actor/node path, save identifier, or script command.
 std::string text(selection_->text(b->key).text);if(!condition(text,e))return false;
 if(const auto*speaker=catalog.binding("house.speaker/"+id))out.speaker=std::string(selection_->text(speaker->key).text);
 out.word_separator=std::string(selection_->text("WORD_SEPARATOR").text);out.bullet=std::string(selection_->text("SYMBOL_BULLET_MAIN").text);size_t max=0;if(!encore::utf8_count(selection_->text("LONGEST_POSSIBLE_NAME").text,max)||!max)return fail(e,"Invalid localized name bound");out.max_name_length=uint32_t(max);
 LocalizedHouseSegment segment;auto token=[&](uint32_t kind,std::string value={}){if(kind==1&&value.empty())return;if(kind==1&&!segment.tokens.empty()&&segment.tokens.back().kind==1)segment.tokens.back().text+=value;else segment.tokens.push_back({kind,std::move(value)});};auto push=[&](bool wait){segment.flags|=wait?2u:4u;out.segments.push_back(std::move(segment));segment={};};
 size_t at=0;
 while(at<text.size()){
  auto open=text.find('[',at);auto stop=open==std::string::npos?text.size():open;std::string literal=text.substr(at,stop-at);replace(literal," - "," – ");token(1,literal);if(open==std::string::npos)break;
  auto close=text.find(']',open);if(close==std::string::npos)return fail(e,"Unclosed localized House token");const auto tag=lower(text.substr(open+1,close-open-1));
  if(tag=="@"||tag=="br@"){if(!segment.tokens.empty()){token(9);}// authored new bullet after a line break
   segment.flags|=1;
  }else if(tag=="wait@"||tag=="w@"||tag=="waitbr"||tag=="wbr"){push(true);if(tag.back()=='@')segment.flags|=1;}
  else if(tag=="wait"||tag=="w")token(10);
  else if(tag=="br")token(9);
  else if(tag=="ninten"||tag=="partylead")token(2);
  else if(tag=="color"||tag=="c"){const auto*hint=catalog.binding("text.hint_color");if(!hint)return fail(e,"Missing source hint color");token(3,std::string(hint->expected));}
  else if(tag=="/color"||tag=="/c")token(4);
  else if(tag=="earnedcash")token(5);
  else if(tag=="bankcash")token(6);
  else if(tag=="currentcash")token(7);
  else if(tag=="ui_toggle"){
   // Resolved through the external checked CTR input binding, not translation.
   const auto*input=catalog.binding("input.ui_toggle");if(!input)return fail(e,"Missing localized CTR toggle binding");token(1,std::string(input->expected));
  }else if(tag=="d"||tag=="delay"||tag.rfind("d:",0)==0||tag.rfind("delay:",0)==0){auto colon=tag.find(':');const auto*def=catalog.binding("text.default_delay");const std::string amount=colon==std::string::npos?(def?std::string(def->expected):std::string()):tag.substr(colon+1);char*end=nullptr;double v=std::strtod(amount.c_str(),&end);if(amount.empty()||end!=amount.c_str()+amount.size()||!std::isfinite(v)||v<=0)return fail(e,"Unreviewed source delay form");token(8,amount);}
  else {std::string replacement;if(!plain_tags(text.substr(open,close-open+1),name,replacement,e))return false;token(1,replacement);}
  at=close+1;
 }
 push(false);if(out.segments.empty()||out.segments.size()>128)return fail(e,"Localized phrase segment bound exceeded");
 // Caller validates all dynamic values/glyphs before presenting the phrase.
 (void)view;e.clear();return true;
}
}
