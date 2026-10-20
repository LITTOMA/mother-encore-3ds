"""Private typed-link/loader fixture. Its texture is synthetic, never rendered."""
import argparse,copy,json,sys
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import resource_catalog as c,world_scene_routes as w,podunk_field as f,field_area,field_objects_assets as objects
def build(out):
 p,m,canvas,meta,proof=objects.source();atlas=f.Atlases(m);paths=[m.recipe['atlas_directory']+'/atlas-%02d.t3x'%i for i in range(len(atlas.pages))];area,_=field_area.source(ROOT/p['area_recipe']);map_blob=f.encode_map(m,atlas,paths,area=area)
 recipe=w.load()
 if recipe['schema']==4:
  recipe=copy.deepcopy(recipe);recipe['schema']=3
  for scene in recipe['scenes']:
   if scene['map']:scene['assets'].pop('objects')
 new=copy.deepcopy(recipe);new['schema']=4
 for scene in new['scenes']:
  if scene['map']:scene['assets']['objects']=1032 if scene['id']==meta['scene']else 0
 sources=f.Sources();routes=w.rows(new,sources);links=w.encode(new,routes);legacy_links=w.encode(recipe,routes)
 texture=b'Explicit synthetic loader texture fixture; not Tex3DS';object_blob=objects.encode(meta,texture);ir=c.load_catalog();ir['bindings']=[r for r in ir['bindings']if r['role']!='FieldObjects'];ir['bindings'].append(dict(id=1032,role='FieldObjects',path=p['output']));c.validate(ir)
 override={p['texture']:texture,p['output']:object_blob,'world/world.enclinks':links,m.recipe['outputs']['map']:map_blob}
 original=c.checked_file;files={}
 def read(root,path):
  raw=override.get(path)
  if raw is None:raw=original(root,path)
  files[path]=raw;return raw
 with patch.object(c,'checked_file',side_effect=read):catalog=c.encode(ir)
 files.update(override);files['system/native.encresources']=catalog;files['system/legacy.encresources']=(ROOT/'tests/fixtures/catalog-cap12-swing.encresources').read_bytes();files['world/legacy.enclinks']=legacy_links
 for name,raw in files.items():
  target=out/name;assert target.resolve().is_relative_to(out);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
 report=dict(scope=__doc__,catalog_capability=13,links_capability=5,objects=1032,role=49,texture=p['texture'],files=len(files),bytes=sum(len(v)for v in files.values()),sources=proof['sources'],legacy_catalog_capability=12,legacy_links_capability=4)
 (out/'source.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');return report
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();out=a.out.resolve();assert out!=ROOT/'build'and(ROOT/'build').resolve()in out.parents;out.mkdir(parents=True,exist_ok=True);report=build(out);print('Private object-link fixture:',report['files'],'files,',report['bytes'],'bytes; synthetic texture, no GPU')
