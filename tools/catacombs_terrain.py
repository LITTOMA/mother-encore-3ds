"""Detached Catacombs source terrain; no actor/root/scene/save admission.

Reuse the existing reviewed field section serializer and geometry primitives;
independent envelopes retain all logical cells and navigation metadata.
"""
import argparse,copy,hashlib,json,math,re,shlex,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as field
from tools import dungeon_keys as keys
from tools.dungeon_key_indicator import unique
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import Document,Node,Vec2,Transform2D
from tools.run_dungeon_key_contact_reference import player_source
RECIPE=ROOT/'content/catacombs-terrain.json';MAGIC=b'ENCTERR1';MAP_MAGIC=b'ENCTMAP1'
STRIDES=(1,1,32,80,32,72,32,8,4);LIMIT=8*1024*1024

def read(file=RECIPE):
 def invalid(v):raise ValueError('Terrain nonfinite JSON')
 p=json.loads(Path(file).read_text('utf-8'),object_pairs_hook=unique,parse_constant=invalid)
 keys.fields(p,'schema kind commit scope scene tileset player scene_id sources layers inactive_script navigation missing_cells locales texture_root engine_commit engine_sha256 native_oracles license_review','terrain review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.catacombs-terrain.reviewed-source'and p['commit']==PIN and type(p['scene_id'])is int and 0<p['scene_id']<65536,'Terrain review identity')
 for k in ['scope','license_review']:require(type(p[k])is str and 0<len(p[k])<=4096,'Terrain review scope/license')
 for k in ['scene','tileset','player','texture_root']:keys.path(p[k])
 require(p['engine_commit']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'and type(p['engine_sha256'])is str and re.fullmatch('[0-9a-f]{64}',p['engine_sha256']),'Terrain engine identity')
 require(type(p['sources'])is dict and set([p['scene'],p['tileset'],p['player'],'project.godot','LICENSE'])<=set(p['sources']),'Terrain source coverage');ex=Extractor(ROOT)
 for name,digest in p['sources'].items():keys.path(name);require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(ex.data(name)).hexdigest()==digest,'Terrain source changed:'+name)
 require(type(p['layers'])is list and 0<len(p['layers'])<=256,'Terrain layer count');ids=set();paths=set()
 for row in p['layers']:
  keys.fields(row,'id path','terrain layer');keys.path(row['path']);require(type(row['id'])is int and 0<row['id']<2**32 and row['id']not in ids and row['path']not in paths,'Terrain layer identity');ids.add(row['id']);paths.add(row['path'])
 require([row['id']for row in p['layers']]==sorted(ids)and type(p['locales'])is list and p['locales']and p['locales'][0]=='en'and all(type(v)is str and re.fullmatch('[A-Za-z_]+',v)for v in p['locales']),'Terrain order/locales')
 keys.fields(p['inactive_script'],'subresource class source_code has_source can_instance base methods properties signals','terrain inactive script');s=p['inactive_script'];require(type(s['subresource'])is int and s['subresource']>0 and s['class']=='GDScript'and s['source_code']==s['base']==''and s['has_source']is False and s['can_instance']is False and s['methods']==s['properties']==s['signals']==[],'Terrain supports only the observed non-instantiable empty script')
 require(type(p['navigation'])is list and 0<len(p['navigation'])<=64 and type(p['missing_cells'])is list and len(p['missing_cells'])<=64,'Terrain navigation/missing-cell review')
 for row in p['navigation']:keys.fields(row,'tile subresource','terrain navigation');require(all(type(v)is int and 0<v<2**32 for v in row.values()),'Terrain navigation identity')
 for row in p['missing_cells']:
  keys.fields(row,'layer x y tile autotile flags','terrain known missing cell');require(type(row['layer'])is int and row['layer']in ids and all(type(row[k])is int for k in ['x','y','tile','flags'])and type(row['autotile'])is list and len(row['autotile'])==2 and all(type(v)is int for v in row['autotile']),'Terrain missing-cell identity')
 require(type(p['native_oracles'])is dict and len(p['native_oracles'])==2 and all(keys.path(k)and k.startswith('reports/catacombs-terrain/native-')and type(v)is str and re.fullmatch('[0-9a-f]{64}',v)for k,v in p['native_oracles'].items()),'Terrain native oracle identities')
 return p,ex

