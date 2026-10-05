#!/usr/bin/env python3
"""Append-only, checked text linking for audited literal house inspections."""
from __future__ import annotations
import copy, re
from pathlib import Path
from tools.extract_battle_entry import ROOT, require

def text_segments(raw):
    require(isinstance(raw,str) and raw.startswith('[@]'),'Inspection source bullet')
    out=[]
    for part in raw[3:].split('[WAIT@]'):
        require(part!='','Empty inspection WAIT segment'); tokens=[]; color=False
        for piece in re.split(r'(\[[^\]]*\])',part):
            if not piece: continue
            if piece in ('[Ninten]','[PartyLead]'): tokens.append(dict(kind=2,text=''))
            elif piece=='[color]':
                require(not color,'Nested inspection hint'); color=True; tokens.append(dict(kind=3,text=''))
            elif piece=='[/color]':
                require(color,'Unmatched inspection hint'); color=False; tokens.append(dict(kind=4,text=''))
            else:
                require('[' not in piece and ']' not in piece,'Unknown inspection text token: '+piece)
                tokens.append(dict(kind=1,text=piece.replace('\\n','\n')))
        require(not color,'Inspection hint crosses WAIT segment'); out.append(tokens)
    return out

def append_house(ex, house, room=None):
    from tools.house_inspection import load
    ir=load(ex.root)
    for path,sha in ir['sources'].items():
        require(ex.text(path) is not None and ex.sources[path]==sha,'Inspection linking source mismatch')
    require(ex.yaml('Data/save_new_game.yaml')['party']==['ninten'],'Inspection PartyLead requires singleton Ninten party')
    hint=ex.text('Scripts/global/text_tools.gd')
    require('global.party[0].get_nickname()' in hint,'Inspection PartyLead mapping changed')
    color=re.search(r'^const DIALOG_HINT_COLOR := "([0-9a-f]{6})"',hint,re.M)
    require(color is not None,'Inspection source hint color')
    out=copy.deepcopy(house); known={d['source_path'] for d in out['dialogues']}
    require(all(t['source_path'] not in known for t in ir['texts']),'Inspection source path already linked')
    next_id=max(d['id'] for d in out['dialogues'])+1
    for row in ir['texts']:
        first=len(out['segments']); parts=text_segments(row['raw'])
        for index,tokens in enumerate(parts):
            for token in tokens:
                if token['kind']==3: token['text']=color[1]
            out['segments'].append(dict(id=len(out['segments'])+1,speaker='',voice='',tokens=tokens,flags=3 if index+1<len(parts) else 5))
        out['dialogues'].append(dict(id=next_id,source_path=row['source_path'],first_segment=first,segment_count=len(parts))); next_id+=1
    # source ledger is owned by the surrounding extractor, matching existing linkers.
    out['sources']=ex.sources
    return out
