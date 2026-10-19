#include "encore/native_session.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
using namespace encore::upstream;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"NativeSession check failed at %d: %s; %s\n",__LINE__,#x,error.c_str());std::exit(1);}}while(0)
static void put(std::vector<uint8_t>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(v>>(8*i));}
static void fix(std::vector<uint8_t>&b){put(b,12,uint32_t(b.size()));put(b,16,encore::crc32(b.data()+24,b.size()-24));}
int main(int argc,char**argv){
 const std::string root=argc>1?argv[1]:"romfs";std::string error;
 NativeSessionData data;RoomData room;HouseData house;BattleRoundData round;ItemData items;InventoryState inventory;
 CHECK(data.load_file((root+"/session/opening.encsession").c_str(),error));CHECK(room.load_file((root+"/world/house/opening.encroom").c_str(),error));CHECK(house.load_file((root+"/world/house/opening.enchouse").c_str(),error));CHECK(round.load_file((root+"/battle/doll/doll-entry.encround").c_str(),error));CHECK(items.load_file((root+"/ui/opening.encitems").c_str(),error));CHECK(inventory.initialize(items.view()));
 auto validate=[&](const SessionSnapshot&s){return validate_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),s,error);};
 auto visited=data.defaults();for(auto&flag:visited.flags)if(flag.id=="visited_podunk")flag.value=true;CHECK(validate(visited));auto flying=data.defaults();for(auto&flag:flying.flags)if(flag.id=="flying_man_in_party")flag.value=true;CHECK(!validate(flying));
 auto cemetery=data.defaults();bool found_cemetery=false;
 for(auto&flag:cemetery.flags)if(flag.id=="cem_roots_grown"){flag.value=true;found_cemetery=true;}
 CHECK(found_cemetery);CHECK(validate(cemetery));
 std::vector<uint8_t>cemetery_bytes;SessionSnapshot cemetery_loaded;
 CHECK(encode_session_save(cemetery,data.compatibility(),cemetery_bytes,error));
 CHECK(decode_session_save(cemetery_bytes.data(),cemetery_bytes.size(),data.compatibility(),cemetery_loaded,error));
 CHECK(validate(cemetery_loaded));bool preserved_cemetery=false;for(const auto&flag:cemetery_loaded.flags)if(flag.id=="cem_roots_grown")preserved_cemetery=flag.value;CHECK(preserved_cemetery);
 CHECK(data.leader_id()=="ninten");CHECK(data.saved_flag_id()=="saved");CHECK(data.earned_cash_flag_id()=="earned_cash");CHECK(data.defaults().flags.size()==178);CHECK(validate(data.defaults()));
 const auto&initial=data.defaults().characters[0];CHECK(initial.nickname=="Ninten");CHECK(initial.permanent_boosts.empty());CHECK(initial.affinity_multipliers.size()==1&&initial.affinity_multipliers[0].id=="asthma"&&initial.affinity_multipliers[0].value==1);CHECK(initial.inventory[0].item_id=="BaseballCap"&&initial.inventory[0].equipped&&initial.inventory[0].doses==1);CHECK(data.defaults().key_items[0].item_id=="CashCard"&&data.defaults().key_items[0].uid!=initial.inventory[0].uid);
 // Additive startup roster never broadens the playable party. Old singleton
 // defaults are retained byte-for-byte; inactive values are frozen source data.
 CHECK(data.defaults().characters.size()==1);CHECK(data.startup_characters().size()==5);
 auto startup=data.defaults();startup.characters=data.startup_characters();CHECK(validate(startup));
 CHECK(startup.characters[1].character_id=="ana"&&startup.characters[1].hp==42&&startup.characters[1].pp==31);
 CHECK(startup.characters[2].character_id=="lloyd"&&startup.characters[2].hp==82&&startup.characters[2].pp==0&&startup.characters[2].learned_skills==std::vector<std::string>{"spy"});
 CHECK(startup.characters[3].character_id=="pippi"&&startup.characters[3].hp==83);CHECK(startup.characters[4].character_id=="teddy"&&startup.characters[4].hp==83);
 for(const auto&character:startup.characters)CHECK(character.nickname.empty());
 CHECK(startup.characters[2].inventory.size()==2&&startup.characters[2].inventory[0].item_id=="KickMeNote"&&startup.characters[2].inventory[1].item_id=="GlassesCelluloid");
 startup.characters[1].nickname="Ann";startup.characters[2].nickname="Lou";startup.characters[3].nickname="Pip";startup.characters[4].nickname="Ted";
 startup.characters[2].inventory[0].uid=4094;startup.characters[2].inventory[1].uid=4095;CHECK(validate(startup));
 const std::vector<std::function<void(SessionSnapshot&)>>bad_startup={
 [](auto&s){s.characters.pop_back();},[](auto&s){s.characters.erase(s.characters.begin()+1);},[](auto&s){std::swap(s.characters[3],s.characters[4]);},[](auto&s){s.party.push_back(s.characters[1].character_id);},[](auto&s){s.party[0]=s.characters[1].character_id;},[](auto&s){s.characters[1].level++;},[](auto&s){s.characters[1].experience++;},[](auto&s){s.characters[1].hp--;},[](auto&s){s.characters[1].pp--;},[](auto&s){s.characters[1].nickname=std::string(256,'x');},[](auto&s){s.characters[1].permanent_boosts.push_back({"maxhp",1});},[](auto&s){s.characters[1].affinity_multipliers.push_back({"asthma",1});},[](auto&s){s.characters[1].status.push_back({"asthma",0});},[](auto&s){s.characters[2].learned_skills.clear();},[](auto&s){s.characters[2].inventory[0].equipped=false;},[](auto&s){s.characters[2].inventory[0].doses++;},[](auto&s){s.characters[2].inventory[0].item_id="unknown";},[](auto&s){std::swap(s.characters[2].inventory[0],s.characters[2].inventory[1]);},[](auto&s){s.characters[2].inventory[0].uid=s.key_items[0].uid;},[](auto&s){s.characters.push_back(s.characters.back());}
 };
 for(const auto&edit:bad_startup){auto bad=startup;edit(bad);CHECK(!validate(bad));}
 StorageState storage;CHECK(storage.initialize(items.view(),data.storage_capacity()));NativeSnapshotInput input;input.state=data.defaults();input.inventory=&inventory;input.storage=&storage;BattleSessionStats live;input.stats=&live;
 const auto&row=data.levels().back();live.level=row.level;live.experience=11;live.hp=19;live.pp=row.stats[1];live.maxhp=row.stats[0];live.maxpp=row.stats[1];live.offense=row.stats[2];live.defense=row.stats[3];live.speed=row.stats[4];live.iq=row.stats[5];live.guts=row.stats[6];live.bank=15;live.cash=0;live.earned_cash=15;live.learned_skills={std::string(round.view().string(round.view().encounter().learned_skill))};
 input.state.position_x=148;input.state.position_y=704;input.state.direction_x=0;input.state.direction_y=-1;input.state.saved_at="2026-10-02T08:00:00Z";input.state.playtime_seconds=450;
 for(auto&flag:input.state.flags)if(flag.id=="mimmie_door_opened"||flag.id=="poltergeist"||flag.id=="doll_defeated"||flag.id=="doll_melody"||flag.id=="phone_ring"||flag.id=="talked_to_dad"||flag.id==data.earned_cash_flag_id())flag.value=true;
 for(uint32_t i=0;i<room.view().battle_count();++i)input.state.encountered.push_back({std::string(room.view().string(room.view().battle(i).enemy_string)),true});
 input.state.seen_dialogue_flags.push_back({std::string(house.view().string(house.view().npc(0).seen_key)),true});
 SessionSnapshot output;auto build=[&](){return build_native_session_snapshot(data,room.view(),house.view(),round.view(),items.view(),input,output,error);};CHECK(build());CHECK(output.characters[0].level==2&&output.characters[0].experience==11&&output.characters[0].hp==19&&output.characters[0].pp==27);CHECK(output.characters[0].learned_skills==row.skills);CHECK(output.characters[0].permanent_boosts.empty());CHECK(output.bank==15&&output.cash==0&&output.earned_cash==15);CHECK(validate(output));
 auto flag_value=[&](const SessionSnapshot&s,const std::string&id){for(auto&f:s.flags)if(f.id==id)return f.value;return false;};CHECK(!flag_value(output,data.saved_flag_id()));CHECK(!flag_value(input.state,data.saved_flag_id()));
 // First disk write preserves false; successful main-layer commit changes live
 // flag afterwards. A second snapshot captures true without codec mutation.
 std::vector<uint8_t>encoded;CHECK(encode_session_save(output,data.compatibility(),encoded,error));SessionSnapshot restored;CHECK(decode_session_save(encoded.data(),encoded.size(),data.compatibility(),restored,error));CHECK(validate(restored));CHECK(!flag_value(restored,data.saved_flag_id()));
 for(auto&f:input.state.flags)if(f.id==data.saved_flag_id())f.value=true;
 CHECK(build());CHECK(flag_value(output,data.saved_flag_id()));
 live.earned_cash=0;for(auto&f:input.state.flags)if(f.id==data.earned_cash_flag_id())f.value=false;CHECK(build());CHECK(output.bank==15&&output.earned_cash==0&&!flag_value(output,data.earned_cash_flag_id()));
 const auto good=output;
 // Mom room and basement are now source-connected native routes.
 auto mom=good;mom.position_x=496;mom.position_y=96;CHECK(validate(mom));
 std::vector<std::function<void(SessionSnapshot&)>>bad={
 [](auto&s){s.scene_id="unknown";},[](auto&s){s.scene_label="unknown";},[](auto&s){s.source_version="unknown";},[](auto&s){s.party[0]="nickname";},[](auto&s){s.characters[0].character_id="unknown";},[](auto&s){s.characters.push_back(s.characters[0]);},[](auto&s){s.characters[0].permanent_boosts.push_back({"defense",5});},[](auto&s){s.characters[0].status.push_back({"unknown",0});},[](auto&s){s.characters[0].affinity_multipliers[0].value=2;},[](auto&s){s.characters[0].level=3;},[](auto&s){s.characters[0].experience=27;},[](auto&s){s.characters[0].hp=66;},[](auto&s){s.characters[0].pp=28;},[](auto&s){s.characters[0].learned_skills.push_back("unknown");},[](auto&s){s.characters[0].inventory[0].item_id="unknown";},[](auto&s){s.characters[0].inventory[0].doses=2;},[](auto&s){s.key_items[0].uid=s.characters[0].inventory[0].uid;},[](auto&s){s.key_items[0].item_id="unknown";},[](auto&s){s.storage=s.key_items;},[](auto&s){s.keys[0].value=1;},[](auto&s){s.rare_drops.push_back({"unknown",1});},[](auto&s){s.object_flags.push_back({"unknown",true});},[](auto&s){s.flags.pop_back();},[](auto&s){s.flags[0].id="unknown";},[](auto&s){s.seen_dialogue_flags.push_back({"unknown",true});},[](auto&s){s.encountered.push_back({"unknown",true});},[](auto&s){s.position_x=1e6;},[](auto&s){s.position_x=96;s.position_y=2000;},[](auto&s){s.direction_x=.2;s.direction_y=.3;},[](auto&s){s.settings.text_speed=.1;},[](auto&s){s.settings.menu_flavor="unknown";},[](auto&s){s.settings.button_prompts="unknown";},[](auto&s){s.run_sound="unknown";},[](auto&s){s.shadow_effect="unknown";}
 };
 for(const auto&edit:bad){auto s=good;edit(s);CHECK(!validate(s));}
 // Registered but unsupported story state must not become a valid save merely
 // because its name exists in the complete source registry.
 auto unrelated=good;bool changed=false;for(auto&f:unrelated.flags)if(std::find(data.mutable_flags().begin(),data.mutable_flags().end(),f.id)==data.mutable_flags().end()&&!f.value){f.value=true;changed=true;break;}CHECK(changed);CHECK(!validate(unrelated));
 live.defense+=5;CHECK(!build());CHECK(output.characters[0].hp==good.characters[0].hp&&output.saved_at==good.saved_at);live.defense-=5;live.learned_skills.push_back("unknown");CHECK(!build());live.learned_skills.pop_back();input.state.characters[0].inventory[0].item_id="unknown";CHECK(!build());input.state.characters[0].inventory=initial.inventory;input.state.characters[0].learned_skills.push_back("unknown");CHECK(!build());input.state.characters[0].learned_skills=initial.learned_skills;CHECK(build());
 // Live battle snapshots overlay only Ninten, retaining the named inactive
 // records and opaque source-allocated IDs exactly, then round-trip in memory.
 input.state.characters=startup.characters;CHECK(build());CHECK(output.characters.size()==5);
 auto inactive_bytes=[&](const SessionSnapshot&state){auto frozen=data.defaults();frozen.characters=state.characters;frozen.characters[0]=data.defaults().characters[0];std::vector<uint8_t>result;CHECK(encode_session_save(frozen,data.compatibility(),result,error));return result;};
 CHECK(inactive_bytes(output)==inactive_bytes(input.state));
 CHECK(encode_session_save(output,data.compatibility(),encoded,error));CHECK(decode_session_save(encoded.data(),encoded.size(),data.compatibility(),restored,error));CHECK(validate(restored));CHECK(inactive_bytes(restored)==inactive_bytes(output));
 auto previous=output;input.state.characters[1].hp++;CHECK(!build());CHECK(inactive_bytes(output)==inactive_bytes(previous));input.state.characters[1].hp--;
 std::vector<uint8_t>bytes;CHECK(encore::read_file((root+"/session/opening.encsession").c_str(),bytes,1024*1024,error));
 for(size_t n=0;n<bytes.size();++n)CHECK(!data.load(bytes.data(),n,error));
 CHECK(data.valid()&&data.leader_id()=="ninten");
 for(size_t i=0;i<bytes.size();++i){auto mutation=bytes;mutation[i]^=1;CHECK(!data.load(mutation.data(),mutation.size(),error));}
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{8,12},{20,12},{8,1},{20,1},{24,0},{36,0xffffffff}}){auto mutation=bytes;put(mutation,entry.first,entry.second);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 CHECK(data.field_encounters()==std::vector<std::string>{"straydog"});
 // v11 keeps its run-sound append and the old v10 reader. The cemetery
 // content now adds one mutable flag; defaults/save encoding remain frozen.
 CHECK(data.run_sounds()==std::vector<std::string>({"wood","stone"}));CHECK(data.supports_run_sound("wood")&&data.supports_run_sound("stone")&&!data.supports_run_sound("unknown"));
 {auto stone=data.defaults();stone.run_sound="stone";CHECK(validate(stone));std::vector<uint8_t>saved;CHECK(encode_session_save(stone,data.compatibility(),saved,error));SessionSnapshot decoded;CHECK(decode_session_save(saved.data(),saved.size(),data.compatibility(),decoded,error));CHECK(decoded.run_sound=="stone"&&validate(decoded));}
 std::vector<uint8_t>v10_bytes;CHECK(encore::read_file("tests/fixtures/session-v10.encsession",v10_bytes,1024*1024,error));const size_t run_at=bytes.size()-21;CHECK(run_at>v10_bytes.size());
 auto current_v10=bytes;current_v10.resize(run_at);put(current_v10,8,10);put(current_v10,20,10);fix(current_v10);NativeSessionData current_v10_data;
 CHECK(current_v10_data.load(current_v10.data(),current_v10.size(),error));CHECK(current_v10_data.mutable_flags()==data.mutable_flags());
 CHECK(validate_native_session_snapshot(current_v10_data,room.view(),house.view(),round.view(),items.view(),cemetery,error));
 std::vector<uint8_t>prior_defaults,cemetery_current_defaults;NativeSessionData baseline_v10;CHECK(baseline_v10.load(v10_bytes.data(),v10_bytes.size(),error));
 CHECK(encode_session_save(baseline_v10.defaults(),baseline_v10.compatibility(),prior_defaults,error));CHECK(encode_session_save(data.defaults(),data.compatibility(),cemetery_current_defaults,error));CHECK(prior_defaults==cemetery_current_defaults);
 CHECK(!validate_native_session_snapshot(baseline_v10,room.view(),house.view(),round.view(),items.view(),cemetery,error));
 NativeSessionData v10;CHECK(v10.load(v10_bytes.data(),v10_bytes.size(),error));CHECK(v10.run_sounds()==std::vector<std::string>{v10.defaults().run_sound});CHECK(!v10.supports_run_sound("stone"));{auto stone=v10.defaults();stone.run_sound="stone";CHECK(!validate_native_session_snapshot(v10,room.view(),house.view(),round.view(),items.view(),stone,error));}
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{run_at,0},{run_at,1},{run_at,17},{run_at+4,0},{run_at+4,33},{8,10},{20,10}}){auto mutation=bytes;put(mutation,entry.first,entry.second);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));CHECK(data.supports_run_sound("stone"));}
 {auto downgrade=bytes;put(downgrade,8,10);put(downgrade,20,10);fix(downgrade);CHECK(!data.load(downgrade.data(),downgrade.size(),error));}
 {auto missing=v10_bytes;put(missing,8,11);put(missing,20,11);fix(missing);CHECK(!data.load(missing.data(),missing.size(),error));}
 {auto missing_default=bytes;missing_default[run_at+8]='g';fix(missing_default);CHECK(!data.load(missing_default.data(),missing_default.size(),error));}
 for(char c:{'W','/','!'}){auto mutation=bytes;mutation[run_at+8]=uint8_t(c);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 {auto mutation=bytes;mutation.resize(run_at);const auto append=[&](uint32_t n){size_t at=mutation.size();mutation.resize(at+4);put(mutation,at,n);};append(2);append(4);mutation.insert(mutation.end(),{'w','o','o','d'});append(4);mutation.insert(mutation.end(),{'w','o','o','d'});fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 // v10 appends encounter admission after the complete mainline v9 prefix.
 {auto legacy=v10_bytes;legacy.resize(legacy.size()-16);put(legacy,8,9);put(legacy,20,9);fix(legacy);NativeSessionData v9;CHECK(v9.load(legacy.data(),legacy.size(),error));CHECK(v9.field_encounters().empty());CHECK(v9.field_items().size()==data.field_items().size());CHECK(v9.supports_settings(data.defaults().settings));}
 const size_t frozen_field_count=v10_bytes.size()-16;CHECK(std::string(v10_bytes.begin()+frozen_field_count+8,v10_bytes.end())=="straydog");const size_t field_count=run_at-16;CHECK(std::string(bytes.begin()+field_count+8,bytes.begin()+run_at)=="straydog");
 for(auto entry:std::vector<std::pair<size_t,uint32_t>>{{field_count,0},{field_count,65},{field_count+4,0},{field_count+4,65}}){auto mutation=bytes;put(mutation,entry.first,entry.second);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 {auto mutation=bytes;mutation[field_count+8]='/';fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 {auto mutation=bytes;const std::vector<uint8_t>duplicate(mutation.begin()+field_count+4,mutation.begin()+run_at);mutation.insert(mutation.begin()+run_at,duplicate.begin(),duplicate.end());put(mutation,field_count,2);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 {auto mutation=bytes;put(mutation,8,9);put(mutation,20,9);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));}
 {auto snapshot=good;snapshot.encountered.push_back({"straydog",true});CHECK(validate(snapshot));snapshot.encountered.push_back({"invented_enemy",true});CHECK(!validate(snapshot));}
 auto trailing=bytes;trailing.push_back(0);fix(trailing);CHECK(!data.load(trailing.data(),trailing.size(),error));CHECK(data.load(bytes.data(),bytes.size(),error));CHECK(validate(good));
 // A schema1/capability1 resource still loads without injecting the new
 // startup roster, as required by the archived rules6 migration resource.
 // Genuine pre-extension v7 resource: new object acquisitions/skills must never be
 // reinterpreted as old fields by truncating today's resource.
 std::vector<uint8_t>historical_bytes;CHECK(encore::read_file("tests/fixtures/pre-basement-session.encsession",historical_bytes,1024*1024,error));
 NativeSessionData historical;CHECK(historical.load(historical_bytes.data(),historical_bytes.size(),error));
 auto template_state=historical.defaults();template_state.characters=historical.startup_characters();std::vector<uint8_t>startup_bytes;CHECK(encode_session_save(template_state,historical.compatibility(),startup_bytes,error));
 size_t settings_bytes=4+8*historical.text_speeds().size()+4+4;for(const auto&v:historical.menu_flavors())settings_bytes+=4+v.size();for(const auto&v:historical.button_prompts())settings_bytes+=4+v.size();
 size_t acquisition_bytes=4;for(const auto&policy:historical.acquisitions())acquisition_bytes+=4+policy.item_id.size()+8+4+policy.flag_id.size();
 size_t storage_bytes=8;for(const auto&p:historical.storage_policies())storage_bytes+=4+p.item_id.size()+12+28;
 size_t key_bytes=4;for(const auto&k:historical.key_acquisitions())key_bytes+=4+k.item_id.size()+4+4+k.flag_id.size()+4+k.consumed_flag_id.size()+4;
 auto v5_resource=historical_bytes;v5_resource.resize(historical_bytes.size()-key_bytes);put(v5_resource,8,5);put(v5_resource,20,5);fix(v5_resource);NativeSessionData v5;CHECK(v5.load(v5_resource.data(),v5_resource.size(),error));CHECK(v5.key_acquisitions().empty());
 auto v6_resource=v5_resource;
 auto append32=[&](uint32_t value){const auto at=v6_resource.size();v6_resource.resize(at+4);put(v6_resource,at,value);};
 auto append_text=[&](const std::string&value){append32(uint32_t(value.size()));v6_resource.insert(v6_resource.end(),value.begin(),value.end());};
 append32(uint32_t(std::count_if(historical.key_acquisitions().begin(),historical.key_acquisitions().end(),[](const auto&k){return k.flag_policy==1;})));
 for(const auto&k:historical.key_acquisitions())if(k.flag_policy==1){append_text(k.item_id);append32(k.doses);append_text(k.flag_id);append_text(k.consumed_flag_id);}
 put(v6_resource,8,6);put(v6_resource,20,6);fix(v6_resource);NativeSessionData v6;
 CHECK(v6.load(v6_resource.data(),v6_resource.size(),error));CHECK(v6.key_acquisitions().size()==2);
 for(const auto&k:v6.key_acquisitions())CHECK(k.flag_policy==1);
 auto v4_resource=v5_resource;v4_resource.resize(v5_resource.size()-storage_bytes);put(v4_resource,8,4);put(v4_resource,20,4);fix(v4_resource);NativeSessionData v4;CHECK(v4.load(v4_resource.data(),v4_resource.size(),error));CHECK(v4.storage_capacity()==0);
 auto v3_resource=v4_resource;v3_resource.resize(v4_resource.size()-acquisition_bytes);put(v3_resource,8,3);put(v3_resource,20,3);fix(v3_resource);NativeSessionData v3;CHECK(v3.load(v3_resource.data(),v3_resource.size(),error));CHECK(v3.acquisitions().empty());
 auto v2_resource=v3_resource;v2_resource.resize(v2_resource.size()-settings_bytes);put(v2_resource,8,2);put(v2_resource,20,2);fix(v2_resource);NativeSessionData v2;CHECK(v2.load(v2_resource.data(),v2_resource.size(),error));CHECK(v2.acquisitions().empty());
 auto legacy_resource=v4_resource;legacy_resource.resize(legacy_resource.size()-acquisition_bytes-settings_bytes-startup_bytes.size()-4);put(legacy_resource,8,1);put(legacy_resource,20,1);fix(legacy_resource);
 NativeSessionData legacy;CHECK(legacy.load(legacy_resource.data(),legacy_resource.size(),error));CHECK(legacy.startup_characters().size()==1);CHECK(legacy.defaults().characters.size()==1);
 std::vector<uint8_t>current_defaults,legacy_defaults;CHECK(encode_session_save(historical.defaults(),historical.compatibility(),current_defaults,error));CHECK(encode_session_save(legacy.defaults(),legacy.compatibility(),legacy_defaults,error));CHECK(current_defaults==legacy_defaults);
 CHECK(validate_native_session_snapshot(legacy,room.view(),house.view(),round.view(),items.view(),good,error));CHECK(!validate_native_session_snapshot(legacy,room.view(),house.view(),round.view(),items.view(),startup,error));
 CHECK(data.acquisitions().size()==3);const auto&policy=data.acquisitions()[0];CHECK(policy.max_count==1);
 CHECK(data.key_acquisitions().size()==4);const auto&key_policy=data.key_acquisitions()[0];CHECK(key_policy.item_id=="DogTreats"&&key_policy.flag_id=="got_dog_treats"&&key_policy.consumed_flag_id=="gave_treats"&&key_policy.doses==1);
 auto acquired=good;uint32_t uid=UINT32_MAX;auto used=[&](uint32_t value){for(const auto&c:acquired.characters)for(const auto&item:c.inventory)if(item.uid==value)return true;for(const auto&item:acquired.key_items)if(item.uid==value)return true;return false;};while(used(uid))--uid;
 {auto held=good;uint32_t key_uid=uid-1;while(used(key_uid))--key_uid;held.key_items.push_back({key_policy.item_id,false,key_policy.doses,key_uid});for(auto&f:held.flags)if(f.id==key_policy.flag_id)f.value=true;CHECK(validate(held));
  {auto s=held;s.key_items.pop_back();CHECK(!validate(s));}{auto s=held;for(auto&f:s.flags)if(f.id==key_policy.flag_id)f.value=false;CHECK(!validate(s));}
  {auto s=held;s.key_items.back().item_id="unknown";CHECK(!validate(s));}{auto s=held;s.key_items.back().doses++;CHECK(!validate(s));}
  {auto consumed=held;for(auto&f:consumed.flags)if(f.id==key_policy.consumed_flag_id)f.value=true;CHECK(!validate(consumed));consumed.key_items.erase(std::remove_if(consumed.key_items.begin(),consumed.key_items.end(),[&](const SessionItem&i){return i.item_id==key_policy.item_id;}),consumed.key_items.end());CHECK(validate(consumed));}}
 {const auto basement=*std::find_if(data.key_acquisitions().begin(),data.key_acquisitions().end(),[](const auto&k){return k.item_id=="KeyBasement";});CHECK(basement.item_id=="KeyBasement"&&basement.flag_id=="mick_telepathy"&&basement.consumed_flag_id.empty()&&basement.doses==1);
  auto held=good;held.key_items.push_back({basement.item_id,false,basement.doses,uid});CHECK(!validate(held));for(auto&f:held.flags)if(f.id==basement.flag_id)f.value=true;CHECK(validate(held));
  for(const auto&door:std::vector<HouseOpenableDoor>{house.view().openable_door(2)})for(auto&f:held.flags)if(f.id==house.view().string(door.flag))f.value=true;
   held.position_x=88;held.position_y=977;CHECK(validate(held));CHECK(encode_session_save(held,data.compatibility(),encoded,error));CHECK(decode_session_save(encoded.data(),encoded.size(),data.compatibility(),restored,error));CHECK(validate(restored)&&restored.key_items.back().uid==uid);
   for(const auto&f:restored.flags)if(f.id==house.view().string(house.view().openable_door(2).flag))CHECK(f.value);
  held.key_items.pop_back();CHECK(!validate(held));}
 acquired.characters[0].inventory.push_back({policy.item_id,false,policy.doses,uid});for(auto&f:acquired.flags)if(f.id==policy.flag_id)f.value=true;CHECK(validate(acquired));
 CHECK(encode_session_save(acquired,data.compatibility(),encoded,error));CHECK(decode_session_save(encoded.data(),encoded.size(),data.compatibility(),restored,error));CHECK(validate(restored));CHECK(restored.characters[0].inventory.back().uid==uid);
 CHECK(!validate_native_session_snapshot(v3,room.view(),house.view(),round.view(),items.view(),acquired,error));
 {auto s=acquired;s.characters[0].inventory.back().doses++;CHECK(!validate(s));}
 {auto s=acquired;s.characters[0].inventory.back().equipped=true;CHECK(!validate(s));}
 {auto s=acquired;s.characters[0].inventory.back().item_id="unknown";CHECK(!validate(s));}
 {auto s=acquired;s.characters[0].inventory.back().uid=s.characters[0].inventory.front().uid;CHECK(!validate(s));}
 {auto s=acquired;auto duplicate=s.characters[0].inventory.back();duplicate.uid=uid-1;s.characters[0].inventory.push_back(duplicate);CHECK(!validate(s));}
 {auto s=acquired;for(auto&f:s.flags)if(f.id==policy.flag_id)f.value=false;CHECK(!validate(s));}
 // Source storage transfers can reorder inventory; stable UIDs identify items.
  {auto s=acquired;std::swap(s.characters[0].inventory.front(),s.characters[0].inventory.back());CHECK(validate(s));CHECK(encode_session_save(s,data.compatibility(),encoded,error));CHECK(decode_session_save(encoded.data(),encoded.size(),data.compatibility(),restored,error));CHECK(validate(restored));CHECK(restored.characters[0].inventory.front().uid==s.characters[0].inventory.front().uid);}
 uint32_t definition=UINT32_MAX;for(uint32_t i=0;i<items.view().count(ItemSection::Definitions);++i)if(items.view().string(items.view().definition(i).source)==policy.item_id)definition=i;CHECK(definition!=UINT32_MAX);
 InventoryState acquired_inventory;CHECK(acquired_inventory.initialize(items.view()));CHECK(acquired_inventory.append(definition,policy.doses,uid,error));input.state=good;for(auto&f:input.state.flags)if(f.id==policy.flag_id)f.value=true;input.inventory=&acquired_inventory;CHECK(build());CHECK(output.characters[0].inventory.back().uid==uid&&output.characters[0].inventory.back().doses==policy.doses);
 for(auto&f:input.state.flags)if(f.id==policy.flag_id)f.value=false;const auto previous_output=output;CHECK(!build());CHECK(output.characters[0].inventory.back().uid==previous_output.characters[0].inventory.back().uid);
 const auto needle=policy.item_id;const auto found=std::search(bytes.begin(),bytes.end(),needle.begin(),needle.end());CHECK(found!=bytes.end());const size_t policy_at=size_t(found-bytes.begin())-8;for(auto change:std::vector<std::pair<size_t,uint32_t>>{{policy_at,17},{policy_at+8+policy.item_id.size(),0},{policy_at+12+policy.item_id.size(),2}}){auto mutation=bytes;put(mutation,change.first,change.second);fix(mutation);CHECK(!data.load(mutation.data(),mutation.size(),error));CHECK(data.acquisitions()[0].item_id==policy.item_id);}
 std::printf("NativeSession: %u checks; source defaults, actual level2 stats, stable identities, complete snapshot and fail-closed scope\n",checks);
}
