"""Source-owned key HUD icon/layout, borrowing the existing checked SAVE font.

Candidate-only compiler. It cannot modify formal RomFS/catalogs or game saves.
"""
import argparse,hashlib,json,math,re,shlex,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import dungeon_key_indicator as indicator
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document
from PIL import Image
import io
RECIPE=ROOT/'content/catacombs-key-hud-graphics.json';MAGIC=b'ENCKGFX1'

def read(file=RECIPE):
 def invalid(v):raise ValueError('Key HUD graphics nonfinite JSON')
 p=json.loads(Path(file).read_text('utf-8'),object_pairs_hook=indicator.unique,parse_constant=invalid)
 indicator.keys.fields(p,'schema kind commit scope sources paths indicator_recipe indicator_sha256 font_pack font_pack_sha256 font_recipe font_recipe_sha256 engine viewports adaptation license_review texture native_layouts','HUD graphics review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-key-hud-graphics.reviewed-source'and p['commit']==PIN,'Key HUD graphics review identity')
 for k in ['scope','adaptation','license_review']:require(type(p[k])is str and 0<len(p[k])<=4096,'Key HUD graphics source scope')
 indicator.keys.fields(p['paths'],'scene script font icon','HUD graphics paths');indicator.keys.fields(p['engine'],'commit native_sha256','HUD graphics engine')
 require(p['engine']['commit']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'and re.fullmatch('[0-9a-f]{64}',p['engine']['native_sha256'])and p['viewports']==[[320,180],[400,240]],'Key HUD graphics reviewed engine/viewports')
 ex=Extractor(ROOT)
 require(type(p['sources'])is dict and len(p['sources'])==9 and set(p['paths'].values())<=set(p['sources'])and 'LICENSE'in p['sources'],'Key HUD graphics source coverage')
 for name,digest in p['sources'].items():
  indicator.keys.path(name);require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(ex.data(name)).hexdigest()==digest,'Key HUD graphics source changed:'+name)
 for name in ['indicator','font_pack','font_recipe']:
  key=name+'_recipe'if name=='indicator'else name;target=ROOT/indicator.keys.path(p[key]);require(target.is_file()and hashlib.sha256(target.read_bytes()).hexdigest()==p[name+'_sha256'],'Key HUD graphics dependency changed:'+name)
 indicator.keys.path(p['texture']);require(p['texture'].endswith('.t3x'),'Key HUD graphics texture suffix')
 require(type(p['native_layouts'])is dict and len(p['native_layouts'])==2 and all(type(k)is str and k.startswith('reports/dungeon-key-hud-graphics/native-')and indicator.keys.path(k)and type(v)is str and re.fullmatch('[0-9a-f]{64}',v)for k,v in p['native_layouts'].items()),'Key HUD graphics native identities')
 return p,ex

def source_model(file=RECIPE):
 p,ex=read(file);state,proof=indicator.source_model(ROOT/p['indicator_recipe']);nested=indicator.encode(state)
 observations=[]
 for name,digest in p['native_layouts'].items():
  raw=(ROOT/name).read_bytes();require(hashlib.sha256(raw).hexdigest()==digest,'Key HUD original layout observation changed');observations.append(json.loads(raw))
 require(observations[0]==observations[1]and observations[0]['engine']['hash']==p['engine']['commit'],'Key HUD original repetitions/engine differ')
 save=json.loads((ROOT/p['font_recipe']).read_text('utf-8'));font_pack=(ROOT/p['font_pack']).read_bytes();glyphs={g['codepoint']:g for g in save['glyphs']}
 image=Image.open(io.BytesIO(ex.data(p['paths']['icon'])));require(image.format=='PNG'and image.size==(10,16),'Key HUD source icon extent')
 doc=Document(p['paths']['scene'],ex.text(p['paths']['scene']));nodes={s.attrs.get('name'):s for s in doc.nodes}
 panel=nodes['HBoxContainer'];label=nodes['Money'];icon=nodes['TextureRect']
 require(label.props['align']==label.props['valign']==1 and icon.props['stretch_mode']==1 and label.props['text']==state['initial_text'],'Key HUD source node mapping')
 texts=[state['initial_text']]+[state['prefix']+str(n)+state['suffix']for n in range(indicator.keys.parse(nested[176:176+struct.unpack_from('<I',nested,40)[0]])['minimum'],indicator.keys.parse(nested[176:176+struct.unpack_from('<I',nested,40)[0]])['maximum']+1)]
 rows=observations[0]['rows'];require([r['text']for r in rows]==texts and len(rows)==5,'Key HUD source text domain differs')
 for cp,advance in observations[0]['advances']:require(cp in glyphs and glyphs[cp]['advance']==advance,'Key HUD original and prepared SAVE font advance differs')
 for row in rows:
  width=sum(glyphs[ord(c)]['advance']for c in row['text'])-save['font_recipe']['char_spacing']
  require(row['panel_position']==state['parameters'][:2]and row['panel_size']==[panel.props['margin_right']-panel.props['margin_left'],panel.props['margin_bottom']-panel.props['margin_top']]and row['icon_position']==[0,0]and row['icon_size']==list(image.size),'Key HUD native/source container geometry differs')
  require(row['string_size']==row['label_size']==row['label_minimum']==[width,save['bottle_height']]and row['label_position']==[label.props['margin_left'],label.props['margin_top']]and row['font_height']==save['bottle_height']and row['font_ascent']==11 and row['separation']==row['label_position'][0]-row['icon_size'][0],'Key HUD native label does not fit its source text')
  require(row['align']==row['valign']==1 and row['text_color']==[1,1,1,1]and row['shadow_color']==[0,0,0,0]and row['outline_color']==[0,0,0,1]and row['outline_size']==1 and row['char_spacing']==save['font_recipe']['char_spacing']and row['space_spacing']==save['font_recipe']['space_spacing'],'Key HUD font/outline/shadow contract differs')
 model=dict(indicator=nested,texture=p['texture'],width=image.width,height=image.height,texture_bytes=0,texture_crc=0,font_bytes=len(font_pack),font_crc=zlib.crc32(font_pack),reference=p['viewports'][0],expanded=p['viewports'][1],icon_offset=rows[0]['icon_position'],text_offset=rows[0]['label_position'],color=sum(round(x*255)<<(8*i)for i,x in enumerate(rows[0]['text_color'])))
 proof.update(scope=p['scope'],hud_graphics_review_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),graphics_sources=p['sources'],native_layouts=p['native_layouts'],borrowed_font_pack=p['font_pack'],borrowed_font_sha256=p['font_pack_sha256'],viewport_adaptation=p['adaptation'],license_review=p['license_review']);return model,proof

