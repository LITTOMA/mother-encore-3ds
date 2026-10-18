"""Bounded source stepping-area and run-loop media candidates; never stages.

Walking does not play footsteps in this source MOVE slice. Player._set_running
selects/restarts an imported loop named by run_sound. This candidate is data,
not an implemented movement/audio consumer or a complete scene admission.
"""
import argparse,hashlib,json,math,re,shutil,struct,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools import podunk_field as f,audio_asset
from tools.godot_text import Document,Vec2,owner_of
from tools.reference_movement import extract
RECIPE=ROOT/'content/mayor-footsteps.json';MAGIC=b'ENCSTEP1';STRIDES=(1,24,56,64);LIMIT=65536

def fields(v,keys,label):require(type(v)is dict and set(v)==set(keys.split()),'Footstep '+label+' fields')
def safe(v):return type(v)is str and v and all(32<=ord(c)<127 and c not in ('\\',':')for c in v)and all(x not in ('','.','..')for x in v.split('/'))
def unique(pairs):
    out={}
    for k,v in pairs:require(k not in out,'Duplicate footstep recipe field');out[k]=v
    return out

def policy(file=RECIPE):
    p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique);fields(p,'schema kind commit field_recipe area_scene area_script player_script areas sounds output audio','recipe')
    require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.field-footsteps.candidate'and p['commit']==PIN,'Footstep recipe schema/pin')
    require(all(safe(p[k])for k in ('field_recipe','area_scene','area_script','player_script','output','audio'))and p['output'].startswith('world/')and p['output'].endswith('.encsteps')and p['audio'].startswith('sound/banks/')and p['audio'].endswith('.encaudio'),'Footstep recipe paths')
    require(type(p['areas'])is list and 1<=len(p['areas'])<=64 and all(safe(v)for v in p['areas'])and len(set(p['areas']))==len(p['areas']),'Footstep source areas')
    require(type(p['sounds'])is list and 1<=len(p['sounds'])<=16,'Footstep sound count');names=set();ids=set();paths=set()
    for s in p['sounds']:
        fields(s,'name id pcm','sound binding');require(type(s['name'])is str and re.fullmatch('[a-z][a-z0-9_]{0,31}',s['name'])and s['name']not in names and type(s['id'])is int and 0<s['id']<2**32 and s['id']not in ids and safe(s['pcm'])and s['pcm'].startswith('sound/effects/')and s['pcm'].endswith('.pcm')and s['pcm']not in paths,'Footstep sound identity/path');names.add(s['name']);ids.add(s['id']);paths.add(s['pcm'])
    return p

