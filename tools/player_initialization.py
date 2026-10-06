#!/usr/bin/env python3
"""Source Player construction and global._init_player; no gameplay probe.

Native scene conversion strips scripts/signals and never enters the scene.
Pure declaration conversion evaluates only copied constants/plain defaults.
The resulting resource grants no native/script Ready capability.
"""
from __future__ import annotations
import argparse, hashlib, json, math, os, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,decode
from tools.extract_battle_entry import Extractor
from tools import field_node_recipe as recipe
SCENE='Nodes/Reusables/Player.tscn'
PLAYER='Scripts/Main/party/Player.gd'
BASE='Scripts/Main/party/party_object.gd'
GLOBAL='Scripts/global/global.gd'
FAMILY=0x454e0056
IR=ROOT/'content/native-player-initialization.json'
REVIEW=ROOT/'reports/player-initialization/source-review.json'
NATIVE=ROOT/'reports/player-initialization/declaration-values.json'
PACK=ROOT/'romfs/data/player.encinitialization'
CLASSES=recipe.STATIC_CLASSES+['CanvasLayer','Control','NinePatchRect','GridContainer','RichTextLabel','VScrollBar','PanelContainer','ColorRect','CenterContainer','MarginContainer','VBoxContainer','TextureButton','ScrollContainer','HScrollBar','TextureProgress','Panel','Path2D','PathFollow2D','WorldEnvironment','ParallaxBackground','ParallaxLayer','AnimationTree']

def stable(path):
    n=int.from_bytes(hashlib.sha256(('recipe:'+SCENE+'#'+path).encode()).digest()[:4],'little')
    require(n,'Zero Player node source identity');return n

def blocks(text):
    lines=text.split('func ',1)[0].splitlines();out=[];at=0
    while at<len(lines):
        line=lines[at]
        if re.match(r'^(?:enum\b|const\b|var\b|onready var\b)',line):
            body=line;balance=sum(line.count(x)for x in '[{(')-sum(line.count(x)for x in ']})')
            while balance:
                at+=1;require(at<len(lines),'Unclosed source declaration');line=lines[at]
                body+='\n'+line;balance+=sum(line.count(x)for x in '[{(')-sum(line.count(x)for x in ']})')
            require(balance==0,'Unbalanced source declaration');out.append(body)
        else:require(not line.strip() or line.startswith(('class_name ','extends ','signal ','#')),'Unclassified Player top-level source '+line)
        at+=1
    return out

