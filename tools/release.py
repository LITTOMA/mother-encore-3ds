#!/usr/bin/env python3
"""Build an SD layout/release ZIP only when real console outputs already exist."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile
from release_notices import stage_files as notice_files
ROOT=Path(__file__).resolve().parents[1]
def main():
    out=ROOT/'dist'
    required=[out/f'encore-native.{ext}' for ext in ('3dsx','smdh','cia')]
    for path in required:
        if not path.is_file() or path.stat().st_size<512:raise SystemExit(f'Release blocked: missing {path.name}')
    if required[0].read_bytes()[:4]!=b'3DSX':raise SystemExit('Release blocked: invalid 3DSX magic')
    notices=notice_files(ROOT)
    staging=out/'sd'
    if (out.is_symlink() or staging.is_symlink() or out.resolve().parent!=ROOT or
            staging.resolve().parent!=out.resolve()):raise SystemExit('Release blocked: linked or escaped output directory')
    if staging.exists():
        if not staging.is_dir():raise SystemExit('Release blocked: SD layout is not a directory')
        shutil.rmtree(staging)
    (staging/'3ds/encore-native').mkdir(parents=True,exist_ok=True);(staging/'cias').mkdir(exist_ok=True)
    for path in required:
        dest=(staging/'cias' if path.suffix=='.cia' else staging/'3ds/encore-native')/path.name
        shutil.copy2(path,dest)
    shutil.copy2(ROOT/'docs/INSTALL.md',staging/'READ_ME_FIRST.md')
    shutil.copy2(ROOT/'docs/STATUS.md',staging/'STATUS.md')
    shutil.copy2(ROOT/'docs/LAMP_SEQUENCE.md',staging/'LAMP_SEQUENCE.md')
    shutil.copy2(ROOT/'docs/BATTLE_ENTRY.md',staging/'BATTLE_ENTRY.md')
    shutil.copy2(ROOT/'docs/BATTLE_ROUND.md',staging/'BATTLE_ROUND.md')
    shutil.copy2(ROOT/'docs/BATTLE_VICTORY.md',staging/'BATTLE_VICTORY.md')
    shutil.copy2(ROOT/'docs/DOLL_SCENE.md',staging/'DOLL_SCENE.md')
    shutil.copy2(ROOT/'docs/DOLL_ENTRY.md',staging/'DOLL_ENTRY.md')
    shutil.copy2(ROOT/'docs/DOLL_ROUND.md',staging/'DOLL_ROUND.md')
    shutil.copy2(ROOT/'docs/ITEMS_MENU.md',staging/'ITEMS_MENU.md')
    shutil.copy2(ROOT/'docs/DOLL_MELODY.md',staging/'DOLL_MELODY.md')
    shutil.copy2(ROOT/'docs/NATIVE_INPUT.md',staging/'NATIVE_INPUT.md')
    shutil.copy2(ROOT/'docs/CAROL_PHONE_RECORD.md',staging/'CAROL_PHONE_RECORD.md')
    shutil.copy2(ROOT/'docs/NATIVE_SESSION_SAVE.md',staging/'NATIVE_SESSION_SAVE.md')
    shutil.copy2(ROOT/'docs/CONTINUE_LOAD.md',staging/'CONTINUE_LOAD.md')
    shutil.copy2(ROOT/'docs/FOREGROUND_FIX.md',staging/'FOREGROUND_FIX.md')
    shutil.copy2(ROOT/'docs/HOUSE_INTERACTIONS.md',staging/'HOUSE_INTERACTIONS.md')
    shutil.copy2(ROOT/'docs/AUDIO_BACKEND.md',staging/'AUDIO_BACKEND.md')
    shutil.copy2(ROOT/'THIRD_PARTY_NOTICES.md',staging/'THIRD_PARTY_NOTICES.md')
    shutil.copy2(ROOT/'LICENSE',staging/'LICENSE')
    license_dir=staging/'licenses'
    if license_dir.exists():shutil.rmtree(license_dir)
    for path,data in notices.items():
        target=staging/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    manifest={p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in required}
    (out/'console-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    with zipfile.ZipFile(out/'encore-native-sd.zip','w',zipfile.ZIP_DEFLATED) as z:
        for path in sorted(staging.rglob('*')):
            if path.is_file():z.write(path,path.relative_to(staging))
    print('Created dist/encore-native-sd.zip (no claim of hardware validation).')
if __name__=='__main__':main()