def source(file=RECIPE,*,root=ROOT):
    root=Path(root).resolve();p=policy(file);files=f.Sources(root);recipe=f.load_recipe(root/p['field_recipe']);scene=f.Scene(files,recipe);ex=files.extractor
    license=ex.text('LICENSE');require("purpose you're using them is related to the game"in license,'Footstep original game-related asset permission changed')
    doc=Document(p['area_scene'],ex.text(p['area_scene']));script=ex.text(p['area_script']);player=ex.text(p['player_script']);require(doc.ext_path(doc.nodes[0].props['script'],'Script')==p['area_script'],'Footstep source class script')
    defaults={}
    for name in ('entering_sound','exiting_sound','enter_shadow_effect','exit_shadow_effect'):
        m=re.search(r'^export [(][^\n]+[)] var '+name+r' = ("[^\"]*")$',script,re.M);require(m,'Footstep export default '+name);defaults[name]=json.loads(m[1])
    require('var enabled = true'in script and 'if body == global.get_player() and enabled:'in script and 'global.get_player().run_sound = sound'in script,'Footstep event source changed')
    run=re.search(r'^var run_sound := "([^\"]+)"$',player,re.M);shadow=re.search(r'^\tset_shadow\("([^\"]+)"\)$',player,re.M);slot=re.search(r'audioManager.add_sfx\(sfx, "([^\"]+)"\)',player);require(run and shadow and slot and 'res://Audio/Sound effects/Footsteps/'in player,'Footstep player ready binding')
    methods,hashes=extract(player.encode(),hashlib.sha256(player.encode()).hexdigest(),['_set_running','_move_state','_movement'])
    require('if !_paused and _state == MOVE:'in methods and 'if enabled and !_climbing:'in methods and '.stream == sfx:'in methods and 'get_sfx("run").stop() # Nasty fix'in methods,'Footstep source MOVE scope changed')
    sounds=[]
    for binding in p['sounds']:
        path='Audio/Sound effects/Footsteps/'+binding['name']+'.mp3';ex.data(path);import_text=ex.text(path+'.import');require(re.search(r'^loop=true$',import_text,re.M)and re.search(r'^loop_offset=0$',import_text,re.M),'Footstep source must retain imported full loop')
        sounds.append(dict(name=binding['name'],id=binding['id'],source='res://'+path,sha256=ex.sources[path],import_sha256=ex.sources[path+'.import'],pcm=binding['pcm'],loop=True))
    names={s['name']for s in sounds};require(run[1]in names,'Footstep default run sound not mapped');areas=[]
    actual=[n.path for n in scene.nodes if n.scene==p['area_scene']and n.type=='Area2D']
    require(actual==p['areas'],'Footstep source area order/coverage differs')
    for path in p['areas']:
        n=scene.by_path[path];require(n.scene==p['area_scene']and n.type=='Area2D'and owner_of(n,'script').source==p['area_scene'],'Footstep source instance class')
        values={k:n.props.get(k,v)for k,v in defaults.items()};require(set(n.props)<=set(defaults)|{'script','position','scale','rotation','collision_layer','collision_mask','monitorable','monitoring','visible','__meta__'},'Footstep unknown area override')
        require(n.props.get('collision_layer',1)==2048 and n.props.get('collision_mask',1)==1 and n.props.get('monitoring',True)and not n.props.get('monitorable',True),'Footstep source filtering changed')
        require(n.props.get('collision_mask',1)&recipe['player']['collision_layer'],'Footstep area does not observe the mapped Player')
        require(values['entering_sound']in names and (not values['exiting_sound']or values['exiting_sound']in names)and values['enter_shadow_effect']==shadow[1]and values['exit_shadow_effect']=='','Footstep unreviewed sound/shadow selection')
        shapes=[s for s in scene.nodes if s.path.rpartition('/')[0]==path and s.type=='CollisionShape2D'];require(len(shapes)==1,'Footstep requires one checked rectangle')
        shape=shapes[0];owner=owner_of(shape,'shape');ref=shape.props['shape'];require(owner.sub_type(ref)=='RectangleShape2D'and not shape.props.get('disabled',False),'Footstep rectangle disabled/type');e=owner.sub_resource(ref)['extents'];xf=scene.global_xf[shape.path]
        require(isinstance(e,Vec2)and e.x>0 and e.y>0 and xf.xy==0 and xf.yx==0 and xf.xx>0 and xf.yy>0,'Footstep transformed rectangle scope')
        areas.append(dict(path=path,enter=values['entering_sound'],exit=values['exiting_sound'],center=[f.f32(xf.ox),f.f32(xf.oy)],extents=[f.f32(e.x*xf.xx),f.f32(e.y*xf.yy)],enter_shadow=values['enter_shadow_effect'],exit_shadow=values['exit_shadow_effect'],layer=n.props['collision_layer'],mask=n.props.get('collision_mask',1),enabled=True))
    metadata=dict(scene=recipe['scene']['id'],default=run[1],shadow=shadow[1],slot=slot[1],sounds=sounds,areas=areas)
    proof=dict(commit=PIN,whole_player_approved=False,sources=dict(sorted(ex.sources.items())),player_methods=hashes,scope='Source MOVE run-loop and always-enabled rectangle step areas only; no movement, audio-output or scene pass')
    validate(metadata);return p,metadata,proof

