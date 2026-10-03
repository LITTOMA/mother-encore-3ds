#!/usr/bin/env python3
"""Build opt-in GPU battle QA at an explicit in-memory Doll diagnostic origin."""
from pathlib import Path
import hashlib,json,os,subprocess,difflib,shlex
import build_continue_text_qa as qa
r=Path(__file__).resolve().parents[1];b=r/'build/gpu-doll-qa';p=r/'reports/gpu-integration';b.mkdir(parents=True,exist_ok=True);p.mkdir(parents=True,exist_ok=True)
original=(r/'platform/ctr/main.cpp').read_text();s=qa.instrument_source(original,True)
s=s.replace('sdmc:/encore-frame-profile-qa.log','sdmc:/encore-gpu-doll-cert-qa.log')
needle='    if(!open_continue(error,&loading)||!loading.finish())'
assert s.count(needle)==1
s=s.replace(needle,'    if(!loading.finish())')
assert s.count('LoadingScope loading("Starting game",21)')==1
s=s.replace('LoadingScope loading("Starting game",21)','LoadingScope loading("Starting game",16)')
needle='    upstream::MenuNavigationRepeat menu_navigation;'
setup='''    // Diagnostic setup only, identical public-operation route to the checked
    // doll_round_tests fresh-world scenario. No save, reward, or release path.
    // This intentionally does not claim complete Lamp/house progression.
    {
        continue_menu.close();auto& world=gameplay_scene->world;world.attach_random(battle_random);
        if(!world.warp_same_scene({64,128},{0,-1})||!world.begin_house_program(1)){error_console(world.error());return 1;}
        unsigned text_frames=0;const double dt=double(float(1.0/60));
        for(unsigned i=0;i<3000&&world.stage()!=upstream::OpeningStage::BattleRequested;++i){
            if(world.pending_dialogue_id()!=upstream::kRoomNoIndex&&++text_frames>=180){if(!world.finish_story_dialogue()){error_console(world.error());return 1;}text_frames=0;}
            if(!world.advance({})||!world.idle_frame(dt)){error_console(world.error());return 1;}
        }
        if(!world.battle_request().requested||std::string(world.battle_request().enemy)!="doll"||!begin_battle(error)){error_console("Doll diagnostic setup: "+error);return 1;}
        if(qa_log){std::fprintf(qa_log,"GPU_QA_START diagnostic_origin=checked_fresh_world_doll_program save_io=none gpu_ready=%u\\n",unsigned(battle_renderer.gpu_background_ready()));std::fflush(qa_log);}
    }
'''
assert s.count(needle)==1;s=s.replace(needle,setup+needle)
needle='            const auto d=battle_renderer.background_diagnostics();'
assert s.count(needle)==1;s=s.replace(needle,needle+'''
            std::fprintf(qa_log,"GPU_BACKGROUND active=%u ready=%u spans=%lu mask=%u shader_time=%.6f certificate=%u certificate_bytes=%lu\\n",unsigned(battle_renderer.gpu_background_active()),unsigned(battle_renderer.gpu_background_ready()),(unsigned long)battle_renderer.gpu_background_spans(),unsigned(battle_entry.mask_active()),shader_time,unsigned(battle_renderer.gpu_certificate_used()),(unsigned long)battle_renderer.gpu_certificate_bytes());''')
s=s.replace('u64 last=osGetTime();uint64_t accumulator=0;','u64 last=osGetTime();uint64_t accumulator=0;gspWaitForVBlank(); // QA starts directly in world; ensure a positive first idle delta.')
(b/'main.cpp').write_text(s);(p/'qa-main.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='production/main.cpp',tofile='generated/gpu-doll-qa/main.cpp')))
sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM']);compile=qa.compile_command(r,b,sdk,arm,True);compile[1:1]=['-DENCORE_EXPERIMENTAL_GPU_BACKGROUND','-DENCORE_GPU_CERTIFICATE_TABLES','-I'+str(r/'build/ctr')]
objects=sorted((r/'build/ctr/runtime').glob('*.o'))+[r/'build/ctr/platform/ctr/audio_player.o',r/'build/ctr/platform/ctr/music_region_player.o',r/'build/ctr/platform/ctr/music_region_service.o',b/'main.o']
commands=[compile,[str(arm/'bin/arm-none-eabi-g++'),'-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-specs=3dsx.specs','-Wl,--gc-sections',*[str(x) for x in objects],'-L'+str(sdk/'libctru/lib'),'-lcitro2d','-lcitro3d','-lctru','-lm','-o',str(b/'qa.elf')],[str(sdk/'tools/bin/3dsxtool'),str(b/'qa.elf'),str(b/'qa.3dsx'),'--smdh='+str(r/'dist/encore-native.smdh'),'--romfs='+str(r/'build/ctr/native-romfs')]]
with (p/'qa-build.log').open('w') as log:
 for command in commands:log.write(shlex.join(command)+'\n');log.flush();subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
sha=lambda x:hashlib.sha256(x.read_bytes()).hexdigest()
assert sha(r/'platform/ctr/main.cpp')==hashlib.sha256(original.encode()).hexdigest()
result={'scope':'Diagnostic fresh-world Doll program to genuine entry/renderer/menu; no game-save IO; production main unchanged','qa_binary':str(b/'qa.3dsx'),'qa_sha256':sha(b/'qa.3dsx'),'production_main_sha256':sha(r/'platform/ctr/main.cpp'),'generated_main_sha256':sha(b/'main.cpp'),'commands':commands,'gpu_sources':{str(x.relative_to(r)):sha(x) for x in [r/'include/encore/region_background_kernel.hpp',r/'platform/ctr/battle_renderer.hpp',r/'platform/ctr/gpu_region_batch.hpp',*sorted((r/'platform/ctr/shaders').glob('*.pica'))]},'sd_log':'sdmc:/encore-gpu-doll-cert-qa.log'}
(p/'qa-build.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
