#!/usr/bin/env python3
"""Record acquired source/release evidence; compare the downloaded archive to every Git blob."""
import hashlib
import json
from pathlib import Path
import sys
import tarfile
from datetime import datetime, timezone
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.upstream import read_json, write_json, snapshot

def compare_archive(archive: Path,files: dict)->None:
    seen=set()
    with tarfile.open(archive,'r:gz') as tar:
        for member in tar:
            if member.isdir():continue
            if not member.isfile():raise ValueError('Archive special entry requires review')
            parts=member.name.split('/',1)
            if len(parts)!=2:raise ValueError('Archive layout mismatch')
            name=parts[1]
            if name not in files or name in seen:raise ValueError('Archive extra/duplicate source: '+name)
            record=files[name]
            if member.size!=record['bytes']:raise ValueError('Archive/Git length differs: '+name)
            data=tar.extractfile(member).read(record['bytes']+1)
            if hashlib.sha256(data).hexdigest()!=record['sha256'] or len(data)!=record['bytes']:
                raise ValueError('Archive/Git bytes differ: '+name)
            seen.add(name)
    if seen!=set(files):raise ValueError('Archive omits tracked source')

def main():
    inv=snapshot(ROOT/'upstream/MOTHER-Encore');lock=read_json(ROOT/'upstream.lock');evidence=ROOT/'build/m1'
    if inv['commit']!=lock['commit'] or inv['tree']!=lock['tree']:raise ValueError('Source pin mismatch')
    archive=evidence/'upstream-source.tar.gz';compare_archive(archive,inv['files'])
    download=(evidence/'official-download.html').read_text(encoding='utf-8')
    version=lock['game_version']
    if f'ACT 2 v{version} (Windows)' not in download or f'ACT 2 v{version} (Linux)' not in download:
        raise ValueError('Observed official release label differs from source version')
    result={'schema':1,'observed_at_utc':datetime.now(timezone.utc).isoformat(),
        'repository':lock['repository'],'commit':inv['commit'],'tree':inv['tree'],'game_version':version,
        'commit_metadata':read_json(evidence/'remote-commit.json')['commit'],
        'official_download_url':'https://mother-encore.itch.io/mother-encore',
        'official_announcement_url':'https://mother-encore.itch.io/mother-encore/devlog/1630037/fall-2026-news-update',
        'release_identity':'Official public download label and source GAME_VERSION agree. No upstream tag or signed binary-to-commit attestation; exact correspondence is inferred, not certified.',
        'github_releases_observed':read_json(evidence/'remote-releases.json'),
        'github_tags_observed':read_json(evidence/'remote-tags.json'),
        'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'archive_git_byte_comparison':'all tracked blobs equal',
        'tracked_blob_count':len(inv['files']),'tracked_bytes':sum(r['bytes'] for r in inv['files'].values()),
        'lfs_pointers':[p for p,r in inv['files'].items() if r['kind']=='lfs_pointer_requires_fetch'],
        'evidence_sha256':{},'godot_tools':{},'reference_game_status':'imports/startup attempted with unresolved errors; no complete gameplay validation',
        'source_semantic_compatibility':'not established; two progression functions reviewed separately'}
    destination=ROOT/'reports/m1-source-evidence';destination.mkdir(parents=True,exist_ok=True)
    for name in ('remote-releases.json','remote-tags.json','remote-commit.json','repository.json','official-download.html','official-announcement.html'):
        data=(evidence/name).read_bytes();(destination/name).write_bytes(data)
        result['evidence_sha256'][name]=hashlib.sha256(data).hexdigest()
    sums={line.split()[1]:line.split()[0] for line in (evidence/'godot/SHA512-SUMS.txt').read_text().splitlines()}
    for local,name in [('headless.zip','Godot_v3.6.2-stable_linux_headless.64.zip'),('windows.zip','Godot_v3.6.2-stable_win64.exe.zip')]:
        actual=hashlib.sha512((evidence/'godot'/local).read_bytes()).hexdigest()
        if actual!=sums[name]:raise ValueError('Official Godot checksum mismatch')
        result['godot_tools'][name]={'sha512':actual,'checksum_source':'https://github.com/godotengine/godot-builds/releases/download/3.6.2-stable/SHA512-SUMS.txt'}
    (destination/'Godot-SHA512-SUMS.txt').write_bytes((evidence/'godot/SHA512-SUMS.txt').read_bytes())
    write_json(ROOT/'reports/m1-source-acquisition.json',result)
    print('Verified all source archive blobs against pristine Git checkout; saved observed release evidence.')

if __name__=='__main__':main()
