#!/usr/bin/env python3
"""Prepare real Doll source packs only on an explicit build preparation run.

The default command compares existing fixtures with freshly extracted bytes;
it never creates directories or writes shared files. No production pack changes.
"""
import argparse
import copy
import json
import os
from pathlib import Path
import sys

ROOT = Path(os.environ.get('ENCORE_SOURCE_ROOT', Path(__file__).resolve().parents[1])).resolve()
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT))
import boss_presentation_bindings as bindings
import doll_round
import native_round


def fixtures():
    recipe = bindings.load(ROOT)
    baseline = doll_round.build()
    assert baseline == json.loads((ROOT / 'content/doll-round.json').read_text(encoding='utf-8'))
    tables = native_round.lower(baseline, root=ROOT)
    original = native_round.encode(tables)
    assert original == (ROOT / 'romfs/data/doll-entry.encround').read_bytes()
    candidate = copy.deepcopy(recipe)
    candidate['animations']['enemy_defeat']['anchor'] = [.25, .5]
    candidate['animations']['defeat_flash']['anchor'] = [.125, .25]
    changed = doll_round.build(candidate)
    # Prove that the recipe mutation leaves every source-derived fact intact.
    normalized = copy.deepcopy(changed)
    enemy = baseline['presentation']['bindings']['EnemyDefeat']
    flash = baseline['encounter']['boss_flash_media']
    for index in (enemy, flash):
        normalized['presentation']['media'][index]['anchor'] = baseline['presentation']['media'][index]['anchor']
    assert normalized == baseline
    altered = native_round.encode(native_round.lower(changed, root=ROOT))
    assert altered != original
    native_round.parse_pack(original)
    native_round.parse_pack(altered)
    bad_anchor = copy.deepcopy(tables)
    bad_anchor['Media'][enemy][-2] = 2.
    corrupted = bytearray(original)
    corrupted[-1] ^= 1
    initial = json.loads((ROOT / 'content/native-battle.json').read_text(encoding='utf-8'))
    skills = initial['party']['initial_save_data']['learnedSkills']
    assert all(isinstance(skill, str) and skill and not any(c.isspace() for c in skill) for skill in skills)
    return {
        'baseline.encround': original,
        'anchor.encround': altered,
        'bad-crc.encround': bytes(corrupted),
        'bad-anchor.encround': native_round.encode(bad_anchor),
        'truncated.encround': original[:63],
        'learned-skills.txt': ('\n'.join(skills) + '\n').encode('utf-8'),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/boss-consumer-fixtures')
    args = parser.parse_args()
    output = args.output.resolve()
    # Preparation is restricted to this checkout's ignored build directory.
    if ROOT / 'build' not in output.parents:
        parser.error('fixture output must be a directory inside the checkout build directory')
    data = fixtures()
    if args.prepare_only:
        output.mkdir(parents=True, exist_ok=True)
        for name, blob in data.items():
            (output / name).write_bytes(blob)
        print('Prepared six checked source Doll consumer fixtures under build')
    else:
        for name, blob in data.items():
            if (output / name).read_bytes() != blob:
                raise ValueError('Stale boss consumer fixture: ' + name)
        print('Six source Doll consumer fixtures reproduced exactly (read-only)')


if __name__ == '__main__':
    main()
