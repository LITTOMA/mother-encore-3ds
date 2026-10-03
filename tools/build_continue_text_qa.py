"""Build a separate text QA artifact; optional frame diagnostics stay off by default."""
from pathlib import Path
import argparse
import difflib
import hashlib
import json
import os
import subprocess


def configuration(frame_profile=False):
    return {
        'output_name': 'frame-profile-qa' if frame_profile else 'continue-text-qa',
        'log_path': 'sdmc:/encore-frame-profile-qa.log' if frame_profile else 'sdmc:/encore-continue-character-qa.log',
        'defines': ['-DENCORE_TEXT_QA'] + (['-DENCORE_FRAME_PROFILE'] if frame_profile else []),
    }


def instrument_source(original, frame_profile=False):
    config = configuration(frame_profile)
    for anchor in ('sdmc:/encore-native-qa.log', 'ENCORE_TEXT_QA v1 no image capture',
                   'const uint64_t state=uint64_t(gameplay_scene->world.stage())'):
        if original.count(anchor) != 1:
            raise ValueError('Expected exactly one reviewed QA replacement anchor: ' + anchor)
    source = original.replace('sdmc:/encore-native-qa.log', config['log_path']).replace(
        'ENCORE_TEXT_QA v1 no image capture', 'ENCORE_CONTINUE_QA v1 no image capture')
    source = source.replace('const uint64_t state=uint64_t(gameplay_scene->world.stage())',
        'const uint64_t state=(uint64_t(continue_menu.phase())<<32)|(uint64_t(continue_menu.pose().title_option)<<40)|(uint64_t(continue_menu.pose().action)<<44)|(uint64_t(continue_menu.selected_slot())<<48)|uint64_t(gameplay_scene->world.stage())')
    needle = '        if(!round_error.empty())std::fprintf(qa_log,"ROUND_ERROR'
    extra = '''        const auto cp=continue_menu.pose();const auto player=gameplay_scene->world.player();
        std::fprintf(qa_log,"CONTINUE phase=%u title=%u action=%u slot=%u visible=%u pending=%u selected_pref=%u viewport=%ux%u status=%s\\n",unsigned(continue_menu.phase()),unsigned(cp.title_option),unsigned(cp.action),unsigned(continue_menu.selected_slot()),unsigned(cp.world_visible),unsigned(restore_input_pending),unsigned(last_save_slot),view_width,view_height,continue_status.c_str());
        std::fprintf(qa_log,"SESSION ready=%u stats=%u level=%u exp=%u hp=%ld pp=%ld cash=%u bank=%u dir=%.6f,%.6f inventory=%u playtime=%.6f\\n",unsigned(session_state_ready),unsigned(session_rewards_valid),unsigned(session_rewards.level),unsigned(session_rewards.experience),long(session_rewards.hp),long(session_rewards.pp),unsigned(session_rewards.cash),unsigned(session_rewards.bank),double(player.direction.x),double(player.direction.y),unsigned(session_inventory.size()),session_state.playtime_seconds);
        if(state!=previous){for(uint32_t i=0;i<session_inventory.size();++i){const auto item=session_inventory.instance(i);std::fprintf(qa_log,"INVENTORY i=%u uid=%u definition=%u equipped=%u doses=%u\\n",unsigned(i),unsigned(item.id),unsigned(item.definition),unsigned(item.equipped),unsigned(item.doses));}for(uint32_t i=0;i<house_data.view().count(upstream::HouseSection::Npcs);++i){const auto npc=gameplay_scene->presentation.npc_pose(i);std::fprintf(qa_log,"NPC i=%u x=%.3f y=%.3f visible=%u\\n",unsigned(i),double(npc.position.x),double(npc.position.y),unsigned(npc.visible));}}
'''
    if source.count(needle) != 1:
        raise ValueError('Expected exactly one reviewed QA instrumentation boundary')
    return source.replace(needle, extra + needle)


