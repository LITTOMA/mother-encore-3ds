"""Private full-source Room14 scene fixture; no Root contact/audio claim."""
import argparse,json,sys
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f,field_story,resource_catalog as catalog,native_content,field_props

def build(out):
 out=Path(out).resolve();assert out!=ROOT/'build'and(ROOT/'build').resolve()in out.parents;out.mkdir(parents=True,exist_ok=True)
 recipe=f.load_recipe();room=f.build_room(recipe,f.Model(recipe),recipe['house']['room_ir']);story=field_story.build();ir=catalog.load_catalog();overrides={recipe['outputs']['room']:room,field_story.OUTPUT:story};files={};original=catalog.checked_file
 # This earlier test owns the direct hit/callback seam, so keep its legacy
 # rendering-only prop packet explicit. The contact test replaces it in full.
 data,clips,_=field_props.source();overrides[field_props.read()['output']]=field_props.encode(data,clips,capability=2)
 def read(root,name):
  raw=overrides.get(name)
  if raw is None:raw=original(root,name)
  files[name]=raw;return raw
 with patch.object(catalog,'checked_file',side_effect=read):data=catalog.encode(ir)
 files['system/native.encresources']=data
 # This genuine small object texture is checked by the immutable scene loader.
 name='graphics/world/podunk/objects/atlas-00.t3x';files[name]=(ROOT/'romfs'/name).read_bytes()
 for name,raw in files.items():
  p=out/name;assert p.resolve().is_relative_to(out);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
 (out/'source.json').write_text(json.dumps(dict(scope=__doc__,files=len(files),bytes=sum(len(v)for v in files.values()),room_capability=native_content.parse_pack(room)['capabilities'],formal_programmes=8,existing_tutorial_commands=87),indent=2)+'\n',encoding='utf8')
 print('Private complete source scene:',len(files),'files; original generators; no shipping mutation or contact/audio claim')

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.out)
