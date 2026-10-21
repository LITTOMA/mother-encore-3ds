"""Audited key HUD state parameters; no font/icon export or live scene admission."""
import argparse,hashlib,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_keys as keys
from tools.godot_text import Document
RECIPE=ROOT/'content/catacombs-key-indicator.json'
MAGIC=b'ENCKUI01'
require=keys.require

def unique(rows):
 out={}
 for k,v in rows:
  require(k not in out,'Key indicator duplicate JSON field');out[k]=v
 return out

def read(file=RECIPE):
 def invalid(value):raise ValueError('Key indicator nonfinite JSON')
 p=json.loads(Path(file).read_text('utf8'),object_pairs_hook=unique,parse_constant=invalid)
 keys.fields(p,'schema kind commit scope keys_recipe keys_review_sha256 engine paths sources','indicator review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-key-indicator.reviewed-source'and p['commit']==keys.PIN,'Key indicator schema/pin')
 require(type(p['scope'])is str and 0<len(p['scope'])<=2048,'Key indicator scope')
 keys.path(p['keys_recipe']);require(hashlib.sha256((ROOT/p['keys_recipe']).read_bytes()).hexdigest()==p['keys_review_sha256'],'Key indicator key policy review changed')
 keys.fields(p['engine'],'commit native_sha256 scope sources','indicator engine review');require(type(p['engine']['commit'])is str and re.fullmatch('[0-9a-f]{40}',p['engine']['commit'])and type(p['engine']['native_sha256'])is str and re.fullmatch('[0-9a-f]{64}',p['engine']['native_sha256'])and type(p['engine']['scope'])is str and 0<len(p['engine']['scope'])<=2048 and type(p['engine']['sources'])is dict and len(p['engine']['sources'])==2,'Key indicator engine identity/scope')
 for name,value in p['engine']['sources'].items():
  keys.path(name);require(name.startswith('reports/dungeon-key-indicator/engine/')and type(value)is str and re.fullmatch('[0-9a-f]{64}',value)and hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==value,'Key indicator engine source changed:'+name)
 receipt=json.loads((ROOT/'reports/dungeon-key-indicator/engine/source.json').read_text('utf8'));require(receipt['commit']==p['engine']['commit']and receipt['native_sha256']==p['engine']['native_sha256']and receipt['outputs']==p['engine']['sources'],'Key indicator engine acquisition receipt differs')
 keys.fields(p['paths'],'scene script','indicator source selectors')
 for name in p['paths'].values():keys.path(name)
 require(type(p['sources'])is dict and set(p['sources'])==set(p['paths'].values()),'Key indicator source coverage')
 _,source=keys.read(ROOT/p['keys_recipe'])
 for name,value in p['sources'].items():
  keys.path(name);require(type(value)is str and re.fullmatch('[0-9a-f]{64}',value)and hashlib.sha256(source.data(name)).hexdigest()==value,'Key indicator reviewed source changed:'+name)
 return p,source

def one(pattern,text,label):
 values=re.findall(pattern,text,re.M);require(len(values)==1,'Key indicator source '+label);return values[0]

def real(value):
 require(type(value)in(int,float)and math.isfinite(value),'Key indicator finite number');return float(value)

def source_model(file=RECIPE):
 p,source=read(file);script=source.text(p['paths']['script']);scene=Document(p['paths']['scene'],source.text(p['paths']['scene']));nodes={('.'if 'parent'not in n.attrs else(n.attrs['name']if n.attrs['parent']=='.'else n.attrs['parent']+'/'+n.attrs['name'])):n for n in scene.nodes}
 require(set(nodes)=={'.','HBoxContainer','HBoxContainer/TextureRect','HBoxContainer/Money'},'Key indicator source node coverage')
 panel=nodes['HBoxContainer'];label=nodes['HBoxContainer/Money'];require(panel.attrs['type']=='HBoxContainer'and label.attrs['type']=='Label'and nodes['.'].attrs['type']=='CanvasLayer','Key indicator source node types')
 require(set(panel.props)=={'margin_left','margin_top','margin_right','margin_bottom'},'Key indicator source panel transform unsupported')
 require('visible'not in panel.props and not re.search(r'^func (?!open\(|close\(|update\()',script,re.M),'Key indicator source methods unsupported')
 opened=keys.function(script,'open');closed=keys.function(script,'close');update=keys.function(script,'update');number=r'(-?\d+(?:\.\d+)?)'
 horizontal=[]
 for body in[opened,closed]:
  target,duration=one(r'tween_property\(\$HBoxContainer, "rect_position:x", '+number+r', '+number+r'\)',body,'horizontal tween')
  horizontal.append((real(float(target)),real(float(duration))))
 require(horizontal[0][1]==horizontal[1][1]and script.count('.set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_OUT)')==3,'Key indicator curve/duration mapping')
 to,first=one(r'"rect_position:y", \$HBoxContainer.rect_position.y \+ '+number+r', '+number+r'\)',update,'bounce first')
 before=one(r'\.from\(\$HBoxContainer.rect_position.y - '+number+r'\)',update,'bounce from')
 last=one(r'"rect_position:y", \$HBoxContainer.rect_position.y, '+number+r'\)',update,'bounce return')
 compare=real(float(one(r'if \$HBoxContainer.rect_position.x == '+number+r':',update,'bounce trigger')));require(compare==horizontal[0][0],'Key indicator open/bounce position differs')
 pattern=one(r'\.text = "([^"\n]*)" % uiManager.get_key_count\(\)',update,'count text');require(pattern.count('%s')==1,'Key indicator counter formatting');prefix,suffix=pattern.split('%s')
 require(one(r'^var _is_open = (false|true)\s*$',script,'initial open')=='false','Key indicator initial state mapping')
 nested,proof=keys.extract(ROOT/p['keys_recipe']);blob=keys.encode(nested)
 key_review,_=keys.read(ROOT/p['keys_recipe']);ui_body=keys.function(source.text(key_review['paths']['ui_script']),'update_key_indicator');threshold=int(one(r'if get_key_count\(\) <= (-?\d+):',ui_body,'indicator close threshold'))
 engine_header=(ROOT/next(name for name in p['engine']['sources']if name.endswith('.h'))).read_text('utf8');require('default_transition = Tween::TRANS_LINEAR;'in engine_header and 'default_ease = Tween::EASE_IN_OUT;'in engine_header and 'custom_step(float p_delta)'in engine_header and 'float duration = 0;'in engine_header,'Key indicator engine defaults/time mechanism changed')
 model=dict(scene=nested['scene'],keys=blob,parameters=[real(panel.props['margin_left']),real(panel.props['margin_top']),horizontal[0][0],horizontal[1][0],horizontal[0][1],real(-float(before)),real(float(to)),real(float(first)),real(float(last))],flags=1,diagnostics=1,threshold=threshold,initial_text=label.props['text'],prefix=prefix,suffix=suffix,curve=1,default_curve=0)
 proof.update(scope=p['scope'],indicator_review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),indicator_sources=p['sources'],engine_review=p['engine'],visual_service_boundary='Native HBox/Label with original positions and unchanged script;fonts/icon pixels and container minimum sizes are not observed',source_panel=panel.props)
 return model,proof

