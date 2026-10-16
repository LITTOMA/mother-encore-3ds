// Platform effects for checked field story and ordinary battle loot. Game
// identities, probabilities, timing and dialogue remain in independent packs.
class FieldBattleLoot final:public upstream::BattleLootHost {
 uint32_t definition(std::string_view name)const{const auto items=items_data.view();for(uint32_t i=0;i<items.count(upstream::ItemSection::Definitions);++i)if(items.string(items.definition(i).source)==name)return i;return UINT32_MAX;}
public:
 bool validate_sound(std::string_view source,std::string& error)const override{
  unsigned found=0;for(uint32_t i=0;i<menu_audio_bank.count();++i){auto path=menu_audio_bank.asset(i).source_path;if(path.substr(0,6)=="res://")path.remove_prefix(6);found+=path==source;}if(found!=1){error="Field battle sound missing/ambiguous";return false;}error.clear();return true;
 }
 bool can_receive(std::string_view item,uint32_t doses,std::string& error)const override{
  const auto index=definition(item);if(index==UINT32_MAX){error="Battle loot definition is not admitted";return false;}
  bool admitted=false;for(const auto&p:native_session_data.acquisitions())admitted|=p.item_id==item&&p.doses==doses&&p.max_count==1;
  if(!admitted){error="Battle loot acquisition policy is absent";return false;}return session_inventory.can_append(index,doses,error);
 }
 bool reserve(std::string_view item,uint32_t doses,upstream::SourceRandom& random,uint32_t& uid,std::string& error)override{
  if(!can_receive(item,doses,error))return false;auto candidate=random;auto ledger=generated_uid_ledger;
  const upstream::LoadRngClockProvider clock=[](upstream::LoadRngClockSample&sample,std::string&why){const auto now=std::time(nullptr);if(now<0){why="Clock unavailable for source item entropy";return false;}const u64 ticks=svcGetSystemTick()-load_epoch_tick;sample.unix_seconds=uint64_t(now);sample.ticks_usec=(ticks/SYSCLOCK_ARM11)*1000000+(ticks%SYSCLOCK_ARM11)*1000000/SYSCLOCK_ARM11;return true;};
  std::vector<upstream::LoadUidAllocation>trace;if(!upstream::apply_load_uid_allocations(candidate,ledger,{{1,1}},clock,error,&trace)||trace.size()!=1)return false;
  auto inventory=session_inventory;if(!inventory.append(definition(item),doses,trace.front().generated_uid,error))return false;
  uid=trace.front().generated_uid;random=candidate;generated_uid_ledger=std::move(ledger);return true;
 }
 bool grant(std::string_view item,uint32_t doses,uint32_t uid,std::string& error)override{return can_receive(item,doses,error)&&session_inventory.append(definition(item),doses,uid,error);}
 bool play_sound(std::string_view source,std::string& error)override{if(!validate_sound(source,error))return false;if(!play_source_audio(std::string(source),ctr::AudioLane::Effect,0,1)){error=audio_status;return false;}error.clear();return true;}
}field_battle_loot;
bool field_basic_action(upstream::RoundView round,upstream::FieldStoryView story,upstream::FieldStoryEncounter binding,std::string_view& source,std::string& error){
 source=story.text(binding.basic);unsigned weapons=0;
 for(const auto& item:session_inventory.instances())if(item.equipped){const auto definition=items_data.view().definition(item.definition);if(definition.equipment_slot!=binding.basic_slot)continue;
  const auto action=story.basic_override(definition.id);if(++weapons>1||!action.definition){error="Equipped basic action is not admitted";return false;}source=story.text(action.source);}
 unsigned matches=0;for(uint32_t i=0;i<round.count(upstream::RoundSection::Skills);++i){const auto skill=round.skill(i);if(round.string(skill.source)==source&&skill.skill_type==uint32_t(upstream::RoundSkillType::Basic)&&skill.target_type==uint32_t(upstream::RoundTargetType::Enemy)&&!skill.traits&&round.action_media(i).preparation!=UINT32_MAX)++matches;}
 if(matches!=1){error="Field basic action is missing/ambiguous";return false;}error.clear();return true;
}
class FieldStoryEffects final:public upstream::FieldStoryHost {
public:
 bool can_start(upstream::FieldStoryView story,uint32_t programme,std::string& error)override{
  const auto room=field_room_data.view();if(programme>=room.program_count()){error="Field programme is not admitted";return false;}const auto program=room.program(programme);
  for(uint32_t i=0;i<program.command_count;++i){const auto command=room.command(program.first_command+i);if(command.opcode==uint16_t(upstream::DialogueActionKind::ShowDialogue)&&!story.dialogue(command.target_index,locale_selection.code()).id){error="Field programme locale is not admitted";return false;}}
  const auto binding=story.encounter_for_programme(programme);if(!binding.battle){error.clear();return true;}
  uint32_t index=UINT32_MAX;for(uint32_t i=0;i<room.battle_count();++i)if(room.battle(i).stable_id==binding.battle)index=i;
  if(index==UINT32_MAX||!session_rewards_valid){error="Field battle session is unavailable";return false;}
  const auto battle=room.battle(index);const std::string entry="romfs:/"+std::string(room.string(room.resource(battle.battle_resource_index).path_string));
  const auto path=companion_path(entry);upstream::BattleRoundData data;if(path.empty()||!data.load_file(path.c_str(),error))return false;const auto round=data.view();
  if(round.binding().battle_id!=binding.battle||round.string(round.skill(round.binding().basic_skill).source)!=story.text(binding.basic)){error="Field battle action binding differs";return false;}
  std::string_view basic;if(!field_basic_action(round,story,binding,basic,error))return false;
  if(!upstream::BattleOutcome::preflight(round,room,&session_rewards,error))return false;
  const auto loot=round.loot();return field_battle_loot.validate_sound(round.string(loot.sound),error)&&field_battle_loot.can_receive(round.string(loot.item),loot.doses,error);
 }
}field_story_host;
bool prepare_field_story_audio(std::string& error){
 const auto room=field_room_data.view();const auto story=field_story_data.view();std::set<std::string>sources;
 for(uint32_t i=0;i<room.resource_count();++i)if(room.resource(i).kind==2)sources.emplace(room.string(room.resource(i).path_string));
 for(uint32_t i=0;i<story.text_count();++i){const auto text=story.text_record(i);if(text.voice.length)sources.emplace(story.text(text.voice));}
 for(uint32_t i=0;i<room.battle_count();++i){const auto battle=room.battle(i);const auto binding=story.encounter(battle.stable_id);if(binding.battle){sources.emplace(story.text(binding.music));const auto path=companion_path("romfs:/"+std::string(room.string(room.resource(battle.battle_resource_index).path_string)));upstream::BattleRoundData round;if(path.empty()||!round.load_file(path.c_str(),error))return false;if(round.view().version()>=6)sources.emplace(round.view().string(round.view().loot().sound));}}
 for(const auto& source:sources)if(!prepare_source_audio(source,error))return false;error.clear();return true;
}
bool advance_field_story(double delta,bool accept,bool cancel,std::string& error){
 auto&world=field_scene->world;auto&box=gameplay_scene->presentation;const auto story=field_scene->story();const auto id=world.pending_dialogue_id();bool fresh=false;
 if(id!=UINT32_MAX&&id!=field_story_text){const auto text=story.dialogue(id,locale_selection.code());if(!text.id){error="Field story locale/text is missing";return false;}if(!box.present_plain(story.text(text.speaker),story.text(text.body),story.text(text.voice))){error=box.error();return false;}field_story_text=id;fresh=true;}
 if(id!=UINT32_MAX&&!fresh&&!box.dialogue_closing()){
  if(!box.dialogue_finished())box.input(accept,cancel);
  else if(world.story_input_allowed()){box.input(accept,cancel,true,true);if(box.take_dialogue_advance()&&!world.finish_story_dialogue()){error="Field story continuation rejected";return false;}}
 }
 for(bool sound:world.take_story_hides()){if(!sound){error="Silent field dialogue close is not admitted";return false;}if(box.dialogue_active()&&!box.dialogue_closing())box.close_story_dialogue();}
 if(world.stage()==upstream::OpeningStage::Walking){field_story_text=UINT32_MAX;if(box.dialogue_active()&&!box.dialogue_closing())box.close_story_dialogue();}
 if(box.dialogue_active()&&!box.advance_chrome(delta)){error=box.error();return false;}error.clear();return true;
}
