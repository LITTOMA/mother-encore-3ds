#!/usr/bin/env python3
"""Build a cached offline format-admission tool; use actual console-core loaders.

This is a producer contract check of real resource bytes, not a test suite or a
desktop game. Compilation uses four processes; unchanged objects are reused.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    'tools/native_resource_admission.cpp',
    'runtime/catalog_resource_admission.cpp',
    'runtime/resource_catalog.cpp',
    'runtime/file_io.cpp',
    'runtime/content.cpp',
    'runtime/room_data.cpp',
    'runtime/house_data.cpp',
    'runtime/items_data.cpp',
    'runtime/audio_data.cpp',
    'runtime/phone_data.cpp',
    'runtime/battle_data.cpp',
    'runtime/battle_round_data.cpp',
    'runtime/dialogue_choices_data.cpp',
    'runtime/save_menu_data.cpp',
    'runtime/native_session.cpp',
    'runtime/startup_settings.cpp',
    'runtime/house_button_prompts.cpp',
    'runtime/continue_menu_data.cpp',
    'runtime/restore_data.cpp',
    'runtime/session_migration.cpp',
    'runtime/new_game_setup.cpp',
    'runtime/localization.cpp',
    'runtime/title_locale_data.cpp',
    'runtime/source_font.cpp',
    'runtime/native_input.cpp',
    'runtime/loading_indicator_data.cpp',
    'runtime/introduction.cpp',
    'runtime/house_inspection_data.cpp',
    'runtime/drawer_program.cpp',
    'runtime/storage_data.cpp',
    'runtime/item_details.cpp',
    'runtime/field_equipment_data.cpp',
    'runtime/item_use.cpp',
    'runtime/basement_progression.cpp',
    'runtime/basement_actor_assets.cpp',
    'runtime/music_regions.cpp',
    'runtime/present_sparkles.cpp',
    'runtime/field_psi_data.cpp',
    'runtime/field_programme_data.cpp',
    'runtime/field_inventory_data.cpp',
    'runtime/field_item_definitions_data.cpp',
    'runtime/field_goods_data.cpp',
    'runtime/world_effect_data.cpp',
    'runtime/startup_resource_admission.cpp',
    'runtime/room_music_admission.cpp',
    'runtime/field_item_admission.cpp',
    'runtime/progression.cpp',
    'runtime/battle_round.cpp',
    'runtime/session_save.cpp',
    'runtime/source_random.cpp',
)


def admit(resources):
    compiler = os.environ.get('HOST_CXX', 'c++')
    version = subprocess.check_output([compiler, '--version'])
    common = hashlib.sha256(version + Path(__file__).read_bytes())
    for header in sorted((ROOT / 'include/encore').glob('*.hpp')):
        common.update(header.name.encode())
        common.update(header.read_bytes())
    directory = Path(os.environ.get('ENCORE_RESOURCE_ADMISSION_CACHE',
                                    str(ROOT / 'build/resource-admission')))
    directory.mkdir(parents=True, exist_ok=True)
    # Match the production console warning policy. Existing loader warnings
    # remain in the raw build log; compiler errors and admission failures still
    # abort publication. This gate is not a new formatting/test requirement.
    flags = ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
             '-ffunction-sections', '-fdata-sections', '-I' + str(ROOT / 'include')]

    def compile_source(name):
        source = ROOT / name
        identity = hashlib.sha256(common.digest() + name.encode() + source.read_bytes()).hexdigest()
        target = directory / (identity + '.o')
        if not target.exists():
            pending = target.with_suffix('.pending.o')
            subprocess.run([compiler, *flags, '-c', str(source), '-o', str(pending)], check=True)
            os.replace(pending, target)
        return target

    with ThreadPoolExecutor(max_workers=4) as workers:
        objects = list(workers.map(compile_source, SOURCES))
    identity = hashlib.sha256(''.join(p.name for p in objects).encode()).hexdigest()
    executable = directory / ('admit-' + identity)
    if not executable.exists():
        pending = executable.with_suffix('.pending')
        subprocess.run([compiler, *map(str, objects), '-Wl,--gc-sections', '-o', str(pending)], check=True)
        os.replace(pending, executable)
    subprocess.run([str(executable), str(resources)], check=True)


if __name__ == '__main__':
    admit(Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else ROOT / 'romfs')
