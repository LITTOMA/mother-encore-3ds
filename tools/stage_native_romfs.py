#!/usr/bin/env python3
"""Stage only checked native content and referenced textures, excluding M0 fixtures."""
import hashlib,shutil,json
from pathlib import Path
from native_content import parse_pack
from audio_asset import stage_files as audio_files
from native_battle import stage_files as battle_files
from native_round import stage_files as round_files
from native_house import stage_files as house_files
from native_items import stage_files as item_files
ROOT=Path(__file__).resolve().parents[1]
def main():
    source=ROOT/'romfs';target=ROOT/'build/ctr/native-romfs'
    from resource_catalog import decode, stage_files as catalog_files
    catalog=decode((source/'data/native.encresources').read_bytes())
    catalog_bindings={b['id']:b['path'] for b in catalog['bindings']}
    companions={catalog_bindings[e['battle_id']]:catalog_bindings[e['round_id']] for e in catalog['encounters']}
    blob=(source/'data/opening.encroom').read_bytes();room=parse_pack(blob)
    files={Path('data/opening.encroom'):blob}
    files.update(audio_files(source))
    files.update(battle_files(source))
    files.update(round_files(source))
    files.update(house_files(source))
    from house_inspection import stage_files as inspection_files
    files.update(inspection_files(source))
    from drawer_program import stage_files as drawer_files
    files.update(drawer_files(source))
    from storage_assets import stage_files as storage_files
    from native_storage import stage_files as storage_data_files
    files.update(storage_files(source))
    files.update(storage_data_files(source))
    files.update(item_files(source))
    from native_input import stage_files as input_files
    files.update(input_files(source))
    from native_phone import stage_files as phone_files
    files.update(phone_files(source))
    from dialogue_choice_assets import stage_files as choice_files
    from save_menu_assets import stage_files as save_files
    files.update(choice_files(source))
    files.update(save_files(source))
    from native_session import stage_files as session_files
    files.update(session_files(source))
    from session_migration import stage_files as migration_files
    files.update(migration_files(source))
    from native_restore import stage_files as restore_files
    from continue_assets import stage_files as continue_files
    files.update(restore_files(source))
    files.update(continue_files(source))
    from loading_indicator_assets import stage_files as loading_files
    files.update(loading_files(source))
    from new_game_assets import stage_files as naming_files
    files.update(naming_files(source))
    from startup_settings_assets import stage_files as startup_settings_files
    files.update(startup_settings_files(source))
    from house_button_prompt_assets import stage_files as house_prompt_files
    files.update(house_prompt_files(source))
    from blackbars_assets import stage_files as blackbar_files
    files.update(blackbar_files(source))
    from localization_assets import stage_files as localization_files
    from source_fonts import stage_files as source_font_files
    from title_locale_assets import stage_files as title_locale_files
    files.update(localization_files(source))
    files.update(source_font_files(source))
    files.update(title_locale_files(source))
    from release_notices import stage_files as notice_files
    files.update(notice_files(ROOT))
    for resource in room['sections']['Resource']:
        if resource['kind'] not in (1,3,4):continue # typed requests have no playback data
        if resource['kind']==3:
            path=Path(room['strings'][resource['path_string']])
            files.update(battle_files(source,path))
            if path.as_posix() not in companions:raise ValueError('Missing checked encounter companion')
            files.update(round_files(source,Path(companions[path.as_posix()])))
        path=Path(room['strings'][resource['path_string']])
        if path.is_absolute()or'..'in path.parts:raise ValueError('Unsafe native resource path')
        data=(source/path).read_bytes()
        if hashlib.sha256(data).hexdigest()!=resource['sha256']:raise ValueError('Texture fingerprint mismatch: '+str(path))
        if resource['kind']==4:
            from world_effect_assets import IR,verify_sources,encode
            ir=json.loads(IR.read_text());verify_sources(ir)
            if data!=encode(ir):raise ValueError("Stale checked world effect")
        files[path]=data
    from native_introduction import stage_files as introduction_files
    from introduction_assets import stage_files as introduction_asset_files
    files.update(introduction_files(source))
    files.update(introduction_asset_files(source))
    files.update(catalog_files(source, files))
    from romfs_layout import check_layout
    check_layout(files)
    temporary=target.with_name(target.name+'-pending')
    if temporary.exists():shutil.rmtree(temporary)
    temporary.mkdir(parents=True)
    for path,data in files.items():
        out=temporary/path;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(data)
    if target.exists():shutil.rmtree(target)
    temporary.rename(target)
    print(f'Staged {len(files)} native RomFS files; no M0 fixture')
if __name__=='__main__':main()
