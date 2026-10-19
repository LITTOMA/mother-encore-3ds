"""Checked ENCSGX01 metadata for separately compiled source swing atlases.

No release staging or renderer binding. Legacy ENCSWG01 remains independent.
"""
import argparse,hashlib,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing_graphics as graphics,field_swing,podunk_field as f
from tools.extract_battle_entry import require
RECIPE=ROOT/'content/field-swing-presentation.json';MAGIC=b'ENCSGX01';STRIDES=(32,128,12,1);LIMIT=64*1024
def unique(pairs):
 out={}
 for k,v in pairs:require(k not in out,'Duplicate swing presentation field');out[k]=v
 return out
def safe(v):return type(v)is str and 0<len(v)<=512 and ':'not in v and '\\'not in v and all(x not in ('','.','..')for x in v.split('/'))and all(32<=ord(c)<127 for c in v)
def read(path=RECIPE):
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(p)is dict and set(p)==set('schema kind commit atlas_recipe texture_directory output'.split()),'Swing presentation binding fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.swing-graphics.binary-bindings'and p['commit']==field_swing.PIN and p['atlas_recipe']=='content/field-swing-graphics.json','Swing presentation schema/pin')
 require(safe(p['texture_directory'])and p['texture_directory'].startswith('graphics/')and safe(p['output'])and p['output'].startswith('world/')and p['output'].endswith('.encgraphics'),'Swing presentation output paths')
 return p
def validate(r):
 require(type(r)is dict and set(r)=={'meta','layers','frames'},'Swing graphics model fields');m=r['meta']
 require(type(m)is dict and set(m)==set('initial_frame frame_width frame_height source_columns source_rows atlas_columns'.split())and all(type(v)is int for v in m.values()),'Swing graphics meta fields')
 require(0<m['frame_width']<=1024 and 0<m['frame_height']<=1024 and 0<m['source_columns']<=256 and 0<m['source_rows']<=256 and m['source_columns']*m['source_rows']<=65535 and 0<=m['initial_frame']<m['source_columns']*m['source_rows']and 0<m['atlas_columns']<=16,'Swing graphics meta bounds')
 require(type(r['frames'])is list and 0<len(r['frames'])<=1024,'Swing graphics frame capacity');seen=set();last=-1
 for i,row in enumerate(r['frames']):
  require(type(row)is dict and set(row)==set('source_frame x y width height'.split())and all(type(v)is int for v in row.values()),'Swing graphics frame fields')
  require(last<row['source_frame']<m['source_columns']*m['source_rows']and row['x']==i%m['atlas_columns']*m['frame_width']and row['y']==i//m['atlas_columns']*m['frame_height']and row['width']==m['frame_width']and row['height']==m['frame_height'],'Swing graphics frame mapping');last=row['source_frame'];seen.add(last)
 require(m['initial_frame']in seen and type(r['layers'])is list and len(r['layers'])==2,'Swing graphics initial/layer coverage')
 paths=set();sources=set();roles=set()
 for row in r['layers']:
  require(type(row)is dict and set(row)==set('role width height parent flags position offset path source sha256 source_sha256 bytes crc32'.split()),'Swing graphics layer model fields')
  require(all(type(row[k])is int for k in ('role','width','height','parent','flags','bytes','crc32'))and row['role']in(1,2)and row['role']not in roles and row['parent']==(0 if row['role']==1 else 1)and 0<=row['flags']<=15 and 0<row['bytes']<=8*1024*1024 and 0<=row['crc32']<=0xffffffff,'Swing graphics layer values');roles.add(row['role'])
  require(row['width']==m['frame_width']*m['atlas_columns']and row['height']==math.ceil(len(r['frames'])/m['atlas_columns'])*m['frame_height']and max(row['width'],row['height'])<=1024,'Swing graphics atlas dimensions')
  require(safe(row['path'])and row['path'].startswith('graphics/')and row['path'].endswith('.t3x')and row['path']not in paths and safe(row['source'])and row['source'].startswith('Graphics/')and row['source'].endswith('.png')and row['source']not in sources,'Swing graphics texture identities');paths.add(row['path']);sources.add(row['source'])
  require(all(type(row[k])is str and re.fullmatch('[0-9a-f]{64}',row[k])and row[k]!='0'*64 for k in ('sha256','source_sha256')),'Swing graphics texture fingerprints')
  require(all(type(row[k])is list and len(row[k])==2 and all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=4096 for v in row[k])for k in ('position','offset')),'Swing graphics sprite transforms')
 return r
