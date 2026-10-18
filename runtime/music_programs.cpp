#include "encore/music_programs.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace encore::upstream {namespace {
constexpr size_t limit=128*1024;
bool bad(std::string&e,const char*why){e=why;return false;}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
struct Reader{
 const uint8_t*p;size_t n;bool okay=true;
 uint32_t integer(){if(n<4){okay=false;return 0;}const auto v=word(p);p+=4;n-=4;return v;}
 MusicValue value(){MusicValue v;v.kind=MusicValueKind(integer());v.reference=integer();uint64_t bits=integer();bits|=uint64_t(integer())<<32;std::memcpy(&v.number,&bits,8);return v;}
 std::string string(){const auto size=integer();if(!okay||!size||size>1024||size>n){okay=false;return {};}std::string s(reinterpret_cast<const char*>(p),size);p+=size;n-=size;for(unsigned char c:s)if(c<32||c>126||c=='\\')okay=false;return s;}
};
bool number_kind(MusicValueKind k){return k==MusicValueKind::Number||k==MusicValueKind::Argument||k==MusicValueKind::Gain||k==MusicValueKind::Register;}
bool string_kind(MusicValueKind k){return k==MusicValueKind::Empty||k==MusicValueKind::String||k==MusicValueKind::Argument;}
bool guard(MusicOp op){return op>=MusicOp::IfExists&&op<=MusicOp::IfBoolean;}
bool valid_value(const MusicValue&v,size_t strings){
 if(!std::isfinite(v.number))return false;
 switch(v.kind){
 case MusicValueKind::Empty:case MusicValueKind::Gain:return !v.reference&&v.number==0;
 case MusicValueKind::String:return v.reference<strings&&v.number==0;
 case MusicValueKind::Number:return !v.reference;
 case MusicValueKind::Boolean:return !v.reference&&(v.number==0||v.number==1);
 case MusicValueKind::Argument:case MusicValueKind::Register:return v.reference<8&&v.number==0;
 }return false;
}
bool valid_instruction(const MusicInstruction&i,const std::vector<std::string>&strings,size_t pc,size_t count){
 if(uint32_t(i.op)<1||i.op>MusicOp::StopAreas||i.target>MusicTargetKind::Contains||!valid_value(i.selector,strings.size()))return false;
 for(const auto&v:i.arguments)if(!valid_value(v,strings.size()))return false;
 if(guard(i.op)){if(!i.skip||i.skip>count-pc-1)return false;}else if(i.skip)return false;
 if(i.target==MusicTargetKind::None||i.target==MusicTargetKind::Latest){if(i.selector.kind!=MusicValueKind::Empty)return false;}
 else if(i.target==MusicTargetKind::Child){if(i.selector.kind!=MusicValueKind::Number&&i.selector.kind!=MusicValueKind::Argument)return false;if(i.selector.kind==MusicValueKind::Number&&(i.selector.number<0||i.selector.number>=256||std::floor(i.selector.number)!=i.selector.number))return false;}
 else if(!string_kind(i.selector.kind))return false;
 if(i.target==MusicTargetKind::Contains&&(i.selector.kind==MusicValueKind::Empty||(i.selector.kind==MusicValueKind::String&&!i.selector.reference)))return false;
 auto range=[](const MusicValue&v,double lo,double hi,bool gain){return number_kind(v.kind)&&(gain||v.kind!=MusicValueKind::Gain)&&(v.kind!=MusicValueKind::Number||(v.number>=lo&&v.number<=hi));};
 auto argument=[&](size_t index,double lo,double hi,bool gain=false){return index<i.arguments.size()&&range(i.arguments[index],lo,hi,gain);};
 if(i.op==MusicOp::Play&&i.arguments.size()==3){auto empty=[](const MusicValue&v){return v.kind==MusicValueKind::Empty||(v.kind==MusicValueKind::String&&!v.reference);};if(empty(i.arguments[0])&&empty(i.arguments[1]))return false;}
 if(i.op==MusicOp::Play){if(i.arguments.size()!=3||!argument(2,0,std::numeric_limits<float>::max()))return false;for(size_t a=0;a<2;++a)if(i.arguments[a].kind==MusicValueKind::String&&i.arguments[a].reference&&!valid_music_stream_uri(strings[i.arguments[a].reference]))return false;}
 if((i.op==MusicOp::Volume||i.op==MusicOp::FadeIn||i.op==MusicOp::FadeTo)&&!argument(0,-120,24,true))return false;
 if((i.op==MusicOp::FadeIn||i.op==MusicOp::FadeTo)&&!argument(1,0,60))return false;
 if((i.op==MusicOp::FadeOut||i.op==MusicOp::FadeAll||i.op==MusicOp::StopAreas)&&!argument(0,0,60))return false;
 const auto n=i.arguments.size();auto numbers=[&](){return std::all_of(i.arguments.begin(),i.arguments.end(),[](const auto&v){return number_kind(v.kind);});};
 const bool single=i.target==MusicTargetKind::Child||i.target==MusicTargetKind::Latest||i.target==MusicTargetKind::Song;
 switch(i.op){
 case MusicOp::Add:case MusicOp::Pause:case MusicOp::Resume:return i.target==MusicTargetKind::None&&!n;
 case MusicOp::FadeAll:case MusicOp::StopAreas:return i.target==MusicTargetKind::None&&n==1&&numbers()&&i.arguments[0].kind!=MusicValueKind::Gain;
 case MusicOp::Play:return single&&n==3&&string_kind(i.arguments[0].kind)&&string_kind(i.arguments[1].kind)&&number_kind(i.arguments[2].kind);
 case MusicOp::Volume:case MusicOp::FadeOut:return single&&n==1&&numbers();
 case MusicOp::FadeIn:case MusicOp::FadeTo:return single&&n==2&&numbers();
 case MusicOp::Remove:return (single||i.target==MusicTargetKind::Contains)&&!n;
 case MusicOp::Stop:return single&&!n;
 case MusicOp::CapturePosition:return single&&n==1&&i.arguments[0].kind==MusicValueKind::Number&&i.arguments[0].number>=0&&i.arguments[0].number<8&&std::floor(i.arguments[0].number)==i.arguments[0].number;
 case MusicOp::IfExists:case MusicOp::IfPlaying:case MusicOp::IfPaused:return single&&!n;
 case MusicOp::IfEqual:case MusicOp::IfDifferent:return single&&n==1&&string_kind(i.arguments[0].kind);
 case MusicOp::IfBoolean:return i.target==MusicTargetKind::None&&n==1&&(i.arguments[0].kind==MusicValueKind::Boolean||i.arguments[0].kind==MusicValueKind::Argument);
 }return false;
}
}
const MusicProgram*MusicPrograms::find(uint32_t id)const{if(valid_)for(const auto&p:programs_)if(p.identity==id)return &p;return nullptr;}
const MusicEncounterMusic*MusicPrograms::encounter(uint32_t id)const{if(valid_)for(const auto&v:encounters_)if(v.battle==id)return &v;return nullptr;}
bool MusicPrograms::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<40||n>limit)return bad(e,"Music program resource size rejected");
 const auto sc=word(p+24),pc=word(p+28),ec=word(p+32);
 if(std::memcmp(p,"ENCMOP01",8)||word(p+8)!=1||word(p+12)!=n||word(p+20)!=1||sc>256||!pc||pc>64||ec>64||word(p+36))return bad(e,"Music program resource version/count/capability rejected");
 const uint8_t zero[4]{};auto crc=encore::crc32_update(~0u,p,16);crc=encore::crc32_update(crc,zero,4);crc=encore::crc32_update(crc,p+20,n-20);if((crc^~0u)!=word(p+16))return bad(e,"Music program resource checksum rejected");
 Reader r{p+40,n-40};MusicPrograms next;next.strings_.push_back({});
 for(uint32_t j=0;j<sc;++j){auto s=r.string();if(!r.okay)return bad(e,"Music program string rejected");if(std::find(next.strings_.begin(),next.strings_.end(),s)!=next.strings_.end())return bad(e,"Music program string duplicated");next.strings_.push_back(std::move(s));}
 size_t total=0;
 for(uint32_t j=0;j<pc;++j){MusicProgram program;program.identity=r.integer();program.delay_idle_frames=r.integer();const auto count=r.integer();total+=count;if(!r.okay||!program.identity||program.delay_idle_frames>1||!count||count>256||total>1024)return bad(e,"Music program identity/instruction count rejected");
  for(const auto&old:next.programs_)if(old.identity==program.identity)return bad(e,"Music program identity duplicated");
  for(uint32_t k=0;k<count;++k){MusicInstruction i;i.op=MusicOp(r.integer());i.skip=r.integer();i.target=MusicTargetKind(r.integer());const auto argc=r.integer();i.selector=r.value();if(!r.okay||argc>3)return bad(e,"Music instruction header rejected");for(uint32_t a=0;a<argc;++a)i.arguments.push_back(r.value());if(!r.okay||!valid_instruction(i,next.strings_,k,count))return bad(e,"Music instruction opcode/operand/target/branch rejected");program.instructions.push_back(std::move(i));}
  next.programs_.push_back(std::move(program));
 }
 for(uint32_t j=0;j<ec;++j){const auto id=r.integer(),music=r.integer(),intro=r.integer();if(!r.okay||!id||music>=next.strings_.size()||intro>=next.strings_.size()||(!music&&intro))return bad(e,"Music encounter binding rejected");for(const auto&v:next.encounters_)if(v.battle==id)return bad(e,"Music encounter binding duplicated");if((music&&!valid_music_stream_uri(next.strings_[music]))||(intro&&!valid_music_stream_uri(next.strings_[intro])))return bad(e,"Music encounter URI rejected");next.encounters_.push_back({id,next.strings_[music],next.strings_[intro]});}
 if(!r.okay||r.n)return bad(e,"Music program truncated/trailing payload rejected");next.valid_=true;*this=std::move(next);e.clear();return true;
}
bool MusicPrograms::load_file(const char*p,std::string&e){if(!p)return bad(e,"Music program resource path absent");std::vector<uint8_t>bytes;return encore::read_file(p,bytes,limit,e)&&load(bytes.data(),bytes.size(),e);}
MusicProgramStep MusicPrograms::evaluate(uint32_t id,const std::vector<MusicArgument>&input,const MusicSceneGraph&original,const MusicPositionQuery&query,MusicProgramResult&out,std::string&e,uint32_t elapsed)const{
 const auto*program=find(id);if(!program||!original.players().ready()||input.size()>8){e="Music program/input/owner unavailable";return MusicProgramStep::Failed;}
 if(elapsed<program->delay_idle_frames){e.clear();return MusicProgramStep::Pending;}
 for(const auto&a:input){if(a.kind>MusicArgument::Kind::Boolean||(a.kind==MusicArgument::Kind::Number&&!std::isfinite(a.number))||(a.kind==MusicArgument::Kind::String&&(a.text.size()>1024||std::any_of(a.text.begin(),a.text.end(),[](unsigned char c){return c<32||c>126||c=='\\';})))){e="Music argument value/type rejected";return MusicProgramStep::Failed;}}
 MusicProgramResult result;result.graph=original;result.program=id;std::array<double,8>registers{};std::array<bool,8>defined{};
 using K=MusicValueKind;using A=MusicArgument::Kind;
 for(size_t pc=0;pc<program->instructions.size();++pc){const auto&i=program->instructions[pc];auto&players=result.graph.players();std::vector<uint64_t>targets;
  auto string=[&](const MusicValue&v,std::string&value){if(v.kind==K::Empty){value.clear();return true;}if(v.kind==K::String){value=strings_[v.reference];return true;}if(v.kind==K::Argument&&v.reference<input.size()&&input[v.reference].kind==A::String){value=input[v.reference].text;return true;}return bad(e,"Music string argument is unavailable or wrong type");};
  auto number=[&](const MusicValue&v,double&value){if(v.kind==K::Number){value=v.number;return true;}if(v.kind==K::Argument&&v.reference<input.size()&&input[v.reference].kind==A::Number){value=input[v.reference].number;return true;}if(v.kind==K::Register&&defined[v.reference]){value=registers[v.reference];return true;}if(v.kind==K::Gain&&targets.size()==1){value=players.find(targets[0])->gain_db;return true;}return bad(e,"Music numeric register/argument/target is unavailable");};
  if(i.target==MusicTargetKind::Latest){if(!players.nodes().empty())targets.push_back(players.nodes().back().identity);}
  else if(i.target==MusicTargetKind::Child){double index;if(!number(i.selector,index))return MusicProgramStep::Failed;if(index<0||index>=256||std::floor(index)!=index){e="Music child index is invalid";return MusicProgramStep::Failed;}if(size_t(index)<players.nodes().size())targets.push_back(players.nodes()[size_t(index)].identity);}
  else if(i.target==MusicTargetKind::Song||i.target==MusicTargetKind::Contains){std::string text;if(!string(i.selector,text))return MusicProgramStep::Failed;if(i.target==MusicTargetKind::Contains&&text.empty()){e="Music substring selector is empty";return MusicProgramStep::Failed;}if(!text.empty())for(const auto&node:players.nodes())if((i.target==MusicTargetKind::Song&&node.stream==text)||(i.target==MusicTargetKind::Contains&&node.stream.find(text)!=node.stream.npos)){targets.push_back(node.identity);if(i.target==MusicTargetKind::Song)break;}}
  ++result.executed;
  if(guard(i.op)){
   bool passes=false;
   if(i.op==MusicOp::IfExists)passes=!targets.empty();
   else if(i.op==MusicOp::IfBoolean){const auto&v=i.arguments[0];if(v.kind==K::Boolean)passes=v.number!=0;else if(v.reference<input.size()&&input[v.reference].kind==A::Boolean)passes=input[v.reference].boolean;else{e="Music boolean argument is unavailable or wrong type";return MusicProgramStep::Failed;}}
   else{if(targets.size()!=1){e="Music property guard requires a current player";return MusicProgramStep::Failed;}const auto*n=players.find(targets[0]);if(i.op==MusicOp::IfPlaying)passes=n->playing;else if(i.op==MusicOp::IfPaused)passes=n->stream_paused;else{std::string text;if(!string(i.arguments[0],text))return MusicProgramStep::Failed;passes=(n->stream==text)==(i.op==MusicOp::IfEqual);}}
   if(!passes)pc+=i.skip;continue;
  }
  if(i.op==MusicOp::CapturePosition){
   if(targets.size()!=1){e="Music position capture requires a player";return MusicProgramStep::Failed;}const auto*n=players.find(targets[0]);const auto*before=original.players().find(n->identity);double position=n->start_seconds;
   if(before&&before->generation==n->generation){if(!query){e="Music position capture needs an output observer";return MusicProgramStep::Failed;}const auto step=query(n->identity,n->generation,position,e);if(step==MusicProgramStep::Pending||step==MusicProgramStep::Failed)return step;if(step!=MusicProgramStep::Ready){e="Music output observer returned an unknown status";return MusicProgramStep::Failed;}}
   if(!std::isfinite(position)||position<0||position>std::numeric_limits<float>::max()){e="Music position observer returned an invalid value";return MusicProgramStep::Failed;}const auto index=uint32_t(i.arguments[0].number);registers[index]=position;defined[index]=true;continue;
  }
  bool okay=true;double first=0,second=0;std::string current,loop;
  if(i.op==MusicOp::Play){if(!string(i.arguments[0],current)||!string(i.arguments[1],loop)||!number(i.arguments[2],first))return MusicProgramStep::Failed;if(current.empty())current=loop;}
  else if(!i.arguments.empty()){if(!number(i.arguments[0],first)||(i.arguments.size()>1&&!number(i.arguments[1],second)))return MusicProgramStep::Failed;}
  const bool gain_op=i.op==MusicOp::Volume||i.op==MusicOp::FadeIn||i.op==MusicOp::FadeTo;
  const bool time_op=i.op==MusicOp::FadeOut||i.op==MusicOp::FadeAll||i.op==MusicOp::StopAreas;
  if((gain_op&&(!std::isfinite(first)||first<-120||first>24))||(time_op&&(!std::isfinite(first)||first<0||first>60))||((i.op==MusicOp::FadeIn||i.op==MusicOp::FadeTo)&&(!std::isfinite(second)||second<0||second>60))||(i.op==MusicOp::Play&&(!valid_music_stream_uri(current)||(!loop.empty()&&!valid_music_stream_uri(loop))||first<0||first>std::numeric_limits<float>::max()))){e="Music operation value/source is outside its admitted range";return MusicProgramStep::Failed;}
  switch(i.op){
  case MusicOp::Add:{uint64_t node;okay=players.add_player(node,e);break;}
  case MusicOp::FadeAll:okay=players.fade_all(first,e);break;
  case MusicOp::Pause:okay=players.pause_all(e);break;
  case MusicOp::Resume:okay=players.resume_all(e);break;
  case MusicOp::StopAreas:if(result.graph.active())okay=result.graph.stop_on_door(result.graph.scene_epoch(),first,e);break;
  default:for(auto target:targets){switch(i.op){
   case MusicOp::Play:okay=players.play_track(target,current,loop,first,e);break;
   case MusicOp::Volume:okay=players.set_volume(target,float(first),e);break;
   case MusicOp::FadeIn:okay=players.fade_in(target,float(first),second,e);break;
   case MusicOp::FadeTo:okay=players.fade_to(target,float(first),second,e);break;
   case MusicOp::FadeOut:okay=players.fade_out(target,first,e);break;
   case MusicOp::Remove:okay=players.queue_remove(target,e);break;
   case MusicOp::Stop:okay=players.stop(target,e);break;
   default:okay=bad(e,"Unimplemented music instruction");break;
  }if(!okay)break;}break;
  }
  if(!okay)return MusicProgramStep::Failed;++result.changes;
 }
 out=std::move(result);e.clear();return MusicProgramStep::Ready;
}
}
