from pathlib import Path
import sys,json,hashlib
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import graveyard_shovel_programme as c,native_content as nc
out=r/'build/full-port/shovel-programme-candidate-1';out.mkdir(exist_ok=False);blob,p,review,ex=c.candidate();rows=c.texts(p,review,ex)
(out/'source.encroom').write_bytes(blob);(out/'lowered.json').write_bytes((json.dumps(p,indent=2,ensure_ascii=False)+'\n').encode('utf8'));(out/'texts.json').write_bytes((json.dumps(rows,indent=2,ensure_ascii=False)+'\n').encode('utf8'))
actual=nc.parse_pack(blob);proof=dict(scope='Unpublished full10labelcandidate with actualsource idle handoffs, movementWaitAction, deferredobject49andactorerasure. NoMap/renderer/rootcontact/pickup/actualquestclaim.',base_sha256=hashlib.sha256((r/'romfs/world/podunk/podunk.encroom').read_bytes()).hexdigest(),room_sha256=hashlib.sha256(blob).hexdigest(),programme=len(actual['sections']['Program'])-1,commands=len(p['commands']),labels=p['labels'],terminal=p['terminal'],dialogues=len(rows),source_sha256=ex.sources)
(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Full source shovel candidate:',len(p['commands']),'commands;',len(rows),'locale rows; programme',proof['programme'],'; unpublished')