def source():
    ex=Extractor(ROOT);base=ex.text(BASE);player=ex.text(PLAYER);global_code=ex.text(GLOBAL)
    require(base.startswith('class_name PartyObject\nextends KinematicBody2D\n') and player.startswith('class_name PartyMemberPlayer\nextends PartyObject\n'),'Unknown Player class inheritance')
    require(not re.search(r'^func _init\(',base+'\n'+player,re.M),'New Player explicit constructor requires source review')
    root_props=ex.text(SCENE).split('[node name="Player" type="KinematicBody2D"]\n',1)[1].split('\n[node ',1)[0]
    require(root_props.strip()=='use_parent_material = true\nposition = Vector2( 0, -3 )\ncollision_mask = 4353\nscript = ExtResource( 2 )','Player native property/script attachment cursor changed')
    rows=[];constants=[];ready=[]
    for path,text in [(BASE,base),(PLAYER,player)]:
        for declaration in blocks(text):
            if declaration.startswith(('const ','enum ')):constants.append(declaration);continue
            onready=declaration.startswith('onready ')
            m=re.fullmatch(r'(?:onready )?var (\w+)\s*(?::\s*(\w+))?\s*(?::?=\s*(.*))?',declaration,re.S)
            require(m,'Unknown Player declaration '+declaration)
            name,hint,expression=m[1],m[2]or'',m[3]
            if expression is None:
                expression={'bool':'false','int':'0','float':'0.0','String':'""'}.get(hint,'null')
            expression=expression.strip();row=dict(source=path,name=name,hint=hint,expression=expression,adapter=0)
            if onready:ready.append(row);continue
            if expression.startswith('globaldata.characters.'):
                key=expression[len('globaldata.characters.'):]
                constructor=read(ROOT/'content/native-global-data-constructor.json')
                d=next(x for x in constructor['declarations']if x['adapter']==1)
                require(key in dict(d['references']),'Player Character constructor reference missing')
                row.update(adapter=1,member=d['name'],key=key,reference_id=dict(d['references'])[key])
            elif expression.startswith('load('):
                mload=re.fullmatch(r'load\("res://([^"\n]+)"\)',expression);require(mload,'Unknown Player dynamic constructor resource')
                row.update(adapter=2,resource=mload[1],native='AudioStreamSample');ex.data(mload[1])
            else:require(not re.search(r'\b(?:new|load|preload|get)\s*\(',expression),'Unreviewed constructor effect '+name)
            rows.append(row)
    require(len({x['name']for x in rows+ready})==len(rows+ready),'Duplicate inherited Player field')
    start=global_code.index('func _init_player():\n');end=global_code.index('\nfunc ',start+5)
    body=global_code[start:end]
    expected='''func _init_player():
\tvar root := get_tree().get_root()
\tcurrentScene = root.get_child(root.get_child_count() - 1)
\tif partyObjects.size() > 0:
\t\treturn
\tparty.append(globaldata.characters.ninten)
\tvar player: PartyMemberPlayer = load("res://Nodes/Reusables/Player.tscn").instance()
\tplayer.name = "player"
\tplayer.position = Vector2.ZERO
\tpartyObjects = [player]
\tvar parent_node := get_current_scene_player_node()
\tif parent_node:
\t\tparent_node.add_child(player)
\telse:
\t\tcurrentScene.add_child(player)
\t\tplayer.hide()
\t\tplayer.connect("ready", player, "pause", [], CONNECT_ONESHOT)
\tcreate_party_followers(false)
\tset_respawn()
'''
    require(body==expected,'Unknown global._init_player source order')
    leader=next(x for x in rows if x['adapter']==1)
    policy=dict(current_scene='currentScene',party='party',party_objects='partyObjects',persist='_persist_array',player_name='player',position=[0,0],parent_candidates=['YSort','Objects'],ready_signal='ready',pause_method='pause',connect_flags=4,followers_method='create_party_followers',followers_emit=False,respawn_method='set_respawn',leader_member=leader['member'],leader_key=leader['key'],leader_id=leader['reference_id'])
    require('return currentScene.get_node_or_null("YSort" if currentScene.has_node("YSort") else "Objects")'in global_code,'Unknown Player parent priority')
    respawn=global_code.split('func set_respawn():\n',1)[1].split('\nfunc ',1)[0]
    require(re.fullmatch(r'\tglobaldata\.(\w+) = get_player\(\).position\n\tglobaldata\.(\w+) = currentScene.get_filename\(\)\n\tglobaldata\.(\w+) = get_player\(\).run_sound\n\tglobaldata\.(\w+) = get_player\(\).get_shadow\(\)\n\tprint\([^\n]+\)\n',respawn),'Unknown source respawn assignments')
    policy['respawn_fields']=re.findall(r'\tglobaldata\.(\w+) =',respawn)
    return ex,rows,ready,constants,policy,body

def pure_source():
    ex,rows,ready,constants,policy,body=source()
    declarations=[('var '+r['name']+(': '+r['hint']if r['hint']else'')+' = '+r['expression'])for r in rows if not r['adapter']]
    names=[r['name']for r in rows if not r['adapter']]
    result='extends Reference\n'+'\n'.join(constants+declarations)+'\nfunc declaration_values():\n\treturn ['+','.join('["'+n+'",'+n+']'for n in names)+']\n'
    require(not re.search(r'\b(?:load|preload|new)\s*\(',result),'Source declaration exporter contains constructor effect')
    return result,ex

