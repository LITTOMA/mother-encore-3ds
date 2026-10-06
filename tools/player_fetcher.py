#!/usr/bin/env python3
"""Player's actual SpriteDataFetcher declarations/onready/getters/process.

Source conversion only. A complete checked Podunk NodeTree closes the static
reflector lookup; runtime still queries the current live scene every process.
"""
from pathlib import Path
import argparse, json, re, struct, sys, zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode
from tools.extract_battle_entry import Extractor
from tools.player_initialization import load as player_load,IR as PLAYER_IR
from tools.field_node_tree import load as tree_load,IR as TREE_IR
IR=ROOT/'content/native-player-fetcher.json'
REVIEW=ROOT/'reports/player-fetcher/source-review.json'
PACK=ROOT/'romfs/data/player.encfetcher'
FAMILY=0x454e005e
SCRIPT='Scripts/Main/SpriteDataFetcher.gd'
PREFAB='Nodes/Reusables/sprite_data_fetcher.tscn'
GLOBAL='Scripts/global/global.gd'
GLOBAL_IR=ROOT/'content/native-field-global-constructor.json'

def extract():
    p=player_load();tree=tree_load();ex=Extractor(ROOT)
    source=ex.text(SCRIPT);prefab=ex.text(PREFAB);global_source=ex.text(GLOBAL)
    clean='\n'.join(x.rstrip()for x in source.splitlines()if x.strip()and not x.lstrip().startswith('#'))
    require(clean=='''class_name SpriteDataFetcher
extends Node
export var _sprite_path: NodePath
export var _ignore_reflection: bool = false;
export var _toggle_flip_reflection: bool = true;
export var reflect_offset: float = 4.0;
onready var _sprite_node = get_node_or_null(_sprite_path)
onready var _parent_node = self.get_parent()
onready var reflect_node = global.currentScene.get_node_or_null("FloorReflector")
var _obj_reflection: CharacterReflection
var _has_reflection: bool = false;
func get_texture() -> Texture:
\treturn _sprite_node.texture
func get_hframes() -> int:
\treturn _sprite_node.hframes
func get_vframes() -> int:
\treturn _sprite_node.vframes
func get_frames() -> int:
\treturn _sprite_node.frame
func get_visibility() -> bool:
\treturn _sprite_node.visible
func get_sprite_node() -> Sprite:
\tvar sprite_ret = _sprite_node
\treturn sprite_ret
func generate_reflection():
\tprint("Reflect");
\t_obj_reflection = reflect_node.create_reflection(self, _parent_node, _toggle_flip_reflection)
\t_parent_node.add_child(_obj_reflection)
\t_has_reflection = true
func delete_reflection():
\tif(is_instance_valid(_obj_reflection)):
\t\t_obj_reflection.queue_free()
\t\t_has_reflection = false
func _process(_delta):
\treflect_node = global.currentScene.get_node_or_null("FloorReflector")
\tif (!_ignore_reflection && reflect_node && !_has_reflection):
\t\tgenerate_reflection()
\telif !(reflect_node):
\t\tdelete_reflection()
\tpass''','Fetcher source changed; whole method review required')
    require(prefab.strip()=='''[gd_scene load_steps=2 format=2]

[ext_resource path="res://Scripts/Main/SpriteDataFetcher.gd" type="Script" id=1]

[node name="SpriteDataFetcher" type="Node"]
script = ExtResource( 1 )''','Fetcher prefab instance override changed')
    g=read(GLOBAL_IR);gm=next(x for x in g['declarations']if x['role']==6)
    require(g['commit']==PIN and re.search(r'^var '+re.escape(gm['name'])+r'\b',global_source,re.M),'CurrentScene actual source member differs')
    reflector=re.search(r'global\.(\w+)\.get_node_or_null\("([^"\n]+)"\)',source)
    require(reflector[1]==gm['name']and'/'not in reflector[2],'Unsupported reflector query shape')
    ns={x['path']:decode(x['properties'])for x in p['native_snapshot']['nodes']}
    rs={x['node']:x for x in p['records']};rows=[]
    scene_source=ex.text(p['scene'])
    states=p['native_snapshot']['scene_states']
    state=next(x for x in states if x['source']=='res://'+p['scene'])
    declared={x['path'].removeprefix('./'):decode(x['properties'])for x in state['nodes']}
    for r in p['records']:
        if r['script']!=SCRIPT:continue
        require(r['native_class']=='Node'and r['script_methods']==64,'Fetcher actual class/method flags changed')
        props=declared[r['node']]
        require(set(props)<={'_sprite_path','_ignore_reflection','_toggle_flip_reflection','reflect_offset'},'Unknown Fetcher source instance property')
        path=props['_sprite_path']['value'];parts=r['node'].split('/')
        for x in path.split('/'):
            if x=='..':require(parts,'Fetcher path escapes Player');parts.pop()
            elif x not in ('','.'):parts.append(x)
        target=rs['/'.join(parts)or'.'];require(target['native_class']=='Sprite','Fetcher target actual native class differs')
        ignore=props.get('_ignore_reflection',False);flip=props.get('_toggle_flip_reflection',True);offset=props.get('reflect_offset',4.0)
        require(type(ignore)is bool and type(flip)is bool and isinstance(offset,(int,float)),'Fetcher override type differs')
        rows.append(dict(id=r['id'],path=r['node'],script_sha256=r['script_sha'],target=target['id'],target_path=target['node'],sprite_path=path,ignore=ignore,toggle_flip=flip,reflect_offset=offset,ready=r['ready']))
    require(len(rows)==3,'Full Player source Fetcher closure changed')
    closure=[dict(id=x['id'],parent=x['parent'],path=x['node'],name=x['name'])for x in tree['records']]
    require(not any(x['parent']==tree['scene_id']and x['name']==reflector[2]for x in closure),'Source scene has FloorReflector; actual factory capability required')
    for path,h in tree['sources'].items():ex.data(path);require(ex.sources[path]==h,'Tree source closure differs '+path)
    ex.data(p['scene']);ex.data(PREFAB)
    getters=[]
    for name,kind,member in re.findall(r'func (get_\w+)\(\) -> (Texture|int|bool):\n\treturn _sprite_node\.(\w+)',source):
        getters.append(dict(method=name,type=kind,member=member))
    require(len(getters)==5,'Fetcher getter source closure differs')
    d=dict(schema=1,format=1,capability=1,rules=1,family=FAMILY,commit=PIN,scene=p['scene'],scene_id=p['scene_id'],source_sha256=p['source_sha256'],player_ir_sha256=sha(PLAYER_IR),global_ir_sha256=sha(GLOBAL_IR),tree_ir_sha256=sha(TREE_IR),script=SCRIPT,script_sha256=ex.sources[SCRIPT],global_script=GLOBAL,global_sha256=ex.sources[GLOBAL],current_scene_member=gm['name'],reflector_path=reflector[2],scene_closure=dict(scene=tree['scene'],scene_id=tree['scene_id'],source_sha256=tree['source_sha256'],records=closure),rows=rows,getters=getters,source_sprite_getter='get_sprite_node',exports=dict(path='_sprite_path',ignore='_ignore_reflection',flip='_toggle_flip_reflection',offset='reflect_offset',default_ignore=False,default_flip=True,default_offset=4.0),initial_has_reflection=False,sources=ex.sources,ready_admitted=False,pending=['CharacterReflection factory is not implemented: a live nonignored reflector explicitly refuses generation','This pack does not approve native Ready, other scene scripts, or an entire scene'])
    write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=ex.sources,scope='All three actual Player fetchers; full source Podunk static NodeTree closure; implicit onready and live native Sprite getters/process, real dynamic reflector presence still queried',delete_flag_policy='has_reflection false only after valid reflection queue_free',ready_admitted=False))

