#!/usr/bin/env python3
"""Actual Podunk Camarea source -> independent checked lifecycle binary."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
SCRIPT='Scripts/Main/camarea.gd';PROTOTYPE='Nodes/Overworld/camarea.tscn';IR=ROOT/'content/podunk-camera-area.json';REVIEW=ROOT/'compatibility/reviews/podunk-camera-area-v0410.json';OUT=ROOT/'romfs/data/podunk.enccamarea'
def resolve(base,relative):
 parts=base.split('/')if base!='.'else[]
 for p in relative.split('/'):
  if p in ('','.'):continue
  if p=='..':require(parts,'Camarea NodePath escaped scene');parts.pop()
  else:parts.append(p)
 return '/'.join(parts)or'.'
def extract(native):
 d=read(native);g=read(ROOT/'content/podunk-scene.json');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources=dict(g['sources']);nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};states={s['source'][6:]:s for s in d['scene_states']};require(d['source']=='res://'+SCENE and len(nm)==8686 and sha(native)==g['export_sha256'],'Camarea needs complete source native export');roots=[]
 def visit(root,f):
  roots.append((root,f))
  for n in states[f]['nodes']:
   if n['instance']:
    p=n['path'].removeprefix('./');visit(p if root=='.'else root+'/'+p,rs[n['instance']['id']]['path'][6:])
 visit('.',SCENE);overrides={}
 for root,f in sorted(roots,key=lambda r:r[0].count('/')if r[0]!='.'else-1,reverse=True):
  for n in states[f]['nodes']:
   p=n['path'].removeprefix('./');p=root if p=='.'else p if root=='.'else root+'/'+p
   if p in nm:overrides.setdefault(p,{}).update(decode(n['properties']))
 text=(ROOT/'upstream/MOTHER-Encore'/SCRIPT).read_text();methods=re.findall(r'^func ([A-Za-z_][A-Za-z0-9_]*)\(',text,re.M);require(methods==['_on_enter','get_size','get_area_global_position','_on_exit','_reset_camera_limits','_on_camarea_body_exited','_on_camarea_body_entered'],'Unreviewed Camarea source method');facts=['global.currentCamera.camareas += 1','global.currentCamera.camareas -= 1','inside = true','inside = false','yield(get_tree(), "idle_frame")','if inside:','get_viewport_rect().size','int(cam_node.margin_top)','int(cam_node.margin_left)','$CollisionShape2D.shape.extents * 2 * self.transform.get_scale()','size.x = round(size.x)','size.y = round(size.y)','global.currentCamera.set_camarea_offset(camera_offset)','if global.currentCamera.camareas == 0:','global.currentCamera.set_camarea_offset(Vector2.ZERO)','body == global.get_player()']
 for f in facts:require(f in text,'Changed Camarea '+f)
 reset=re.search(r'func _reset_camera_limits\(\):([\s\S]*?)(?=\nfunc )',text)[1];limits={k:int(v)for k,v in re.findall(r'global.currentCamera.limit_(top|left|right|bottom) = (-?\d+)',reset)};require(list(limits)==['top','left','right','bottom'],'Camarea source reset order changed');records=[]
 for b in g['pending']:
  if b['script']!=SCRIPT:continue
  p=b['node'];v=decode(nm[p]['properties']);o=overrides[p];ref=o.get('cam_rect_path',dict(type='NodePath',value=''));require(ref.get('type')=='NodePath','Camarea source NodePath differs');refpath=resolve(p,ref['value'])if ref['value']else'';exists=refpath in nm;refv=decode(nm[refpath]['properties'])if exists else{};require(not exists or nm[refpath]['class']=='ReferenceRect'and refv['script']is None,'Camarea reference has unknown mechanism');s=p+'/CollisionShape2D';sv=decode(nm[s]['properties']);resource=rs[sv['shape']['id']];require(resource['class']=='RectangleShape2D' and v['rotation']==0 and sv['rotation']==0,'Camarea unreviewed shape/rotation');extent=decode(resource['properties'])['extents'];camera_offset=o.get('camera_offset',[0,0]);require('export var camera_offset: Vector2'in text,'Camarea typed default differs')
  records.append(dict(id=b['stable_id'],ready=b['ready_ordinal'],node=p,parent_id=stable(p.rsplit('/',1)[0]),shape_id=stable(s),reference_id=stable(refpath)if exists else 0,reference_path=ref['value'],reference_node=refpath if exists else'',reference_exists=exists,reference_margins=[refv.get('margin_'+k,0)for k in ['left','top','right','bottom']],local_position=v['position'],local_scale=v['scale'],world=decode(nm[p]['world_transform']),shape_position=sv['position'],shape_scale=sv['scale'],shape_world=decode(nm[s]['world_transform']),shape_extents=extent,camera_offset=camera_offset,flags=sum(int(x)<<i for i,x in enumerate([v['monitoring'],v['monitorable'],sv['disabled'],sv['one_way_collision']])),layer=v['collision_layer'],mask=v['collision_mask'],pause=v['pause_mode'],priority=v['process_priority']))
 require(len(records)==1,'Camarea complete Podunk count differs');prototype=(ROOT/'upstream/MOTHER-Encore'/PROTOTYPE).read_text();signals=re.findall(r'^\[connection ([^\n]+)\]',prototype,re.M);require(signals==['signal="body_entered" from="." to="." method="_on_camarea_body_entered"','signal="body_exited" from="." to="." method="_on_camarea_body_exited"'],'Camarea source signal roster differs')
 for p in [SCRIPT,PROTOTYPE,'Scripts/Main/Camera2D.gd','Nodes/Ui/Camera.tscn','LICENSE']:sources[p]=inv[p]['sha256']
 for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed Camarea source '+p)
 write(IR,dict(schema=1,kind='encore.field-camera-area.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],scene_admitted=False,native_sha256=sha(native),sources=sources,reset_limits=limits,records=records,signals=signals,pending=['Actual admitted current GameCamera owner and live integer limit/camareas writes','Real Area body_entered/body_exited identity and native idle_frame continuation; no proximity event','Live ReferenceRect and collision shape/scale viewport values are read when source requests them','Full GameCamera14/Shaker/MapArrows and other scene families remain independent pending']))
def load():
 d=read(IR);review=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']==SCENE and not d['scene_admitted']and review['ir_sha256']==sha(IR)and review['commit']==PIN,'Camarea source review differs')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed Camarea proof '+p)
 return d
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):b.extend(struct.pack('<'+'i'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);t(SCRIPT);i(*d['reset_limits'].values())
 for r in d['records']:
  u(r['id'],r['ready'],r['parent_id'],r['shape_id'],r['reference_id'],int(r['reference_exists']),r['flags'],r['layer'],r['mask'],r['pause']);i(r['priority']);f(*r['local_position'],*r['local_scale'],*[v for row in r['world']for v in row],*r['shape_position'],*r['shape_scale'],*[v for row in r['shape_world']for v in row],*r['shape_extents'],*r['camera_offset'],*r['reference_margins']);t(r['node']);t(r['reference_path']);t(r['reference_node'])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCFCAA1',1,128,len(b),0,0x454e0023,3,len(d['records']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)
def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk.enccamarea').read_bytes()==raw,'StagedCamarea differs');return {Path('data/podunk.enccamarea'):raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native,'Explicit complete native source required');extract(a.native);return
 raw=encode(load())
 if a.action=='verify':require(OUT.read_bytes()==raw,'Stale Camarea binary')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Camarea complete source:',len(load()['records']),'actual records;',len(raw),'bytes; scene_admitted=False')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD CAMERA AREA ERROR: '+str(e))