def validate(m,*,packed=False):
    fields(m,'scene default shadow slot sounds areas','metadata');require(type(m['scene'])is int and 0<m['scene']<2**32,'Footstep scene identity')
    require(type(m['sounds'])is list and 1<=len(m['sounds'])<=16 and type(m['areas'])is list and 1<=len(m['areas'])<=64,'Footstep metadata capacities');names=set();ids=set();paths=set()
    for s in m['sounds']:
        fields(s,'name id source sha256 loop'if packed else 'name id source sha256 import_sha256 pcm loop','sound')
        require(type(s['name'])is str and re.fullmatch('[a-z][a-z0-9_]{0,31}',s['name'])and s['name']not in names and type(s['id'])is int and 0<s['id']<2**32 and s['id']not in ids,'Footstep sound identity');names.add(s['name']);ids.add(s['id'])
        require(type(s['source'])is str and s['source'].startswith('res://Audio/Sound effects/Footsteps/')and safe(s['source'][6:])and s['source'].endswith('.mp3')and s['source']not in paths and (packed or safe(s['pcm'])and s['pcm'].startswith('sound/effects/')and s['pcm'].endswith('.pcm')),'Footstep sound paths');paths.add(s['source'])
        require(all(type(s[k])is str and re.fullmatch('[0-9a-f]{64}',s[k])and int(s[k],16)for k in (('sha256',)if packed else ('sha256','import_sha256')))and s['loop']is True,'Footstep fingerprint/loop')
    require(type(m['default'])is str and m['default']in names and type(m['shadow'])is str and re.fullmatch('[a-z][a-z0-9_]{0,31}',m['shadow'])and type(m['slot'])is str and re.fullmatch('[a-z][a-z0-9_]{0,31}',m['slot']),'Footstep default identity')
    areas=set()
    for a in m['areas']:
        fields(a,'path enter exit center extents enter_shadow exit_shadow layer mask enabled','area')
        require(safe(a['path'])and a['path']not in areas and type(a['enter'])is str and type(a['exit'])is str and a['enter']in names and (not a['exit']or a['exit']in names),'Footstep area identity/selection');areas.add(a['path'])
        for key in ('center','extents'):require(type(a[key])is list and len(a[key])==2 and all(type(v)in(int,float)and math.isfinite(v)and (v>0 if key=='extents'else True)and abs(v)<=1e6 for v in a[key]),'Footstep rectangle numbers')
        require(a['enter_shadow']==m['shadow']and a['exit_shadow']==''and type(a['enabled'])is bool and all(type(a[k])is int and 0<a[k]<2**32 for k in ('mask','layer')),'Footstep filtering/shadow policy')
    return m

def encode(m):
    validate(m);pool=bytearray();cache={}
    def S(s):
        if not s:return 0,0
        if s not in cache:raw=s.encode();cache[s]=(len(pool),len(raw));pool.extend(raw)
        return cache[s]
    names={s['name']:i for i,s in enumerate(m['sounds'])};policy=struct.pack('<6I',names[m['default']],*S(m['shadow']),*S(m['slot']),0);sounds=bytearray();areas=bytearray()
    for s in m['sounds']:sounds.extend(struct.pack('<4I32s2I',*S(s['name']),*S(s['source']),bytes.fromhex(s['sha256']),s['id'],1))
    for a in m['areas']:areas.extend(struct.pack('<4I4f8I',*S(a['path']),names[a['enter']],names[a['exit']]if a['exit']else f.NONE,*a['center'],*a['extents'],*S(a['enter_shadow']),*S(a['exit_shadow']),a['layer'],a['mask'],int(a['enabled']),0))
    return f.pack_container(MAGIC,m['scene'],[pool,policy,sounds,areas],STRIDES,LIMIT)

