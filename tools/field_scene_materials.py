#!/usr/bin/env python3
"""Full actual scene ShaderMaterial sharing/defaults and offline constant outline."""
from __future__ import annotations
import argparse,json,math,re,struct,sys,zlib,subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import read,write,sha,require,decode,stable,PIN,SCENE
from tools.extract_battle_entry import properties
IR=ROOT/'content/native-field-scene-materials.json';REVIEW=ROOT/'compatibility/reviews/field-scene-materials-v0410.json';PACK=ROOT/'romfs/data/podunk.encmaterials';RECEIPT=ROOT/'content/asset-receipts/graphics/materials/source.json'
def extract(native):
 d=read(native);c=read(ROOT/'content/podunk-canvas-art.json');tree=read(ROOT/'content/podunk-node-tree.json');require(sha(native)==c['native_sha256']==tree['native_sha256'],'Material actual full source differs');rs={r['id']:r for r in d['resources']};ns={n['path']:n for n in d['nodes']};ss={s['source'][6:]:s for s in d['scene_states']};tr={r['id']:r for r in tree['records']};textures={r['id']:r for r in c['textures']};inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources=dict(c['sources']);roots=[];ov={}
 def src(p):require(p in inv and sha(ROOT/'upstream/MOTHER-Encore'/p)==inv[p]['sha256'],'Material changed source '+p);sources[p]=inv[p]['sha256'];return(ROOT/'upstream/MOTHER-Encore'/p).read_text(encoding='utf8')
 def visit(root,f):
  roots.append((root,f))
  for n in ss[f]['nodes']:
   if n['instance']:
    p=n['path'].removeprefix('./');visit(p if root=='.' else root+'/'+p,rs[n['instance']['id']]['path'][6:])
 visit('.',SCENE)
 for root,f in sorted(roots,key=lambda x:x[0].count('/')if x[0]!='.'else-1,reverse=True):
  for n in ss[f]['nodes']:
   p=n['path'].removeprefix('./');p=root if p=='.' else p if root=='.'else root+'/'+p
   if p in ns:ov.setdefault(p,{}).update(decode(n['properties']))
 def actual(row):
  p=decode(ns[row['node']]['properties'])
  if p.get('use_parent_material') and row['canvas_parent']:return actual(tr[row['canvas_parent']])
  return p.get('material'),ov.get(row['node'],{}).get('material')
 def props(path):
  if '::'in path:
   f,i=path.split('::');body=re.search(r'^\[sub_resource type="ShaderMaterial" id='+re.escape(i)+r'\]\n(.*?)(?=^\[|\Z)',src(f),re.M|re.S);require(body,'Material source subresource missing');return properties(body[1])
  text=src(path);return properties(text.split('[resource]\n')[1])
 def code(path):
  if '::'not in path:return src(path)
  f,i=path.split('::');b=re.search(r'^\[sub_resource type="Shader" id='+re.escape(i)+r'\]\n(.*?)(?=^\[|\Z)',src(f),re.M|re.S);require(b,'Material shader missing');return json.JSONDecoder(strict=False).raw_decode(b[1].split('code = ',1)[1])[0]
 def uniforms(shader,params,kind):
  out=[]
  for t,n,v in re.findall(r'uniform\s+(vec[24]|float|int|bool|sampler2D)\s+(\w+)(?:\s*:[^=;]+)?(?:\s*=\s*([^;]+))?;',shader):
   v=v.strip()or('vec'+t[-1]+'(0)'if t.startswith('vec')else'false'if t=='bool'else'0')
   if t.startswith('vec'):
    values=[float(x)for x in re.search(r'\(([^)]+)\)',v)[1].split(',')];values=values*int(t[-1])if len(values)==1 else values
   else:values=[float(v=='true')if t=='bool'else float(v)]
   q=params.get('shader_param/'+n,values);require(t!='sampler2D'or q==[0],'Material palette Resource requires implemented palette owner');values=q if isinstance(q,list)else[float(q)];require(len(values)==({'vec4':4,'vec2':2}.get(t,1))and all(math.isfinite(x)for x in values),'Material uniform shape '+n)
   roles=({"flash_color":1,"glow_color":2,"flash_modifier":3,"glow_modifier":4}if kind==3 else{"color":1,"width":2,"pattern":3,"inside":4}if kind==1 else{})
   out.append(dict(name=n,type={'float':1,'int':2,'bool':3,'vec2':4,'vec4':5,'sampler2D':6}[t],role=roles.get(n,0),values=values))
  require(set(k[13:]for k in params if k.startswith('shader_param/'))<=set(x['name']for x in out),'Unknown material override');return out
 materials={};bindings=[];assets={};capture_ids={}
 for r in c['records']:
  if not r['shader']:continue
  actualref,sourceref=actual(tr[r['id']]);require(actualref and sourceref,'Missing actual/source material');a=rs[actualref['id']];s=rs[sourceref['id']];require(a['class']==s['class']=='ShaderMaterial'and s['path'].startswith('res://'),'Invalid material provenance');path=s['path'][6:];q=props(path);cp=decode(a['properties']);require(cp['script']is None and cp['resource_local_to_scene']==q.get('resource_local_to_scene',False),'Unknown scripted material/local clone');sh=rs[cp['shader']['id']]['path'][6:];require(sh==r['shader_source'],'Effective shader binding differs');u=uniforms(code(sh),q,r['shader']);mid=capture_ids.setdefault(actualref['id'],stable('material:'+path+'#'+r['node']))
  m=dict(id=mid,capture_id=actualref['id'],source=path,shader=sh,kind=r['shader'],local=cp['resource_local_to_scene'],uniforms=u,name=q.get('resource_name',''),priority=cp['render_priority'])
  if mid in materials:require(materials[mid]==m,'Material clone alias differs')
  else:materials[mid]=m
  asset=0
  if r['shader']==1:
   params={x['role']:x['values']for x in u};require(params[2]==[1.]and params[3]==[2.]and params[4]==[1.]and params[1]==[0.,0.,0.,1.],'Outline dynamic capability requires review');require(r['hframes']==r['vframes']==1,'Outline whole texture extent');src(r['owner_script']);require('shader_param'not in src(r['owner_script']),'Outline source runtime mutates material');t=textures[r['texture']];asset=stable('outline:'+t['source']+':'+json.dumps(u,sort_keys=True));assets[asset]=dict(id=asset,texture=r['texture'],source=t['source'],path='graphics/materials/'+str(asset)+'.t3x',size=t['size'],material=mid)
  bindings.append(dict(id=r['id'],material=mid,asset=asset,owner=r['owner'],owner_id=r['owner_id'],texture=r['texture'],node=r['node']))
 require(len(bindings)==161,'Full actual material closure lost')
 out=dict(schema=1,kind='encore.field-scene-materials.source-ir',commit=PIN,scene=SCENE,scene_id=c['scene_id'],source_sha256=c['scene_sha256'],native_sha256=sha(native),canvas_ir_sha256=sha(ROOT/'content/podunk-canvas-art.json'),tree_ir_sha256=sha(ROOT/'content/podunk-node-tree.json'),prompt_ir_sha256=sha(ROOT/'content/native-field-prompts.json'),melody_ir_sha256=sha(ROOT/'content/native-field-melody-background.json'),sources=dict(sorted(sources.items())),materials=list(materials.values()),bindings=bindings,assets=list(assets.values()),scene_admitted=False,semantics=['Actual full exported effective material and source SceneState overrides preserve shared versus local_to_scene identity','All original numeric/bool/vector uniform defaults and overrides checked; no arbitrary shader approval','Jump constant inside square alpha-neighbor shader compiled offline, same original RGBA source at texel centers; GPU renders live affine pose, modulation and source depth','Flash reads actual unique Prompt clip state; GPU adds source glow before flash interpolation and inherited modulation','Melody reuses checked existing vertical distortion kernel and global shader time; no private clock','No script/native Ready approval'])
 write(IR,out);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=out['sources'],semantics=out['semantics']));return out