def prepare(directory):
    pure,ex=pure_source();directory.mkdir(parents=True,exist_ok=True)
    (directory/'project.godot').write_bytes(b'config_version=4\n[application]\nconfig/name="Player declaration values only"\n[logging]\nfile_logging/enable_logging=false\n')
    (directory/'pure.gd').write_bytes(pure.encode())
    (directory/'export.gd').write_bytes((ROOT/'tools/godot_exporter/global_data_constructor.gd').read_bytes())
    write(directory/'manifest.json',dict(sources=ex.sources,pure_sha256=sha(directory/'pure.gd')))

def prepare_scene(directory):
    from tools.scene_reference import prepare as native_prepare
    for key in ('GIT_DIR','GIT_WORK_TREE','GIT_INDEX_FILE'):os.environ.pop(key,None)
    native_prepare(ROOT/'upstream/MOTHER-Encore',directory,SCENE)
    exporter=(ROOT/'tools/godot_exporter/field_node_recipe.gd').read_text().replace('Nodes/Ui/DialogueBox.tscn',SCENE)
    (directory/'player_node_recipe.gd').write_bytes(exporter.encode())

def plain(value):
    k=value['kind']
    if k==0:return None
    if k==1:return value['value']
    if k==2:return int(value['value'])
    if k==3:return struct.unpack('<d',bytes.fromhex(value['f64_le']))[0]
    if k==4:return value['value']
    if k==5:return [plain(x)for x in value['values']]
    if k==6:return {x:plain(v)for x,v in value['entries']}
    if k==7:return [struct.unpack('<d',bytes.fromhex(x))[0]for x in value['coordinates']]
    raise ValueError('Unknown pure Player native type')

