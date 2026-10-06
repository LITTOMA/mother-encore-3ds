#!/usr/bin/env python3
"""Source Shadow setter and NintenBatSwing implicit onready/process."""
from pathlib import Path
import argparse, hashlib, json, re, struct, sys, zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,sha,require,decode
from tools.extract_battle_entry import Extractor
from tools.player_initialization import load as player_load,IR as PLAYER_IR
IR=ROOT/'content/native-player-visual-scripts.json'
REVIEW=ROOT/'reports/player-visual-scripts/source-review.json'
PACK=ROOT/'romfs/data/player.encvisualscripts'
FAMILY=0x454e0058
SHADOW='Scripts/Main/Shadow.gd';BAT='Scripts/misc/NintenBatSwing.gd'
FETCHER='Scripts/Main/SpriteDataFetcher.gd';PLAYER='Scripts/Main/party/Player.gd'
def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((json.dumps(d,indent=2,ensure_ascii=False)+'\n').encode())
def extract():
    d=player_load();ex=Extractor(ROOT);s=ex.text(SHADOW);b=ex.text(BAT);f=ex.text(FETCHER);p=ex.text(PLAYER)
    require(s=='''tool
extends AnimatedSprite

export (String, "shadow", "ripple", "magic") var start_anim setget set_anim

var in_front_anims = ["ripple"]


func set_anim(anim) -> void:
\tstart_anim = anim
\tplay(anim)
\t_set_behind_parent(!anim in in_front_anims)

func _set_behind_parent(enabled = false) -> void:
\tshow_behind_parent = enabled
''','Shadow source changed; constructor/setter review required')
    require(re.fullmatch(r'extends Sprite\n\nonready var (\w+):SpriteDataFetcher = get_node_or_null\("([^"\n]+)"\)\n\n# Really quick and hacky script for Ninten\'s bat to\n# render as a seperate object\.\n\nfunc _process\(delta\):\n\tif global.get_player\(\).(\w+)\(\) == "([^"\n]+)":\n\t\tvisible = true;\n\telse:\n\t\tvisible = false;\n\tif \(special\):\n\t\tframe = special.(\w+)\(\)\n?',b),'Bat source changed')
    m=re.search(r'onready var (\w+):SpriteDataFetcher = get_node_or_null\("([^"\n]+)"\)',b)
    getter=re.search(r'global.get_player\(\).(\w+)\(\) == "([^"\n]+)"',b)
    backing=re.search(r'func '+getter[1]+r'\(\) -> String:\n\treturn (\w+)',p)[1]
    fetch=re.search(r'frame = special.(\w+)\(\)',b)[1]
    framefield=re.search(r'func '+fetch+r'\(\) -> int:\n\treturn _sprite_node\.(\w+)',f)[1]
    rs={x['node']:x for x in d['records']};ns={x['path']:decode(x['properties'])for x in d['native_snapshot']['nodes']};resources={x['id']:x for x in d['native_snapshot']['resources']}
    root,shadow,bat,target,fetcher=[rs[x]for x in ['.','Shadow','SpecialAnimations/NintenBat','SpecialAnimations','SpriteDataFetcher2']]
    require(shadow['script']==SHADOW and shadow['native_class']=='AnimatedSprite'and shadow['script_methods']==0 and bat['script']==BAT and bat['native_class']=='Sprite'and bat['script_methods']==64,'Visual native/script source identity differs')
    props=ns['Shadow'];frames=resources[props['frames']['id']];require(frames['class']=='SpriteFrames','Shadow frames codec differs')
    shadow_scene=ex.text('Nodes/Reusables/Shadow.tscn')
    texture_paths={int(n):p for p,n in re.findall(r'\[ext_resource path="res://([^"\n]+)" type="Texture" id=(\d+)\]',shadow_scene)}
    def atlas_values(resource):
        key=resource['path'].split('::')[-1]
        body=shadow_scene.split('[sub_resource type="AtlasTexture" id='+key+']\n',1)[1].split('\n[',1)[0]
        m=re.fullmatch(r'atlas = ExtResource\( (\d+) \)\nregion = Rect2\( ([0-9, .]+) \)\n?',body.strip()+'\n')
        require(m,'Unknown Shadow AtlasTexture properties')
        rect=[float(x.strip())for x in m[2].split(',')];require(len(rect)==4,'Shadow source rect differs')
        return texture_paths[int(m[1])],rect
    clips=[]
    for anim in decode(frames['properties'])['animations']:
        require(anim.get('type')=='Dictionary'and len(anim['pairs'])==4,'Unknown native SpriteFrames animation fields');anim=dict(anim['pairs'])
        rows=[]
        for ref in anim['frames']:
            atlas=resources[ref['id']];require(atlas['class']=='AtlasTexture','Shadow atlas source codec differs');path,rect=atlas_values(atlas);ex.data(path);ex.data(path+'.import')
            texture_id=int.from_bytes(hashlib.sha256(('player-visual-texture:'+path).encode()).digest()[:4],'little')
            rows.append(dict(resource_id=atlas['id'],texture_id=texture_id,source=path,source_sha256=ex.sources[path],rect=rect))
        clips.append(dict(name=anim['name'],speed=anim['speed'],loop=anim['loop'],frames=rows))
    require({a['name']for a in clips}==set(json.loads(re.search(r'var in_front_anims = (\[[^\n]+\])',s)[1]))|set(re.findall(r'"([^"\n]+)"',s.split('var start_anim')[0])),'Shadow source animation closure differs')
    start=re.search(r'^start_anim = "([^"\n]*)"$',shadow_scene,re.M)[1]
    bp=ns['SpecialAnimations/NintenBat'];tex=resources[bp['texture']['id']];path=tex['path'][6:];ex.data(path);ex.data(path+'.import')
    spec=ex.text('Nodes/Reusables/Player.tscn').split('[node name="SpriteDataFetcher2"',1)[1].split('\n[node ',1)[0]
    fetchpath=re.search(r'_sprite_path = NodePath\("([^"\n]+)"\)',spec)[1]
    doc=dict(schema=1,format=1,capability=1,rules=1,family=FAMILY,commit=PIN,scene=d['scene'],scene_id=d['scene_id'],source_sha256=d['source_sha256'],player_ir_sha256=sha(PLAYER_IR),
      root=dict(id=root['id'],script=root['script'],sha=root['script_sha']),
      shadow=dict(id=shadow['id'],script=SHADOW,sha=shadow['script_sha'],start_member='start_anim',start_default='',setter='set_anim',front_member='in_front_anims',front=json.loads(re.search(r'var in_front_anims = (\[[^\n]+\])',s)[1]),start=start,frames_id=frames['id'],clips=clips),
      bat=dict(id=bat['id'],script=BAT,sha=bat['script_sha'],special_member=m[1],special_path=m[2],action_getter=getter[1],action_member=backing,visible_action=getter[2],fetcher_getter=fetch,frame_member=framefield,fetcher_id=fetcher['id'],fetcher_script=fetcher['script'],fetcher_sha=fetcher['script_sha'],fetcher_path=fetchpath,target_id=target['id'],columns=bp['hframes'],rows=bp['vframes'],initial_frame=bp['frame'],texture_id=tex['id'],texture=path,texture_sha256=ex.sources[path]),
      sources=ex.sources,ready_admitted=False)
    write(IR,doc);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=ex.sources,scope='Source Shadow export setter and Bat implicit onready/process, actual source SpriteFrames sevenframes; same actual Player native owners required',ready_admitted=False))
