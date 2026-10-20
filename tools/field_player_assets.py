"""Compile and stage the bounded, reviewed persistent player resource group.

The two GPU atlases are imported from an actual tex3ds conversion receipt;
ordinary compilation never fabricates or reconverts third-party textures.
"""
import argparse,hashlib,json,re,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_swing as swing,field_swing_graphics as graphics,field_swing_presentation as presentation,field_player_audio as audio,field_player_tree as tree,field_swing_entry as entry
from tools.extract_battle_entry import require
from tools.godot_text import Document,SubRef
RECIPE=ROOT/'content/field-player-assets.json'
PRODUCERS=('tools/field_player_assets.py','tools/field_swing.py','tools/field_swing_action.py','tools/field_swing_graphics.py','tools/field_swing_presentation.py','tools/field_player_audio.py','tools/field_player_tree.py','tools/field_swing_entry.py','tools/podunk_field.py')
def sha(blob):return hashlib.sha256(blob).hexdigest()
def json_bytes(value):return(json.dumps(value,indent=2)+'\n').encode()
def read(path=RECIPE):
 p=json.loads(Path(path).read_text(encoding='utf8'),object_pairs_hook=audio.unique)
 require(type(p)is dict and set(p)=={'schema','kind','swing_output','atlas_receipt','receipt','outline_shader'}and type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.player-assets.release-bindings','Player assets binding schema/fields')
 require(type(p['outline_shader'])is dict and set(p['outline_shader'])=={'path','sha256'}and presentation.safe(p['outline_shader']['path'])and p['outline_shader']['path'].startswith('Shaders/')and type(p['outline_shader']['sha256'])is str and re.fullmatch('[0-9a-f]{64}',p['outline_shader']['sha256']),'Player outline source binding')
 require(presentation.safe(p['swing_output'])and p['swing_output'].startswith('world/')and p['swing_output'].endswith('.encswing'),'Player assets swing output')
 for k in ('atlas_receipt','receipt'):require(presentation.safe(p[k])and p[k].startswith('content/asset-receipts/')and p[k].endswith('.json'),'Player assets receipt path')
 require(p['receipt']!=p['atlas_receipt'],'Player assets distinct receipts');return p
def graphics_packet(actual,load):
 p=presentation.read();_,expected=graphics.build(ROOT/p['atlas_recipe'])
 require(type(actual)is dict and set(actual)==set(expected)|{'tex3ds_sha256'}and all(actual[k]==v for k,v in expected.items()if k!='layers')and type(actual['tex3ds_sha256'])is str and re.fullmatch('[0-9a-f]{64}',actual['tex3ds_sha256']),'Player atlas conversion receipt/source differs')
 require(type(actual['layers'])is list and len(actual['layers'])==2,'Player atlas layer count');_,base,_,nodes=graphics.read(ROOT/p['atlas_recipe']);main=expected['layers'][0];node=nodes[main['node']]
 model=dict(meta=dict(initial_frame=node.props.get('frame',0),frame_width=main['frames'][0]['width'],frame_height=main['frames'][0]['height'],source_columns=node.props['hframes'],source_rows=node.props['vframes'],atlas_columns=expected['atlas_columns']),layers=[],frames=None);files={}
 for row,want in zip(actual['layers'],expected['layers']):
  require(type(row)is dict and set(row)==set(want)|{'t3x_bytes','t3x_sha256'}and all(row[k]==v for k,v in want.items()),'Player atlas source frame receipt differs')
  raw=load(row['output']);require(type(row['t3x_bytes'])is int and len(raw)==row['t3x_bytes']and sha(raw)==row['t3x_sha256'],'Player atlas actual texture differs')
  require(row['source']==base['texture'if row['role']==1 else'bat_texture']and nodes[row['node']].attrs['parent']==('.'if row['role']==1 else main['node']),'Player atlas source hierarchy differs')
  frames=[{k:v for k,v in frame.items()if k!='rgba_sha256'}for frame in row['frames']];require(model['frames']is None or model['frames']==frames,'Player atlas layer frame maps differ');model['frames']=frames
  path=p['texture_directory']+'/'+row['output'];files[Path(path)]=raw
  model['layers'].append(dict(role=row['role'],width=row['width'],height=row['height'],parent=0 if row['role']==1 else 1,flags=int(row['centered'])|int(row['flip_h'])<<1|int(row['flip_v'])<<2|int(row['initial_visible'])<<3,position=row['position'],offset=row['offset'],path=path,source=row['source'],sha256=sha(raw),source_sha256=row['source_sha256'],bytes=len(raw),crc32=zlib.crc32(raw)))
 return presentation.encode(model),files,expected
def generate(actual,load):
 p=read();gfx,files,atlas=graphics_packet(actual,load);action,action_proof=audio.source();graph,graph_proof=tree.source();source=swing.source()
 # At width zero the original shader samples only the current UV. With
 # inside=false its neighbour predicate cannot add a pixel. Other outline
 # animations are not admitted as an implicit unshaded fallback.
 base,ex=swing.read();doc=Document(base['scene'],ex.text(base['scene']));node=next(n for n in doc.nodes if n.attrs['name']=='SpecialAnimations'and n.attrs.get('parent')=='.');ref=node.props.get('material');require(isinstance(ref,SubRef)and doc.sub_type(ref)=='ShaderMaterial','Player outline material type');material=doc.sub_resource(ref)
 require(set(material)=={'shader','shader_param/color','shader_param/width','shader_param/pattern','shader_param/inside'}and material['shader_param/width']==0 and material['shader_param/inside']is False and material['shader_param/pattern']==0 and doc.ext_path(material['shader'],'Shader')==p['outline_shader']['path'],'Player unshaded special material policy differs')
 shader=ex.data(p['outline_shader']['path']);require(sha(shader)==p['outline_shader']['sha256'],'Player reviewed outline shader differs')
 packets={p['swing_output']:swing.encode(source),presentation.read()['output']:gfx,audio.read()['output']:audio.encode(action),entry.read()['output']:tree.encode(graph)}
 require(len(packets)==4,'Player resource output collision');files.update({Path(k):v for k,v in packets.items()});sources={}
 for proof in (atlas,action_proof,graph_proof):
  for k,v in proof['sources'].items():require(k not in sources or sources[k]==v,'Player source digest conflict');sources[k]=v
 sources[p['outline_shader']['path']]=sha(shader)
 recipes=['content/field-player-assets.json','content/field-swing.json','content/field-swing-action.json','content/field-player-audio.json','content/field-swing-entry.json','content/field-player-tree.json','content/field-swing-graphics.json','content/field-swing-presentation.json']
 receipt=dict(schema=1,commit=swing.PIN,permission=atlas['permission'],atlas_receipt_sha256=sha(json_bytes(actual)),producers={name:sha((ROOT/name).read_bytes())for name in PRODUCERS},recipes={name:sha((ROOT/name).read_bytes())for name in recipes},sources=dict(sorted(sources.items())),outputs={path.as_posix():dict(bytes=len(blob),sha256=sha(blob))for path,blob in files.items()})
 return files,receipt
def checked(root,path):
 root=Path(root).resolve();target=(root/path).resolve();require(target.is_relative_to(root)and target.is_file()and not target.is_symlink(),'Player resource missing/unsafe: '+str(path));return target.read_bytes()
def stage_files(root=ROOT/'romfs'):
 p=read();actual=json.loads((ROOT/p['atlas_receipt']).read_text(encoding='utf8'),object_pairs_hook=audio.unique);directory=presentation.read()['texture_directory'];files,expected=generate(actual,lambda name:checked(root,directory+'/'+name))
 receipt=json.loads((ROOT/p['receipt']).read_text(encoding='utf8'),object_pairs_hook=audio.unique);require(receipt==expected,'Player resource receipt/source differs')
 for path,blob in files.items():require(checked(root,path)==blob,'Player resource is stale: '+str(path))
 return files
def main():
 ap=argparse.ArgumentParser();ap.add_argument('action',choices=('assets','compile','verify'));ap.add_argument('--from-atlases',type=Path);o=ap.parse_args();p=read();root=ROOT/'romfs'
 if o.action=='assets':
  require(o.from_atlases is not None,'Player assets need actual converted atlases');folder=o.from_atlases.resolve();require(folder.is_relative_to(ROOT/'build')and folder.is_dir(),'Player atlases must be owned build outputs')
  # Independent candidate producer admission must agree with the release model.
  canonical,_=presentation.build(folder);actual=json.loads((folder/'source.json').read_text(encoding='utf8'),object_pairs_hook=audio.unique);files,receipt=generate(actual,lambda name:checked(folder,name));require(files[Path(presentation.read()['output'])]==canonical,'Player release/candidate graphics differ')
  for path,blob in files.items():target=root/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
  for key,value in [('atlas_receipt',actual),('receipt',receipt)]:target=ROOT/p[key];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(json_bytes(value))
 else:
  # Admission precedes any ordinary compile write; asset changes are explicit.
  files=stage_files(root)
  if o.action=='compile':
   for path,blob in files.items():
    if path.suffix!='.t3x':(root/path).write_bytes(blob)
 print('Player resources:',len(files),'checked files;',sum(len(v)for v in files.values()),'bytes;',o.action,'; input/GPU integration verified separately')
if __name__=='__main__':main()