def parse(blob):
 require(type(blob)is bytes and 128<=len(blob)<=1024*1024,'Key HUD graphics binary extent')
 magic,version,total,crc,cap,header,reserved=struct.unpack_from('<8s6I',blob)
 require(magic==MAGIC and version==cap==1 and header==128 and reserved==0 and total==len(blob)and crc==zlib.crc32(blob[32:]),'Key HUD graphics header/version/capability/CRC')
 at,amount,text,length,w,h,tb,tc,fb,fc=struct.unpack_from('<10I',blob,32)
 require(at==128 and 176<=amount<=len(blob)-128 and text==128+amount and 0<length<=512 and text+length==len(blob)and blob[108:128]==bytes(20),'Key HUD graphics canonical sections/reserved')
 nested=blob[at:at+amount];indicator.parse(nested);texture=blob[text:].decode('ascii');indicator.keys.path(texture)
 require(0<w<=1024 and 0<h<=1024 and 0<tb<=8*1024*1024 and 24<=fb<=1024*1024,'Key HUD graphics image/font bounds')
 geometry=list(struct.unpack_from('<8f',blob,72));require(all(math.isfinite(v)and abs(v)<=65535 for v in geometry)and 0<geometry[0]<=geometry[2]<=1024 and 0<geometry[1]<=geometry[3]<=1024 and geometry[:2]!=geometry[2:4],'Key HUD graphics finite geometry/viewports')
 return dict(indicator=nested,texture=texture,width=w,height=h,texture_bytes=tb,texture_crc=tc,font_bytes=fb,font_crc=fc,reference=geometry[:2],expanded=geometry[2:4],icon_offset=geometry[4:6],text_offset=geometry[6:8],color=struct.unpack_from('<I',blob,104)[0])

def encode(model):
 indicator.keys.fields(model,'indicator texture width height texture_bytes texture_crc font_bytes font_crc reference expanded icon_offset text_offset color','HUD graphics model');indicator.keys.path(model['texture']);require(type(model['indicator'])is bytes,'Key HUD graphics nested bytes')
 for k in ['width','height','texture_bytes','texture_crc','font_bytes','font_crc','color']:require(type(model[k])is int and 0<=model[k]<2**32,'Key HUD graphics integer')
 geometry=[]
 for k in ['reference','expanded','icon_offset','text_offset']:
  require(type(model[k])is list and len(model[k])==2 and all(type(v)in(int,float)and math.isfinite(v)for v in model[k]),'Key HUD graphics vector');geometry+=model[k]
 text=model['texture'].encode('ascii');nested=model['indicator'];blob=bytearray(128)+nested+text
 struct.pack_into('<8s6I',blob,0,MAGIC,1,len(blob),0,1,128,0);struct.pack_into('<10I',blob,32,128,len(nested),128+len(nested),len(text),*[model[k]for k in ['width','height','texture_bytes','texture_crc','font_bytes','font_crc']]);struct.pack_into('<8fI',blob,72,*geometry,model['color']);struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]));out=bytes(blob);parse(out);return out

def compile(out,tool,command=None,file=RECIPE):
 out=Path(out).resolve();tool=Path(tool).resolve();require(out.is_relative_to(ROOT/'build')and out.suffix=='.enckgx'and not out.exists()and tool.is_file(),'Fresh key HUD graphics candidate under build required')
 p,ex=read(file);model,proof=source_model(file);dest=out.parent/model['texture'];require(not dest.exists(),'Fresh key HUD icon required');dest.parent.mkdir(parents=True,exist_ok=True)
 source=out.parent/'original-key-icon.png';source.write_bytes(ex.data(p['paths']['icon']));args=command or[str(tool)]
 with(out.parent/'tex3ds.log').open('wb')as log:subprocess.run(args+['-f','rgba8','-z','none','-o',str(dest),str(source)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
 raw=dest.read_bytes();model.update(texture_bytes=len(raw),texture_crc=zlib.crc32(raw));blob=encode(model);out.write_bytes(blob);proof.update(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),icon_bytes=len(raw),icon_sha256=hashlib.sha256(raw).hexdigest(),converter_sha256=hashlib.sha256(tool.read_bytes()).hexdigest());out.with_suffix('.source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8');print('Compiled source key HUD graphics:',len(blob),'bytes;one icon,existing resident font;no formal scene/save admission');return blob
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--tex3ds',type=Path,required=True);p.add_argument('--tex3ds-command');a=p.parse_args();compile(a.out,a.tex3ds,shlex.split(a.tex3ds_command)if a.tex3ds_command else None)
