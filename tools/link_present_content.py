#!/usr/bin/env python3
"""Append House present text only; grant, clip, sound and flag effects execute in ENCPRS01."""
from __future__ import annotations
import copy,re
from tools.extract_battle_entry import require


def append_house(ex, house, room=None):
    from tools.house_presents import load, FIRST_TEXT_ID
    from tools.link_house_inspections import text_segments
    ir = load(ex.root)
    for path, sha in ir['sources'].items():
        ex.data(path)
        require(ex.sources[path] == sha, 'Present linking source mismatch: ' + path)
    groups = [g for g in ir['text_groups'] if g['after_id'] == max(d['id'] for d in house['dialogues'])]
    require(len(groups) == 1, 'Present stable text prefix changed')
    rows = [t for t in ir['texts'] if t['id'] in groups[0]['ids']]
    known = {d['id'] for d in house['dialogues']}
    require(all(t['id'] not in known for t in rows), 'Present source text already linked')
    require([t['id'] for t in rows] == groups[0]['ids'], 'Present text IDs differ from admitted append group')
    out = copy.deepcopy(house)
    for row in rows:
        parts = text_segments(row['raw'])
        hint=re.search(r'^const DIALOG_HINT_COLOR := "([0-9a-f]{6})"',ex.text('Scripts/global/text_tools.gd'),re.M)
        require(hint is not None,'Present hint color missing')
        for tokens in parts:
            for token in tokens:
                if token['kind']==3:token['text']=hint[1]
        first = len(out['segments'])
        for i, tokens in enumerate(parts):
            out['segments'].append(dict(id=len(out['segments']) + 1, speaker=row.get('speaker',{}).get('en',''), voice='', tokens=tokens,
                                        flags=3 if i + 1 < len(parts) else 5))
        out['dialogues'].append(dict(id=row['id'], source_path=row['source_path'], first_segment=first, segment_count=len(parts)))
    out['sources'] = dict(sorted(ex.sources.items()))
    out['scope'] += '; original House present text with independent present programme'
    return out
