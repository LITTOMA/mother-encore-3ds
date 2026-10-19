from pathlib import Path
import sys,json,hashlib
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools.emulator_artifact import checked_resources
from tools import resource_catalog as c
package=r/'build/full-port/player7d-current/verified-artifact';receipt=json.loads((package/'build-receipt.json').read_bytes());source=receipt['source_commit'];assert source=='7d2e552f86c9ef0c995c85c301e3a2f26a78f32a';game=package/'3ds/encore-native/encore-native.3dsx';raw=game.read_bytes();assert hashlib.sha256(raw).hexdigest()==receipt['artifacts']['encore-native.3dsx']['sha256'];catalog=checked_resources(raw,source,['romfs/system/native.encresources'],r)['romfs/system/native.encresources'];bindings=c.decode(catalog)['bindings']
# Only the independent Props/FieldScene consumer's actual metadata inputs;
# shared UI blobs intentionally occur in multiple packs and are not consumed.
single={'Room','Items','WorldLinks','MickTreats','FieldMusic','FieldMusicZones','FieldAudio'};scene={'FieldRoom','FieldMap','FieldStory','FieldNpc','FieldProps'}
paths=['romfs/'+row['path']for row in bindings if(row['role']in single and row['id']==c.ROLES[row['role']])or(row['role']in scene and row['path'].startswith('world/podunk/'))];files={}
for first in range(0,len(paths),64):files.update(checked_resources(raw,source,paths[first:first+64],r))
out=r/'build/full-port/prop-catalog12-7d-source';out.mkdir(parents=True,exist_ok=True)
files['romfs/system/native.encresources']=catalog
for path,data in files.items():target=out/Path(path).relative_to('romfs');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
(out/'system/native.encresources').write_bytes(catalog);proof=dict(source=source,game_sha256=hashlib.sha256(raw).hexdigest(),files={p:hashlib.sha256(b).hexdigest()for p,b in files.items()},scope='Exact genuine7d artifact metadata, checked against committed source. No current tutorial WIP is substituted.');(r/'build/full-port/prop-catalog12-artifact-source.json').write_bytes((json.dumps(proof,indent=2)+'\n').encode());print('Prepared',len(files),'exact-source artifact metadata resources')
