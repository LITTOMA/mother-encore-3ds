"""Unpublished source map/object binding fixture; no release texture or gameplay."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tests.prepare_field_objects import build as standalone
from tools import podunk_field as f,field_area,field_ready_rng,mick_treats
from tools.godot_text import owner_of

def build(out):
 audit=json.loads((ROOT/'reports/iggy-tutorial/shovel-preparation/shovel-next-source-audit.json').read_bytes());path=audit['object_path']
 # This fixture changes only the scene selected by the existing checked
 # AreaRoom source admission. It does not synthesize AreaRoom metadata.
 policy=field_area.policy();policy['field_recipe']='content/podunk-field.json';policy_path=out/'area-policy.json';policy_path.write_text(json.dumps(policy,indent=2)+'\n',encoding='utf8');area,area_sources=field_area.source(policy_path)
 model=f.Model(f.load_recipe(),object_bindings={path:1});atlases=f.Atlases(model);paths=[model.recipe['atlas_directory']+'/atlas-%02d.t3x'%i for i in range(len(atlases.pages))]
 blob=f.encode_map(model,atlases,paths,area=area);scene,parts=f.unpack_container(blob,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT)
 def text(span):return model.strings[span[0]:span[0]+span[1]].decode('utf8')
 draw=[i for i,r in enumerate(model.items)if r['kind']==7];notice=[i for i,r in enumerate(model.notices)if text(r['path'])==path];assert len(draw)==len(notice)==1
 body_path=path+'/StaticBody2D';body_node=model.scene.by_path[body_path];shape_node=model.scene.by_path[body_path+'/CollisionShape2D'];owner=owner_of(shape_node,'shape');assert owner.sub_type(shape_node.props['shape'])=='RectangleShape2D';ext=owner.sub_resource(shape_node.props['shape'])['extents'];xf=model.scene.global_xf[shape_node.path];assert(xf.xx,xf.xy,xf.yx,xf.yy)==(1,0,0,1)
 center=[xf.ox,xf.oy];extents=[ext.x,ext.y];layer=body_node.props.get('collision_layer',1);mask=body_node.props.get('collision_mask',1);collides=model.collides(layer,mask);body=model.body_by_path.get(body_path,f.NONE);assert collides==(body!=f.NONE)
 ops,rng=field_ready_rng.replay(model.sources.extractor,model.scene.nodes,mick_treats.READY_SCRIPTS);sample=rng['samples'][path+'/Sparkles'];assert sample['seeded']
 original=standalone();_,sections=f.unpack_container(original,b'ENCFOB01',(1,64,96,16,4),1024*1024,capability=1)
 sections=list(sections);policy=bytearray(sections[1]);struct.pack_into('<Q',policy,8,rng['state']);struct.pack_into('<2I',policy,56,model.player_layer,model.player_mask);sections[1]=bytes(policy)
 obj=bytearray(sections[2]);struct.pack_into('<I',obj,8,struct.unpack_from('<I',obj,8)[0]|256);resolved=min(int(sample['value']),struct.unpack_from('<I',obj,20)[0]-1);struct.pack_into('<I',obj,28,resolved+1)
 obj+=struct.pack('<4I4f2I',body,draw[0],notice[0],0,*center,*extents,layer,mask);sections[2]=bytes(obj)
 objects=f.pack_container(b'ENCFOB01',scene,sections,(1,64,136,16,4),1024*1024,capability=2)
 return blob,objects,dict(scope=__doc__,scene=scene,object_path=path,body=body,draw=draw[0],notice=notice[0],body_center=center,body_extents=extents,body_layer=layer,body_mask=mask,player_layer=model.player_layer,player_mask=model.player_mask,player_collides=collides,ready_state=rng['state'],resolved=resolved,sample=sample,operations=len(ops),sources=dict(sorted({**model.sources.extractor.sources,**area_sources}.items())),map_sha256=hashlib.sha256(blob).hexdigest(),objects_sha256=hashlib.sha256(objects).hexdigest())

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve();assert out!=ROOT/'build'and(ROOT/'build').resolve()in out.parents;out.mkdir(parents=True,exist_ok=True);blob,objects,proof=build(out);(out/'source.encmap').write_bytes(blob);(out/'source.encobjects').write_bytes(objects);(out/'source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Unpublished bound object fixture:',len(blob),'map bytes;',len(objects),'object bytes; source ready frame',proof['resolved'],'body',proof['body_extents'])