def load():
 d=read(IR);r=read(REVIEW);require(d['schema']==1 and d['commit']==PIN and d['kind']=='encore.field-scene-materials.source-ir'and not d['scene_admitted']and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Material review mismatch')
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h,'Material source differs '+p)
 for key,path in [('canvas_ir_sha256','content/podunk-canvas-art.json'),('tree_ir_sha256','content/podunk-node-tree.json'),('prompt_ir_sha256','content/native-field-prompts.json'),('melody_ir_sha256','content/native-field-melody-background.json')]:require(d[key]==sha(ROOT/path),'Material consumer dependency differs')
 return d

def encode(d,a):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(v):v=v.encode();u(len(v));b.extend(v)
 t(d['scene'])
 for k in ['canvas_ir_sha256','tree_ir_sha256','prompt_ir_sha256','melody_ir_sha256']:b.extend(bytes.fromhex(d[k]))
 u(len(d['materials']))
 for m in d['materials']:
  u(m['id'],m['kind'],int(m['local']));t(m['source']);t(m['shader']);t(m['name']);u(len(m['uniforms']))
  for q in m['uniforms']:t(q['name']);u(q['type'],q['role'],len(q['values']));f(*q['values'])
 u(len(d['bindings']))
 for r in d['bindings']:u(*(r[k]for k in ['id','material','asset','owner','owner_id','texture']));t(r['node'])
 u(len(a['assets']))
 for x in a['assets']:u(x['id'],x['texture'],*x['size'],x['bytes'],x['crc']);t(x['source']);t(x['path']);b.extend(bytes.fromhex(x['output_sha256']))
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCMAT01',1,128,len(b),0,0x454e006f,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)

