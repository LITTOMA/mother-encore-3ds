"""Test-only projection of audited directory relocation; never a runtime alias.

Actual old 33e246f packs were compared section by section with current packs:
all semantics match after path dereferencing. See FULL_CONTENT_REGRESSIONS.md.
The old prefix hashes remain unchanged; unknown current hashes are rejected.
"""
import copy,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
MOVES={'actor-preview/':'graphics/actors/','house-layers/':'graphics/world/house-layers/','map-preview/':'graphics/world/house-map/','house-preview/':'graphics/ui/house/','world-effect/':'data/'}
RELOCATED={24:('battle/lamp/opening.encbattle','0211ff0c02be4a9f4de74c080d47c52a9fb80e3f1ebc87821e109d93f4f45d4f'),27:('battle/doll/doll-entry.encbattle','60977b67c72352c56aea3c2f807254a4892afb77835ba55e580217aac496f27d')}
def historical_room(ir):
 folder=ROOT/'content/legacy-rules6';raw=(folder/'native-opening.json').read_bytes();manifest=json.loads((folder/'manifest.json').read_text(encoding='utf8'))
 if hashlib.sha256(raw).hexdigest()!=manifest['recipes']['native-opening.json']:raise ValueError('Frozen Room recipe changed')
 old=json.loads(raw);result=copy.deepcopy(ir)
 for i,text in enumerate(old['strings']):
  expected=text
  for before,after in MOVES.items():
   if text.startswith(before):expected=after+text[len(before):];break
  if result['strings'][i]!=expected:raise ValueError('Unreviewed Room string change: '+str(i))
  result['strings'][i]=text
 for identity,(path,sha)in RELOCATED.items():
  row=result['sections']['Resource'][identity-1];previous=old['sections']['Resource'][identity-1]
  if row['stable_id']!=identity or row['sha256']!=sha or hashlib.sha256((ROOT/'romfs'/path).read_bytes()).hexdigest()!=sha:raise ValueError('Unreviewed relocated battle resource: '+str(identity))
  row['sha256']=previous['sha256']
 return result

def historical_house_presentation(ir):
 result=copy.deepcopy(ir)
 for row in result['resources']:
  path=row['path']
  if path.startswith('graphics/ui/house/'):
   if hashlib.sha256((ROOT/'romfs'/path).read_bytes()).hexdigest()!=row['sha256']:raise ValueError('Changed relocated House texture')
   row['path']='house-preview/'+path[len('graphics/ui/house/'):]
 return result

def historical_audio(ir):
 result=copy.deepcopy(ir)
 for asset in result['assets']:
  if asset['pcm_path'].startswith(('sound/music/','sound/effects/')):
   asset['pcm_path']='audio/'+asset['pcm_path'].rsplit('/',1)[1]
 return result
