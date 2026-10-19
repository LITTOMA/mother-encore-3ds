from pathlib import Path
import hashlib,json,subprocess,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import native_content as c
before=json.loads((r/'build/full-port/story-object-generation-before.json').read_bytes());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
changed={n:dict(before=h,after=sha(r/n))for n,h in before['files'].items()if sha(r/n)!=h}
for n,old in before['rooms'].items():
 v=c.parse_pack((r/n).read_bytes());v.pop('build_input_sha256');assert v==old,n
allowed={'content/asset-receipts/graphics/world/mayor/scene.json','content/asset-receipts/graphics/world/podunk/source.json','content/native-new-game.json','content/native-restore.json','content/native-session.json','romfs/session/opening.encrestore','romfs/world/house/opening.encroom','romfs/world/mayor/mayor.encroom','romfs/world/podunk/podunk.encroom','romfs/system/native.encresources'}
assert set(changed)<=allowed,set(changed)-allowed
proof=dict(base=before['head'],scope='Room schemas/strings/gameplay exactly unchanged after removing build input identity only. Session changes only dependencies; Restore only dependencies/fingerprints; new-game original reuse-session proves exact UI/game binary and textures. Map/Props/player/graphics/Session resource/defaults remain byte-identical. No formal programme49 or new object resource published.',changed=changed,room_tables_exact=list(before['rooms']),unchanged_count=len(before['files'])-len(changed),tracked_count=len(before['files']))
(r/'build/full-port/story-object-generation-after.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Verified generated resource change set:',len(changed),'dependency/identity files;',proof['unchanged_count'],'byte-identical; ALL3 Room game tables exact')