class TerrainModel(field.Model):
 """Only initialize the row model; reuse layer/tile/shape/image execution.

 The two removed resource-view entries are separately retained and validated
 as metadata. Original bytes/documents are never modified, and this view is
 not a permissive catch-all for script/navigation or unreviewed properties.
 """
 def __init__(self,p,sources,native):
  self.sources=sources;self.recipe={'scene':{'id':p['scene_id']},'locales':p['locales'],'tileset':p['tileset']};self.object_bindings={};self.npc_bindings={}
  doc=sources.doc(p['tileset']);script=doc.resource.get('script');require(script is not None and script.id==p['inactive_script']['subresource']and doc.sub[script.id]==('GDScript',{}),'Terrain inactive script changed')
  require(native['script']==dict(present=True,**{k:v for k,v in p['inactive_script'].items()if k!='subresource'}),'Terrain original inactive script behavior differs')
  nav_keys={str(row['tile'])+'/navigation'for row in p['navigation']};require({k for k in doc.resource if k.endswith('/navigation')}==nav_keys,'Terrain navigation coverage changed')
  for row in p['navigation']:require(doc.resource[str(row['tile'])+'/navigation'].id==row['subresource']and doc.sub[row['subresource']][0]=='NavigationPolygon','Terrain navigation binding changed')
  filtered=copy.copy(doc);filtered.resource={k:v for k,v in doc.resource.items()if k!='script'and k not in nav_keys}
  class CheckedResourceView:
   def doc(_self,name):return filtered if name==p['tileset']else sources.doc(name)
  self.tileset=field.TileSet(CheckedResourceView(),p['tileset'])
  self.images=field.Images(sources);self.pictures=field.Pictures(self.images);self.textures=field.Textures(sources,self.recipe,self.images)
  self.strings=bytearray();self.string_index={};self.conditions=[];self.condition_index={};self.tile_rows=[];self.tile_index={};self.shape_rows=[];self.points=[];self.layers=[];self.chunks=[];self.cells=[];self.sprites=[];self.items=[];self.groups=[];self.openables=[];self.doors=[];self.boundaries=[];self.notices=[];self.cameras=[];self.bodies=[];self.undefined_cells=0;self.layer_by_path={}
  player,proof=player_source();require(proof['scene']==p['player']if 'scene'in proof else p['player']=='Nodes/Reusables/Player.tscn','Terrain player binding changed');self.player_layer=player['layer'];self.player_mask=player['mask'];self.player_proof=proof
  original=Document(p['scene'],sources.text(p['scene']));root=original.nodes[0];require(root.attrs['type']=='Node2D'and set(root.props)=={'script','_region_name'},'Terrain root canvas transform/state changed')
  by_name={n.attrs['name']:n for n in original.nodes if n.attrs.get('type')=='TileMap'};require(set(by_name)=={r['path']for r in p['layers']},'Terrain TileMap coverage changed')
  # Construct only the five declared native TileMaps. Unported actor scenes
  # are not imported or silently classified as supported terrain.
  by_path={}
  for name,decl in by_name.items():
   require(set(decl.attrs)=={'name','type','parent'}and decl.attrs['parent']=='.','Terrain layer attributes changed')
   by_path[name]=Node(path=name,name=name,type='TileMap',script=None,scene=None,props=dict(decl.props),groups=[],owner_scene=p['scene'],resolve=original)
  self.source_nodes=[by_path[row['path']]for row in p['layers']]
  require(all(n.type=='TileMap'and '/'not in n.path and n.script is None for n in self.source_nodes),'Terrain source layer hierarchy/script changed')
  class ScopedCanvas:
   global_xf={n.path:field.local_transform(n)for n in self.source_nodes}
   def conditions(_self,path):return []
   def visible(_self,path):return by_path[path].props.get('visible',True)
  self.scene=ScopedCanvas();missing=[]
  for binding,node in zip(p['layers'],self.source_nodes):
   source_cells=field.decode_cells(node)
   for c in source_cells:
    if c[2]not in self.tileset.tiles:missing.append({'layer':binding['id'],'x':c[0],'y':c[1],'tile':c[2],'autotile':list(c[5]),'flags':c[3]|c[4]<<1})
   require(all(v in p['missing_cells']for v in missing),'Terrain unreviewed missing source tile')
   layer=self.layer(node);kind=2 if node.props.get('cell_y_sort',False)else 1;first=len(self.items);self.items.append(self._item(kind,layer,1,order=len(self.groups)));self.groups.append({'kind':2 if kind==2 else 1,'first':first,'count':1})
  require(missing==p['missing_cells']and self.undefined_cells==len(missing),'Terrain missing-cell scope differs')
  self.root_name=root.attrs['name'];self.declared_scene_nodes=len(original.nodes)

