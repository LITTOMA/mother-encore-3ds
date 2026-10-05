#!/usr/bin/env python3
"""Append Drawer text only; all original effects execute in ENCDRP01."""
from __future__ import annotations
import copy,re
from tools.extract_battle_entry import require

def append_house(ex,house,room=None):
    from tools.drawer_program import load,text_segments
    ir=load(ex.root)
    for path,sha in ir['sources'].items():
        if hasattr(ex,'data'):ex.data(path)
        else:ex.source(path)
        require(ex.sources.get(path,ex.sources.get('upstream/MOTHER-Encore/'+path))==sha,'Drawer linking source mismatch: '+path)
    require(max(d['id'] for d in house['dialogues'])==61,'Drawer stable text prefix changed')
    require(all(d['source_path']!=ir['source_path'] for d in house['dialogues']),'Drawer source text already linked')
    hint=re.search(r'^const DIALOG_HINT_COLOR := "([0-9a-f]{6})"',ex.text('Scripts/global/text_tools.gd'),re.M)
    require(hint is not None,'Drawer hint color source')
    out=copy.deepcopy(house)
    for row in ir['texts']:
        parts=text_segments(row['raw']);first=len(out['segments'])
        for i,tokens in enumerate(parts):
            for token in tokens:
                if token['kind']==3:token['text']=hint[1]
            out['segments'].append(dict(id=len(out['segments'])+1,speaker='',voice='',tokens=tokens,flags=3 if i+1<len(parts) else 5))
        out['dialogues'].append(dict(id=row['id'],source_path=row['source_path'],first_segment=first,segment_count=len(parts)))
    out['sources']=dict(sorted(ex.sources.items()));out['scope']+='; checked Drawer phrase text with independent effect programme'
    return out