def encode(r):
 validate(r);m=r['meta'];pool=bytearray();cache={}
 def ref(v):
  if v not in cache:cache[v]=(len(pool),len(v.encode()));pool.extend(v.encode())
  return cache[v]
 meta=struct.pack('<8I',m['initial_frame'],m['frame_width'],m['frame_height'],m['source_columns'],m['source_rows'],m['atlas_columns'],len(r['frames']),0)
 layers=[]
 for v in sorted(r['layers'],key=lambda x:x['role']):layers.append(struct.pack('<6I4f4I32s32s2I',v['role'],v['width'],v['height'],v['parent'],v['flags'],0,*v['position'],*v['offset'],*ref(v['path']),*ref(v['source']),bytes.fromhex(v['sha256']),bytes.fromhex(v['source_sha256']),v['bytes'],v['crc32']))
 frames=b''.join(struct.pack('<I4H',v['source_frame'],v['x'],v['y'],v['width'],v['height'])for v in r['frames'])
 return f.pack_container(MAGIC,0,[meta,b''.join(layers),frames,bytes(pool)],STRIDES,LIMIT)
def build(assets,path=RECIPE):
 p=read(path);assets=Path(assets).resolve();require(assets.is_relative_to(ROOT/'build')and assets.is_dir(),'Swing graphics need owned original atlas outputs')
 images,expected=graphics.build(ROOT/p['atlas_recipe']);actual=json.loads((assets/'source.json').read_text(encoding='utf8'),object_pairs_hook=unique)
 require(set(actual)==set(expected)|{'tex3ds_sha256'}and all(actual[k]==v for k,v in expected.items()if k!='layers')and type(actual['tex3ds_sha256'])is str and re.fullmatch('[0-9a-f]{64}',actual['tex3ds_sha256']),'Swing graphics source conversion receipt differs')
 require(type(actual['layers'])is list and len(actual['layers'])==2,'Swing graphics converted layer count')
 _,base,ex,nodes=graphics.read(ROOT/p['atlas_recipe']);main=next(v for v in expected['layers']if v['role']==1);n=nodes[main['node']];meta=dict(initial_frame=n.props.get('frame',0),frame_width=main['frames'][0]['width'],frame_height=main['frames'][0]['height'],source_columns=n.props['hframes'],source_rows=n.props['vframes'],atlas_columns=expected['atlas_columns']);layers=[];mapping=None
 for a,b in zip(actual['layers'],expected['layers']):
  require(set(a)==set(b)|{'t3x_bytes','t3x_sha256'}and all(a[k]==v for k,v in b.items()),'Swing graphics source frame receipt differs')
  texture=(assets/a['output']).resolve();require(texture.is_relative_to(assets)and texture.is_file(),'Swing graphics texture output missing/unsafe');raw=texture.read_bytes();require(type(a['t3x_bytes'])is int and len(raw)==a['t3x_bytes']and hashlib.sha256(raw).hexdigest()==a['t3x_sha256'],'Swing graphics actual texture fingerprint differs')
  require(a['source']==base['texture'if a['role']==1 else'bat_texture'],'Swing graphics source roles differ')
  shape=[{k:v for k,v in row.items()if k!='rgba_sha256'}for row in a['frames']]
  require(mapping is None or mapping==shape,'Swing graphics layers have different frame maps');mapping=shape
  parent=nodes[a['node']].attrs['parent'];require(parent==('.'if a['role']==1 else main['node']),'Swing graphics source hierarchy changed')
  layers.append(dict(role=a['role'],width=a['width'],height=a['height'],parent=0 if a['role']==1 else 1,flags=int(a['centered'])|int(a['flip_h'])<<1|int(a['flip_v'])<<2|int(a['initial_visible'])<<3,position=a['position'],offset=a['offset'],path=p['texture_directory']+'/'+a['output'],source=a['source'],sha256=a['t3x_sha256'],source_sha256=a['source_sha256'],bytes=len(raw),crc32=zlib.crc32(raw)))
 model=dict(meta=meta,layers=layers,frames=mapping);blob=encode(model);f.unpack_container(blob,MAGIC,STRIDES,LIMIT)
 proof=dict(scope=__doc__,commit=field_swing.PIN,output=p['output'],model=model,sources=expected['sources'],atlas_receipt_sha256=hashlib.sha256((assets/'source.json').read_bytes()).hexdigest(),dependencies={Path(path).resolve().relative_to(ROOT).as_posix():hashlib.sha256(Path(path).read_bytes()).hexdigest(),'tools/field_swing_presentation.py':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest())
 return blob,proof
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--assets',type=Path,required=True);a.add_argument('--out',type=Path,required=True);o=a.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and not out.exists(),'Fresh owned binary graphics candidate required');blob,proof=build(o.assets);out.mkdir(parents=True);(out/'swing.encgraphics').write_bytes(blob);(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Source swing graphics:',len(blob),'metadata bytes; original action resource unchanged; no release binding')