def source_model(file=RECIPE):
 p,ex=read(file);native=[]
 for name,sha in p['native_oracles'].items():
  raw=(ROOT/name).read_bytes();require(hashlib.sha256(raw).hexdigest()==sha,'Terrain native observation changed');native.append(json.loads(raw))
 require(native[0]==native[1]and native[0]['engine']['hash']==p['engine_commit'],'Terrain native repeats/engine differ')
 source=field.Sources(ROOT);model=TerrainModel(p,source,native[0]);atlases=field.Atlases(model);paths=[p['texture_root']+'/atlas-'+str(i).zfill(2)+'.t3x'for i in range(len(atlases.pages))]
 # Reuse the typed section serializer, then build a NEW independent envelope.
 # The temporary ordinary-map envelope is never saved/admitted as a scene;
 # this model contains no Player, actors, events or other scene behaviors.
 ordinary=field.encode_map(model,atlases,paths);scene,sections=field.unpack_container(ordinary,field.MAP_MAGIC,field.MAP_STRIDES,field.MAP_LIMIT,capability=1)
 terrain=field.pack_container(MAP_MAGIC,scene,sections,field.MAP_STRIDES,field.MAP_LIMIT,capability=1)
 proof={'scope':p['scope'],'commit':PIN,'review_sha256':hashlib.sha256(Path(file).read_bytes()).hexdigest(),'sources':source.digests(),'graphics_bytes':len(terrain),'logical_cells':sum(len(field.decode_cells(n))for n in model.source_nodes),'render_cells':len(model.cells),'tile_variants':len(model.tile_rows),'shapes':len(model.shape_rows),'pictures':len(model.pictures.rows),'atlas_pages':len(atlases.pages),'source_script':'Exact native non-instantiable empty GDScript is retained as metadata;no generic script execution.','navigation':'Original nonempty navigation data remains source metadata;not interpreted as a navigation solver.','missing_cell':'The known unresolved source cell remains in logical metadata;no visual/physics tile definition is invented.'}
 return model,atlases,terrain,native[0],proof

def encode_source(model,terrain,native,policy):
 # Every logical original cell, including the unresolved native cell, is kept.
 strings=bytearray();text_index={}
 def text(value):
  if value=='':return(0,0)
  require(type(value)is str and '\0'not in value and len(value.encode('utf-8'))<=4096,'Terrain text')
  if value not in text_index:text_index[value]=(len(strings),len(value.encode('utf-8')));strings.extend(value.encode('utf-8'))
  return text_index[value]
 meta=struct.pack('<8I',*text(policy['scene']),*text(model.root_name),policy['inactive_script']['subresource'],1,model.player_layer,model.player_mask)
 layers=bytearray();cells=bytearray();definitions=bytearray();navigation=bytearray();points=bytearray();indices=bytearray()
 native_layers={v['name']:v for v in native['layers']};native_tiles={t['id']:t for t in native['tiles']}
 require(set(native_tiles)==set(model.tileset.tiles),'Terrain source/native tile definitions differ')
 for order,(binding,node)in enumerate(zip(policy['layers'],model.source_nodes)):
  source_cells=field.decode_cells(node);observed=native_layers[node.path]
  expected=sorted([[c[0],c[1],c[2],bool(c[3]),bool(c[4]),False,c[5][0],c[5][1],c[2]in model.tileset.tiles]for c in source_cells],key=lambda c:(c[1],c[0]))
  require(expected==observed['cells'],'Terrain native logical cells differ:'+node.path)
  position=model.scene.global_xf[node.path];require(observed['position']==[position.ox,position.oy]and observed['cell_size']==[node.props['cell_size'].x,node.props['cell_size'].y]and observed['layer']==node.props.get('collision_layer',1)and observed['mask']==node.props.get('collision_mask',1)and observed['ysort']==node.props.get('cell_y_sort',False)and observed['origin']==field.tilemap_policy(node),'Terrain native layer settings differ')
  first=len(cells)//32
  for c in sorted(source_cells,key=lambda c:(c[1],c[0])):
   compiled=model.tile_index.get((c[2],c[5],c[3],c[4]),field.NONE);require((compiled!=field.NONE)==(c[2]in model.tileset.tiles),'Terrain compiled/logical cell identity mismatch')
   cells.extend(struct.pack('<I2hI2H4I',binding['id'],c[0],c[1],c[2],*c[5],c[3]|c[4]<<1,compiled,c[6],0))
  flags=(1 if observed['ysort']else 0)|(2 if model.scene.visible(node.path)else 0)
  layers.extend(struct.pack('<4I4f4I4i4I',binding['id'],order,*text(node.path),*observed['position'],*observed['cell_size'],observed['layer'],observed['mask'],flags,observed['origin'],*observed['used_rect'],first,len(source_cells),order,0))
 for tid,tile in sorted(native_tiles.items()):
  source_tile=model.tileset.tiles[tid];r=source_tile.get('region',field.Rect2(0,0,0,0));offset=source_tile.get('tex_offset',Vec2(0,0));texture=model.tileset.doc.ext_path(source_tile['texture'],'Texture')
  require(tile['name']==source_tile.get('name','')and tile['region']==[r.x,r.y,r.w,r.h]and tile['offset']==[offset.x,offset.y]and tile['z']==source_tile.get('z_index',0)and tile['mode']==source_tile.get('tile_mode',0)and tile['texture']['path']=='res://'+texture,'Terrain source/native tile metadata differs')
  frame_paths,fps=model.textures.frames_of(texture)
  if len(frame_paths)>1:
   animation=tile['texture'].get('animation');require(animation is not None and animation['fps']==fps and [v['path']for v in animation['frames']]==['res://'+v for v in frame_paths]and all(v['delay']==0 for v in animation['frames'])and not animation['pause']and not animation['oneshot'],'Terrain native texture animation differs')
  nav=tile['navigation'];nav_first=0;nav_count=0
  if nav is not None:
   binding=next((n for n in policy['navigation']if n['tile']==tid),None);require(binding is not None,'Terrain undeclared native navigation')
   doc=model.sources.doc(policy['tileset']);kind,props=doc.sub[binding['subresource']];require(kind=='NavigationPolygon'and set(props)=={'vertices','polygons'}and [[v.x,v.y]for v in props['vertices']]==nav['vertices']and props['polygons']==nav['polygons'],'Terrain source/native navigation geometry differs')
   require(nav['offset']==[0,0]if str(tid)+'/navigation_offset'not in doc.resource else nav['offset']==[doc.resource[str(tid)+'/navigation_offset'].x,doc.resource[str(tid)+'/navigation_offset'].y],'Terrain navigation offset differs')
   nav_first=len(navigation)//32;nav_count=len(nav['polygons'])
   for polygon in nav['polygons']:
    pf=len(points)//8;ix=len(indices)//4
    for v in nav['vertices']:points.extend(struct.pack('<2f',*v))
    for value in polygon:indices.extend(struct.pack('<I',value))
    navigation.extend(struct.pack('<5I2fI',tid,pf,len(nav['vertices']),ix,len(polygon),*nav['offset'],0))
  definitions.extend(struct.pack('<5I6fi6I',tid,*text(tile['name']),*text(texture),*tile['region'],*tile['offset'],tile['z'],tile['mode'],len(tile['shapes']),nav_first,nav_count,0,0))
 sections=[strings,terrain,meta,layers,cells,definitions,navigation,points,indices]
 return field.pack_container(MAGIC,policy['scene_id'],sections,STRIDES,LIMIT,capability=1)


