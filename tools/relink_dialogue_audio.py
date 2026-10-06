#!/usr/bin/env python3
"""Relink an unchanged reviewed DialogueBox to an admitted audio-bank closure.

This is a binding step, not a scene/native re-extraction. It cannot introduce a
stream, change its stable identity, change the bus or alter a reviewed node.
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import field_dialogue_audio as source
from tools import audio_asset


def verify_bank_projection(reviewed, bank):
    source.require(bank['upstream_commit'] == reviewed['commit'],
                   'Dialogue audio bank source pin differs')
    source.require(bank['bus_sha256'] == reviewed['sources'][source.BUS],
                   'Dialogue audio bus source changed; semantic review required')
    projection = [dict(id=a['stable_id'], source=a['source_path'],
                       sha256=a['source_sha256']) for a in bank['assets']
                  if a['source_path'].endswith('.mp3')]
    source.require(projection == reviewed['assets'],
                   'Dialogue audio stream closure changed; source extraction required')
    source.require(len({a['id'] for a in projection}) == len(projection) and
                   len({a['source'] for a in projection}) == len(projection),
                   'Dialogue audio duplicate bank identity')
    source.require(all(any(a['id'] == n['stream'] for a in projection)
                       for n in reviewed['nodes']),
                   'Dialogue audio default stream absent')


def relink():
    d, r = source.read(source.IR), source.read(source.REVIEW)
    source.require(d['schema'] == 1 and d['commit'] == source.PIN and
                   not d['scene_admitted'] and
                   r['ir_sha256'] == source.sha(source.IR) and
                   r['producer_sha256'] == source.sha(source.Path(source.__file__)) and
                   d['recipe_sha256'] == source.sha(source.ROOT / 'content/dialogue-node-recipe.json'),
                   'Dialogue audio reviewed scene/producer changed')
    inventory = source.read(source.ROOT / 'compatibility/upstream-inventory.json')['files']
    for path, digest in d['sources'].items():
        source.require(digest == inventory[path]['sha256'] ==
                       source.sha(source.ROOT / 'upstream/MOTHER-Encore' / path),
                       'Dialogue audio original source changed: ' + path)
    bank = source.read(source.ROOT / 'content/native-audio.json')
    verify_bank_projection(d, bank)
    # The predecessor audio target has emitted and admitted the actual bank and
    # PCM; inspect those bytes too rather than trusting a renamed JSON digest.
    audio_asset.stage_files(source.ROOT / 'romfs')
    digest = source.sha(source.ROOT / 'content/native-audio.json')
    if d['bank_source_sha256'] != digest:
        d['bank_source_sha256'] = digest
        source.write(source.IR, d)
        r['ir_sha256'] = source.sha(source.IR)
        source.write(source.REVIEW, r)
    source.load()
    print('Dialogue audio relink: original streams/nodes/bus preserved')


if __name__ == '__main__':
    relink()
