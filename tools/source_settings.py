"""Reviewed source settings choices, shared by session and startup resource compilers."""
import json
from tools.extract_battle_entry import one,require

def choices(ex):
    source=ex.text('Scripts/global/globalData.gd')
    result={key:json.loads(one(r'^const '+name+r' := (\[[^\n]+\])',source,name)[1]) for key,name in [('speeds','TEXT_SPEEDS'),('speed_names','TEXT_SPEEDS_NAMES'),('flavors','FLAVORS'),('prompts','BUTTON_PROMPTS')]}
    require(len(result['speeds'])==len(result['speed_names'])==3 and len(result['flavors'])==7 and len(result['prompts'])==4,'Unreviewed source settings options')
    return result