def parse(blob):
 require(type(blob)is bytes and 176<=len(blob)<=512*1024,'Key indicator binary extent')
 magic,version,total,crc,cap,header,reserved=struct.unpack_from('<8s6I',blob)
 require(magic==MAGIC and version==cap==1 and header==176 and reserved==0 and total==len(blob)and crc==zlib.crc32(blob[32:]),'Key indicator header/version/capability/CRC')
 scene,at,amount,text_at,text_size,count,flags,diagnostics=struct.unpack_from('<8I',blob,32)
 require(at==176 and 32<=amount<=len(blob)-176 and text_at==176+amount and text_size==len(blob)-text_at and count==9 and flags<=3 and diagnostics<=1,'Key indicator canonical sections')
 nested=blob[at:at+amount];policy=keys.parse(nested);require(scene==policy['scene'],'Key indicator nested scene identity')
 params=list(struct.unpack_from('<9d',blob,64));require(all(math.isfinite(v)and abs(v)<=65535 for v in params)and all(0<params[i]<=60 and struct.unpack('<f',struct.pack('<f',params[i]))[0]>=2.**-126 for i in[4,7,8])and params[2]!=params[3],'Key indicator position/duration parameters')
 strings=[];end=0
 for offset,length in struct.iter_unpack('<2I',blob[136:160]):
  require(offset==end and length<=256 and length<=text_size-offset,'Key indicator canonical text')
  raw=blob[text_at+offset:text_at+offset+length];require(all(32<=v<127 for v in raw),'Key indicator text encoding');strings.append(raw.decode('ascii'));end+=length
 threshold=struct.unpack_from('<i',blob,164)[0];require(end==text_size and struct.unpack_from('<I',blob,160)[0]==1 and -65535<=threshold<=65535 and blob[168:176]==bytes(8),'Key indicator curve/threshold/default curve/unowned text')
 return dict(scene=scene,keys=nested,parameters=params,flags=flags,diagnostics=diagnostics,threshold=threshold,initial_text=strings[0],prefix=strings[1],suffix=strings[2],curve=1,default_curve=0)

def encode(model):
 keys.fields(model,'scene keys parameters flags diagnostics threshold initial_text prefix suffix curve default_curve','indicator model')
 require(type(model['keys'])is bytes and type(model['scene'])is int and model['scene']==keys.parse(model['keys'])['scene'],'Key indicator model nested scene')
 require(type(model['parameters'])is list and len(model['parameters'])==9 and type(model['flags'])is int and 0<=model['flags']<=3 and type(model['diagnostics'])is int and 0<=model['diagnostics']<=1 and type(model['threshold'])is int and -65535<=model['threshold']<=65535 and type(model['curve'])is int and model['curve']==1,'Key indicator model fields')
 require(type(model['default_curve'])is int and model['default_curve']==0,'Key indicator default curve')
 params=[real(v)for v in model['parameters']];text=bytearray();records=[]
 for name in['initial_text','prefix','suffix']:
  value=model[name];require(type(value)is str and len(value)<=256 and all(32<=ord(c)<127 for c in value),'Key indicator model text');records.extend([len(text),len(value)]);text.extend(value.encode('ascii'))
 nested=model['keys'];blob=bytearray(176)+nested+text
 struct.pack_into('<8s6I',blob,0,MAGIC,1,len(blob),0,1,176,0);struct.pack_into('<8I',blob,32,model['scene'],176,len(nested),176+len(nested),len(text),9,model['flags'],model['diagnostics']);struct.pack_into('<9d',blob,64,*params);struct.pack_into('<7I',blob,136,*records,model['curve']);struct.pack_into('<i',blob,164,model['threshold']);struct.pack_into('<I',blob,168,model['default_curve']);struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]));result=bytes(blob);parse(result);return result

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--recipe',type=Path,default=RECIPE);parser.add_argument('--out',type=Path,required=True);args=parser.parse_args();out=args.out.resolve();require(out.is_relative_to(ROOT/'build')and out.suffix=='.enckui','Key HUD candidate stays under build with .enckui suffix');model,proof=source_model(args.recipe);blob=encode(model);out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(blob);proof.update(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest());out.with_suffix('.source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Compiled key HUD state:',len(blob),'bytes;no scene,fonts/icon or save admission')
