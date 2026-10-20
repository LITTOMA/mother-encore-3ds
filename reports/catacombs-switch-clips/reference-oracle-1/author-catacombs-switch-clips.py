from pathlib import Path
import hashlib,json,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools.extract_battle_entry import Extractor,PIN
from tools.godot_text import Document
ex=Extractor(r);profiles=[];sources={}
for i,(name,scene,texture,script,frame,body)in enumerate([
 ('lever','Lever','Lever','Scripts/Main/Switches/TwoStatesSwitch.gd','main','collision/CollisionShape2D'),
 ('plate','plate','Plate','Scripts/Main/Switches/TwoStatesSwitch.gd','Sprite',None),
 ('gate','Gate','Gate','Scripts/Main/Switches/ControlledDoor.gd','main','collision/CollisionShape2D')],1):
 scene='Nodes/Overworld/switches/'+scene+'.tscn';texture='Graphics/Objects/Catacombs/'+texture+'.png';doc=Document(scene,ex.text(scene));anim=next(n for n in doc.nodes if n.attrs['name']=='AnimationPlayer')
 roles={'TurnOff':1,'TurnOn':2,'Off':3,'On':4,'StayOff':5,'StayOn':6,'RESET':7}
 clips=[dict(id=roles[k[6:]],name=k[6:])for k in anim.props]
 profiles.append(dict(id=i,name=name,scene=scene,script=script,texture=texture,frame_owner=frame,body_owner=body,clips=clips))
 for p in(scene,texture,script):sources[p]=hashlib.sha256(ex.data(p)).hexdigest()
review=dict(schema=1,kind='encore.dungeon-switch-clips.reviewed-source',commit=PIN,scope='Frame/body property clips for source Lever,Plate,Gate only. Reuses existing ENCPCL01; no script/scene/texture binding, physical contact or release admission. Continuous multi-key curves, methods, audio and other properties reject.',profiles=profiles,sources=sources)
p=r/'content/catacombs-switch-clips.json';assert not p.exists();p.write_text(json.dumps(review,indent=2)+'\n',encoding='utf8');print('Authored3 source clip profiles with stable animation-role IDs; no generated pack changed')