def compile_command(root, output, sdk, arm, frame_profile=False):
    arch = ['-march=armv6k', '-mtune=mpcore', '-mfloat-abi=hard', '-mtp=soft']
    return [str(arm / 'bin/arm-none-eabi-g++'), *arch, '-std=gnu++17', '-ffp-contract=off',
        '-O2', '-g', '-Wall', '-Wextra', '-Wpedantic', '-mword-relocations',
        '-ffunction-sections', '-fdata-sections', '-fno-exceptions', '-fno-rtti',
        '-D__3DS__', *configuration(frame_profile)['defines'], '-I' + str(root / 'include'),
        '-I' + str(root / 'platform/ctr'), '-I' + str(sdk / 'libctru/include'),
        '-c', str(output / 'main.cpp'), '-o', str(output / 'main.o')]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frame-profile', action='store_true',
        help='Enable diagnostic-only frame/region telemetry in an independent QA artifact and SD log')
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    config = configuration(args.frame_profile)
    output = root / 'build' / config['output_name']
    reports = root / 'reports' / config['output_name']
    output.mkdir(parents=True, exist_ok=True)
    reports.mkdir(parents=True, exist_ok=True)
    production_before = sha(root / 'dist/encore-native.3dsx')
    original = (root / 'platform/ctr/main.cpp').read_text()
    source = instrument_source(original, args.frame_profile)
    (output / 'main.cpp').write_text(source)
    (reports / 'instrumentation.diff').write_text(''.join(difflib.unified_diff(
        original.splitlines(True), source.splitlines(True),
        fromfile='production/platform/ctr/main.cpp', tofile='qa/main.cpp')))
    sdk, arm = Path(os.environ['DEVKITPRO']), Path(os.environ['DEVKITARM'])
    cxx = str(arm / 'bin/arm-none-eabi-g++')
    arch = ['-march=armv6k', '-mtune=mpcore', '-mfloat-abi=hard', '-mtp=soft']
    objects = sorted((root / 'build/ctr/runtime').glob('*.o')) + [
        root / 'build/ctr/platform/ctr/audio_player.o',
        root / 'build/ctr/platform/ctr/music_region_player.o',
        root / 'build/ctr/platform/ctr/music_region_service.o', output / 'main.o']
    compile = compile_command(root, output, sdk, arm, args.frame_profile)
    link = [cxx, *arch, '-specs=3dsx.specs', '-g',
        '-Wl,--gc-sections,-Map,' + str(output / 'qa.map'), *[str(p) for p in objects],
        '-L' + str(sdk / 'libctru/lib'), '-lcitro2d', '-lcitro3d', '-lctru', '-lm',
        '-o', str(output / 'qa.elf')]
    pack = [str(sdk / 'tools/bin/3dsxtool'), str(output / 'qa.elf'), str(output / 'qa.3dsx'),
        '--smdh=' + str(root / 'dist/encore-native.smdh'),
        '--romfs=' + str(root / 'build/ctr/native-romfs')]
    with (reports / 'build.txt').open('w') as log:
        for command in (compile, link, pack):
            subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
    record = {
        'production_sha256': sha(root / 'dist/encore-native.3dsx'),
        'qa_sha256': sha(output / 'qa.3dsx'),
        'changes': 'ENCORE_TEXT_QA enabled; read-only Continue/session/NPC fields and separate log path. '
                   'Same production core objects, input, content and random policy. '
                   'Logging may affect timing; no performance equivalence claimed.',
        'frame_profile': args.frame_profile,
        'defines': config['defines'], 'sd_log_path': config['log_path'],
        'production_main_sha256': sha(root / 'platform/ctr/main.cpp'),
        'qa_main_sha256': sha(output / 'main.cpp'),
        'shared_objects': {str(p.relative_to(root)): sha(p) for p in objects if p != output / 'main.o'},
        'romfs': {str(p.relative_to(root / 'build/ctr/native-romfs')): sha(p)
                  for p in (root / 'build/ctr/native-romfs').rglob('*') if p.is_file()},
    }
    if args.frame_profile:
        record['profile_note'] = ('Seven stages plus total over 60 completed frames; QA stage includes log writing/flush. '
            'Previous completed windows and current composed-frame region counters are emitted separately. '
            'Diagnostic lower-screen text differs; game pixel sampling/algorithm is unchanged.')
        record['diagnostic_sources'] = {p: sha(root / p) for p in (
            'include/encore/frame_profile.hpp', 'platform/ctr/battle_renderer.hpp',
            'tools/build_continue_text_qa.py')}
    if record['production_sha256'] != production_before:
        raise RuntimeError('Production artifact changed during isolated QA build')
    (reports / 'build-receipt.json').write_text(json.dumps(record, indent=2) + '\n')
    print(record['qa_sha256'])


if __name__ == '__main__':
    main()