def parse(blob):
    scene,sections=f.unpack_container(blob,MAGIC,STRIDES,LIMIT);pool,policy,sounds,areas=sections
    require(0<len(pool)<=32768 and len(policy)==24 and 1<=len(sounds)//56<=16 and 1<=len(areas)//64<=64,'Footstep packed capacities')
    def text(off,length):
        require((length>0 or off==0)and off<=len(pool)and length<=len(pool)-off,'Footstep packed text reference');return pool[off:off+length].decode('utf8')
    default,so,sl,ko,kl,reserved=struct.unpack('<6I',policy);require(not reserved,'Footstep policy reserved');rows=[]
    for at in range(0,len(sounds),56):
        no,nl,po,pl,digest,identity,flags=struct.unpack_from('<4I32s2I',sounds,at);require(flags==1,'Footstep packed sound flags');rows.append(dict(name=text(no,nl),source=text(po,pl),sha256=digest.hex(),id=identity,loop=True))
    require(default<len(rows),'Footstep packed default sound');zones=[]
    for at in range(0,len(areas),64):
        v=struct.unpack_from('<4I4f8I',areas,at);require(v[2]<len(rows)and(v[3]==f.NONE or v[3]<len(rows))and v[14]in(0,1)and not v[15],'Footstep packed area binding/flags')
        zones.append(dict(path=text(*v[:2]),enter=rows[v[2]]['name'],exit=''if v[3]==f.NONE else rows[v[3]]['name'],center=list(v[4:6]),extents=list(v[6:8]),enter_shadow=text(*v[8:10]),exit_shadow=text(*v[10:12]),layer=v[12],mask=v[13],enabled=bool(v[14])))
    m=dict(scene=scene,default=rows[default]['name'],shadow=text(so,sl),slot=text(ko,kl),sounds=rows,areas=zones);return validate(m,packed=True)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);ap.add_argument('--audio',action='store_true');ap.add_argument('--ffmpeg',default='ffmpeg');ap.add_argument('--ffprobe',default='ffprobe');o=ap.parse_args();out=o.out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build','Footstep candidates must stay under build');p,m,proof=source();blob=encode(m);parse(blob);out.mkdir(parents=True,exist_ok=True)
    for name in ('source.json','audio-source.json'):
        if(out/name).exists():(out/name).unlink()
    target=out/p['output'];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
    if o.audio:
        recipe=dict(schema=1,upstream_commit=PIN,bus_source='Audio/Audio Buses/default_bus_layout.tres',manager_source='Scripts/global/audioManager.gd');ex=Extractor(ROOT)
        for key in ('bus','manager'):ex.data(recipe[key+'_source']);recipe[key+'_sha256']=ex.sources[recipe[key+'_source']]
        recipe['assets']=[dict(stable_id=s['id'],source_path=s['source'],source_sha256=s['sha256'],import_sha256=s['import_sha256'],pcm_path=s['pcm'],gain_db=0)for s in m['sounds']]
        bindings=[]
        with tempfile.TemporaryDirectory(prefix='step-decode-',dir=out)as temp:
            temp=Path(temp);receipt=audio_asset.compile_assets(recipe,ex.upstream,temp,ffmpeg=o.ffmpeg,ffprobe=o.ffprobe)
            for row in receipt['files']:
                original=temp/row['path'];name=p['audio']if row['path']=='sound/banks/opening.encaudio'else row['path'];dest=out/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(original,dest);bindings.append(dict(conversion_path=row['path'],candidate_path=name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
            (out/'audio-source.json').write_bytes((json.dumps(dict(conversion=receipt,output_bindings=bindings,scope='Real generated bank renamed without editing bytes; converter receipt retained verbatim'),indent=2)+'\n').encode())
    outputs=[p['output']]+([p['audio']]+[s['pcm']for s in m['sounds']]if o.audio else [])
    receipt=dict(outputs={name:dict(bytes=(out/name).stat().st_size,sha256=hashlib.sha256((out/name).read_bytes()).hexdigest())for name in outputs},asset_permission='Original LICENSE permits assets/music in game-related forks and modifications; these are original game-related port derivatives, with notices retained and no MIT relicensing',source=proof,metadata=m,producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),staged=False)
    (out/'source.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode());print('Footstep candidate:',len(blob),'bytes;',len(m['sounds']),'source loops;',len(m['areas']),'areas; no runtime consumer/staging yet')
if __name__=='__main__':main()
