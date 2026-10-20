"""Extract only the original swing frames into compact, untrimmed atlases.

Candidate asset conversion only. No source pixels are painted, resampled or
trimmed; source shader/material and live rendering remain separate work.
"""
import argparse,hashlib,io,json,math,re,shlex,subprocess,sys
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing
from tools.extract_battle_entry import require
from tools.godot_text import Document,Vec2
RECIPE=ROOT/'content/field-swing-graphics.json'

def read(path=RECIPE):
 def unique(pairs):
  out={}
  for k,v in pairs:require(k not in out,'Duplicate swing graphics field');out[k]=v
  return out
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=unique)
 require(type(p)is dict and set(p)==set('schema kind source_recipe atlas_columns layers sources scope'.split()),'Swing graphics recipe fields')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.swing-graphics.reviewed-atlas'and p['source_recipe']=='content/field-swing.json','Swing graphics schema/source')
 require(type(p['atlas_columns'])is int and 1<=p['atlas_columns']<=16 and type(p['scope'])is str and 0<len(p['scope'])<=2048,'Swing graphics packing/scope')
 require(type(p['sources'])is dict and set(p['sources'])=={'LICENSE'}and type(p['sources']['LICENSE'])is str and re.fullmatch('[0-9a-f]{64}',p['sources']['LICENSE']),'Swing graphics permission review')
 base,ex=field_swing.read(ROOT/p['source_recipe']);require(hashlib.sha256(ex.data('LICENSE')).hexdigest()==p['sources']['LICENSE']and "purpose you're using them is related to the game"in ex.text('LICENSE'),'Swing graphics source permission changed')
 require(type(p['layers'])is list and len(p['layers'])==2,'Swing graphics source layers')
 for row in p['layers']:
  require(type(row)is dict and set(row)=={'role','node','source_key','output'}and type(row['role'])is int and row['role']in(1,2)and row['source_key']in('texture','bat_texture')and type(row['node'])is str and row['node']and all(re.fullmatch('[A-Za-z_][A-Za-z0-9_]*',x)for x in row['node'].split('/'))and type(row['output'])is str and re.fullmatch('[a-z][a-z0-9_-]*\.t3x',row['output']),'Swing graphics layer fields')
 require({r['role']for r in p['layers']}=={1,2}and len({r['node']for r in p['layers']})==2 and len({r['output']for r in p['layers']})==2 and {r['source_key']for r in p['layers']}=={'texture','bat_texture'},'Duplicate swing graphics owner')
 doc=Document(base['scene'],ex.text(base['scene']));nodes={}
 for node in doc.nodes:
  parent=node.attrs.get('parent');key='.'if parent is None else node.attrs['name']if parent=='.'else parent+'/'+node.attrs['name'];nodes[key]=node
 for r in p['layers']:
  require(r['node']in nodes,'Swing graphics source node missing');n=nodes[r['node']]
  require(n.attrs.get('type')=='Sprite'and doc.ext_path(n.props['texture'],'Texture')==base[r['source_key']],'Swing graphics source texture differs')
  require(not(set(n.props)&{'rotation','rotation_degrees','scale','transform','region_enabled','region_rect'}),'Unreviewed swing sprite transform/region')
  require(all(type(n.props.get(k,v))is bool for k,v in [('centered',True),('flip_h',False),('flip_v',False),('visible',True)]),'Swing graphics sprite flags')
  for k in ('position','offset'):require(isinstance(n.props.get(k,Vec2(0,0)),Vec2),'Swing graphics sprite origin')
 return p,base,ex,nodes

def build(path=RECIPE):
 p,base,ex,nodes=read(path);action=field_swing.source(ROOT/p['source_recipe']);frames=sorted({int(k['value'])for c in action['clips']for k in c['keys']if k['kind']==1})
 require(frames and all(0<=f<100 for f in frames),'Swing graphics frame domain');layers=[];images={};mapping=[]
 for row in p['layers']:
  node=nodes[row['node']];source=base[row['source_key']]
  with Image.open(io.BytesIO(ex.data(source)))as original:
   original=original.convert('RGBA');columns=node.props['hframes'];rows=node.props['vframes'];width=original.width//columns;height=original.height//rows
   packed=Image.new('RGBA',(width*p['atlas_columns'],height*math.ceil(len(frames)/p['atlas_columns'])))
   require(max(packed.size)<=1024,'Swing graphics atlas exceeds texture limit');squares=[]
   for i,frame in enumerate(frames):
    sx=(frame%columns)*width;sy=(frame//columns)*height;x=(i%p['atlas_columns'])*width;y=(i//p['atlas_columns'])*height;tile=original.crop((sx,sy,sx+width,sy+height));packed.paste(tile,(x,y))
    squares.append(dict(source_frame=frame,x=x,y=y,width=width,height=height,rgba_sha256=hashlib.sha256(tile.tobytes()).hexdigest()))
   images[row['role']]=packed
   origin=lambda k:[node.props.get(k,Vec2(0,0)).x,node.props.get(k,Vec2(0,0)).y]
   layers.append(dict(**row,source=source,source_sha256=hashlib.sha256(ex.data(source)).hexdigest(),width=packed.width,height=packed.height,position=origin('position'),offset=origin('offset'),centered=node.props.get('centered',True),flip_h=node.props.get('flip_h',False),flip_v=node.props.get('flip_v',False),initial_visible=node.props.get('visible',True),rgba_sha256=hashlib.sha256(packed.tobytes()).hexdigest(),frames=squares))
 proof=dict(commit=field_swing.PIN,scope=p['scope'],source_recipe=p['source_recipe'],sources=ex.sources,dependencies={Path(path).resolve().relative_to(ROOT).as_posix():hashlib.sha256(Path(path).read_bytes()).hexdigest(),'content/field-swing.json':hashlib.sha256((ROOT/p['source_recipe']).read_bytes()).hexdigest(),'tools/field_swing_graphics.py':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},atlas_columns=p['atlas_columns'],frame_count=len(frames),layers=layers,permission='Pinned upstream LICENSE game-related port; original notices retained; no tool binary distributed')
 return images,proof

def main():
 a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);a.add_argument('--tex3ds',type=Path,required=True);a.add_argument('--tex3ds-command');o=a.parse_args();out=o.out.resolve();require(not out.exists()and out.is_relative_to(ROOT/'build')and o.tex3ds.is_file(),'Fresh owned output and actual tex3ds required')
 images,proof=build();out.mkdir(parents=True);command=shlex.split(o.tex3ds_command)if o.tex3ds_command else[str(o.tex3ds.resolve())]
 for row in proof['layers']:
  png=out/(Path(row['output']).stem+'.png');target=out/row['output'];images[row['role']].save(png)
  relative=lambda p:p.resolve().relative_to(ROOT).as_posix()
  args=[relative(target),relative(png)]if o.tex3ds_command else[str(target),str(png)]
  subprocess.run(command+['-f','rgba8','-z','none','-o',args[0],args[1]],cwd=ROOT,check=True);row['t3x_bytes']=target.stat().st_size;row['t3x_sha256']=hashlib.sha256(target.read_bytes()).hexdigest()
 proof['tex3ds_sha256']=hashlib.sha256(o.tex3ds.read_bytes()).hexdigest();(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Source swing atlases:',proof['frame_count'],'untrimmed frames per layer,',sum(r['t3x_bytes']for r in proof['layers']),'actual T3X bytes; candidate only')
if __name__=='__main__':main()