def load():
    d=read(IR);r=read(REVIEW)
    require(d['schema']==d['format']==d['capability']==d['rules']==1 and d['family']==FAMILY and d['commit']==PIN and d['ready_admitted']is False and r['ir_sha256']==sha(IR),'Fetcher source review/schema differs')
    require(d['player_ir_sha256']==sha(PLAYER_IR)and d['tree_ir_sha256']==sha(TREE_IR)and d['global_ir_sha256']==sha(GLOBAL_IR),'Fetcher checked source dependency changed')
    ex=Extractor(ROOT)
    for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Fetcher source changed '+p)
    return d

def encode(d):
    b=bytearray(128)
    def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
    def t(v):raw=v.encode();u(len(raw));b.extend(raw)
    def h(v):b.extend(bytes.fromhex(v))
    t(d['scene']);h(d['player_ir_sha256']);h(d['global_ir_sha256']);h(d['tree_ir_sha256']);t(d['script']);h(d['script_sha256']);t(d['global_script']);h(d['global_sha256']);t(d['current_scene_member']);t(d['reflector_path']);x=d['exports'];[t(x[k])for k in ['path','ignore','flip','offset']];u(int(x['default_ignore']),int(x['default_flip']),int(d['initial_has_reflection']));b.extend(struct.pack('<d',x['default_offset']))
    u(len(d['rows']))
    for x in d['rows']:
        u(x['id'],x['target'],x['ready'],int(x['ignore']),int(x['toggle_flip']));b.extend(struct.pack('<d',x['reflect_offset']));t(x['path']);t(x['target_path']);t(x['sprite_path']);h(x['script_sha256'])
    u(len(d['getters']))
    for x in d['getters']:t(x['method']);t(x['type']);t(x['member'])
    t(d['source_sprite_getter']);c=d['scene_closure'];u(c['scene_id']);h(c['source_sha256']);t(c['scene']);u(len(c['records']))
    for x in c['records']:u(x['id'],x['parent']);t(x['path']);t(x['name'])
    u(len(d['sources']))
    for path,v in d['sources'].items():t(path);h(v)
    struct.pack_into('<8s8I',b,0,b'ENCPFET1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def stage_files(root):
    raw=encode(load());p=Path('data/player.encfetcher');require((Path(root)/p).read_bytes()==raw,'Fetcher staged binary differs');return{p:raw}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
    try:
        if a.action=='extract':extract()
        else:
            b=encode(load())
            if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
            else:require(PACK.read_bytes()==b,'Fetcher binary stale')
            print('Actual Player Fetcher:',len(b),'bytes')
    except(ValueError,KeyError,TypeError,OSError,struct.error,AttributeError,StopIteration)as e:sys.exit('PLAYER FETCHER ERROR: '+str(e))
