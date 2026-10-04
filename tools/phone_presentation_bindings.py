"""Bounded reviewed phone presentation data; no script evaluation or fallback."""
import hashlib,json,math,re,struct,yaml
from pathlib import Path
from tools.doll_dialogue import PIN,require
from tools.extract_battle_entry import Extractor,node,animation,one

ROOT=Path(__file__).resolve().parents[1]
PATH=Path('content/phone-presentation-bindings.json')

def fields(value,names,label):
    require(type(value)is dict and set(value)==set(names),'Unreviewed phone binding fields: '+label)

def path(value):
    require(type(value)is str and value and not value.startswith('/')and '\\'not in value and ':'not in value and all(p not in ('','.','..')for p in value.split('/')),'Unsafe phone binding path')
    return value

def integer(value,lo,hi):
    require(type(value)is int and lo<=value<=hi,'Invalid phone binding integer')

def read(root=ROOT):
    def unique(pairs):
        result={}
        for key,value in pairs:
            require(key not in result,'Duplicate phone binding field: '+key)
            result[key]=value
        return result
    def nonfinite(value):raise ValueError('Nonfinite phone binding number: '+value)
    return json.loads((Path(root)/PATH).read_text(encoding='utf-8'),object_pairs_hook=unique,parse_constant=nonfinite)

def identity(root=ROOT):
    # Public legacy tool selectors remain available, but their values belong to
    # the checked manifest. Every emitting path performs the complete load.
    value=read(root)
    fields(value,['schema','kind','commit','sources','scene','script','sprite','audio','sounds','player','clips','callbacks','declarations','ring_binding'],'root')
    fields(value['sounds'],['ring','hangup'],'sounds')
    return value

def function(text,name):
    require(type(name)is str and re.fullmatch('[A-Za-z_][A-Za-z0-9_]*',name),'Invalid phone callback name')
    return one(r'^func '+re.escape(name)+r'\([^\n]*\):\n(.*?)(?=^func |\Z)',text,'phone callback',re.M|re.S)[1]

