from pathlib import Path
import os,subprocess,json,hashlib,difflib
import build_continue_text_qa as qa
from build_pillow_playable_qa import DRIVER,SETUP
r=Path(__file__).resolve().parents[1];b=r/'build/seamless-entry-qa';b.mkdir(parents=True,exist_ok=True)
(r/'reports').mkdir(exist_ok=True)
asset_root=r if (r/'build/ctr/native-romfs').is_dir() else r.parent.parent/'encore-native'
original=(r/'platform/ctr/main.cpp').read_text();s=qa.instrument_source(original,True)
def replace(old,new):
 global s
 assert s.count(old)==1,(old,s.count(old))
 s=s.replace(old,new)
replace('sdmc:/encore-frame-profile-qa.log','sdmc:/encore-seamless-entry-qa.log')
replace('LoadingScope loading("Starting game",21)','LoadingScope loading("Starting game",16)')
replace('if(!open_continue(error,&loading)||!loading.finish())','if(!loading.finish())')
driver=DRIVER.replace('    case 4:\n', '    case 4:\n        if(!world.warp_same_scene({64,386},{0,-1})){pillow_qa_failed=true;return false;}pillow_qa_step=11;pillow_qa_note("DIAGNOSTIC_ALIGNMENT_BEFORE_REAL_DOOR_RAM");break;\n    case 99:\n').replace('    switch(pillow_qa_step){',r'''
    if(pillow_qa_step==11&&in_battle()&&std::string(world.battle_request().enemy)=="doll"){
        down=held=0;
        if(battle_entry.phase()==upstream::BattleEntryPhase::Commands&&++pillow_qa_selected>=121){pillow_qa_success=true;pillow_qa_note("PASS_REAL_PILLOW_AND_DOLL_ENTRY");return false;}
        return true;
    }
    switch(pillow_qa_step){''')
replace('int main(int argc,char** argv){',driver+'\nint main(int argc,char** argv){')
replace('    upstream::MenuNavigationRepeat menu_navigation;',SETUP+'    if(!admit_encounter_scene(error)){error_console(error);return 1;}\n    upstream::MenuNavigationRepeat menu_navigation;')
replace('hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();if(down&KEY_START)break;','hidScanInput();u32 down=0,held=0;if((hidKeysDown()&KEY_START)||!pillow_qa_input(down,held))break;')
replace('u64 last=osGetTime();uint64_t accumulator=0;','u64 last=osGetTime();uint64_t accumulator=0;gspWaitForVBlank();')
replace('                if(event.kind==upstream::DialogueChoicesEventKind::Selected){','                if(event.kind==upstream::DialogueChoicesEventKind::Selected){\n                    ++pillow_qa_selected;pillow_qa_note("tutorial_choice_selected");')
for name in ('read_prepared_slot','scan_record_slots','remember_slot','write_record_slot'):
 start=s.index('bool '+name+'(');body=s.index('{',start);depth=1;end=body+1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 s=s[:body]+'{error="Seamless entry QA forbids save IO";return false;}'+s[end:]
replace('    if(qa_log){std::fclose(qa_log);qa_log=nullptr;}','    pillow_qa_note(pillow_qa_success?"EXIT_SUCCESS":"EXIT_INCOMPLETE");if(qa_log){std::fclose(qa_log);qa_log=nullptr;}')
(b/'main.cpp').write_text(s)
(r/'reports/qa-main.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='production/main.cpp',tofile='generated/seamless-qa/main.cpp')))
sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM']);compile=qa.compile_command(r,b,sdk,arm,True);compile[1:1]=['-DENCORE_EXPERIMENTAL_GPU_BACKGROUND','-DENCORE_GPU_CERTIFICATE_TABLES','-DENCORE_REFERENCE_SEED=34','-I'+str(r/'build/ctr')]
objects=sorted((r/'build/ctr/runtime').glob('*.o'))+[r/'build/ctr/platform/ctr/audio_player.o',r/'build/ctr/platform/ctr/music_region_player.o',r/'build/ctr/platform/ctr/music_region_service.o',b/'main.o']
commands=[compile,[str(arm/'bin/arm-none-eabi-g++'),'-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-specs=3dsx.specs','-Wl,--gc-sections',*[str(x)for x in objects],'-L'+str(sdk/'libctru/lib'),'-lcitro2d','-lcitro3d','-lctru','-lm','-o',str(b/'qa.elf')],[str(sdk/'tools/bin/3dsxtool'),str(b/'qa.elf'),str(b/'qa.3dsx'),'--smdh='+str(asset_root/'dist/encore-native.smdh'),'--romfs='+str(asset_root/'build/ctr/native-romfs')]]
with (r/'reports/qa-build.log').open('w')as log:
 for command in commands:log.write(' '.join(command)+'\n');log.flush();subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(r/'reports/qa-build.json').write_text(json.dumps({'scope':'Generated post-Lamp origin; real Pillow combat/Minnie script/ram/Doll entry, no save IO, no hardware claim','binary':str(b/'qa.3dsx'),'sha256':sha(b/'qa.3dsx'),'main_sha256':sha(r/'platform/ctr/main.cpp'),'commands':commands},indent=2)+'\n')
print(str(b/'qa.3dsx'))