def load():
    d=read(IR);r=read(REVIEW);require(d['schema']==d['format']==d['capability']==d['rules']==1 and d['family']==FAMILY and d['commit']==PIN and d['ready_admitted']is False and sha(IR)==r['ir_sha256']and sha(PLAYER_IR)==d['player_ir_sha256'],'Visual source review/dependency changed')
    ex=Extractor(ROOT)
    for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Visual source changed '+p)
    return d
def encode(d):
    b=bytearray(128)
    def u(*xs):b.extend(struct.pack('<'+'I'*len(xs),*xs))
    def t(s):raw=s.encode();u(len(raw));b.extend(raw)
    def h(s):b.extend(bytes.fromhex(s))
    t(d['scene']);h(d['player_ir_sha256']);u(d['root']['id']);t(d['root']['script']);h(d['root']['sha'])
    s=d['shadow'];u(s['id']);t(s['script']);h(s['sha'])
    for key in ['start_member','start_default','setter','front_member','start']:t(s[key])
    u(len(s['front']));[t(x)for x in s['front']];u(s['frames_id'],len(s['clips']))
    for c in s['clips']:
        t(c['name']);b.extend(struct.pack('<d',c['speed']));u(int(c['loop']),len(c['frames']))
        for f in c['frames']:u(f['resource_id'],f['texture_id']);t(f['source']);h(f['source_sha256']);b.extend(struct.pack('<4f',*f['rect']))
    x=d['bat'];u(x['id']);t(x['script']);h(x['sha'])
    for key in ['special_member','special_path','action_getter','action_member','visible_action','fetcher_getter','frame_member']:t(x[key])
    u(x['fetcher_id']);t(x['fetcher_script']);h(x['fetcher_sha']);t(x['fetcher_path']);u(x['target_id'],x['columns'],x['rows'],x['initial_frame'],x['texture_id']);t(x['texture']);h(x['texture_sha256']);u(len(d['sources']))
    for p,v in d['sources'].items():t(p);h(v)
    struct.pack_into('<8s8I',b,0,b'ENCPVIS1',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def stage_files(root):
    raw=encode(load());p=Path('data/player.encvisualscripts');require((Path(root)/p).read_bytes()==raw,'Visual staged binary differs');return{p:raw}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
    try:
        if a.action=='extract':extract()
        else:
            raw=encode(load())
            if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
            else:require(PACK.read_bytes()==raw,'Visual resource stale')
            print('Player visual source:',len(raw),'bytes; native owner lifecycle required')
    except(ValueError,KeyError,TypeError,OSError,struct.error,AttributeError)as e:sys.exit('PLAYER VISUAL ERROR: '+str(e))
