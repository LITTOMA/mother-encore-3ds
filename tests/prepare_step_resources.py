"""Metadata-only schema fixture: Mayor stepping bytes deliberately bound to
Podunk identity2. No texture/PCM copy and no claim of original gameplay.
"""
import argparse,copy,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import resource_catalog as c,world_scene_routes as w,field_footsteps as s

def build(out):
    out=Path(out).resolve();c.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Step fixture must stay under build');out.mkdir(parents=True,exist_ok=True)
    ir=copy.deepcopy(c.load_catalog());graph=copy.deepcopy(w.load());graph['scenes']=graph['scenes'][:2];graph['routes']=graph['routes'][:2];graph['scenes'][1]['assets']['steps']=1030
    links,_=w.build(graph);_,metadata,_=s.source();metadata['scene']=2;packet=s.encode(metadata)
    for row in ir['bindings']:
        target=out/row['path'];target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/'romfs'/row['path'],target)
    target=out/'world/world.enclinks';target.write_bytes(links)
    path='world/fixture/source.encsteps';target=out/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(packet)
    ir['bindings'].append(dict(id=1030,role='FieldSteps',path=path));blob=c.encode(ir,out);target=out/'system/native.encresources';c.decode(blob);target.write_bytes(blob)
    print('Step schema fixture:',len(blob),'catalog bytes; metadata only, artificial scene binding')
if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('--out',type=Path,required=True);o=a.parse_args();build(o.out)
