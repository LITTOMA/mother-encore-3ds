#include "encore/field_equipment_menu.hpp"
#include "encore/battle_entry.hpp"
#include "encore/session_save.hpp"
#include <algorithm>
#include <cmath>
#include <set>
namespace encore::upstream {
bool field_equipment_character(FieldEquipmentView data,const SessionSnapshot& session,uint32_t& index,std::string& error){
 if(!data.valid()||session.party.size()!=1||session.party.front()!=data.binding(FieldBinding::Owner)){
  error="Field equipment requires the admitted singleton party";return false;
 }
 uint32_t selected=UINT32_MAX;
 for(uint32_t i=0;i<session.characters.size();++i)if(session.characters[i].character_id==session.party.front()){
  if(selected!=UINT32_MAX){error="Field equipment active character is duplicated";return false;}
  selected=i;
 }
 if(selected==UINT32_MAX){error="Field equipment active character is absent";return false;}
 index=selected;error.clear();return true;
}
bool field_healing(FieldPsiSkill skill,int32_t hp,int32_t maxhp,int32_t pp,int32_t iq,SourceRandom&random,FieldHealingResult&out,std::string&e){
 if(skill.action!=2||!skill.power||!skill.iq_divisor||skill.iq_divisor>65535||skill.cost>65535||skill.power>65535||skill.variance>65535||hp<0||maxhp<=0||hp>maxhp||pp<0||iq<0||int64_t(pp)<skill.cost){e="Field healing skill/target/PP rejected";return false;}
 auto next_random=random;
 const double value=double(skill.power)+iq/int32_t(skill.iq_divisor);
 const double rolled=std::floor(value+next_random.randf()*skill.variance-double(skill.variance)/2);
 if(rolled>INT32_MAX){e="Field healing value overflow";return false;}
 const int32_t amount=int32_t(std::max(0.0,rolled));const int64_t next=int64_t(hp)+amount;
 out={int32_t(std::min(next,int64_t(maxhp))),pp-int32_t(skill.cost),amount,next>=maxhp};random=next_random;e.clear();return true;
}
bool FieldEquipmentMenu::fail(const char*s){error_=s;return false;}
bool FieldEquipmentMenu::validate_snapshot(const FieldEquipmentSnapshot&s){
 if(s.owner!=data_.binding(FieldBinding::Owner)||s.nickname.empty()||!s.level||s.cash<0||s.cash>UINT32_MAX||!s.inventory.valid()||!data_.bind_items(s.inventory.content(),error_))return fail("Field equipment character/inventory scope rejected");
 if(s.stats[0]<=0||std::any_of(s.stats.begin(),s.stats.end(),[](int32_t n){return n<0;}))return fail("Field equipment current stats rejected");
 if(s.pp<0||s.pp>s.stats[1])return fail("Field PSI current PP rejected");
 std::set<std::string> skills;
 for(const auto&skill:s.learned_skills)if(!skills.insert(skill).second||([&]{if(skill==data_.binding(FieldBinding::PsiBaseSkill0)||skill==data_.binding(FieldBinding::PsiBaseSkill1))return false;for(uint32_t i=0;i<data_.psi_skill_count();++i)if(skill==data_.psi_skill(i).id)return false;return true;}()))return fail("Field PSI unknown/duplicate learned skill rejected");
 if(!skills.count(std::string(data_.binding(FieldBinding::PsiBaseSkill0)))||!skills.count(std::string(data_.binding(FieldBinding::PsiBaseSkill1))))return fail("Field PSI base skill identity missing");
 const bool knows=skills.count(std::string(data_.binding(FieldBinding::PsiSkillId)))!=0;
 if(s.level+1<uint32_t(data_.parameter(FieldParameter::PsiLearnLevel))||s.level>uint32_t(data_.parameter(FieldParameter::PsiLearnLevel)))return fail("Field PSI level scope rejected");
 if(knows!=(s.level>=uint32_t(data_.parameter(FieldParameter::PsiLearnLevel))))return fail("Field PSI learned skill/level binding rejected");
 std::set<uint32_t>uids,slots;for(const auto&i:s.inventory.instances()){
  if(i.definition>=s.inventory.content().count(ItemSection::Definitions)||!uids.insert(i.id).second||i.equipped>1||!i.doses||i.doses>65535)return fail("Field equipment inventory identity rejected");
  auto d=s.inventory.content().definition(i.definition);if(!(d.flags&uint32_t(ItemDefinitionFlag::Equipment))){if(i.equipped)return fail("Field equipment ordinary item marked equipped");continue;}
  bool found=false;for(uint32_t j=0;j<data_.count(FieldSection::Equipment);++j){auto e=data_.equipment(j);if(e.definition==i.definition){found=true;if(i.equipped&&!slots.insert(e.slot).second)return fail("Field equipment duplicate slot rejected");break;}}
  if(!found)return fail("Field equipment inventory has unmapped equipment");
 }return true;
}
bool FieldEquipmentMenu::read_snapshot(){FieldEquipmentSnapshot next;if(!host_.read(next,error_))return false;if(!validate_snapshot(next))return false;snapshot_=std::move(next);return true;}
bool FieldEquipmentMenu::initialize(FieldEquipmentView data,FieldEquipmentHost host,bool chinese){if(!data.valid()||!host.read||!host.preview||!host.commit)return fail("Field equipment requires checked content/host");FieldEquipmentMenu next;next.data_=data;next.host_=std::move(host);next.chinese_=chinese;if(!next.read_snapshot())return fail(next.error());next.preview_=next.snapshot_.stats;*this=std::move(next);return true;}
bool FieldEquipmentMenu::open(){if(!data_.valid()||!host_.read||active())return fail("Field equipment open state rejected");if(!read_snapshot())return false;command_=slot_=candidate_=offset_=psi_=0;psi_message_.clear();candidates_.clear();preview_=snapshot_.stats;phase_=FieldEquipmentPhase::PauseOpening;phase_time_=pause_time_=equip_time_=description_time_=0;description_open_=equip_closing_=has_description_=psi_closing_=psi_request_=false;psi_time_=0;description_from_=description_to_=0;cursor_time_=0;retarget_cursor(true);sounds_.clear();sounds_.push_back(FieldEquipmentSound::PauseOpen);error_.clear();return true;}
uint32_t FieldEquipmentMenu::psi_skill_count()const{uint32_t count=0;for(uint32_t i=0;i<data_.psi_skill_count();++i)if(std::find(snapshot_.learned_skills.begin(),snapshot_.learned_skills.end(),data_.psi_skill(i).id)!=snapshot_.learned_skills.end())++count;return count;}
uint32_t FieldEquipmentMenu::psi_skill_index(uint32_t row)const{for(uint32_t i=0;i<data_.psi_skill_count();++i)if(std::find(snapshot_.learned_skills.begin(),snapshot_.learned_skills.end(),data_.psi_skill(i).id)!=snapshot_.learned_skills.end()){if(!row)return i;--row;}return UINT32_MAX;}
bool FieldEquipmentMenu::psi_learned()const{return psi_skill_count()!=0;}
bool FieldEquipmentMenu::equipment_visible()const{return equip_closing_||phase_==FieldEquipmentPhase::EquipOpening||phase_==FieldEquipmentPhase::Slots||phase_==FieldEquipmentPhase::Candidates;}
const ItemInstance*FieldEquipmentMenu::equipped_item(uint32_t slot)const{for(const auto&i:snapshot_.inventory.instances())if(i.equipped)for(uint32_t j=0;j<data_.count(FieldSection::Equipment);++j){auto e=data_.equipment(j);if(e.definition==i.definition&&e.slot==slot)return &i;}return nullptr;}
const ItemInstance*FieldEquipmentMenu::selected_item()const{if(phase_==FieldEquipmentPhase::Candidates)return candidate_<candidates_.size()&&!candidates_[candidate_].none?&candidates_[candidate_].item:nullptr;if(phase_==FieldEquipmentPhase::Slots||phase_==FieldEquipmentPhase::EquipOpening)return equipped_item(slot_);return nullptr;}
void FieldEquipmentMenu::retarget_cursor(bool instant){
 auto rows=uint32_t(data_.parameter(FieldParameter::ListRows));if(phase_==FieldEquipmentPhase::Candidates){if(candidate_<offset_)offset_=candidate_;else if(candidate_>=offset_+rows)offset_=candidate_-rows+1;cursor_target_row_=float(candidate_-offset_);cursor_target_col_=0;}
 else if(phase_==FieldEquipmentPhase::Slots||phase_==FieldEquipmentPhase::EquipOpening){cursor_target_row_=float(slot_);cursor_target_col_=0;}
 else if(phase_==FieldEquipmentPhase::PsiOpening||phase_==FieldEquipmentPhase::Psi||phase_==FieldEquipmentPhase::PsiFailed||phase_==FieldEquipmentPhase::PsiTarget||phase_==FieldEquipmentPhase::PsiResult){cursor_target_row_=float(psi_);cursor_target_col_=0;}
  else{auto columns=uint32_t(data_.parameter(FieldParameter::PauseColumns));cursor_target_row_=float(command_/columns);cursor_target_col_=float(command_%columns);}
 cursor_from_row_=cursor_row_;cursor_from_col_=cursor_col_;cursor_move_time_=0;if(instant){cursor_row_=cursor_from_row_=cursor_target_row_;cursor_col_=cursor_from_col_=cursor_target_col_;}
}
void FieldEquipmentMenu::update_description(){const auto*item=selected_item();const bool next=item!=nullptr;if(item){description_item_=*item;has_description_=true;}if(next==description_open_)return;description_from_=description_progress();description_to_=next?1.f:0.f;description_time_=0;description_open_=next;}
bool FieldEquipmentMenu::update_preview(){
 preview_=snapshot_.stats;if(phase_==FieldEquipmentPhase::Candidates){const auto&c=candidates_.at(candidate_);if(!host_.preview(slot_,c.none,c.item.id,preview_,error_))return false;std::array<int64_t,7>expected{};for(size_t j=0;j<7;++j)expected[j]=snapshot_.stats[j];const auto*old=equipped_item(slot_);for(uint32_t i=0;i<data_.count(FieldSection::Equipment);++i){auto e=data_.equipment(i);for(size_t j=0;j<7;++j){if(old&&old->definition==e.definition)expected[j]-=e.boosts[j];if(!c.none&&c.item.definition==e.definition)expected[j]+=e.boosts[j];}}for(size_t j=0;j<7;++j)if(expected[j]<0||expected[j]>INT32_MAX||(j==0&&!expected[j])||expected[j]!=preview_[j])return fail("Field equipment projected stats disagree with admitted boosts");}
 update_description();return true;
}
bool FieldEquipmentMenu::enter_candidates(){
 if(!read_snapshot())return false;bool suitable=false;candidates_.clear();for(const auto&i:snapshot_.inventory.instances())for(uint32_t j=0;j<data_.count(FieldSection::Equipment);++j){auto e=data_.equipment(j);if(e.definition==i.definition&&e.slot==slot_){suitable=true;if(!i.equipped)candidates_.push_back(FieldEquipmentCandidate{false,i});break;}}
 if(!suitable){sounds_.push_back(FieldEquipmentSound::Restricted);return true;}
 if(equipped_item(slot_)||candidates_.empty())candidates_.push_back(FieldEquipmentCandidate{});
 if(candidates_.empty())return fail("Field equipment empty candidate set rejected");phase_=FieldEquipmentPhase::Candidates;candidate_=offset_=0;retarget_cursor(true);sounds_.push_back(FieldEquipmentSound::Confirm);return update_preview();
}
bool FieldEquipmentMenu::input(int x,int y,bool confirm,bool cancel,bool scope,bool pause_toggle){
 if(x<-1||x>1||y<-1||y>1)return fail("Field equipment direction rejected");if(!active())return true;
 if(phase_==FieldEquipmentPhase::PauseClosing)return true;
 if(phase_==FieldEquipmentPhase::Pause||phase_==FieldEquipmentPhase::PauseOpening){
  if(cancel||pause_toggle){phase_=FieldEquipmentPhase::PauseClosing;pause_time_=phase_time_=0;sounds_.push_back(FieldEquipmentSound::PauseClose);return true;}
  if(phase_==FieldEquipmentPhase::PauseOpening)return true;
  if(y||x){auto columns=uint32_t(data_.parameter(FieldParameter::PauseColumns)),rows=data_.count(FieldSection::Commands)/columns;auto row=int(command_/columns),col=int(command_%columns);if(y)row=(row+y+int(rows))%int(rows);else col=(col+x+int(columns))%int(columns);auto next=uint32_t(row)*columns+uint32_t(col);if(next!=command_){command_=next;retarget_cursor();sounds_.push_back(FieldEquipmentSound::Move);}}
  if(confirm){if(!data_.command(command_).enabled){sounds_.push_back(FieldEquipmentSound::Restricted);return true;}if(!read_snapshot())return false;if(data_.command(command_).id==2){phase_=FieldEquipmentPhase::PsiOpening;psi_=0;psi_closing_=false;psi_time_=phase_time_=0;retarget_cursor(true);sounds_.push_back(FieldEquipmentSound::EquipOpen);return true;}if(data_.command(command_).id!=3)return fail("Field command action rejected");phase_=FieldEquipmentPhase::EquipOpening;phase_time_=equip_time_=0;equip_closing_=false;slot_=candidate_=offset_=0;preview_=snapshot_.stats;retarget_cursor(true);update_description();sounds_.push_back(FieldEquipmentSound::EquipOpen);}return true;
 }
 if(phase_==FieldEquipmentPhase::PsiOpening||phase_==FieldEquipmentPhase::Psi||phase_==FieldEquipmentPhase::PsiFailed||phase_==FieldEquipmentPhase::PsiTarget||phase_==FieldEquipmentPhase::PsiResult){
  if(phase_==FieldEquipmentPhase::PsiResult){if(confirm||cancel){phase_=FieldEquipmentPhase::Psi;psi_message_.clear();}return true;}
  if(phase_==FieldEquipmentPhase::PsiTarget){
   if(cancel){phase_=FieldEquipmentPhase::Psi;sounds_.push_back(FieldEquipmentSound::Back);return true;}
   if(confirm){if(!host_.use_skill)return fail("Field healing host is unbound");
    if(!host_.use_skill(psi_skill_index(psi_),psi_message_,error_))return false;
    if(!read_snapshot())return false;phase_=FieldEquipmentPhase::PsiResult;}
   return true;
  }
  if(phase_==FieldEquipmentPhase::PsiFailed){if(confirm||cancel)phase_=FieldEquipmentPhase::Psi;return true;}
  if(cancel){phase_=FieldEquipmentPhase::Pause;psi_closing_=true;psi_time_=0;retarget_cursor(true);sounds_.push_back(FieldEquipmentSound::EquipClose);return true;}
  if(phase_==FieldEquipmentPhase::PsiOpening)return true;
  if(y&&psi_skill_count()){const auto n=psi_skill_count();psi_=uint32_t((int(psi_)+(y>0?1:-1)+int(n))%int(n));retarget_cursor();sounds_.push_back(FieldEquipmentSound::Move);}
  // Skill identities and effects come from checked source records.
  if(confirm&&psi_learned()){
   if(!read_snapshot())return false;
   if(!psi_learned())return fail("Field PSI skill changed during selection");
   if(snapshot_.pp<int32_t(psi_selected_skill().cost)){phase_=FieldEquipmentPhase::PsiFailed;sounds_.push_back(FieldEquipmentSound::Restricted);return true;}
   if(psi_selected_skill().action==2){phase_=FieldEquipmentPhase::PsiTarget;sounds_.push_back(FieldEquipmentSound::Confirm);return true;}
   if(psi_selected_skill().action!=1)return fail("Field PSI action is unsupported");
   psi_requested_=psi_skill_index(psi_);psi_request_=true;psi_closing_=true;psi_time_=pause_time_=phase_time_=0;phase_=FieldEquipmentPhase::PauseClosing;sounds_.push_back(FieldEquipmentSound::Confirm);
  }return true;
 }
 if(phase_!=FieldEquipmentPhase::EquipOpening&&phase_!=FieldEquipmentPhase::Slots&&phase_!=FieldEquipmentPhase::Candidates)return fail("Field equipment unknown active phase");
 if(cancel){if(phase_==FieldEquipmentPhase::Candidates){phase_=FieldEquipmentPhase::Slots;candidates_.clear();candidate_=offset_=0;retarget_cursor(true);sounds_.push_back(FieldEquipmentSound::Back);return update_preview();}
  // EquipMenuUI emits back when Close starts, immediately reactivating Pause.
  phase_=FieldEquipmentPhase::Pause;equip_closing_=true;equip_time_=0;retarget_cursor(true);description_from_=description_progress();description_to_=0;description_open_=false;description_time_=0;sounds_.push_back(FieldEquipmentSound::EquipClose);return true;
 }
 // With the admitted singleton party, selecting the sole character keeps it.
 // Candidate mode disables the source character tabs entirely.
 if(scope)return true;
 if(y){auto n=phase_==FieldEquipmentPhase::Candidates?uint32_t(candidates_.size()):data_.count(FieldSection::Slots);auto&selection=phase_==FieldEquipmentPhase::Candidates?candidate_:slot_;auto next=uint32_t((int64_t(selection)+y+n)%n);if(selection!=next){selection=next;retarget_cursor();sounds_.push_back(FieldEquipmentSound::Move);if(!update_preview())return false;}}
 if(confirm){if(phase_!=FieldEquipmentPhase::Candidates)return enter_candidates();const auto c=candidates_.at(candidate_);if(!host_.commit(slot_,c.none,c.item.id,error_))return false;if(!read_snapshot())return false;phase_=FieldEquipmentPhase::Slots;candidates_.clear();candidate_=offset_=0;preview_=snapshot_.stats;retarget_cursor(true);sounds_.push_back(c.none?FieldEquipmentSound::Clear:FieldEquipmentSound::Equip);update_description();}
 return true;
}
float FieldEquipmentMenu::sample(FieldClipRole role,float time)const{auto c=data_.clip(role);if(!c.key_count)return 0;uint32_t i=0;while(i+1<c.key_count&&data_.key(c.first_key+i+1).time<=time)++i;auto a=data_.key(c.first_key+i);if(i+1==c.key_count||time<=a.time)return a.value;auto b=data_.key(c.first_key+i+1);auto t=battle_ease((time-a.time)/(b.time-a.time),a.ease);return a.value+(b.value-a.value)*t;}
float FieldEquipmentMenu::pause_progress()const{if(!active())return 0;if(phase_==FieldEquipmentPhase::PauseOpening)return std::clamp(pause_time_/data_.clip(FieldClipRole::PauseOpen).duration,0.f,1.f);if(phase_==FieldEquipmentPhase::PauseClosing)return 1-std::clamp(pause_time_/data_.clip(FieldClipRole::PauseClose).duration,0.f,1.f);return 1;}
float FieldEquipmentMenu::equip_progress()const{if(equip_closing_)return 1-std::clamp(equip_time_/data_.clip(FieldClipRole::EquipClose).duration,0.f,1.f);if(phase_==FieldEquipmentPhase::EquipOpening)return std::clamp(equip_time_/data_.clip(FieldClipRole::EquipOpen).duration,0.f,1.f);return equipment_visible()?1.f:0.f;}
float FieldEquipmentMenu::description_progress()const{if(!data_.valid())return 0;auto role=description_open_?FieldClipRole::DescriptionOpen:FieldClipRole::DescriptionClose;auto c=data_.clip(role);return description_from_+(description_to_-description_from_)*std::clamp(description_time_/c.duration,0.f,1.f);}
float FieldEquipmentMenu::animation_value(FieldClipRole role)const{
 if(role==FieldClipRole::PauseOpen||role==FieldClipRole::PauseClose){auto c=phase_==FieldEquipmentPhase::PauseClosing?FieldClipRole::PauseClose:FieldClipRole::PauseOpen;return sample(c,phase_==FieldEquipmentPhase::PauseOpening||phase_==FieldEquipmentPhase::PauseClosing?pause_time_:data_.clip(c).duration);}
 if(role==FieldClipRole::EquipOpen||role==FieldClipRole::EquipClose){auto c=equip_closing_?FieldClipRole::EquipClose:FieldClipRole::EquipOpen;return sample(c,equip_time_);}
 if(role==FieldClipRole::DescriptionOpen||role==FieldClipRole::DescriptionClose){auto c=description_open_?FieldClipRole::DescriptionOpen:FieldClipRole::DescriptionClose;return sample(c,description_time_);}
 if(role==FieldClipRole::PsiOpen||role==FieldClipRole::PsiClose){auto c=psi_closing_?FieldClipRole::PsiClose:FieldClipRole::PsiOpen;return sample(c,psi_time_);}
 return 0;
}
uint32_t FieldEquipmentMenu::cursor_frame()const{if(!data_.valid())return 0;auto i=uint32_t(std::fmod(cursor_time_*data_.parameter(FieldParameter::CursorFps),4.0));return uint32_t(data_.parameter(FieldParameter(uint32_t(FieldParameter::CursorFrame0)+i)));}
bool FieldEquipmentMenu::idle_frame(double dt){if(!data_.valid()||!std::isfinite(dt)||dt<0||dt>1)return fail("Field equipment delta rejected");if(!active())return true;phase_time_+=float(dt);pause_time_+=float(dt);equip_time_+=float(dt);psi_time_+=float(dt);description_time_+=float(dt);cursor_time_+=dt;cursor_move_time_+=float(dt);float t=std::min(cursor_move_time_/data_.parameter(FieldParameter::CursorMoveSeconds),1.f);float ease=1-std::pow(1-t,4.f);cursor_row_=cursor_from_row_+(cursor_target_row_-cursor_from_row_)*ease;cursor_col_=cursor_from_col_+(cursor_target_col_-cursor_from_col_)*ease;
 if(phase_==FieldEquipmentPhase::PauseOpening&&pause_time_>=data_.clip(FieldClipRole::PauseOpen).duration)phase_=FieldEquipmentPhase::Pause;
 if(phase_==FieldEquipmentPhase::EquipOpening&&equip_time_>=data_.clip(FieldClipRole::EquipOpen).duration)phase_=FieldEquipmentPhase::Slots;
 if(phase_==FieldEquipmentPhase::PsiOpening&&psi_time_>=data_.clip(FieldClipRole::PsiOpen).duration)phase_=FieldEquipmentPhase::Psi;
 if(psi_closing_&&psi_time_>=data_.clip(FieldClipRole::PsiClose).duration)psi_closing_=false;
 if(equip_closing_&&equip_time_>=data_.clip(FieldClipRole::EquipClose).duration)equip_closing_=false;
 if(phase_==FieldEquipmentPhase::PauseClosing&&pause_time_>=data_.clip(FieldClipRole::PauseClose).duration){phase_=FieldEquipmentPhase::Closed;equip_closing_=psi_closing_=false;command_=slot_=candidate_=offset_=0;candidates_.clear();}
 return true;
}
}
