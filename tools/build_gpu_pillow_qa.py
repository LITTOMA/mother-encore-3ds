#!/usr/bin/env python3
"""Isolated exact readback then genuine Pillow menu diagnostic; no save IO."""
from pathlib import Path
import hashlib,json,os,subprocess,difflib,shlex,sys
import build_continue_text_qa as qa
r=Path(__file__).resolve().parents[1];b=r/'build/gpu-pillow-texture-qa';p=r/'reports/gpu-pillow-texture';b.mkdir(parents=True,exist_ok=True);p.mkdir(parents=True,exist_ok=True)
# Reviewed diagnostic origin retained from build_pillow_playable_qa.py.
SETUP='\n    // Explicit GENERATED post-Lamp origin, matching the host sequence fixture.\n    // No LOAD, save scan, file-based gameplay state, or live-user profile input.\n    {\n        continue_menu.close();auto& world=gameplay_scene->world;world.attach_random(battle_random);\n        if(!world.set_story_flag("poltergeist",true,true)||!world.warp_same_scene({176,394},{0,-1})){error_console(world.error());return 1;}\n        const auto source=battle_data.view().participant(0);auto& s=session_rewards;\n        s.hp=source.hp;s.pp=source.pp;s.maxhp=source.maxhp;s.maxpp=source.maxpp;s.offense=source.offense;s.defense=source.defense;s.speed=source.speed;s.iq=source.iq;s.guts=source.guts;s.level=1;s.experience=3;s.bank=5;s.earned_cash=5;s.learned_skills={"strike","splitShot"};session_rewards_valid=true;\n        pillow_qa_started=osGetTime();pillow_qa_note("GENERATED_POST_LAMP_ORIGIN_SAVE_IO_NONE");\n    }\n'
original=(r/'platform/ctr/main.cpp').read_text();s=qa.instrument_source(original,True)
def replace(old,new):
 global s
 if s.count(old)!=1:raise ValueError('nonunique anchor '+old)
 s=s.replace(old,new)
replace('sdmc:/encore-frame-profile-qa.log','sdmc:/encore-gpu-pillow-texture-qa.log')
replace('LoadingScope loading("Starting game",21)','LoadingScope loading("Starting game",16)')
replace('if(!open_continue(error,&loading)||!loading.finish())','if(!loading.finish())')
driver=r'''
#include "tests/gpu_row_linear_readback.hpp"
unsigned pillow_qa_step=0,pillow_qa_frame=0,pillow_qa_active=0;
u64 pillow_qa_started=0;
void pillow_qa_note(const char* event){if(qa_log){std::fprintf(qa_log,"PILLOW_GPU_EVENT %s frame=%u active_frames=%u entry=%u ready=%u active=%u row=%u row_bytes=%lu spans=%lu mask=%u\n",event,pillow_qa_frame,pillow_qa_active,unsigned(battle_entry.phase()),unsigned(battle_renderer.gpu_background_ready()),unsigned(battle_renderer.gpu_background_active()),unsigned(battle_renderer.gpu_row_linear_ready()),(unsigned long)battle_renderer.gpu_row_linear_bytes(),(unsigned long)battle_renderer.gpu_background_spans(),unsigned(battle_entry.mask_active()));std::fflush(qa_log);}}
bool pillow_qa_input(u32& down,u32& held){
 down=held=0;++pillow_qa_frame;
 if(!gameplay_scene->world.healthy()||!house_error.empty()||!round_error.empty()||osGetTime()-pillow_qa_started>300000){pillow_qa_note("FAIL_ERROR_OR_TIMEOUT");return false;}
 if(!in_battle()){held=KEY_DUP;if(gameplay_scene->presentation.dialogue_active()&&pillow_qa_frame%20==0)down=KEY_A;}
 else{
  if(!pillow_qa_step){pillow_qa_step=1;pillow_qa_note("REAL_PILLOW_ENTRY");}
  if(battle_renderer.gpu_background_active()&&!battle_entry.mask_active())++pillow_qa_active;
  if(pillow_qa_active==360){down=KEY_A;pillow_qa_note("BASH_TARGET_INPUT");}
  if(pillow_qa_active==380){down=KEY_B;pillow_qa_note("CANCEL_TARGET_INPUT");}
  if(pillow_qa_active>=400){pillow_qa_note("PASS_STEADY_MENU_EXIT");return false;}
 }
 if(pillow_qa_frame%20==1)held=0; // Release after every input-owner change.
 return true;
}
'''
replace('int main(int argc,char** argv){',driver+'\nint main(int argc,char** argv){')
replace('    auto* top=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);','    if(qa_log)std::fprintf(qa_log,"READBACK_SKIPPED explicit scene-only build; use --readback for this texture candidate\\n");\n    auto* top=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);')
replace('    upstream::MenuNavigationRepeat menu_navigation;',SETUP+'    upstream::MenuNavigationRepeat menu_navigation;')
replace('hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();if(down&KEY_START)break;','hidScanInput();u32 down=0,held=0;if((hidKeysDown()&KEY_START)||!pillow_qa_input(down,held))break;')
replace('u64 last=osGetTime();uint64_t accumulator=0;','u64 last=osGetTime();uint64_t accumulator=0;gspWaitForVBlank();')
replace('            const auto d=battle_renderer.background_diagnostics();',r'''            const auto d=battle_renderer.background_diagnostics();
            std::fprintf(qa_log,"TEXTURE_BACKGROUND active=%u rows=%lu strips=%lu extra_bytes=%lu backend=explicit_experimental_hardware_unverified\n",unsigned(battle_renderer.gpu_texture_active()),(unsigned long)battle_renderer.gpu_texture_rows(),(unsigned long)battle_renderer.gpu_texture_strips(),(unsigned long)battle_renderer.gpu_texture_bytes());
            std::fprintf(qa_log,"GPU_BACKGROUND active=%u ready=%u spans=%lu mask=%u shader_time=%.6f row=%u row_bytes=%lu evaluations=%llu\n",unsigned(battle_renderer.gpu_background_active()),unsigned(battle_renderer.gpu_background_ready()),(unsigned long)battle_renderer.gpu_background_spans(),unsigned(battle_entry.mask_active()),shader_time,unsigned(battle_renderer.gpu_row_linear_ready()),(unsigned long)battle_renderer.gpu_row_linear_bytes(),(unsigned long long)battle_renderer.gpu_row_linear_evaluations());''')
