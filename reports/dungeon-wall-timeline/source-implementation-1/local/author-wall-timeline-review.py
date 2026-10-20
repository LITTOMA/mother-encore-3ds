from pathlib import Path
import hashlib,json,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools import dungeon_switches
from tools.extract_battle_entry import Extractor,PIN
from tools.godot_text import Document
graph,graph_proof=dungeon_switches.extract();owners=[o for o in graph['objects']if o['kind']==2];assert len(owners)==1;owner=owners[0]
ex=Extractor(r);scene='Nodes/Overworld/switches/Wall Door.tscn';script='Scripts/misc/wall door.gd';doc=Document(scene,ex.text(scene))
texture=next(p for p,k in doc.ext.values()if k=='Texture');audio=next(p for p,k in doc.ext.values()if k=='AudioStream')
existing=json.loads((r/'content/native-audio.json').read_bytes())['assets'];same=[a for a in existing if a['source_path']=='res://'+audio]
if same:assert len(same)==1;ident=same[0]['stable_id']
else:
 ident=int.from_bytes(hashlib.sha256(('encore.audio:'+audio).encode()).digest()[:4],'little')
 assert ident and ident not in {a['stable_id']for a in existing}
paths=dict(scene=scene,script=script,texture=texture,audio=audio,audio_import=audio+'.import',camera='Scripts/Main/Camera2D.gd',shaker='Scripts/misc/Shaker.gd',license='LICENSE')
proof={p:hashlib.sha256(ex.data(p)).hexdigest()for p in paths.values()}
recipe=dict(schema=1,kind='encore.dungeon-wall-timeline.reviewed-source',commit=PIN,
 scope='Source Wall Door frame/body properties, synchronous playing-property writes and deferred _shake calls only. Independent candidate metadata; no scene, texture, PCM or save admission.',
 graph_recipe='content/catacombs-switches.json',graph_review_sha256=hashlib.sha256((r/'content/catacombs-switches.json').read_bytes()).hexdigest(),
 owner=dict(scene_id=graph['scene'],id=owner['id'],path=owner['path']),paths=paths,
 roles=[dict(role=name,id=ident_,name=name)for name,ident_ in [('On',4),('RESET',7),('StayOff',5),('StayOn',6)]],
 audio_identity=dict(kind='stable',value=ident,assignment='Port-owned existing identity if present; otherwise first32 little-endian bits of SHA256(encore.audio: + source path), collision-checked against current root bank. Not an upstream identity.'),
 sources=proof,license_review='Pinned upstream LICENSE permits game-related use. Candidate carries checked identities only; no source audio, decoder binary, DSP firmware or PCM is staged by this resource compiler.')
target=r/'content/catacombs-wall-timeline.json';assert not target.exists();target.write_text(json.dumps(recipe,indent=2)+'\n',encoding='utf8')
print('Authored bounded wall timeline review; source graph owner',owner['id'],'scene',graph['scene'],'audio port ID',ident,'already in root bank',bool(same),'; no generated pack changed.')
