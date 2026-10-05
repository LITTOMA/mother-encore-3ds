"""Source text input names mapped to already checked native control masks."""
import json
from pathlib import Path
from tools.extract_battle_entry import require

def load(root):
    root=Path(root)
    adapter=json.loads((root/'content/story-input-bindings.json').read_text(encoding='utf-8'))
    require(set(adapter)=={'schema','kind','scope','bindings'} and adapter['schema']==1 and adapter['kind']=='encore.story-input.native-adapter','Story input adapter schema')
    require(type(adapter['scope']) is str and 0<len(adapter['scope'])<=1024,'Story input scope')
    equipment=json.loads((root/'content/native-field-equipment.json').read_text(encoding='utf-8'))
    labels={};masks=set()
    for b in adapter['bindings']:
        require(set(b)=={'action','parameter','mask','label'} and b['action'] in ('ui_select','ui_accept','ui_toggle'),'Unreviewed story input action')
        require(b['parameter']=={'ui_select':'OpenMask','ui_accept':'ConfirmMask','ui_toggle':'CancelMask'}[b['action']] and type(b['mask']) is int and equipment['parameters'][b['parameter']]==b['mask'],'Story input mask differs from executing adapter')
        require(b['action'] not in labels and b['mask'] not in masks and type(b['label']) is str and 0<len(b['label'])<=16 and b['label'].isascii(),'Story input duplicate/label')
        labels[b['action']]=b['label'];masks.add(b['mask'])
    require(set(labels)=={'ui_select','ui_accept','ui_toggle'},'Incomplete story input adapter')
    pillow=json.loads((root/'content/pillow-input.json').read_text(encoding='utf-8'))
    require(pillow['label']==labels[pillow['action']],'Story input differs from reviewed tutorial')
    return labels
