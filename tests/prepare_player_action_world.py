"""Generate real metadata bytes for persistent World/action integration."""
import argparse,copy,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import resource_catalog as c,field_player_tree as tree
def prepare(out):
 out=Path(out).resolve();c.require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','World action fixture must stay in build');out.mkdir(parents=True,exist_ok=True);ir=copy.deepcopy(c.load_catalog())
 for row in ir['bindings']:
  target=out/row['path'];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(c.checked_file(ROOT/'romfs',row['path']))
 model,proof=tree.source()
 for role,fixture in [('FieldSwing','field-swing-v0410.encswing'),('FieldSwingGraphics','field-swing-graphics-v0410.encgraphics'),('FieldSwingAction','field-swing-action-v0410.encaction'),('FieldSwingEntry',None)]:
  path='world/fixture/'+role+c.SUFFIXES[role];target=out/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(tree.encode(model)if fixture is None else(ROOT/'tests/fixtures'/fixture).read_bytes());ir['bindings'].append(dict(id=c.ROLES[role],role=role,path=path))
 blob=c.encode(ir,out);c.decode(blob);(out/'system/native.encresources').write_bytes(blob)
 legacy=copy.deepcopy(ir);legacy['bindings'][-1]['path']='world/fixture/legacy.encentry';(out/'world/fixture/legacy.encentry').write_bytes((ROOT/'tests/fixtures/field-swing-entry-v0410.encentry').read_bytes());old=c.encode(legacy,out);c.decode(old);(out/'system/legacy.encresources').write_bytes(old)
 (out/'action-source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('World action actual metadata:',len(blob),'catalog bytes; complete capability2 graph plus independently catalogued legacy entry; no texture/audio or shipping mutation')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);prepare(p.parse_args().out)
