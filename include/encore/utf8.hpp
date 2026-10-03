#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
namespace encore {
// Strict Unicode scalar decoding. Cursor advances only after a complete valid
// character; overlongs, surrogates, truncated sequences and >U+10FFFF reject.
inline bool utf8_next(std::string_view text,size_t&cursor,uint32_t&codepoint){
 if(cursor>=text.size())return false;
 const size_t start=cursor;const auto first=static_cast<uint8_t>(text[start]);
 uint32_t value=0,minimum=0;size_t width=0;
 if(first<0x80){value=first;width=1;}
 else if(first>=0xc2&&first<=0xdf){value=first&0x1f;width=2;minimum=0x80;}
 else if(first>=0xe0&&first<=0xef){value=first&0x0f;width=3;minimum=0x800;}
 else if(first>=0xf0&&first<=0xf4){value=first&7;width=4;minimum=0x10000;}
 else return false;
 if(width>text.size()-start)return false;
 for(size_t n=1;n<width;++n){const auto byte=static_cast<uint8_t>(text[start+n]);if((byte&0xc0)!=0x80)return false;value=(value<<6)|(byte&0x3f);}
 if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
 cursor=start+width;codepoint=value;return true;
}
inline bool utf8_decode(std::string_view text,std::u32string&out){
 std::u32string decoded;decoded.reserve(text.size());size_t cursor=0;uint32_t value=0;
 while(cursor<text.size()){if(!utf8_next(text,cursor,value))return false;decoded.push_back(char32_t(value));}
 out.swap(decoded);return true;
}
inline bool utf8_append(uint32_t value,std::string&out){
 if(value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
 if(value<0x80)out.push_back(char(value));
 else if(value<0x800){out.push_back(char(0xc0|(value>>6)));out.push_back(char(0x80|(value&63)));}
 else if(value<0x10000){out.push_back(char(0xe0|(value>>12)));out.push_back(char(0x80|((value>>6)&63)));out.push_back(char(0x80|(value&63)));}
 else {out.push_back(char(0xf0|(value>>18)));out.push_back(char(0x80|((value>>12)&63)));out.push_back(char(0x80|((value>>6)&63)));out.push_back(char(0x80|(value&63)));}
 return true;
}
inline bool utf8_count(std::string_view text,size_t&count){count=0;size_t cursor=0;uint32_t cp=0;while(cursor<text.size()){if(!utf8_next(text,cursor,cp))return false;++count;}return true;}
inline bool utf8_prefix(std::string_view text,size_t count,std::string&out){size_t cursor=0;uint32_t cp=0;while(cursor<text.size()&&count){if(!utf8_next(text,cursor,cp))return false;--count;}out=std::string(text.substr(0,cursor));return true;}

}