def compile(out,tool,command=None,file=RECIPE):
 out=Path(out).resolve();tool=Path(tool).resolve();require(out.is_relative_to(ROOT/'build')and out.suffix=='.encterr'and not out.parent.exists()and tool.is_file(),'Fresh terrain candidate under build and actual tex3ds required')
 model,atlases,graphics,native,proof=source_model(file);policy,_=read(file);blob=encode_source(model,graphics,native,policy);out.parent.mkdir(parents=True);(out.parent/'.owned-terrain-candidate').write_text('source-terrain-candidate-v1',encoding='ascii');args=command or[str(tool)];assets=[]
 for i,page in enumerate(atlases.pages):
  relative=policy['texture_root']+'/atlas-'+str(i).zfill(2)+'.t3x';target=out.parent/relative;target.parent.mkdir(parents=True,exist_ok=True);png=target.with_suffix('.png');page.save(png)
  with target.with_suffix('.log').open('wb')as log:subprocess.run(args+['-f','rgba8','-z','none','-o',str(target),str(png)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
  raw=target.read_bytes();assets.append({'path':relative,'width':page.width,'height':page.height,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()})
 out.write_bytes(blob);out.with_suffix('.enctmap').write_bytes(graphics);proof.update(resource_bytes=len(blob),resource_sha256=hashlib.sha256(blob).hexdigest(),graphics_sha256=hashlib.sha256(graphics).hexdigest(),graphics_assets=assets,converter_sha256=hashlib.sha256(tool.read_bytes()).hexdigest());out.with_suffix('.source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8');print('Compiled detached terrain:',len(blob),'bytes;',len(assets),'actual T3X atlas;no actor/root/navsolver/save admission');return blob
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--tex3ds',type=Path,required=True);p.add_argument('--tex3ds-command');a=p.parse_args();compile(a.out,a.tex3ds,shlex.split(a.tex3ds_command)if a.tex3ds_command else None)
