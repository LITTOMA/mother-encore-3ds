from pathlib import Path
import sys,hashlib,json
from PIL import Image
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools import podunk_field as f
m=f.Model(f.load_recipe());sc=m.scene;path='Objects/Items/Item';node=sc.by_path[path];sprite=sc.by_path[path+'/Sprite'];sparkle=sc.by_path[path+'/Sparkles']
assert node.props['item']=='Shovel'and node.props['flag']=='got_shovel'
source='upstream/MOTHER-Encore/';files=['Data/Dialogue/Podunk/cutscenes/gravedigger_getshovel.yaml','Data/Dialogue/Podunk/cutscenes/gravedigger_trytoleave.yaml','Data/Dialogue/Podunk/iggy_return_shovel.yaml','Data/Items/Shovel.yaml','Nodes/Overworld/Objects/DroppedItem.tscn','Scripts/Main/DroppedItem.gd','Scripts/Main/ItemHolder.gd','Scripts/Main/FlaggableObject.gd','Nodes/Reusables/Effects/Sparkles.tscn','Scripts/misc/sparkles.gd','Graphics/Objects/Items/Shovel.png','Graphics/Objects/Common/Sparkles.png','Maps/podunk/podunk.tscn']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();proof=dict(commit=f.PIN,sources={name:sha(r/source/name)for name in files},object_path=path,ready=dict(item=node.props['item'],flag=node.props['flag'],position=[node.props['position'].x,node.props['position'].y]),images={})
for name in ['Graphics/Objects/Items/Shovel.png','Graphics/Objects/Common/Sparkles.png']:
 with Image.open(r/source/name)as im:proof['images'][name]=dict(size=list(im.size),mode=im.mode)
doc=f.owner_of(sparkle,'frames');frames=doc.sub_resource(sparkle.props['frames'])['animations'];assert len(frames)==1;clip=frames[0];rects=[]
for frame in clip['frames']:
 texture=doc.sub_resource(frame);rect=texture['region'];rects.append([rect.x,rect.y,rect.w,rect.h])
assert clip['name']=='Sparkle On'and clip['loop']is True and clip['speed']==12 and sparkle.props['frame']==22 and sparkle.props['playing']is True
proof['sparkles']=dict(name=clip['name'],speed=clip['speed'],loop=clip['loop'],serialized_initial_frame=22,random_range=[0,47],frames=rects,position=[sparkle.props['position'].x,sparkle.props['position'].y],note='sparkles.gd ready consumes rand_range(0,47), then AnimatedSprite clamps frame to valid source frame count. Child ready occurs even if parent later queues itself for got_shovel. Future runtime must preserve actual RNG order; not implemented by this audit.')
proof['lifecycle']='DroppedItem ready flag suppresses on reentry only; gravedigger_getshovel phrase0 sets got_shovel, phrase3 explicitly queues Objects/Items/Item deletion. No automatic dynamic flag-hide, no invented item grant. Root callback and full pickup/return alternate remain to implement.'
proof['limits']='Source audit only, not a reviewed shipping programme/format or implemented object consumer. Existing world atlas contains no Shovel image; needs genuine conversion and checked drawing/object lifecycle.'
root_policy=json.loads((r/'content/field-props.json').read_bytes());audio=sc.by_path[root_policy['instances'][0]['path']+'/AudioStreamPlayer'];proof['root_audio_raw']={key:repr(value)for key,value in audio.props.items()}
from tools import field_props
root_data,_,_=field_props.source();proof['shovel_root_binding']=root_data['props'][11];proof['root_flag_names']=root_data['flags']
(r/'build/full-port/shovel-next-source-audit.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print(proof['images']);print('Sparkle source frames',len(rects),'unique',len({tuple(v)for v in rects}))
