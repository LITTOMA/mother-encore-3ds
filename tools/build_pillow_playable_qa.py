#!/usr/bin/env python3
"""Build a separate rendered Pillow/Minnie diagnostic from combined ARM objects."""
from pathlib import Path
import argparse,difflib,hashlib,importlib.util,json,os,shlex,subprocess

DRIVER=r'''
unsigned pillow_qa_step=0,pillow_qa_frame=0,pillow_qa_selected=0;
bool pillow_qa_success=false,pillow_qa_failed=false;
u64 pillow_qa_started=0;
void pillow_qa_note(const char* event){
    if(!qa_log)return;
    const auto p=gameplay_scene->world.player();const auto m=gameplay_scene->presentation.npc_pose(3);
    std::fprintf(qa_log,"PILLOW_QA event=%s step=%u frame=%u pos=%.3f,%.3f minnie=%.3f,%.3f world=%u house=%u program=%u phase=%u flags=%u,%u,%u,%u,%u exp=%u bank=%u gpu_ready=%u gpu_active=%u certificate=%u\n",event,pillow_qa_step,pillow_qa_frame,double(p.position.x),double(p.position.y),double(m.position.x),double(m.position.y),unsigned(gameplay_scene->world.stage()),unsigned(gameplay_scene->house.phase()),gameplay_scene->world.story_program_index(),gameplay_scene->world.phrase(),unsigned(gameplay_scene->world.story_flag("pillow_attack")),unsigned(gameplay_scene->world.story_flag("minnie_leave")),unsigned(gameplay_scene->world.story_flag("minnie_door")),unsigned(gameplay_scene->world.story_flag("mimmie_door_opened")),unsigned(gameplay_scene->world.story_flag("doll_melody")),session_rewards.experience,session_rewards.bank,unsigned(battle_renderer.gpu_background_ready()),unsigned(battle_renderer.gpu_background_active()),unsigned(battle_renderer.gpu_certificate_used()));std::fflush(qa_log);
}
bool pillow_qa_input(u32& down,u32& held){
    down=held=0;++pillow_qa_frame;
    auto& world=gameplay_scene->world;const auto p=world.player();
    const bool walking=world.stage()==upstream::OpeningStage::Walking&&!world.has_cutscene_actors()&&!gameplay_scene->presentation.dialogue_active()&&gameplay_scene->house.phase()==upstream::HousePhase::Idle;
    if(!world.healthy()||!house_error.empty()||!round_error.empty()||osGetTime()-pillow_qa_started>600000){pillow_qa_failed=true;pillow_qa_note("FAIL_ERROR_OR_TIMEOUT");return false;}
    if(pillow_qa_frame%120==0)pillow_qa_note("progress");
    if((in_battle()||gameplay_scene->presentation.dialogue_active())&&pillow_qa_frame%20==0)down=KEY_A;
    if(dialogue_choices.active()&&pillow_qa_frame%20==0){down=KEY_A;pillow_qa_note("tutorial_yes_input");}
    switch(pillow_qa_step){
    case 0:
        held=KEY_DUP;
        if(in_battle()){pillow_qa_step=1;pillow_qa_note("pillow_entry");}
        break;
    case 1:
        if(walking&&battle_outcome.phase()==upstream::BattleOutcomePhase::Complete&&world.story_flag("minnie_leave")){
            if(session_rewards.experience!=8||session_rewards.bank!=10||world.story_flag("minnie_door")||world.story_flag("doll_melody")||world.body_visible(12)){pillow_qa_failed=true;pillow_qa_note("FAIL_REWARD_OR_FLAGS");return false;}
            const auto m=gameplay_scene->presentation.npc_pose(3).position;
            if(m.x!=64||m.y!=370){pillow_qa_failed=true;pillow_qa_note("FAIL_LIVE_LEAVE_POSITION");return false;}
            pillow_qa_step=2;pillow_qa_note("pillow_reward_and_minnie_leave");
        }
        break;
    case 2:
        held=KEY_DDOWN;
        if(p.position.y>300){pillow_qa_step=3;pillow_qa_note("actual_mom_return_warp");}
        break;
    case 3:
        if(!world.story_flag("minnie_door"))held=KEY_DUP;
        if(walking&&world.story_flag("minnie_door")){
            const auto m=gameplay_scene->presentation.npc_pose(3).position;
            if(m.x!=40||m.y!=370){pillow_qa_failed=true;pillow_qa_note("FAIL_LIVE_DOOR_POSITION");return false;}
            pillow_qa_step=4;pillow_qa_note("minnie_hallway_event");
        }
        break;
    case 4:
        if(p.position.y<397)held=KEY_DDOWN;else {pillow_qa_step=5;pillow_qa_note("approach_tutorial_row");}
        break;
    case 5:
        if(p.position.x>42)held=KEY_DLEFT;else {pillow_qa_step=6;pillow_qa_note("approach_tutorial_column");}
        break;
    case 6:
        if(p.direction.y!=-1)held=KEY_DUP;else {pillow_qa_step=7;pillow_qa_note("face_minnie");}
        break;
    case 7:
        if(walking&&pillow_qa_frame%20==0)down=KEY_A;
        if(dialogue_choices.active()){pillow_qa_step=8;pillow_qa_note("tutorial_choices_visible");}
        break;
    case 8:
        if(walking&&pillow_qa_selected){
            if(world.story_flag("mimmie_door_opened")||world.story_flag("doll_melody")){pillow_qa_failed=true;pillow_qa_note("FAIL_TUTORIAL_MUTATED_PROGRESS");return false;}
            pillow_qa_step=9;pillow_qa_note("tutorial_yes_complete");
        }
        break;
    case 9:
        if(p.position.x<63)held=KEY_DRIGHT;else {pillow_qa_step=10;pillow_qa_note("sister_door_column");}
        break;
    case 10:
        if(p.position.y>386)held=KEY_DUP;else {pillow_qa_step=11;pillow_qa_note("running_collision_start");}
        break;
    case 11:held=KEY_DUP|KEY_B;break;
    }
    // Every input-owner transition requires an actual released sample.
    if(pillow_qa_frame%20==1)held=0;
    return true;
}
'''
SETUP=r'''
    // Explicit GENERATED post-Lamp origin, matching the host sequence fixture.
    // No LOAD, save scan, file-based gameplay state, or live-user profile input.
    {
        continue_menu.close();auto& world=gameplay_scene->world;world.attach_random(battle_random);
        if(!world.set_story_flag("poltergeist",true,true)||!world.warp_same_scene({176,394},{0,-1})){error_console(world.error());return 1;}
        const auto source=battle_data.view().participant(0);auto& s=session_rewards;
        s.hp=source.hp;s.pp=source.pp;s.maxhp=source.maxhp;s.maxpp=source.maxpp;s.offense=source.offense;s.defense=source.defense;s.speed=source.speed;s.iq=source.iq;s.guts=source.guts;s.level=1;s.experience=3;s.bank=5;s.earned_cash=5;s.learned_skills={"strike","splitShot"};session_rewards_valid=true;
        pillow_qa_started=osGetTime();pillow_qa_note("GENERATED_POST_LAMP_ORIGIN_SAVE_IO_NONE");
    }
'''

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--candidate',type=Path,required=True);args=parser.parse_args()
    lane=Path(__file__).resolve().parents[1];r=args.candidate.resolve();b=lane/'build/pillow-playable-qa';p=lane/'reports/pillow-playable-qa';b.mkdir(parents=True,exist_ok=True);p.mkdir(parents=True,exist_ok=True)
    spec=importlib.util.spec_from_file_location('qa',r/'tools/build_continue_text_qa.py');qa=importlib.util.module_from_spec(spec);spec.loader.exec_module(qa)
    original=(r/'platform/ctr/main.cpp').read_text();s=qa.instrument_source(original)
    def replace(old,new):
        nonlocal s
        if s.count(old)!=1:raise ValueError('Nonunique QA anchor: '+old)
        s=s.replace(old,new)
    replace('sdmc:/encore-continue-character-qa.log','sdmc:/encore-pillow-playable-qa.log')
    replace('LoadingScope loading("Starting game",21)','LoadingScope loading("Starting game",16)')
    replace('if(!open_continue(error,&loading)||!loading.finish())','if(!loading.finish())')
    replace('int main(int argc,char** argv){',DRIVER+'\nint main(int argc,char** argv){')
    replace('    upstream::MenuNavigationRepeat menu_navigation;',SETUP+'    upstream::MenuNavigationRepeat menu_navigation;')
    replace('hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();if(down&KEY_START)break;','hidScanInput();u32 down=0,held=0;if((hidKeysDown()&KEY_START)||!pillow_qa_input(down,held))break;')
    replace('u64 last=osGetTime();uint64_t accumulator=0;','u64 last=osGetTime();uint64_t accumulator=0;gspWaitForVBlank();')
    replace('        if(gameplay_scene->world.battle_request().requested&&!in_battle()&&!battle_handoff_pending()){',r'''        if(pillow_qa_step==11&&gameplay_scene->world.battle_request().requested&&std::string(gameplay_scene->world.battle_request().enemy)=="doll"){
            pillow_qa_success=gameplay_scene->world.story_flag("mimmie_door_opened")&&!gameplay_scene->world.story_flag("doll_melody")&&pillow_qa_selected==1;
            pillow_qa_note(pillow_qa_success?"PASS_REAL_RAM_AND_DOLL_REQUEST":"FAIL_FINAL_FLAGS");break;
        }
        if(gameplay_scene->world.battle_request().requested&&!in_battle()&&!battle_handoff_pending()){''')
    replace('                if(event.kind==upstream::DialogueChoicesEventKind::Selected){','                if(event.kind==upstream::DialogueChoicesEventKind::Selected){\n                    ++pillow_qa_selected;pillow_qa_note("tutorial_choice_selected");')
    # Fail closed if this bounded diagnostic ever strays into save/file entrypoints.
    for name in ('read_prepared_slot','scan_record_slots','remember_slot','write_record_slot'):
        start=s.index('bool '+name+'(');body=s.index('{',start);depth=1;end=body+1
        while depth:
            depth+=(s[end]=='{')-(s[end]=='}');end+=1
        s=s[:body]+'{error="Pillow QA forbids save IO";return false;}'+s[end:]
    replace('    if(qa_log)std::fclose(qa_log);','    pillow_qa_note(pillow_qa_success?"EXIT_SUCCESS":"EXIT_INCOMPLETE");if(qa_log)std::fclose(qa_log);')
    (b/'main.cpp').write_text(s);(p/'instrumentation.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='production/main.cpp',tofile='generated/pillow-playable-qa/main.cpp')))
    sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM']);compile=qa.compile_command(r,b,sdk,arm)
    compile[1:1]=['-DENCORE_EXPERIMENTAL_GPU_BACKGROUND','-DENCORE_GPU_CERTIFICATE_TABLES','-DENCORE_REFERENCE_SEED=34','-I'+str(r/'build/ctr')]
    objects=sorted((r/'build/ctr/runtime').glob('*.o'))+[r/'build/ctr/platform/ctr/audio_player.o',r/'build/ctr/platform/ctr/music_region_player.o',r/'build/ctr/platform/ctr/music_region_service.o',b/'main.o']
    commands=[compile,[str(arm/'bin/arm-none-eabi-g++'),'-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-specs=3dsx.specs','-Wl,--gc-sections',*[str(x)for x in objects],'-L'+str(sdk/'libctru/lib'),'-lcitro2d','-lcitro3d','-lctru','-lm','-o',str(b/'qa.elf')],[str(sdk/'tools/bin/3dsxtool'),str(b/'qa.elf'),str(b/'qa.3dsx'),'--smdh='+str(r/'dist/encore-native.smdh'),'--romfs='+str(r/'build/ctr/native-romfs')]]
    with (p/'build.log').open('w')as log:
        for command in commands:log.write(shlex.join(command)+'\n');log.flush();subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    sha=lambda x:hashlib.sha256(x.read_bytes()).hexdigest()
    assert sha(r/'platform/ctr/main.cpp')==hashlib.sha256(original.encode()).hexdigest()
    result={'scope':'Generated post-Lamp origin, actual Mom warp/Pillow attack/combat/reward/Minnie scripts/Yes tutorial/run collision to Doll request; rendered combined code; no full NewGame or hardware claim','candidate':str(r),'qa_binary':str(b/'qa.3dsx'),'qa_sha256':sha(b/'qa.3dsx'),'production_main_sha256':sha(r/'platform/ctr/main.cpp'),'generated_main_sha256':sha(b/'main.cpp'),'commands':commands,'shared_objects':{str(x.relative_to(r)):sha(x)for x in objects if x!=b/'main.o'},'romfs':{str(x.relative_to(r/'build/ctr/native-romfs')):sha(x)for x in (r/'build/ctr/native-romfs').rglob('*')if x.is_file()},'sd_log':'sdmc:/encore-pillow-playable-qa.log'}
    (p/'build-receipt.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k not in ('shared_objects','romfs','commands')}))
if __name__=='__main__':main()