def node_rows(native,tree,proof,ex):
    inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
    require(native['source']=='res://'+SCENE and not native['native_compatible'] and tree['scene']==SCENE and not tree['scene_entered'],'Player scene data only required')
    for d in (native['godot'],tree['engine']):require([d.get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'Player native engine changed')
    require(proof['scene']==SCENE and proof['commit']==PIN,'Player source scene proof changed')
    for p,r in proof['files'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==r['sha256']==inv[p]['sha256'],'Player source closure changed '+p);ex.sources[p]=r['sha256']
    recipe.SCENE=SCENE;recipe.CLASSES=CLASSES
    scripts,nulls=recipe.source_scripts(native,proof,ex.sources,inv)
    class_sources={}
    for file in(ROOT/'upstream/MOTHER-Encore').rglob('*.gd'):
        c=re.search(r'^class_name\s+(\w+)',file.read_text(encoding='utf-8'),re.M)
        if c:class_sources[c[1]]=file.relative_to(ROOT/'upstream/MOTHER-Encore').as_posix()
    nm={n['path']:n for n in native['nodes']};rows=tree['nodes'];require([r['path']for r in rows]==list(nm),'Incomplete Player native structure')
    children={r['path']:[]for r in rows};records=[];cache={}
    for r in rows:
        p=r['path'];parent=r['parent'];require(r['class']==nm[p]['class']and r['class']in CLASSES and not r['name'].startswith('@'),'Unknown native Player constructor child')
        if parent:children[parent].append(p)
        canvas=r['canvas'];flags=int(canvas);local=world=[[1,0],[0,1],[0,0]];color=selfcolor=[1,1,1,1];z=light=0;cp=''
        if canvas:
            local=recipe.numbers(r['local']);world=recipe.numbers(r['world']);color=recipe.numbers(r['modulate']);selfcolor=recipe.numbers(r['self_modulate']);z=r['z'];light=r['light_mask'];cp=r['canvas_parent']
            flags|=sum(int(r[k])<<(i+1)for i,k in enumerate(['visible','top_level','behind','use_parent_material','material','z_relative','y_sort','notify_transform','notify_local_transform']))
        script,h=scripts.get(p,('','0'*64));methods=recipe.method_inventory(script,inv,ex.sources,cache,class_sources)if script else 0
        records.append(dict(id=stable(p),parent=stable(parent)if parent else 0,owner=stable(r['owner'])if r['owner']else 0,canvas_parent=stable(cp)if cp else 0,class_index=CLASSES.index(r['class']),native_class=r['class'],ready=0,pause=r['pause'],flags=flags,light_mask=light,script_methods=methods,index=r['index'],priority=r['priority'],z=z,local=local,world=world,modulate=color,self_modulate=selfcolor,node=p,name=r['name'],script=script,script_sha=h,groups=r['groups'],native_generated=False))
    order=[]
    def visit(p):
        for c in children[p]:visit(c)
        order.append(p)
    visit('.');require(len(order)==len(records),'Incomplete Player Ready source postorder')
    for r in records:r['ready']=order.index(r['node'])
    return records,nulls

def extract(native_path,tree_path,source_path,values_path):
    ex,fields,onready,constants,policy,body=source();pure,_=pure_source();values=read(values_path)
    require(values['schema']==1 and not values['source_scene_entered']and not values['original_constructor_entered']and values['pure_sha256']==hashlib.sha256(pure.encode()).hexdigest(),'Changed pure source Player declarations')
    require([values['engine'].get(k)for k in ['major','minor','patch','status']]==[3,6,2,'stable'],'Player declaration source engine changed')
    native=read(native_path);tree=read(tree_path);proof=read(source_path)
    records,nulls=node_rows(native,tree,proof,ex);values_by_name=dict(values['values'])
    require(set(values_by_name)=={r['name']for r in fields if not r['adapter']},'Incomplete inherited source field defaults')
    for r in fields:
        if not r['adapter']:r['value']=values_by_name[r['name']]
    by_path={r['node']:r for r in records};resolved={}
    for r in onready:
        exp=r['expression'];node=re.fullmatch(r'\$([^\s]+)',exp);load=re.fullmatch(r'preload\("res://([^"\n]+)"\)',exp);prop=re.fullmatch(r'(\w+)\.get\("([^"\n]+)"\)',exp)
        if node:require(node[1]in by_path,'Player onready actual source node missing');r.update(kind=1,node_id=stable(node[1]),path=node[1]);resolved[r['name']]=node[1]
        elif load:r.update(kind=2,resource=load[1],native='PackedScene');ex.text(load[1])
        elif prop:require(prop[1]in resolved,'Player onready receiver unresolved');r.update(kind=3,node_id=stable(resolved[prop[1]]),path=resolved[prop[1]],property=prop[2],native='AnimationNodeStateMachinePlayback')
        else:raise ValueError('Unknown Player onready operation '+exp)
    ex.text('LICENSE')
    write(NATIVE,values)
    write(IR,dict(schema=1,kind='encore.player-initialization.source-ir',commit=PIN,family=FAMILY,scene=SCENE,scene_id=stable('.'),source_sha256=ex.sources[SCENE],owner=GLOBAL,player_script=PLAYER,base_script=BASE,sources=ex.sources,native_sha256=sha(native_path),tree_sha256=sha(tree_path),source_receipt_sha256=sha(source_path),declarations_sha256=sha(NATIVE),constructor_ir_sha256=sha(ROOT/'content/native-global-data-constructor.json'),fields=fields,onready=onready,init_policy=policy,init_method_sha256=hashlib.sha256(body.encode()).hexdigest(),classes=CLASSES,records=records,native_snapshot=native,script_null_overrides=nulls,ready_admitted=False,pending=['Actual script construction attachment cursor before child allocation','Every Player child native/script constructor and Ready owner','Actual AnimationTree/StateMachinePlayback runtime','AfterImage/Dust/Sweat/Beam/PKOV source consumers','Actual source audio ResourceLoader and player footsteps SFX lane','Original inherited onready/Ready and status/party synchronous connections']))

def load():
    d=read(IR);review=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
    require(d['schema']==1 and d['commit']==PIN and d['family']==FAMILY and d['scene']==SCENE and d['classes']==CLASSES and d['ready_admitted']is False and review['ir_sha256']==sha(IR),'Player source review/resource capabilities changed')
    for p,h in d['sources'].items():require(inv[p]['sha256']==h==sha(ROOT/'upstream/MOTHER-Encore'/p),'Player changed source '+p)
    require(d['declarations_sha256']==sha(NATIVE)and d['constructor_ir_sha256']==sha(ROOT/'content/native-global-data-constructor.json'),'Player independent source dependencies changed')
    return d

def encode(d):
    b=bytearray(128)
    def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
    def i(*v):b.extend(struct.pack('<'+'i'*len(v),*v))
    def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
    def t(s):v=s.encode();u(len(v));b.extend(v)
    def value(v):
        if v is None:u(0)
        elif type(v)is bool:u(1,int(v))
        elif type(v)is int:u(2);b.extend(struct.pack('<q',v))
        elif type(v)is float:require(math.isfinite(v),'Nonfinite source Player value');u(3);b.extend(struct.pack('<d',v))
        elif isinstance(v,str):u(4);t(v)
        elif isinstance(v,list):u(5,len(v));[value(x)for x in v]
        elif isinstance(v,dict):u(6,len(v));[(t(k),value(x))for k,x in v.items()]
        else:raise ValueError('Unknown Player source native Variant')
    t(d['scene']);t(d['owner']);t(d['player_script']);t(d['base_script']);b.extend(bytes.fromhex(d['constructor_ir_sha256']))
    u(len(d['sources']))
    for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
    # Embed checked NodeRecipe cap6: structural AnimationTree only, no Ready.
    sub=bytearray(128)
    def su(*v):sub.extend(struct.pack('<'+'I'*len(v),*v))
    def st(x):v=x.encode();su(len(v));sub.extend(v)
    st(d['scene']);su(len(CLASSES));[st(x)for x in CLASSES]
    for r in d['records']:
        su(*[r[k]for k in ['id','parent','owner','canvas_parent','class_index','ready','pause','flags','light_mask','script_methods']])
        sub.extend(struct.pack('<3i20f',r['index'],r['priority'],r['z'],*[v for row in r['local']for v in row],*[v for row in r['world']for v in row],*r['modulate'],*r['self_modulate']))
        st(r['node']);st(r['name']);st(r['script']);sub.extend(bytes.fromhex(r['script_sha']));su(len(r['groups']));[st(x)for x in r['groups']];su(0)
    su(len(d['sources']))
    for path,h in d['sources'].items():st(path);sub.extend(bytes.fromhex(h))
    su(0,0)
    struct.pack_into('<8s8I',sub,0,b'ENCFNRC1',1,128,len(sub),0,0x454e003d,6,len(d['records']),d['scene_id'])
    sub[40:60]=bytes.fromhex(PIN);sub[60:92]=bytes.fromhex(d['source_sha256']);sub[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',sub,20,zlib.crc32(sub[128:]))
    u(len(sub));b.extend(sub)
    value(d['fields']);value(d['onready']);value(d['init_policy']);value(d['native_snapshot'])
    struct.pack_into('<8s8I',b,0,b'ENCPLYI1',1,128,len(b),0,FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)

def stage_files(source_root):
    raw=encode(load());require((Path(source_root)/'data/player.encinitialization').read_bytes()==raw,'Staged Player initialization differs');return {Path('data/player.encinitialization'):raw}

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['prepare','prepare-scene','extract','compile','verify']);p.add_argument('--directory',type=Path);p.add_argument('--native',type=Path);p.add_argument('--tree',type=Path);p.add_argument('--source',type=Path);p.add_argument('--values',type=Path);a=p.parse_args()
    if a.action in('prepare','prepare-scene'):require(a.directory,'Explicit isolated output directory required');(prepare if a.action=='prepare'else prepare_scene)(a.directory);return
    if a.action=='extract':require(a.native and a.tree and a.source and a.values,'Complete original source conversion inputs required');extract(a.native,a.tree,a.source,a.values);return
    raw=encode(load())
    if a.action=='verify':require(PACK.read_bytes()==raw,'Player resource stale')
    else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
    print('Player source initialization:',len(load()['records']),'native nodes;',len(raw),'bytes; native/script Ready remains pending')
if __name__=='__main__':
    try:main()
    except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('PLAYER INITIALIZATION ERROR: '+str(e))
