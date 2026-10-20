from pathlib import Path
import hashlib,json,zlib,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools.extract_battle_entry import Extractor,PIN
from tools.godot_text import instantiate
from tools.podunk_field import Sources
paths=dict(scene='Maps/podunk/Catacombs.tscn',lever='Nodes/Overworld/switches/Lever.tscn',plate='Nodes/Overworld/switches/plate.tscn',gate='Nodes/Overworld/switches/Gate.tscn',wall='Nodes/Overworld/switches/Wall Door.tscn',switch_script='Scripts/Main/Switches/TwoStatesSwitch.gd',list_script='Scripts/Main/Switches/ObjectsListSwitch.gd',abstract_script='Scripts/Main/Switches/AbstractSwitch.gd',flag_script='Scripts/Main/FlaggableObject.gd',object_script='Scripts/Main/Switches/ControlledObject.gd',two_state_script='Scripts/Main/Switches/ControlledTwoStatesObject.gd',gate_script='Scripts/Main/Switches/ControlledDoor.gd',wall_script='Scripts/misc/wall door.gd',scene_script='Scripts/Dungeons/Catacombs.gd',area_script='Scripts/Main/RoomTypes/AreaRoom.gd',globaldata_script='Scripts/global/globalData.gd')
ex=Extractor(r);s=Sources(r);nodes=instantiate(s.doc,paths['scene'])
review=dict(schema=1,kind='encore.dungeon-switches.reviewed-source',commit=PIN,scene_id=4,scope='Candidate graph metadata for5 switches and9 controlled objects. Source flag identity, reset, XOR state, manual gates and asynchronous request order reviewed. No live dungeon, animation timeline, audio, camera, save admission or generic script approval.',paths=paths,sources={p:hashlib.sha256(ex.data(p)).hexdigest()for p in paths.values()},nodes=[dict(id=zlib.crc32((paths['scene']+'::'+n.path).encode()),path=n.path,kind=k)for n in nodes for k in('lever','plate','gate','wall')if n.scene==paths[k]])
assert len(review['nodes'])==14 and len({n['id']for n in review['nodes']})==14
out=r/'content/catacombs-switches.json';assert not out.exists();out.write_text(json.dumps(review,indent=2)+'\n',encoding='utf8');print('Authored scoped14-node source review; no pack or live admission')
