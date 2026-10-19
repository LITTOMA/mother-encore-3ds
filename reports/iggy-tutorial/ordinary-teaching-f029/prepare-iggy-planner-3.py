from pathlib import Path
import json,struct,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools import podunk_field as f
recipe=json.loads((r/'content/graveyard-swing-programme.json').read_bytes());_,ss=f.unpack_container((r/'romfs/world/podunk/story.encstory').read_bytes(),b'ENCFST01',(1,24,36,40,12,32,1),128*1024,capability=4)
p=list(struct.iter_unpack('<6I',ss[1]))[6];assert p[0]==6;boundary=ss[0][p[1]:p[1]+p[2]].decode();assert boundary==recipe['boundary']
(r/'build/full-port/iggy-walk-destination.json').write_text(json.dumps(dict(boundary=boundary,programme=6,scope='Only this exact source-admitted destination Area may be entered by the static plan; the guest must acquire programme6 through ordinary movement. All other unknown areas remain blocked.'))+'\n',encoding='utf8')
s=(r/'tools/field_walk_plan.cpp').read_text();s=s.replace('if(argc!=9)','if(argc!=10)').replace('padding=512/grid','padding=1536/grid').replace('if(active(b.conditions)&&b.kind!=uint16_t(FieldBoundaryKind::Confirm))','if(active(b.conditions)&&b.kind!=uint16_t(FieldBoundaryKind::Confirm)&&map.text(b.path)!=argv[9])')
assert 'map.text(b.path)!=argv[9]'in s
(r/'build/full-port/iggy-walk-planner-3.cpp').write_text(s,encoding='utf8')
