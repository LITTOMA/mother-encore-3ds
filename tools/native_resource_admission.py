#!/usr/bin/env python3
import json
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
    'runtime/podunk_bundle.cpp',
    'runtime/field_scene_destination.cpp',
    'runtime/house_return_sources.cpp',
    'runtime/house_reentry_data.cpp',
    'runtime/field_geometry.cpp',
    'runtime/field_npc_data.cpp',
    'runtime/field_npc_world_data.cpp',
    'runtime/field_visibility_data.cpp',
    'runtime/field_sprite_bridge_data.cpp',
    'runtime/field_sprite_npc_binding.cpp',
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
    'runtime/global_yaml_caches_data.cpp',
    'runtime/global_yaml_caches.cpp',
    'runtime/global_packed_directory_data.cpp',
    'runtime/global_yaml_file_data.cpp',
    'runtime/field_global_data_data.cpp',
    'runtime/global_data_constructor_data.cpp',
    'runtime/field_character_load_data.cpp',
    'runtime/global_load_data.cpp',
    'runtime/field_global_constructor_data.cpp',
    'runtime/player_initialization_data.cpp',
    'runtime/player_visual_scripts_data.cpp',
    'runtime/player_graphics_data.cpp',
    'runtime/player_motion_data.cpp',
    'runtime/player_ready_data.cpp',
    'runtime/player_effects_data.cpp',
    'runtime/global_child_ready_data.cpp',
    'runtime/global_ready_data.cpp',
    'runtime/player_fetcher_data.cpp',
    'runtime/player_resources_data.cpp',
    'runtime/house_global_bridge_data.cpp',
    'runtime/house_status_effects_data.cpp',
    'runtime/house_ui_continuation_data.cpp',
    'runtime/field_ui_manager_data.cpp',
    'runtime/field_door.cpp',
    'runtime/player_child_scripts_data.cpp',
    'runtime/player_native_timer_data.cpp',
    'runtime/field_game_camera_data.cpp',
    'runtime/field_camera_arrows_data.cpp',
    'runtime/field_node_tree_data.cpp',
    'runtime/field_native_timer_data.cpp',
    'runtime/field_global_flags_data.cpp',
    'runtime/field_global_registry_data.cpp',
    'runtime/field_node_recipe_data.cpp',
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


def admit(resources, *, global_items=True):
    compiler = os.environ.get('HOST_CXX', 'c++')
    version = subprocess.check_output([compiler, '--version'])
    common = hashlib.sha256(version + Path(__file__).read_bytes())
    for header in sorted((ROOT / 'include/encore').glob('*.hpp')):
        common.update(header.name.encode())
        common.update(header.read_bytes())
    common.update((ROOT / 'runtime/global_yaml_file_hash.hpp').read_bytes())
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
    command = [str(executable), str(resources)]
    if global_items:
        # Independent reviewed compiler inputs supply the expected Registry
        # identity; do not trust a candidate resource's own header as its proof.
        sys.path.insert(0,str(ROOT))
        from tools.field_global_registry import load as load_registry
        from tools.global_yaml_caches import load as load_caches
        registry = load_registry()
        caches = load_caches()
        command += ['--global-items', str(ROOT / 'romfs/data/global.encfielditems'),
                    '--global-caches', str(ROOT / 'romfs/data/global.encyamlcaches'),
                    str(ROOT / 'romfs/data/global.encregistry'),
                    str(registry['scene_id']), registry['commit'],
                    registry['source_sha256'], caches['owner'],
                    '--global-directory', str(ROOT / 'romfs/data/global.encpackeddir'),
                    '--global-yaml-file', str(ROOT / 'romfs/data/global.encyamlfile'),
                    '--global-constructor', str(ROOT / 'romfs/data/global.encconstructor'),
                    str(ROOT / 'romfs/data/global.encdata')]
        from tools.field_character_load import derive as derive_characters
        from tools.podunk_scene import stable
        characters = derive_characters()
        command += ['--global-characters', str(ROOT / 'romfs/data/global.enccharacterload'),
                    str(stable(characters['source_save'])), characters['commit'],
                    characters['sources'][characters['source_save']]]
        command += ['--global-load', str(ROOT / 'romfs/data/global.encload'),
                    str(ROOT / 'romfs/data/global.encflags')]
        node_constructor = json.loads((ROOT / 'content/native-field-global-constructor.json').read_text())
        command += ['--global-node-constructor', str(ROOT / 'romfs/data/global.encnodeconstructor'),
                    str(node_constructor['scene_id']), node_constructor['source_sha256']]
        command += ['--player-initialization', str(ROOT / 'romfs/data/player.encinitialization'),
                    '--global-child-ready', str(ROOT / 'romfs/data/global.encchildready')]
        command += ['--player-visual-scripts', str(ROOT / 'romfs/data/player.encvisualscripts'),
                    '--player-ready', str(ROOT / 'romfs/data/player.encready'),
                    '--player-effects', str(ROOT / 'romfs/data/player.enceffects'),
                    '--player-graphics', str(ROOT / 'romfs/data/player.encgraphics'),
                    '--player-motion', str(ROOT / 'romfs/data/player.encmotion'),
                    '--global-ready', str(ROOT / 'romfs/data/global.encready')]
        tree = json.loads((ROOT / 'content/podunk-node-tree.json').read_text())
        command += ['--player-fetcher', str(ROOT / 'romfs/data/player.encfetcher'),
                    str(ROOT / 'romfs/data/podunk.encnodetree'),
                    str(tree['scene_id']), tree['source_sha256']]
        command += ['--player-resources', str(ROOT / 'romfs/data/player.encresources')]
        command += ['--house-global-bridge', str(ROOT / 'romfs/data/house.encglobalbridge'),
                    '--player-child-scripts', str(ROOT / 'romfs/data/player.encchildren'),
                    '--house-status-effects', str(ROOT / 'romfs/data/house.encstatuseffects')]
        ui = json.loads((ROOT / 'content/field-ui-manager.json').read_text(encoding='utf-8'))
        house = json.loads((ROOT / 'content/native-house-exit-door.json').read_text(encoding='utf-8'))
        command += ['--house-ui-continuation', str(ROOT / 'romfs/data/house.encuicontinuation'),
                    str(ROOT / 'romfs/data/global.encuimanager'), str(ui['scene_id']), ui['source_sha256'],
                    '--house-exit-door', str(ROOT / 'romfs/data/house.encdoor'),
                    str(house['scene_id']), house['source_sha256']]
    subprocess.run(command, check=True)


if __name__ == '__main__':
    admit(Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else ROOT / 'romfs')