def checked(value,ex):
    fields(value,['schema','kind','commit','sources','scene','script','sprite','audio','sounds','player','clips','callbacks','declarations','ring_binding'],'root')
    require(type(value['schema'])is int and value['schema']==1 and value['kind']=='encore.phone-presentation.reviewed-bindings'and value['commit']==PIN==ex.lock['commit'],'Phone binding schema/pin rejected')
    fields(value['sounds'],['ring','hangup'],'sounds')
    fields(value['sprite'],['node','resource_id','role','path','kind','source','size','review'],'sprite')
    for group in ['audio','player']:fields(value[group],['node','review'],group)
    fields(value['callbacks'],['ring','use'],'callbacks')
    fields(value['ring_binding'],['source','object','method','flag','function_entry','flag_entry','review'],'ring binding')
    required={path(value['scene']),path(value['script']),path(value['ring_binding']['source']),path(value['sprite']['source']),*[path(s)for s in value['sounds'].values()]}
    require(type(value['sources'])is dict and set(value['sources'])==required,'Phone binding provenance coverage')
    for source,digest in value['sources'].items():
        require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest),'Invalid phone source fingerprint')
        require(hashlib.sha256(ex.data(source)).hexdigest()==digest,'Phone binding source mismatch')
    scene=ex.text(value['scene']);script=ex.text(value['script'])
    for group in ['sprite','audio','player']:
        row=value[group];path(row['node'])
        require(node(scene,row['node'])==row['review'],'Phone source node review mismatch: '+group)
    sprite=value['sprite'];integer(sprite['resource_id'],1,65535);integer(sprite['kind'],1,1)
    require(type(sprite['role'])is str and sprite['role']and '\0'not in sprite['role'],'Invalid phone resource role')
    path(sprite['path']);require(sprite['path'].endswith('.t3x'),'Unsupported phone texture output')
    require(type(sprite['size'])is list and len(sprite['size'])==2,'Invalid phone image size')
    for n in sprite['size']:integer(n,1,4096)
    require(ex.png_size(sprite['source'])==sprite['size'],'Phone source image review mismatch')
    for group,key,source,kind in [('sprite','texture',sprite['source'],'Texture'),('audio','stream',value['sounds']['ring'],'AudioStream')]:
        ref=value[group]['review'][key];fields(ref,['ExtResource'],'source resource');integer(ref['ExtResource'],1,65535)
        attrs=one(r'^\[ext_resource ([^\n]+) id='+str(ref['ExtResource'])+r'\]',scene,'phone resource declaration')[1]
        require(dict(re.findall(r'(\w+)="([^"]*)"',attrs))==dict(path='res://'+source,type=kind),'Phone resource source binding mismatch')
    require(type(value['clips'])is list and len(value['clips'])==2,'Phone clip topology rejected')
    roles=set();names=set()
    for clip in value['clips']:
        fields(clip,['role','native_kind','animation','events'],'clip')
        require(clip['role']in ['idle','ring']and clip['role']not in roles,'Unknown/duplicate phone clip role');roles.add(clip['role'])
        integer(clip['native_kind'],1,2);require(clip['native_kind']==(1 if clip['role']=='idle'else 2),'Phone clip role/kind mismatch')
        a=clip['animation'];fields(a,['name','source','source_resource_id','length','loop','step','tracks'],'animation')
        require(type(a['name'])is str and a['name']and a['name']not in names and a['source']==value['scene'],'Phone animation identity rejected');names.add(a['name'])
        integer(a['source_resource_id'],1,65535)
        require(value['player']['review'].get('anims/'+a['name'])=={'SubResource':a['source_resource_id']},'Phone animation reference mismatch')
        require(animation(scene,a['source_resource_id'],value['scene'],a['name'])==a,'Phone animation source facts mismatch')
        require(type(a['length'])in(int,float)and math.isfinite(a['length'])and 0<a['length']<=60 and type(a['loop'])is bool,'Phone animation mode rejected')
        require(type(a['step'])in(int,float)and math.isfinite(a['step'])and 0<a['step']<=a['length'],'Phone animation step rejected')
        require(a['loop']==(clip['role']=='ring'),'Phone clip loop role mismatch')
        require(type(a['tracks'])is list and a['tracks']and len(a['tracks'])<=8 and type(clip['events'])is list and len(clip['events'])==len(a['tracks']),'Phone event coverage rejected')
        seen=set();kinds=[]
        for event in clip['events']:
            fields(event,['track','kind','sound','time_encoding'],'event');integer(event['track'],0,len(a['tracks'])-1)
            require(event['time_encoding']in ['source','f32'],'Unknown phone time encoding')
            require(event['track']not in seen,'Duplicate phone event track');seen.add(event['track'])
            require(event['kind']in ['Frame','PlaySound'],'Unknown phone event action');kinds.append(event['kind'])
            track=a['tracks'][event['track']]
            fields(track,['type','path','interp','loop_wrap','imported','enabled','keys'],'track')
            require(track['type']=='value'and type(track['interp'])is int and track['interp']==1 and track['loop_wrap']is True and track['enabled']is True and track['imported']is False,'Unsupported phone track mechanism')
            keys=track['keys'];fields(keys,['times','transitions','update','values'],'keys')
            require(type(keys['times'])is list and type(keys['transitions'])is list and type(keys['values'])is list and keys['times']and len(keys['times'])==len(keys['transitions'])==len(keys['values'])and type(keys['update'])is int and keys['update']==1 and all(type(t)in(int,float)and t==1 for t in keys['transitions']),'Unsupported phone key mechanism')
            require(all(type(t)in(int,float)and math.isfinite(t)and 0<=t<=a['length']for t in keys['times'])and keys['times']==sorted(set(keys['times'])),'Invalid phone key times')
            if event['kind']=='Frame':
                require(event['sound']==''and track['path']==sprite['node']+':frame','Phone frame target mismatch')
                cols=sprite['review'].get('hframes',1);rows=sprite['review'].get('vframes',1);integer(cols,1,4096);integer(rows,1,4096)
                require(sprite['size'][0]%cols==0 and sprite['size'][1]%rows==0,'Phone atlas divisibility')
                for frame in keys['values']:integer(frame,0,cols*rows-1)
            else:
                require(event['sound']in value['sounds']and track['path']==value['audio']['node']+':playing'and all(v is True for v in keys['values']),'Unsupported phone sound event target/value')
        require(kinds.count('Frame')==1 and kinds.count('PlaySound')==(1 if clip['role']=='ring'else 0),'Phone runtime event topology')
    require(type(value['declarations'])is list and len(value['declarations'])==2 and len(set(value['declarations']))==2,'Phone alias declaration topology')
    aliases={}
    for declaration in value['declarations']:
        require(type(declaration)is str and len(re.findall('^'+re.escape(declaration)+'$',script,re.M))==1,'Phone source declaration mismatch')
        match=re.fullmatch(r'onready var ([A-Za-z_][A-Za-z0-9_]*) = \$([^\n]+)',declaration)
        require(match is not None,'Unknown phone alias declaration');aliases[match[2]]=match[1]
    require(set(aliases)=={value['player']['node'],value['audio']['node']},'Phone source alias binding mismatch')
    player,audio=map(re.escape,[aliases[value['player']['node']],aliases[value['audio']['node']]])
    clips={c['role']:c for c in value['clips']}
    for role,row in value['callbacks'].items():
        fields(row,['method','review'],'callback');require(type(row['review'])is str and function(script,row['method'])==row['review'],'Phone callback source expression mismatch')
        body=row['review'];clip=clips['ring'if role=='ring'else'idle']['animation']['name'];sound=value['sounds']['ring'if role=='ring'else'hangup']
        stream=one(r'^\s*'+audio+r'\.stream = ResourceLoader\.load\("res://([^"]+)"\)',body,'phone source stream')[1]
        target=one(r'^\s*'+player+r'\.play\("([^"]+)"\)',body,'phone source animation')[1]
        require(stream==sound and target==clip,'Phone callback resource/clip binding mismatch')
        if role=='ring':
            current=one(r'^\s*if '+player+r'\.current_animation != "([^"]+)":',body,'phone ring source guard')[1]
            require(current==clip,'Phone ring idempotence binding mismatch')
    binding=value['ring_binding'];doc=yaml.safe_load(ex.text(binding['source']))
    require(type(doc)is dict and type(binding['review'])is dict and set(binding['review'])=={binding['function_entry'],binding['flag_entry']},'Phone story binding coverage')
    require({entry:doc.get(entry)for entry in binding['review']}==binding['review'],'Phone ring story source mismatch')
    require(doc[binding['function_entry']].get('objectsfunction')=={binding['object']:binding['method']}and binding['method']==value['callbacks']['ring']['method']and doc[binding['flag_entry']].get('setflags')==binding['flag'],'Phone ring object/flag association mismatch')
    path(binding['object']);require(type(binding['flag'])is str and binding['flag'],'Invalid phone flag binding')
    return value

def load(root=ROOT):
    return checked(read(root),Extractor(root))

def events(clip,value):
    result=[]
    for binding in clip['events']:
        track=clip['animation']['tracks'][binding['track']]
        for time,key in zip(track['keys']['times'],track['keys']['values']):
            row=dict(time=time if binding['time_encoding']=='source'else struct.unpack('<f',struct.pack('<f',time))[0],track=binding['track'],kind=binding['kind'])
            if binding['kind']=='Frame':row['frame']=key
            else:row['resource']=value['sounds'][binding['sound']]
            result.append(row)
    return sorted(result,key=lambda event:(event['time'],event['track']))
