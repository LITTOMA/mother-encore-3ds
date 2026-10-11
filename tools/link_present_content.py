#!/usr/bin/env python3
"""Append House present text only; grant, clip, sound and flag effects execute in ENCPRS01."""
from __future__ import annotations
import copy
from tools.extract_battle_entry import require


def append_house(ex, house, room=None):
    from tools.house_presents import load, FIRST_TEXT_ID
    from tools.link_house_inspections import text_segments
    ir = load(ex.root)
    for path, sha in ir['sources'].items():
        ex.data(path)
        require(ex.sources[path] == sha, 'Present linking source mismatch: ' + path)
    require(max(d['id'] for d in house['dialogues']) == FIRST_TEXT_ID - 1, 'Present stable text prefix changed')
    known = {d['source_path'] for d in house['dialogues']}
    require(all(t['source_path'] not in known for t in ir['texts']), 'Present source text already linked')
    require([t['id'] for t in ir['texts']] == list(range(FIRST_TEXT_ID, FIRST_TEXT_ID + len(ir['texts']))), 'Present text IDs not contiguous')
    out = copy.deepcopy(house)
    for row in ir['texts']:
        parts = text_segments(row['raw'])
        first = len(out['segments'])
        for i, tokens in enumerate(parts):
            out['segments'].append(dict(id=len(out['segments']) + 1, speaker='', voice='', tokens=tokens,
                                        flags=3 if i + 1 < len(parts) else 5))
        out['dialogues'].append(dict(id=row['id'], source_path=row['source_path'], first_segment=first, segment_count=len(parts)))
    out['sources'] = dict(sorted(ex.sources.items()))
    out['scope'] += '; original House present text with independent present programme'
    return out
