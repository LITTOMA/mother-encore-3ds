#!/usr/bin/env python3
"""Actual native AP bindings; playback remains in reviewed typed core owners."""
import argparse,hashlib,json,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import read,write,sha,require,PIN,SCENE,decode
from tools.extract_battle_entry import Extractor
IR=ROOT/'content/native-scene-clip-native.json'
PACK=ROOT/'romfs/data/podunk.encclipnative'
REVIEW=ROOT/'reports/scene-clip-native/source-review.json'
INPUTS=[('content/podunk-openable-door.json','records'),('content/native-field-present.json','holders'),('content/native-field-emotes.json','records'),('content/native-field-dead-bush.json','records')]
SOURCES=['Nodes/Overworld/Objects/Openable Door.tscn','Nodes/Overworld/Objects/Present.tscn','Nodes/Ui/emotes.tscn','Nodes/Overworld/Objects/Dead Bush.tscn']
def derive(native,engine):
    ex=Extractor(ROOT);t=read(ROOT/'content/podunk-node-tree.json');by={v['node']:v for v in t['records']}
    d=read(native); require(d['source']=='res://'+SCENE,'Native source scene changed')
    nm={n['path']:n for n in d['nodes']};records=[]
    deps={p:sha(ROOT/p) for p,_ in INPUTS};deps['content/podunk-node-tree.json']=sha(ROOT/'content/podunk-node-tree.json')
    for kind,((p,key),source) in enumerate(zip(INPUTS,SOURCES),1):
        owner=read(ROOT/p);ex.text(source)
        for n,h in owner['sources'].items():ex.data(n);require(ex.sources[n]==h,'Clip source closure changed '+n)
        for v in owner[key]:
            path=v['node']+'/AnimationPlayer';ap=by[path];props=decode(nm[path]['properties'])
            require(t['classes'][ap['class_index']]=='AnimationPlayer' and not ap['script'] and ap['parent']==v['id'],'AP actual source parent/class changed')
            expected={'_import_path','pause_mode','physics_interpolation_mode','unique_name_in_owner','process_priority','root_node','autoplay','reset_on_save','playback_process_mode','playback_default_blend_time','playback_speed','method_call_mode','blend_times','script'}
            require(set(props)==expected|{k for k in props if k.startswith('anims/')},'Unknown native AP property')
            require(props['root_node']=={'type':'NodePath','value':'..'} and props['autoplay']=='' and props['playback_process_mode']==1 and props['playback_speed']==1 and props['playback_default_blend_time']==0 and props['method_call_mode']==0 and props['blend_times']==[] and props['script'] is None,'Unsupported native AP playback/default/blend')
            timer=0;method=''
            if kind==1:
                timer=by[v['node']+'/Timer']['id'];txt=ex.text('Scripts/Main/Openable Door.gd')
                method=re.search(r'\[connection signal="timeout" from="Timer" to="\." method="([^"]+)"\]',ex.text(source))[1]
                require(re.search(r'^func '+re.escape(method)+r'\(\):',txt,re.M),'Source Timer callback changed')
            records.append(dict(id=ap['id'],owner=v['id'],parent=ap['parent'],kind=kind,path=path,timer=timer,timer_method=method,frame_target=0 if kind==1 else v['id'] if kind==3 else by[v['node']+'/Sprite']['id']))
    records.sort(key=lambda v: next(i for i,r in enumerate(t['records'])if r['id']==v['id']))
    text=engine.read_text(encoding='utf8')
    require('ADD_SIGNAL(MethodInfo("animation_started"' in text and 'ADD_SIGNAL(MethodInfo("animation_finished"' in text and 'set_process_internal' in text,'Fixed native AP semantics evidence changed')
    return dict(schema=1,capability=1,rules=1,family=0x454e006d,commit=PIN,scene=SCENE,scene_id=t['scene_id'],source_sha256=t['source_sha256'],dependencies=deps,sources=ex.sources,native_sha256=sha(native),engine_sha256=sha(engine),symbols=['idle_process_internal','animation_started','animation_finished','timeout'],playback=dict(root='..',autoplay='',mode=1,speed=1,blend=0,method_call_mode=0),records=records)
def encode(d):
    o=bytearray()
    def u(*v):o.extend(struct.pack('<'+'I'*len(v),*v))
    def s(v):b=v.encode();u(len(b));o.extend(b)
    for v in d['symbols']:s(v)
    s(d['playback']['root']);s(d['playback']['autoplay']);u(d['playback']['mode'],d['playback']['method_call_mode']);o.extend(struct.pack('<2f',d['playback']['speed'],d['playback']['blend']))
    u(len(d['sources']))
    for p,h in sorted(d['sources'].items()):s(p);o.extend(bytes.fromhex(h))
    u(len(d['records']))
    for v in d['records']:u(v['id'],v['owner'],v['parent'],v['kind'],v['timer'],v['frame_target']);s(v['path']);s(v['timer_method'])
    return b'ENCSCL01'+struct.pack('<7I',1,1,1,d['family'],d['scene_id'],len(o),zlib.crc32(o))+bytes.fromhex(PIN)+bytes.fromhex(d['source_sha256'])+o
def load():
    d=read(IR);require(d['schema']==d['capability']==d['rules']==1 and d['family']==0x454e006d and d['commit']==PIN and d['scene']==SCENE,'Native AP scope/version changed')
    require(sha(IR)==read(REVIEW)['ir_sha256'],'Native AP source review stale')
    ex=Extractor(ROOT)
    for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Native AP actual source changed '+p)
    for p,h in d['dependencies'].items():require(sha(ROOT/p)==h,'Native AP dependency changed '+p)
    require(len(d['records'])==sum(len(read(ROOT/p)[key])for p,key in INPUTS),'Native AP closure changed')
    return d
def stage_files(root):
    raw=(Path(root)/'data/podunk.encclipnative').read_bytes();require(raw==encode(load()),'Native AP staged bytes changed');return {Path('data/podunk.encclipnative'):raw}
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
    if a.action=='extract':
        require(a.native is not None and a.engine is not None,'Actual source-native conversion and engine AnimationPlayer source required');d=derive(a.native,a.engine);write(IR,d)
        write(REVIEW,dict(schema=1,ir_sha256=sha(IR),commit=PIN,engine_sha256=d['engine_sha256'],semantics=['One native AP leaf per reviewed typed owner; no second tracks or clock','Openable Timer remains the actual shared native Timer owner; source timeout is a persistent ObjectDB connection','Non-autoplay native AP Ready creates no script Ready; actual internal process membership changes only at source play/stop/finish','Present Sparkles remain separately owned actual AnimatedSprite; AP advances only the original present animation state']))
    else:PACK.write_bytes(encode(load()));print('Scene clips:',len(load()['records']),'actual AP bindings')
if __name__=='__main__':main()
