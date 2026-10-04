#include "encore/localized_presentation.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace encore::upstream;
namespace {
unsigned checks=0;
void check(bool v,const std::string&why){++checks;if(!v){std::cerr<<why<<'\n';std::exit(1);}}
uint32_t get(const std::vector<uint8_t>&b,size_t p){return uint32_t(b.at(p))|uint32_t(b.at(p+1))<<8|uint32_t(b.at(p+2))<<16|uint32_t(b.at(p+3))<<24;}
void put(std::vector<uint8_t>&b,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)b.at(p+i)=uint8_t(v>>(i*8));}
void fix(std::vector<uint8_t>&b,size_t grammar){put(b,12,uint32_t(b.size()));put(b,grammar+16,encore::crc32(b.data()+grammar+32,b.size()-grammar-32));put(b,16,encore::crc32(b.data()+24,b.size()-24));}
size_t text_end(const std::vector<uint8_t>&b,size_t p){return p+4+get(b,p);}
std::string render(const LocaleCatalog& c,const char*tag,const std::string&name){std::string error,out;LocaleSelection s;check(s.bind(c,error),error);LocalizedPresentation p(s);check(p.plain_tags(tag,name,out,error),error);return out;}
}
int main(int argc,char**argv){
 check(argc==2,"locale resource required");std::string error;std::vector<uint8_t> blob;check(encore::read_file(argv[1],blob,16*1024*1024,error),error);
 const std::string magic="ENCAFX01";const auto start=std::search(blob.begin()+24,blob.end(),magic.begin(),magic.end());check(start!=blob.end(),"independent affix block present");const size_t grammar=size_t(start-blob.begin());
 check(grammar+get(blob,grammar+12)==blob.size(),"grammar block has exact checked boundary");LocaleCatalog a;check(a.load(blob.data(),blob.size(),error),error);
 for(const std::string name:{"A","E","I","O","U","á","à","â","ä","æ","é","è","ê","ë","í","ì","î","ï","ó","ò","ô","ö","œ","ú","ù","û","ü","Á","À","Â","Ä","Æ","É","È","Ê","Ë","Í","Ì","Î","Ï","Ó","Ò","Ô","Ö","Œ","Ú","Ù","Û","Ü"})check(render(a,"[elision:ninten]",name)=="'","source vowels and audited uppercase partners elide");
 for(const std::string name:{"Ninten","Bob","","X"})check(render(a,"[elision:partylead]",name)=="e ","source nonvowel/empty affix");
 for(const std::string name:{"Hans","Alex"})check(render(a,"[genitive:ninten]",name)==name+"'","German terminal s/x source behavior");
 for(const std::string name:{"HansS","ALEXX","Ninten",""})check(render(a,"[genitive:partylead]",name)==name+"s","German source match is case-sensitive, including empty name");
 check(render(a,"Hello [ninten]","Ab")=="Hello Ab","English name consumer preserved");
 LocaleSelection selection;check(selection.bind(a,error)&&selection.select("zh_CN",error),error);LocalizedPresentation chinese(selection);std::string out;check(chinese.plain_tags("名字[ninten]","Ab",out,error)&&out=="名字Ab","Chinese existing name consumer preserved");check(!selection.select("fr",error)&&!selection.select("de",error),"migration does not enable unaccepted languages");
 std::vector<uint8_t> preference;std::string code;check(encode_locale_preference(selection,preference,error),error);check(get(preference,8)==1,"independent locale preference schema remains version1");check(decode_locale_preference(a,preference.data(),preference.size(),code,error)&&code=="zh_Hans_CN","existing stable saved locale identity remains accepted");
 check(!chinese.plain_tags("[elision:unknown]","Ab",out,error),"unreviewed affix target rejected");check(!chinese.plain_tags("[elision]","Ab",out,error),"source implicit next-word context remains unsupported");check(!chinese.plain_tags("[decline:ninten:M:0]","Ab",out,error),"unimplemented declension remains rejected");check(!chinese.plain_tags("[elision:ninten]",std::string("\xc3",1),out,error),"truncated UTF-8 name rejected");
 auto reject=[&](std::vector<uint8_t> bad,const char*why,bool checksum=true){if(checksum)fix(bad,grammar);check(!a.load(bad.data(),bad.size(),error),why);check(!error.empty(),"failed grammar load provides reason");check(render(a,"[elision:ninten]","É")=="'","failed load preserves previous owner and grammar");};
 for(const auto change:std::vector<std::pair<size_t,uint32_t>>{{8,1},{8,3},{grammar+8,0},{grammar+8,2},{grammar+12,1},{grammar+20,0},{grammar+20,2},{grammar+24,1},{grammar+28,1},{grammar+52,0},{grammar+52,1},{grammar+52,3},{grammar+56,0},{grammar+56,3},{grammar+60,1},{grammar+64,UINT32_MAX}}){auto bad=blob;put(bad,change.first,change.second);reject(std::move(bad),"unknown version/capability/reserved/profile/span rejected");}
 auto bad=blob;bad[grammar]^=1;reject(std::move(bad),"unknown grammar magic rejected");bad=blob;bad[grammar+32]^=1;reject(std::move(bad),"wrong grammar source pin rejected");bad=blob;bad.back()^=1;reject(std::move(bad),"corruption rejected",false);
 const size_t matching=grammar+64,pairs=text_end(blob,matching),matched=text_end(blob,pairs),otherwise=text_end(blob,matched),second=text_end(blob,otherwise);
 bad=blob;put(bad,second,1);reject(std::move(bad),"duplicate profile ID rejected");bad=blob;bad[matching+4]=0xff;reject(std::move(bad),"malformed UTF-8 data rejected");bad=blob;bad[matching+5]=bad[matching+4];reject(std::move(bad),"duplicate match codepoint rejected");bad=blob;bad[pairs+6]=bad[pairs+4];reject(std::move(bad),"duplicate upper mapping rejected");bad=blob;bad[pairs+5]=bad[pairs+4];reject(std::move(bad),"self mapping rejected");bad=blob;bad[matched+4]='[';reject(std::move(bad),"nested tag output rejected");
 for(size_t n=grammar;n<blob.size();++n){bad=blob;bad.resize(n);check(!a.load(bad.data(),bad.size(),error),"every affix block truncation rejected");}
 bad=blob;bad.push_back(0);fix(bad,grammar);reject(std::move(bad),"trailing unrecognized data rejected");
 // A/B resource mutation preserves executable and all other catalog content.
 auto changed=blob;changed[matched+4]='!';fix(changed,grammar);LocaleCatalog b;check(b.load(changed.data(),changed.size(),error),error);
 check(render(a,"[elision:ninten]","É")=="'"&&render(b,"[elision:ninten]","É")=="!","same actual localized consumer changes affix from external binary data");
 check(b.lookup("OPTIONS_LANGUAGE","zh_CN").text==a.lookup("OPTIONS_LANGUAGE","zh_CN").text,"binary grammar change preserves translated content");
 std::fill(changed.begin(),changed.end(),0);check(render(b,"[elision:ninten]","É")=="!","checked catalog owns affix views");
 std::cout<<"Locale grammar: "<<checks<<" checks; source vowels/case pairs, German case semantics, fail-closed rollback and actual consumer A/B\n";
}
