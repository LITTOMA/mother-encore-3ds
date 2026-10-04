#!/usr/bin/env python3
"""Restore missing upstream-derived PCM without rewriting reviewed metadata.

The source checkout, recipes, bank metadata and manifest fingerprints are gates.
Existing correct PCM is only verified; a corrupt file or changed decoder output
fails closed. No network access, source refresh, or arbitrary receipt command is
performed. FFmpeg is needed only when at least one reviewed PCM file is absent.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zlib

import audio_asset as audio
import ci_bootstrap

ROOT = Path(__file__).resolve().parents[1]
BANKS = (
    ('content/native-audio.json', 'content/asset-receipts/audio/opening.json', 'sound/banks/opening.encaudio'),
    ('content/podunk-audio.json', 'content/asset-receipts/audio/podunk.json', 'sound/banks/podunk.encaudio'),
)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def safe_target(output, relative):
    audio.safe_path(relative)
    audio.check(relative.startswith(('sound/music/', 'sound/effects/')) and relative.endswith('.pcm'), 'Unexpected PCM path')
    target = output / relative
    audio.check(not output.is_symlink() and not target.is_symlink(), 'PCM symlink is not allowed')
    audio.check(target.resolve().is_relative_to(output.resolve()), 'PCM output escaped root')
    return target


def validated_plan(project, output, banks=BANKS):
    project, output = Path(project).resolve(), Path(output).absolute()
    lock, baseline = ci_bootstrap.load_pin(project)
    upstream = project / 'upstream/MOTHER-Encore'
    plan = []
    seen = set()
    for recipe_path, manifest_path, bank_path in banks:
        recipe = json.loads((project / recipe_path).read_bytes())
        manifest = json.loads((project / manifest_path).read_bytes())
        audio.check(recipe['schema'] == manifest['schema'] == 1, 'Unsupported audio schema')
        audio.check(recipe['upstream_commit'] == manifest['upstream_commit'] == lock['commit'], 'Audio source pin mismatch')
        encoded = json.dumps(recipe, sort_keys=True, separators=(',', ':')).encode()
        audio.check(audio.sha(encoded) == manifest['recipe_sha256'], 'Reviewed audio recipe changed')
        audio.verified(upstream, recipe['bus_source'], recipe['bus_sha256'])
        audio.verified(upstream, recipe['manager_source'], recipe['manager_sha256'])
        files = {item['path']: item for item in manifest['files']}
        audio.check(len(files) == len(manifest['files']), 'Duplicate audio manifest path')
        bank_bytes = (project / 'romfs' / bank_path).read_bytes()
        audio.check(bank_path in files and len(bank_bytes) == files[bank_path]['size'] and
                    audio.sha(bank_bytes) == files[bank_path]['sha256'], 'Reviewed audio bank changed')
        bank = audio.parse_bank(bank_bytes)
        receipts = {item['pcm_path']: item for item in manifest['assets']}
        records = {item['pcm_path']: item for item in bank['assets']}
        entries = {item['pcm_path']: item for item in recipe['assets']}
        audio.check(len(receipts) == len(manifest['assets']) and len(entries) == len(recipe['assets']), 'Duplicate PCM recipe/receipt')
        audio.check(set(entries) == set(receipts) == set(records) and set(files) == set(entries) | {bank_path}, 'Audio manifest/recipe/bank path mismatch')
        for path, entry in entries.items():
            audio.check(path not in seen, 'PCM belongs to multiple banks')
            seen.add(path)
            receipt, record, item = receipts[path], records[path], files[path]
            for key, value in record.items():
                audio.check(receipt[key] == value, 'Audio receipt/bank mismatch: ' + key)
            for key in ('stable_id', 'source_path', 'source_sha256', 'import_sha256', 'pcm_path', 'gain_db'):
                audio.check(entry[key] == receipt[key], 'Audio recipe/receipt mismatch: ' + key)
            audio.check(item['sha256'] == receipt['pcm_sha256'] and item['size'] == receipt['pcm_bytes'], 'PCM fingerprint disagreement')
            source = audio.source_path(entry['source_path'])
            audio.verified(upstream, source, entry['source_sha256'])
            imported = audio.verified(upstream, source + '.import', entry['import_sha256'])
            loop, offset = audio.import_settings(imported, entry['source_path'])
            rate = entry.get('output_sample_rate', receipt['source_sample_rate'])
            audio.check(rate == receipt['sample_rate'] and loop == receipt['loop'] and
                        offset == receipt['loop_offset_seconds'] and int(offset * rate) == receipt['loop_start'], 'Audio importer/output settings changed')
            target = safe_target(output, path)
            if target.exists():
                audio.check(target.is_file() and target.stat().st_size == item['size'] and sha(target) == item['sha256'],
                            'Existing PCM differs from reviewed bytes; refusing overwrite: ' + path)
            plan.append(dict(path=path, target=target, source=upstream / source,
                             receipt=receipt, missing=not target.exists()))
    return plan, upstream, lock, baseline


def restore(project=ROOT, output=None, ffmpeg='ffmpeg', ffprobe='ffprobe', banks=BANKS):
    project = Path(project).resolve()
    output = Path(output) if output is not None else project / 'romfs'
    plan, upstream, lock, baseline = validated_plan(project, output, banks)
    missing = [item for item in plan if item['missing']]
    if not missing:
        return dict(status='verified', assets=len(plan), restored=0, converted_bytes=0)
    # Verify the real reviewed checkout before decoding any source. This never
    # fetches, resets, imports or mutates the upstream.
    ci_bootstrap.verify(upstream, lock, baseline)
    encoder, probe = shutil.which(ffmpeg), shutil.which(ffprobe)
    audio.check(encoder and probe, 'Missing FFmpeg/ffprobe. Install FFmpeg; reviewed reference is 7.1.5. PCM must match checked hashes.')
    converted = 0
    for item in missing:
        target, receipt = item['target'], item['receipt']
        info = json.loads(subprocess.run([probe, '-v', 'error', '-show_entries',
            'stream=codec_type,channels,sample_rate', '-of', 'json', str(item['source'])],
            check=True, capture_output=True, text=True).stdout)
        streams = info.get('streams', [])
        audio.check(len(streams) == 1 and streams[0]['codec_type'] == 'audio' and
                    int(streams[0]['sample_rate']) == receipt['source_sample_rate'] and
                    int(streams[0]['channels']) == receipt['channels'], 'Decoded source format changed')
        target.parent.mkdir(parents=True, exist_ok=True)
        safe_target(output, item['path'])
        with tempfile.TemporaryDirectory(prefix='.restore-audio-', dir=target.parent) as temporary:
            candidate = Path(temporary) / 'decoded.pcm'
            command = [encoder, '-nostdin', '-v', 'error', '-i', str(item['source']),
                '-map', '0:a:0', '-vn', '-sn', '-dn', '-c:a', 'pcm_s16le', '-f',
                's16le', '-bitexact', '-threads', '1']
            if receipt['sample_rate'] != receipt['source_sample_rate']:
                command += ['-ar', str(receipt['sample_rate'])]
            subprocess.run(command + [str(candidate)], check=True, capture_output=True)
            data = candidate.read_bytes()
            audio.check(len(data) == receipt['pcm_bytes'] and audio.sha(data) == receipt['pcm_sha256'] and
                        zlib.crc32(data) & 0xffffffff == receipt['pcm_crc32'],
                        'FFmpeg output differs from reviewed PCM for ' + item['path'] +
                        '; no file published. Use the documented reference decoder, do not refresh expected hashes.')
            # Atomic create-if-absent, never clobber a file that appeared while decoding.
            os.link(candidate, target)
            converted += len(data)
    return dict(status='restored', assets=len(plan), restored=len(missing), converted_bytes=converted)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, help='Optional isolated PCM destination for reproducibility checks')
    args = parser.parse_args()
    try:
        print(json.dumps(restore(output=args.output), indent=2))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print('Audio restoration blocked: ' + str(error))
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
