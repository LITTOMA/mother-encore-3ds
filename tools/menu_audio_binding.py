"""Resolve original named menu sounds to checked audio-resource identities."""
import hashlib,json,re
from pathlib import Path
from tools.extract_battle_entry import Extractor,require
ROOT=Path(__file__).resolve().parents[1]
def source_sound(name):
    require(isinstance(name,str) and re.fullmatch(r'[a-z][a-z0-9_]*',name),'Invalid source sound name')
    recipe=json.loads((ROOT/'content/native-audio.json').read_text())
    ex=Extractor(ROOT)
    require(recipe['upstream_commit']==ex.lock['commit'],'Menu audio pin mismatch')
    raw=ex.data(recipe['manager_source'])
    require(hashlib.sha256(raw).hexdigest()==recipe['manager_sha256'],'Changed menu audio manager')
    matches=re.findall(r'^\s*"'+re.escape(name)+r'": load\("(res://[^"\n]+)"\),?\s*$',raw.decode(),re.M)
    require(len(matches)==1,'Missing or ambiguous named menu sound')
    path=matches[0]
    assets=[asset for asset in recipe['assets'] if asset['source_path']==path]
    require(len(assets)==1,'Menu sound missing from compiled audio recipe')
    require(hashlib.sha256(ex.data(path[6:])).hexdigest()==assets[0]['source_sha256'],'Changed menu sound source')
    return path[6:]