for name in ('read_prepared_slot','scan_record_slots','remember_slot','write_record_slot'):
 start=s.index('bool '+name+'(');body=s.index('{',start);depth=1;end=body+1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 s=s[:body]+'{error="Pillow GPU QA forbids save IO";return false;}'+s[end:]
replace('    if(qa_log)std::fclose(qa_log);','    pillow_qa_note("CLEAN_EXIT");if(qa_log)std::fclose(qa_log);')
if '--readback' in sys.argv:
 marker='    if(qa_log)std::fprintf(qa_log,"READBACK_SKIPPED explicit scene-only build; use --readback for this texture candidate\\n");'
 replace(marker,'    if(!pillow_gpu_probe::run(qa_log)){error_console("Pillow GPU readback failed");return 1;}')
(b/'main.cpp').write_text(s);(p/'qa-main.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='production/main.cpp',tofile='generated/gpu-pillow/main.cpp')))
sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM']);compile=qa.compile_command(r,b,sdk,arm,True);compile[1:1]=['-DENCORE_EXPERIMENTAL_GPU_BACKGROUND','-DENCORE_GPU_CERTIFICATE_TABLES','-DENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS','-DENCORE_REFERENCE_SEED=34','-I'+str(r),'-I'+str(r/'build/ctr')]
objects=sorted((r/'build/ctr/runtime').glob('*.o'))+[r/'build/ctr/platform/ctr/audio_player.o',r/'build/ctr/platform/ctr/music_region_player.o',r/'build/ctr/platform/ctr/music_region_service.o',b/'main.o']
commands=[compile,[str(arm/'bin/arm-none-eabi-g++'),'-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-specs=3dsx.specs','-Wl,--gc-sections',*[str(x)for x in objects],'-L'+str(sdk/'libctru/lib'),'-lcitro2d','-lcitro3d','-lctru','-lm','-o',str(b/'qa.elf')],[str(sdk/'tools/bin/3dsxtool'),str(b/'qa.elf'),str(b/'qa.3dsx'),'--smdh='+str(r/'dist/encore-native.smdh'),'--romfs='+str(r/'build/ctr/native-romfs')]]
with (p/'build.log').open('w')as log:
 for command in commands:log.write(shlex.join(command)+'\n');log.flush();subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
sha=lambda x:hashlib.sha256(x.read_bytes()).hexdigest()
assert sha(r/'platform/ctr/main.cpp')==hashlib.sha256(original.encode()).hexdigest()
result={'scope':'Generated post-Lamp origin, actual Pillow entry/menu; optional startup GPU readback; no save IO, production main unchanged','readback_in_this_binary':'--readback' in sys.argv,'qa_binary':str(b/'qa.3dsx'),'qa_sha256':sha(b/'qa.3dsx'),'production_main_sha256':sha(r/'platform/ctr/main.cpp'),'generated_main_sha256':sha(b/'main.cpp'),'commands':commands,'shared_objects':{str(x.relative_to(r)):sha(x)for x in objects if x!=b/'main.o'},'sources':{str(x.relative_to(r)):sha(x)for x in [r/'include/encore/row_linear_background_kernel.hpp',r/'platform/ctr/battle_renderer.hpp',r/'platform/ctr/gpu_region_batch.hpp',r/'tests/gpu_row_linear_readback.hpp',r/'tests/gpu_row_linear_host.cpp',r/'tools/build_gpu_pillow_qa.py',r/'platform/ctr/gpu_row_texture_batch.hpp',r/'platform/ctr/shaders/gpu_row_texture.v.pica',r/'platform/ctr/shaders/gpu_row_texture.g.pica']},'sd_log':'sdmc:/encore-gpu-pillow-texture-qa.log'}
(p/'build.json').write_text(json.dumps(result,indent=2)+'\n');print(result['qa_sha256'])