def assets(tex3ds):
 from PIL import Image
 d=load()
 def one(x):
  im=Image.open(ROOT/'upstream/MOTHER-Encore'/x['source']).convert('RGBA');w,h=im.size;require([w,h]==x['size'],'Outline source extent');p=im.load();out=im.copy();dst=out.load()
  for y in range(h):
   for z in range(w):
    if p[z,y][3]and any(xx<0 or yy<0 or xx>=w or yy>=h or p[xx,yy][3]==0 for yy in range(y-1,y+2)for xx in range(z-1,z+2)):dst[z,y]=(0,0,0,255)
  png=ROOT/'build/generated/materials'/(str(x['id'])+'.png');png.parent.mkdir(parents=True,exist_ok=True);out.save(png);target=ROOT/'romfs'/x['path'];target.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(png)],check=True);return{**x,'bytes':target.stat().st_size,'crc':zlib.crc32(target.read_bytes()),'output_sha256':sha(target),'converted_png_sha256':sha(png)}
 with ThreadPoolExecutor(max_workers=4)as p:a=list(p.map(one,d['assets']))
 proof=dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(Path(__file__)),workers=4,tex3ds_sha256=sha(tex3ds),assets=a);write(RECEIPT,proof);PACK.write_bytes(encode(d,proof));return proof

def checked(d,source):
 a=read(RECEIPT);require(a['ir_sha256']==sha(IR)and a['producer_sha256']==sha(Path(__file__))and a['commit']==PIN and a['workers']>=4 and len(a['assets'])==len(d['assets']),'Material asset receipt differs')
 for x,q in zip(d['assets'],a['assets']):require(all(x[k]==q[k]for k in x)and sha(Path(source)/q['path'])==q['output_sha256']and(Path(source)/q['path']).stat().st_size==q['bytes'],'Material output differs')
 return a

def stage_files(source):
 d=load();a=checked(d,source);raw=encode(d,a);require((Path(source)/'data/podunk.encmaterials').read_bytes()==raw,'Material staged pack differs');return{Path('data/podunk.encmaterials'):raw,**{Path(x['path']):(Path(source)/x['path']).read_bytes()for x in a['assets']}}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','assets','compile']);p.add_argument('--native',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 try:
  if a.action=='extract':d=extract(a.native);print(len(d['materials']),len(d['bindings']))
  elif a.action=='assets':print(len(assets(a.tex3ds)['assets']))
  else:d=load();PACK.write_bytes(encode(d,checked(d,ROOT/'romfs')));print(PACK.stat().st_size)
 except(ValueError,KeyError,OSError,TypeError,struct.error,subprocess.CalledProcessError)as e:sys.exit('MATERIAL ERROR: '+str(e))
