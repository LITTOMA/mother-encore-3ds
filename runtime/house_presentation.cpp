#include "encore/house_presentation.hpp"
#include "encore/utf8.hpp"
#include "encore/battle_entry.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
namespace encore::upstream {
namespace {
float cubic(float before,float from,float to,float after,float t){return .5f*((2*from)+(-before+to)*t+(2*before-5*from+4*to-after)*t*t+(-before+3*from-3*to+after)*t*t*t);}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y);}
}
bool HousePresentation::fail(const char*s){error_=s;return false;}
HouseClip HousePresentation::clip(HouseClipRole role)const{return content_.clip(content_.clip_for(role));}
HouseKey HousePresentation::sample(HouseClipRole role,double seconds,uint32_t previous_frame)const{
 const auto c=clip(role);HouseKey result;result.frame=previous_frame;if(!c.key_count)return result;
 double t=std::max(0.,seconds);if(c.loop)t=std::fmod(t,c.duration);else t=std::min(t,c.duration);
 const auto first=content_.key(c.first_key);if(t<first.time)return result;
 uint32_t index=0;while(index+1<c.key_count&&(c.interpolation==1?content_.key(c.first_key+index+1).time<t:content_.key(c.first_key+index+1).time<=t))++index;
 result=content_.key(c.first_key+index);
 if(c.interpolation!=1&&index+1<c.key_count){const auto next=content_.key(c.first_key+index+1);const float f=battle_ease(float((t-result.time)/(next.time-result.time)),result.ease);
  if(c.interpolation==2){const auto before=content_.key(c.first_key+(index?index-1:index));const auto after=content_.key(c.first_key+std::min(index+2,c.key_count-1));result.position={cubic(before.position.x,result.position.x,next.position.x,after.position.x,f),cubic(before.position.y,result.position.y,next.position.y,after.position.y,f)};}
  else result.position={result.position.x+(next.position.x-result.position.x)*f,result.position.y+(next.position.y-result.position.y)*f};
 }return result;
}
float HousePresentation::width(std::u32string_view text)const{float result=0;for(char32_t c:text){if(c==0||c==0x2063)continue;if(glyph_advance_){float advance=0;if(!glyph_advance_(glyph_state_,uint32_t(c),advance))return 0;result+=advance;continue;}for(uint32_t i=0;i<font_.count(BattleSection::Glyphs);++i){const auto g=font_.glyph(i);if(g.codepoint==uint32_t(c)){result+=g.advance;break;}}}return result;}
float HousePresentation::width(std::string_view text)const{std::u32string decoded;return encore::utf8_decode(text,decoded)?width(std::u32string_view(decoded)):0;}
bool HousePresentation::valid_text(std::string_view text)const{size_t cursor=0;uint32_t c=0;while(cursor<text.size()){if(!encore::utf8_next(text,cursor,c))return false;if(glyph_advance_){float advance=0;if(!glyph_advance_(glyph_state_,c,advance))return false;continue;}bool found=false;for(uint32_t i=0;i<font_.count(BattleSection::Glyphs);++i){const auto g=font_.glyph(i);if(g.codepoint==c&&g.advance>0){found=true;break;}}if(!found)return false;}return true;}
namespace {
uint32_t direction_index(Vec2 v){const float values[]={v.y,-v.x,v.x,-v.y};uint32_t best=0;for(uint32_t i=1;i<4;++i)if(values[i]>values[best])best=i;return best;}
}
bool HouseNpcAnimation::begin(HouseView c,Vec2 direction,uint32_t profile){
 if(!c.valid()||!finite(direction)||profile>=c.count(HouseSection::Profiles))return false;
 content_=c;profile_=profile;direction_=direction;dir_=c.profile(profile).directions==1?0:direction_index(direction);time_=0;talk_=requested_=false;frame_revision_=0;
 const auto index=c.clip_for(HouseClipRole(uint32_t(HouseClipRole::NpcIdleDown)+dir_),profile_);if(index==house_no_index)return false;
 const auto clip=c.clip(index);frame_=c.key(clip.first_key).frame;return true;
}
void HouseNpcAnimation::request(bool talking,Vec2 direction){requested_=talking;direction_=direction;}
bool HouseNpcAnimation::idle_frame(double dt){
 if(!content_.valid()||profile_>=content_.count(HouseSection::Profiles)||!std::isfinite(dt)||!std::isfinite(float(dt))||dt<0||!finite(direction_))return false;
 const auto profile=content_.profile(profile_);
 // Source travel() has no effect when a state does not exist. The Doll's
 // initial Floater profile has Idle only; Float is outside this checkpoint.
 const bool requested=requested_&&(profile.flags&4);
 const auto role=HouseClipRole(uint32_t(talk_?HouseClipRole::NpcTalkDown:HouseClipRole::NpcIdleDown)+dir_);const auto old=content_.clip(content_.clip_for(role,profile_));
 const auto dir=profile.directions==1?0:direction_index(direction_);
 // StateMachine transitions leave the outgoing sprite for the switching
 // frame and initialize the new blend space at time zero.
 if((!talk_&&requested)||(talk_&&!requested&&float(old.duration)-time_<=float(dt))){talk_=requested;time_=0;dir_=dir;return true;}
 const auto previous_time=time_;const bool direction_changed=dir!=dir_;
 if(direction_changed){dir_=dir;time_=0;}else time_=float(time_+float(dt));
 const auto c=content_.clip(content_.clip_for(HouseClipRole(uint32_t(talk_?HouseClipRole::NpcTalkDown:HouseClipRole::NpcIdleDown)+dir_),profile_));
 const auto advanced_time=time_;
 if(c.loop)time_=std::fmod(time_,float(c.duration));else time_=std::min(time_,float(c.duration));
 for(uint32_t k=0;k<c.key_count;++k){const auto key=content_.key(c.first_key+k);if(key.time==0||float(key.time)<time_)frame_=key.frame;else break;}
 // Godot's discrete track emits Sprite.frame_changed for property writes,
 // including repeated numeric values. A looping time-zero key is written
 // at the wrap and again on the following interval when its start is zero.
 // The original cleanup coroutine awaits this signal, not frame inequality.
 bool frame_written=false;
 for(uint32_t k=0;k<c.key_count;++k){const auto key_time=float(content_.key(c.first_key+k).time);
  if(direction_changed){frame_written|=key_time==0;continue;}
  if(!c.loop||advanced_time<float(c.duration)){frame_written|=previous_time<=key_time&&key_time<time_;continue;}
  frame_written|=advanced_time>=2*float(c.duration)||key_time>=previous_time||key_time==0||key_time<time_;
 }
 if(frame_written)++frame_revision_;
 return true;
}
bool HousePresentation::begin(HouseView c,BattleView font,SourceRandom&r){
 if(!c.valid()||!font.valid())return fail("House presentation needs validated content and glyphs");
 content_=c;font_=font;random_=&r;npcs_.assign(c.count(HouseSection::Npcs),{});
 text_seconds_=c.interaction().text_seconds;
 for(uint32_t i=0;i<npcs_.size();++i){const auto n=c.npc(i);auto&s=npcs_[i];s.position=n.position;s.direction=s.blend=n.default_direction;if(!s.animation.begin(c,s.blend,n.profile))return fail("Invalid NPC presentation profile");}
 active_=closing_=finished_=stopped_=talking_=voice_playing_=external_=name_closing_=false;done_=true;choice_rows_=0;audio_.clear();lines_.clear();error_="";return true;
}
bool HousePresentation::set_text_speed(double seconds){
 if(!content_.valid()||!std::isfinite(seconds)||seconds<=0)return fail("Invalid world dialogue text speed");
 text_seconds_=seconds;return true;
}
void HousePresentation::voice(HouseAudioKind kind,double pitch){audio_.push_back({kind,voice_,pitch});voice_playing_=kind==HouseAudioKind::VoiceStart;}
bool HousePresentation::append_segment(uint32_t index){
 if(index>=segment_count_)return fail("Unknown world dialogue segment");
 auto s=content_.segment(first_segment_+(localized_.segments.empty()?index:0));if(!localized_.segments.empty()){s.flags=localized_.segments[index].flags;s.token_count=uint32_t(localized_.segments[index].tokens.size());}std::u32string text;std::vector<uint32_t>colors;uint32_t color=0xffffffffu;
 const auto interaction=content_.interaction();
 // Bound expansion before allocating: delay tags are source timing cells,
 // not drawable spaces and not a timer independent of input acceleration.
 constexpr size_t maximum_cells=1024*1024;
 for(uint32_t i=0;i<s.token_count;++i){auto token=content_.token(s.first_token+(localized_.segments.empty()?i:0));std::string_view token_text;if(localized_.segments.empty())token_text=content_.string(token.text);else{token.kind=localized_.segments[index].tokens[i].kind;token_text=localized_.segments[index].tokens[i].text;}const auto kind=HouseTokenKind(token.kind);std::string value;
  if(kind==HouseTokenKind::Literal)value=std::string(token_text);
  else if(kind==HouseTokenKind::PlayerName)value=player_name_;
  else if(kind==HouseTokenKind::Color){uint32_t rgb=0;for(char c:token_text){uint32_t digit=c>='0'&&c<='9'?uint32_t(c-'0'):c>='a'&&c<='f'?uint32_t(c-'a'+10):uint32_t(c-'A'+10);rgb=(rgb<<4)|digit;}color=0xff000000u|((rgb>>16)&255u)|(rgb&0xff00u)|((rgb&255u)<<16);continue;}
  else if(kind==HouseTokenKind::ColorReset){color=0xffffffffu;continue;}
  else if(kind==HouseTokenKind::EarnedCash||kind==HouseTokenKind::BankCash||kind==HouseTokenKind::CurrentCash){if(!text_value_||!text_value_(text_value_state_,kind,value))return fail("Missing checked dialogue value");}
  else if(kind==HouseTokenKind::SourceDelay){
   const std::string amount(token_text);char*end=nullptr;const double number=std::strtod(amount.c_str(),&end);
   if(end!=amount.c_str()+amount.size()||!std::isfinite(number)||number<=0)return fail("Invalid world dialogue delay");
   const double cells=number/(text_seconds_*16);
   if(!std::isfinite(cells)||cells<0||cells>double(maximum_cells-text.size()))return fail("World dialogue delay exceeds capacity");
   value.assign(size_t(cells),'\0');
  }
  else if(kind==HouseTokenKind::ForcedNewline)value="\n";
  else if(token.kind==10)encore::utf8_append(0x2063,value);
  else return fail("Unknown dialogue token");
  if(kind!=HouseTokenKind::SourceDelay&&kind!=HouseTokenKind::ForcedNewline&&token.kind!=10&&!valid_text(value))return fail("World dialogue glyph is unavailable");
  std::u32string decoded;if(!encore::utf8_decode(value,decoded))return fail("Invalid world dialogue UTF-8");
  if(decoded.size()>maximum_cells-text.size())return fail("World dialogue text exceeds capacity");
  text+=decoded;colors.insert(colors.end(),decoded.size(),color);
 }
 std::u32string separator;if(!encore::utf8_decode(localized_.segments.empty()?content_.string(interaction.word_separator):std::string_view(localized_.word_separator),separator))return fail("Invalid word separator UTF-8");
 if(separator.empty())return fail("Missing word separator");
 const float maximum=content_.parameter(HouseParameter::DialogueText).z;
 std::u32string line;std::vector<uint32_t>line_colors;bool first=true;
 auto push=[&](){
  auto emit=[&](size_t from,size_t length){uint32_t offset=lines_.empty()?0:lines_.back().first+uint32_t(lines_.back().text.size())+(lines_.back().wait?1:0);lines_.push_back({line.substr(from,length),offset,index,first&&bool(s.flags&uint32_t(HouseSegmentFlag::Bullet)),false,std::vector<uint32_t>(line_colors.begin()+from,line_colors.begin()+from+length)});first=false;};
  // RichTextLabel additionally wraps long unseparated runs. Source CJK text
  // relies on this after TextTools.add_line_breaks (native probe retained).
  if(!localized_.segments.empty()&&width(line)>maximum){size_t start=0;float used=0;for(size_t i=0;i<line.size();++i){const float advance=width(std::u32string_view(line).substr(i,1));if(i>start&&used+advance>maximum){emit(start,i-start);start=i;used=0;}used+=advance;}emit(start,line.size()-start);}else emit(0,line.size());
  line.clear();line_colors.clear();};
 // TextTools.add_line_breaks wraps each existing newline segment separately
 // and omits empty result lines. In particular, the source newline immediately
 // before a W@ tag does not invent a blank WAIT-only row.
 size_t paragraph=0;
 for(;;){const auto newline=text.find('\n',paragraph);const auto limit=newline==std::u32string::npos?text.size():newline;size_t start=paragraph;
  for(;;){const auto end=text.find(separator,start);const auto stop=end==std::u32string::npos||end>=limit?limit:end;const auto word=text.substr(start,stop-start);
   if(start==paragraph){line=word;line_colors.assign(colors.begin()+start,colors.begin()+stop);}
   else if(width(line+separator+word)>maximum){if(!line.empty())push();line=word;line_colors.assign(colors.begin()+start,colors.begin()+stop);}
   else {line+=separator+word;line_colors.insert(line_colors.end(),colors.begin()+start-separator.size(),colors.begin()+stop);}
   if(stop==limit)break;
   start=end+separator.size();
  }
  if(!line.empty())push();
  if(newline==std::u32string::npos)break;
  paragraph=newline+1;
 }
 if(first)push();
 lines_.back().wait=bool(s.flags&uint32_t(HouseSegmentFlag::Wait));return true;
}
bool HousePresentation::begin_dialogue(uint32_t index,std::string_view name,Vec2 player){
 if(!content_.valid()||active_||index>=npcs_.size()||!finite(player))return fail("Invalid world dialogue start");
 const auto n=content_.npc(index);if(!begin_dialogue(n.first_segment,n.segment_count,name))return false;
 npc_=index;if(n.flags&2)npcs_[index].direction=npcs_[index].blend={player.x-npcs_[index].position.x,player.y-npcs_[index].position.y};return true;
}
bool HousePresentation::begin_dialogue(uint32_t first,uint32_t count,std::string_view name){
 if(!content_.valid()||active_||!count||first>=content_.count(HouseSection::Segments)||count>content_.count(HouseSection::Segments)-first)return fail("Invalid world dialogue span");
 localized_={};if(locale_resolver_&&!locale_resolver_(locale_state_,content_,first,count,name,localized_,locale_error_))return fail(locale_error_.c_str());
 npc_=house_no_index;first_segment_=first;segment_count_=localized_.segments.empty()?count:uint32_t(localized_.segments.size());if(!encore::utf8_prefix(name,localized_.segments.empty()?content_.interaction().max_player_name_length:localized_.max_name_length,player_name_))return fail("Invalid player name UTF-8");if(!valid_text(player_name_))return fail("Unsupported player name glyph");
 lines_.clear();for(uint32_t i=0;i<segment_count_;++i)if(!append_segment(i))return false;
 const auto s=content_.segment(first);speaker_=std::string(content_.string(s.speaker));voice_=std::string(content_.string(s.voice));bullet_=std::string(content_.string(content_.interaction().bullet_string));
 // The reviewed span represents one source phrase split at WAIT markers.
 // A later phrase changing name/voice needs an explicit scheduler extension.
 for(uint32_t i=1;i<count;++i){const auto part=content_.segment(first+i);if(content_.string(part.speaker)!=speaker_||content_.string(part.voice)!=voice_)return fail("World dialogue span changes speaker or voice");}
 if(!localized_.segments.empty()){speaker_=localized_.speaker;bullet_=localized_.bullet;}
 if(!valid_text(speaker_)||!valid_text(bullet_))return fail("Unsupported speaker/bullet glyph");
 choice_rows_=0;segment_=loaded_line_=visible_=0;loaded_count_=uint32_t(lines_[0].text.size())+(lines_[0].wait?1:0);
 box_time_=name_time_=name_size_time_=cursor_time_=text_time_=0;multiplier_=1;active_=true;external_=name_closing_=false;advance_requested_=false;done_=closing_=finished_=stopped_=talking_=false;voice_playing_=false;
 name_width_=std::max(width(speaker_),content_.parameter(HouseParameter::NameSizing).w)+content_.parameter(HouseParameter::NameSizing).x;
 audio_.push_back({HouseAudioKind::MenuOpen,{},1});return true;
}
bool HousePresentation::present_story_dialogue(uint32_t first,uint32_t count,std::string_view name){
 const bool reuse=active_&&!closing_;
 const auto old_box=box_time_,old_name=name_time_,old_size=name_size_time_,old_cursor=cursor_time_;
 const auto old_speaker=speaker_;const auto audio_count=audio_.size();const auto old_visible=visible_;
 auto old_lines=reuse?std::move(lines_):std::vector<Line>{};
 active_=false;
 if(!begin_dialogue(first,count,name))return false;
 if(reuse){
  const uint32_t offset=old_lines.empty()?0:old_lines.back().first+uint32_t(old_lines.back().text.size())+(old_lines.back().wait?1u:0u);
  const auto first_new=uint32_t(old_lines.size());
  for(auto&line:lines_){line.first+=offset;old_lines.push_back(std::move(line));}
  lines_=std::move(old_lines);loaded_line_=first_new;visible_=old_visible;
  const auto&line=lines_[loaded_line_];loaded_count_=line.first+uint32_t(line.text.size())+(line.wait?1u:0u);
  box_time_=old_box;cursor_time_=old_cursor;
  if(speaker_==old_speaker){name_time_=old_name;name_size_time_=old_size;}
  audio_.resize(audio_count);
 }
 return true;
}
void HousePresentation::clear_story_text(){
 lines_.clear();loaded_line_=loaded_count_=visible_=segment_=choice_rows_=0;
 text_time_=0;multiplier_=1;advance_requested_=false;finished_=true;stopped_=talking_=false;
 if(voice_playing_)voice(HouseAudioKind::VoiceStop);
}
void HousePresentation::close(bool sound){choice_rows_=0;if(npc_!=house_no_index)stop_npc_interaction(npc_);closing_=true;name_closing_=false;box_time_=name_time_=0;talking_=false;stopped_=false;visible_=0;if(voice_playing_)voice(HouseAudioKind::VoiceStop);if(sound)audio_.push_back({HouseAudioKind::MenuClose,{},1});}
void HousePresentation::hide_story_dialogue(bool sound){
 if(!active_||closing_)return;
 close(sound);lines_.assign(1,Line{});loaded_line_=loaded_count_=visible_=0;
 finished_=false;text_time_=0;
}
void HousePresentation::input(bool accept,bool cancel,bool defer_close,bool confirm_next){
 if(!active_||closing_||choice_rows_||(!accept&&!cancel)||box_time_<clip(HouseClipRole::DialogueOpen).duration)return;
 if(!finished_&&!stopped_)multiplier_=content_.interaction().cancel_multiplier;
 if(!finished_&&!stopped_&&!cancel)multiplier_=content_.interaction().accept_multiplier;
 else if(finished_){if(defer_close){advance_requested_=true;if(confirm_next)audio_.push_back({HouseAudioKind::Confirm,{},1});}else close();}else if(stopped_){stopped_=false;audio_.push_back({HouseAudioKind::Confirm,{},1});}
}
bool HousePresentation::begin_npc_dialogue(uint32_t i,uint32_t first,uint32_t count,std::string_view name,Vec2 player){
 if(i>=npcs_.size()||!begin_dialogue(first,count,name)||!begin_npc_interaction(i,player))return false;npc_=i;return true;
}
bool HousePresentation::begin_npc_interaction(uint32_t i,Vec2 player){
 if(i>=npcs_.size()||!finite(player))return fail("Invalid original NPC talker");
 auto&s=npcs_[i];const auto n=content_.npc(i);if(n.flags&2)s.direction=s.blend={player.x-s.position.x,player.y-s.position.y};return true;
}
bool HousePresentation::set_npc_talking(uint32_t i,bool talking){if(i>=npcs_.size())return fail("Unknown original NPC talker");npcs_[i].story_talking=talking;return true;}
bool HousePresentation::stop_npc_interaction(uint32_t i){
 if(i>=npcs_.size())return fail("Unknown stopped NPC");auto&s=npcs_[i];s.story_talking=false;
 if(!(content_.npc(i).flags&1)){s.return_pending=true;s.return_timer=content_.parameter(HouseParameter::NpcInteractionReturn).x;}
 return true;
}
bool HousePresentation::set_npc_replaced(uint32_t i,bool value){if(i>=npcs_.size())return fail("Unknown replaced NPC");npcs_[i].replaced=value;return true;}
bool HousePresentation::set_npc_visible(uint32_t i,bool value){if(i>=npcs_.size())return fail("Unknown NPC visibility");npcs_[i].visible=value;return true;}
bool HousePresentation::restore_npc_pose(uint32_t i,Vec2 position,Vec2 direction){if(i>=npcs_.size()||!finite(position)||!finite(direction))return fail("Invalid NPC restoration");auto&s=npcs_[i];s.position=position;s.direction=s.blend=direction;s.animation.request(false,direction);return true;}
bool HousePresentation::set_npc_looking(uint32_t i,bool looking){if(i>=npcs_.size())return fail("Unknown NPC view area");auto&s=npcs_[i];const auto n=content_.npc(i);if(!(n.flags&1))return true;if(s.looking&&!looking){s.return_timer=n.return_delay;s.return_pending=true;}s.looking=looking;return true;}
bool HousePresentation::advance_dialogue(double dt){
 if(!std::isfinite(dt)||dt<0)return fail("Invalid house physics delta/position");
 if(!active_||closing_||lines_.empty())return true;
 if(!finished_&&!stopped_){
  const double period=text_seconds_/multiplier_;
  if(!std::isfinite(period)||period<=0)return fail("Invalid effective world dialogue text speed");
  text_time_+=dt;
  while(text_time_>text_seconds_/multiplier_&&!finished_&&!stopped_){++visible_;
   if(visible_>loaded_count_){if(loaded_line_+1<lines_.size()){++loaded_line_;segment_=lines_[loaded_line_].segment;const auto&line=lines_[loaded_line_];loaded_count_=line.first+uint32_t(line.text.size())+(line.wait?1:0);}else {finished_=true;if(text_completion_&&!text_completion_(text_completion_state_))return fail("World rejected automatic text completion");}}
   // _get_last_visible_char clamps the counter to loaded content. Thus a
   // final overrun repeats the last cell: a trailing delay still suppresses
   // voice/RNG, while WAIT, spaces and ordinary final glyphs do not.
   const auto&line=lines_[loaded_line_];bool wait=line.wait&&visible_>=loaded_count_;
   bool delay=false;
   if(!wait&&loaded_count_){
    const uint32_t last=std::min(visible_,loaded_count_)-1;
    for(uint32_t i=loaded_line_+1;i>0;--i){const auto&candidate=lines_[i-1];if(last>=candidate.first&&last-candidate.first<candidate.text.size()){delay=candidate.text[last-candidate.first]=='\0';wait|=candidate.text[last-candidate.first]==0x2063;break;}}
   }
   if(!voice_.empty()&&!delay){const auto pitch=content_.interaction();voice(HouseAudioKind::VoiceStart,random_->rand_range(pitch.voice_pitch_min,pitch.voice_pitch_max));}
   // Source subtracts the current speed before handling its WAIT marker.
   text_time_-=text_seconds_/multiplier_;
   if(wait){stopped_=true;multiplier_=1;}
  }
 }
 const auto&line=lines_[loaded_line_];talking_=visible_<loaded_count_&&!(line.wait&&visible_==0);
 if(visible_>=loaded_count_&&voice_playing_)voice(HouseAudioKind::VoiceStop);
 return true;
}
bool HousePresentation::physics_frame(double dt,Vec2 player){
 if(!std::isfinite(dt)||dt<0||!finite(player))return fail("Invalid house physics delta/position");
 for(uint32_t i=0;i<npcs_.size();++i){auto&s=npcs_[i];if(s.replaced||!s.visible)continue;s.blend=s.direction;if(s.looking&&!(s.story_talking||(active_&&!closing_&&i==npc_&&talking_)))s.direction={player.x-s.position.x,player.y-s.position.y};s.animation.request(s.story_talking||(active_&&!closing_&&i==npc_&&talking_),s.blend);}
 return advance_dialogue(dt);
}
bool HousePresentation::advance_chrome(double dt){
 if(!std::isfinite(dt)||dt<0)return fail("Invalid house idle delta");
 if(active_){box_time_+=dt;name_time_+=dt;name_size_time_+=dt;cursor_time_+=dt;
  if(name_closing_&&name_time_>=clip(HouseClipRole::NameClose).duration)name_closing_=false;
  if(closing_&&box_time_>=clip(HouseClipRole::DialogueClose).duration){active_=false;done_=true;closing_=false;external_=false;name_closing_=false;npc_=house_no_index;}}
 return true;
}
bool HousePresentation::idle_frame(double dt){
 if(!std::isfinite(dt)||dt<0)return fail("Invalid house idle delta");
 for(uint32_t i=0;i<npcs_.size();++i){auto&s=npcs_[i];const auto n=content_.npc(i);
  if(s.return_pending){s.return_timer-=dt;if(s.return_timer<0){s.return_pending=false;if(!s.looking&&!(active_&&!closing_&&i==npc_))s.direction=n.default_direction;}}
  if(!s.animation.idle_frame(dt))return fail("Invalid NPC animation step");
 }
 return advance_chrome(dt);
}
HouseNpcPose HousePresentation::npc_pose(uint32_t i)const{
 HouseNpcPose p;if(i>=npcs_.size())return p;const auto n=content_.npc(i);const auto profile=content_.profile(n.profile);
 p.primary_resource=n.primary_resource;p.shadow_resource=(profile.flags&2)?n.shadow_resource:house_no_index;p.position=npcs_[i].position;p.sprite_offset=profile.sprite_offset;p.shadow_offset=profile.shadow_offset;p.frame=npcs_[i].animation.frame();p.visible=bool(profile.flags&1)&&npcs_[i].visible&&!npcs_[i].replaced;return p;
}
HouseDoorPose HousePresentation::openable_door_pose(uint32_t i,bool visible)const{
 HouseDoorPose p;if(i>=content_.count(HouseSection::OpenableDoors))return p;const auto d=content_.openable_door(i);
 p.resource=d.sprite_resource;p.position=d.sprite_position;p.offset=d.sprite_offset;p.visible=visible;return p;
}
WorldDialoguePose HousePresentation::dialogue_pose()const{
 WorldDialoguePose p;if(!active_)return p;p.visible=true;p.name_visible=!speaker_.empty()||(name_closing_&&!closing_);p.text_visible=!closing_;p.cursor_visible=!closing_&&!choice_rows_&&!lines_.empty()&&(finished_||stopped_);const auto br=content_.parameter(HouseParameter::DialogueRect),nr=content_.parameter(HouseParameter::NameRect);
 const auto box=sample(closing_?HouseClipRole::DialogueClose:HouseClipRole::DialogueOpen,box_time_);const auto name=sample(closing_||name_closing_?HouseClipRole::NameClose:HouseClipRole::NameOpen,name_time_);
 p.box={box.position.x,box.position.y,br.z,br.w};const auto sizing=content_.parameter(HouseParameter::NameSizing);const float progress=float(std::min(name_size_time_/sizing.y,1.));const float w=nr.z+(name_width_-nr.z)*(1-std::pow(1-progress,sizing.z));
 p.name={p.box.x+name.position.x,p.box.y+name.position.y,w,nr.w};p.clip=content_.parameter(HouseParameter::DialogueClip);p.clip.x+=p.box.x;p.clip.y+=p.box.y;
 p.text_layout=content_.parameter(HouseParameter::DialogueText);p.bullet_layout=content_.parameter(HouseParameter::DialogueBullet);p.name_label=content_.parameter(HouseParameter::NameLabel);p.speaker=speaker_;p.bullet=bullet_;
 const auto cursor=content_.parameter(HouseParameter::CursorGeometry);p.cursor={p.box.x+cursor.x,p.box.y+cursor.y,cursor.z,cursor.w};p.cursor_rotation=content_.parameter(HouseParameter::CursorRotation).x;
 p.box_resource=clip(HouseClipRole::DialogueOpen).resource;p.name_resource=clip(HouseClipRole::NameOpen).resource;p.cursor_resource=clip(HouseClipRole::Cursor).resource;p.cursor_frame=sample(HouseClipRole::Cursor,cursor_time_).frame;
 const auto display=content_.parameter(HouseParameter::DisplayReference);p.anchor={display.z,display.w};const auto metrics=content_.parameter(HouseParameter::FontMetrics);const float line_height=font_line_height_>0?font_line_height_+metrics.y:metrics.x+metrics.y;
 const float scroll=std::max(0.f,(float(loaded_line_)+2+float(choice_rows_))*line_height-p.text_layout.w);
 for(uint32_t i=0;i<=loaded_line_&&i<lines_.size();++i){const auto&line=lines_[i];const uint32_t shown=visible_>line.first?std::min(visible_-line.first,uint32_t(line.text.size())):0;
  HouseTextLine display_line;display_line.y=float(i+1)*line_height-scroll;display_line.bullet=line.bullet;
  for(uint32_t cell=0;cell<shown;++cell)if(line.text[cell]!='\0'&&line.text[cell]!=0x2063){const auto bytes=display_line.text.size();encore::utf8_append(uint32_t(line.text[cell]),display_line.text);const uint32_t color=cell<line.colors.size()?line.colors[cell]:0xffffffffu;display_line.colors.insert(display_line.colors.end(),display_line.text.size()-bytes,color);}
  p.lines.push_back(std::move(display_line));
 }
 return p;
}
bool HousePresentation::plain_glyphs(std::string_view text)const{
 size_t cursor=0;uint32_t c=0;
 while(cursor<text.size()){if(!encore::utf8_next(text,cursor,c))return false;if(c=='\n')continue;std::string one;if(!encore::utf8_append(c,one)||!valid_text(one))return false;}
 return true;
}
bool HousePresentation::wrap_plain(std::string_view source,std::vector<Line>& out)const{
 std::u32string text;if(!encore::utf8_decode(source,text))return false;
 const auto interaction=content_.interaction();std::u32string separator;
 if(!encore::utf8_decode(content_.string(interaction.word_separator),separator)||separator.empty())return false;
 const float maximum=content_.parameter(HouseParameter::DialogueText).z;
 std::u32string line;
 auto push=[&]{
  auto emit=[&](size_t from,size_t length){uint32_t offset=out.empty()?0:out.back().first+uint32_t(out.back().text.size());Line row;row.text=line.substr(from,length);row.first=offset;row.colors.assign(row.text.size(),0xffffffffu);out.push_back(std::move(row));};
  if(width(line)>maximum){size_t start=0;float used=0;for(size_t i=0;i<line.size();++i){const float advance=width(std::u32string_view(line).substr(i,1));if(i>start&&used+advance>maximum){emit(start,i-start);start=i;used=0;}used+=advance;}if(line.size()>start)emit(start,line.size()-start);}
  else if(!line.empty())emit(0,line.size());
  line.clear();
 };
 size_t paragraph=0;
 for(;;){const auto newline=text.find('\n',paragraph);const auto limit=newline==std::u32string::npos?text.size():newline;size_t start=paragraph;
  for(;;){const auto end=text.find(separator,start);const auto stop=end==std::u32string::npos||end>=limit?limit:end;const auto word=text.substr(start,stop-start);
   if(start==paragraph)line=word;else if(width(line+separator+word)>maximum){if(!line.empty())push();line=word;}else line+=separator+word;
   if(stop==limit)break;start=end+separator.size();}
  if(!line.empty())push();if(newline==std::u32string::npos)break;paragraph=newline+1;}
 return true;
}
bool HousePresentation::present_plain(std::string_view speaker,std::string_view text){
 if(!content_.valid())return fail("World dialogue box is not ready");
 if(closing_)return fail("World dialogue box is closing");
 if(active_&&!external_)return fail("House dialogue owns the box");
 if(text.empty()||!plain_glyphs(speaker)||!plain_glyphs(text))return fail("Unsupported dialogue glyph");
 std::vector<Line> built;if(!wrap_plain(text,built)||built.empty())return fail("World dialogue line produced no text");
 const std::string speaker_text(speaker);const bool reuse=active_;const bool same=reuse&&!name_closing_&&speaker_==speaker_text;
 const auto old_box=box_time_,old_name=name_time_,old_size=name_size_time_,old_cursor=cursor_time_;const auto old_visible=visible_;
 if(same){
  const uint32_t offset=lines_.empty()?0:lines_.back().first+uint32_t(lines_.back().text.size())+(lines_.back().wait?1u:0u);
  const auto first_new=uint32_t(lines_.size());
  for(auto& row:built){row.first+=offset;lines_.push_back(std::move(row));}
  loaded_line_=first_new;visible_=old_visible;const auto& row=lines_[loaded_line_];loaded_count_=row.first+uint32_t(row.text.size());
  box_time_=old_box;cursor_time_=old_cursor;name_time_=old_name;name_size_time_=old_size;
 }else{
  const bool had_name=!speaker_.empty()||name_closing_;lines_=std::move(built);speaker_=speaker_text;
  if(speaker_.empty()){if(reuse&&had_name){name_closing_=true;name_time_=0;}}
  else{name_closing_=false;name_time_=name_size_time_=0;const auto sizing=content_.parameter(HouseParameter::NameSizing);name_width_=std::max(width(speaker_),sizing.w)+sizing.x;}
  loaded_line_=visible_=0;loaded_count_=uint32_t(lines_[0].text.size());
  if(!reuse){box_time_=name_time_=name_size_time_=cursor_time_=0;audio_.push_back({HouseAudioKind::MenuOpen,{},1});active_=true;done_=false;}
  else box_time_=old_box,cursor_time_=old_cursor;
 }
 text_time_=0;multiplier_=1;advance_requested_=false;choice_rows_=0;segment_=0;finished_=stopped_=talking_=false;voice_.clear();voice_playing_=false;
 external_=true;npc_=house_no_index;done_=false;bullet_=std::string(content_.string(content_.interaction().bullet_string));error_="";return true;
}
std::vector<HouseAudioEvent>HousePresentation::take_audio_events(){auto result=std::move(audio_);audio_.clear();return result;}
}
