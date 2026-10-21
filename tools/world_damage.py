"""Checked original world-damage rules and Flash data;independent candidate."""
import argparse,hashlib,json,math,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import podunk_field as f
from tools.extract_battle_entry import Extractor,PIN,require
from tools.dungeon_keys import fields,path
from tools.dungeon_key_indicator import unique
from tools.godot_text import Document,SubRef,NodePathValue,Color
RECIPE=ROOT/'content/world-damage.json';MAGIC=b'ENCWDM01';STRIDES=(1,224,32,32,48);LIMIT=32768

def text(v):require(type(v)is str and 0<len(v)<=512 and all(32<=ord(c)<127 for c in v),'World damage text');path(v);return v

def integer(v,lo=0,hi=0xffffffff):require(type(v)is int and lo<=v<=hi,'World damage integer');return v

def real(v,lo=-1e6,hi=1e6):require(type(v)in(int,float)and math.isfinite(v)and lo<=v<=hi and f.f32(v)==v,'World damage binary32');return v

def method(raw,name):
 found=re.findall(r'^func '+re.escape(name)+r'\([^\n]*\n(?:[\t ].*\n|\n)*',raw,re.M);require(len(found)==1,'World damage source function missing/ambiguous: '+name);return found[0].rstrip()+'\n\n'

def read(file=RECIPE):
 p=json.loads(Path(file).read_bytes(),object_pairs_hook=unique,parse_constant=lambda v:(_ for _ in()).throw(ValueError('World damage nonfinite JSON')))
 fields(p,'schema kind commit scope paths sources clips targets statuses engine_commit engine_sources license_review','world damage recipe');require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.world-damage.reviewed-source'and p['commit']==PIN,'World damage reviewed identity');fields(p['paths'],'base player character status ui global flying scene poison unconscious hurt hurt_import license','world damage source paths');require(type(p['sources'])is dict and set(p['sources'])==set(p['paths'].values()),'World damage source coverage');ex=Extractor(ROOT)
 for n,h in p['sources'].items():text(n);require(type(h)is str and re.fullmatch('[0-9a-f]{64}',h)and hashlib.sha256(ex.data(n)).hexdigest()==h,'World damage source SHA changed: '+n)
 require(p['engine_commit']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'and set(p['engine_sources'])=={'scene/animation/animation_player.cpp','scene/resources/animation.cpp','core/math/math_funcs.h','core/math/math_defs.h'},'World damage engine profile')
 for n,h in p['engine_sources'].items():require(re.fullmatch('[0-9a-f]{64}',h)and hashlib.sha256((ROOT/('reports/world-damage/official-engine-source'if n.startswith('core/')else'reports/geyser-timeline/official-engine-source')/n).read_bytes()).hexdigest()==h,'World damage engine source changed')
 require(len(p['clips'])==2 and {c['name']for c in p['clips']}=={'Flash','RESET'}and len({c['id']for c in p['clips']})==2,'World damage clips')
 for c in p['clips']:fields(c,'id name','world damage clip selector');integer(c['id'],1,65535)
 require(type(p['targets'])is list and len(p['targets'])==9 and {t['id']for t in p['targets']}==set(range(1,10))and len({t['path']for t in p['targets']})==9,'World damage property coverage')
 for t in p['targets']:fields(t,'id path type','world damage target');require(type(t['type'])is int and t['type']in[1,2,4]and type(t['path'])is str and 0<len(t['path'])<=256,'World damage target type')
 require(type(p['statuses'])is list and len(p['statuses'])==2 and len({s['id']for s in p['statuses']})==2,'World damage status selectors')
 for s in p['statuses']:fields(s,'id path','world damage status selector');text(s['id']);require(s['path']in[p['paths']['poison'],p['paths']['unconscious']],'World damage status source')
 require(type(p['scope'])is str and len(p['scope'])>0 and type(p['license_review'])is str and len(p['license_review'])>0,'World damage scope/license');return p,ex

def source(file=RECIPE):
 p,ex=read(file);n=p['paths'];base=ex.text(n['base']);player=ex.text(n['player']);fly=ex.text(n['flying']);ui=ex.text(n['ui']);status=ex.text(n['status']);symbols={}
 for key,methods in {'base':['_is_invulnerable','damage','start_continuous_damage','stop_continuous_damage','play_flash_anim','pause_flash_anim','resume_flash_anim'],'player':['damage','start_continuous_damage'],'character':['set_stat','set_hp','add_status','remove_status','remove_all_statuses','_has_exclusive_status','is_incapacitated'],'status':['_init','to_dict'],'ui':['create_flying_num'],'global':['get_conscious_party'],'flying':['run']}.items():symbols[n[key]]={name:hashlib.sha256(method(ex.text(n[key]),name).encode()).hexdigest()for name in methods}
 def match(pattern,raw,label):m=re.findall(pattern,raw);require(len(m)==1,'World damage source binding: '+label);return m[0]
 flag=lambda raw:int(raw=='true')
 damage_method=method(base,'damage');default=flag(match(r'cant_kill := (true|false)',damage_method,'nonlethal default'));show=match(r'info_plates_show\((true|false), (true|false)\)',damage_method,'plate visibility');update=flag(match(r'info_plates_update\((true|false)\)',damage_method,'plate update'));flags=default|flag(show[0])<<1|flag(show[1])<<2|update<<3
 floor=int(match(r'max\(_party_member.get_hp\(\) - damage, (\d+)\)',method(base,'damage'),'HP floor'))
 hurt,slot=match(r'play_sfx\(load\("res://([^"]+)"\), "([^"]+)"\)',method(base,'damage'),'hurt slot')
 require(hurt==n['hurt']and n['hurt_import']==hurt+'.import','World damage sound source binding')
 camera=[float(v)for v in match(r'shake_camera\(([-.\d]+), ([-.\d]+), Vector2\(([-.\d]+), ([-.\d]+)\)\)',method(player,'damage'),'camera')]
 vib=match(r'start_joy_vibration\((\d+), ([-.\d]+), ([-.\d]+), ([-.\d]+)\)',method(player,'damage'),'vibration');device=int(vib[0]);vib=list(map(float,vib[1:]));offset=list(map(float,match(r'target_pos - Vector2\(([-.\d]+), ([-.\d]+)\)',method(ui,'create_flying_num'),'number offset')));bounds=list(map(float,match(r'var distance = rand_range\(([-.\d]+), ([-.\d]+)\)',fly,'flying range')));mod,negative=map(int,match(r'if \(randi\(\)%(\d+)\+0\) == (\d+):',fly,'flying sign'))
 xdur=float(match(r'"rect_position:x", rect_position.x \+ distance, ([-.\d]+)\)',fly,'x duration'));up,updur=map(float,match(r'"rect_position:y", rect_position.y - ([-.\d]+), ([-.\d]+)\)',fly,'up curve'));down,downdur=map(float,match(r'"rect_position:y", rect_position.y \+ ([-.\d]+), ([-.\d]+)\)',fly,'down curve'));require(float(match(r'set_delay\(([-.\d]+)\)',fly,'down delay'))==updur and 'Tween.TRANS_QUAD' in fly and 'Tween.EASE_OUT'in fly and 'Tween.TRANS_CIRC'in fly,'World damage inspected flying curve profile')
 force=float(match(r'_knockback = _hit_direct \* ([-.\d]+)',player,'hit force'));decay=float(match(r'_knockback.move_toward\(Vector2.ZERO, ([-.\d]+)\)',player,'knockback decay'));uncon=match(r'const AILMENT_UNCONSCIOUS := "([^"]+)"',status,'unconscious stable id')
 m=dict(strings=[n['scene'],hurt,slot,uncon],integers=[next(c['id']for c in p['clips']if c['name']=='Flash'),next(c['id']for c in p['clips']if c['name']=='RESET'),floor,device,mod,negative,flags],numbers=camera+vib+offset+bounds+[up,down,xdur,updur,downdur,force,decay],clips=[],keys=[],statuses=[])
 doc=Document(n['scene'],ex.text(n['scene']));animator=[v for v in doc.nodes if v.attrs.get('name')=='FlashAnim'];require(len(animator)==1,'World damage animator');animator=animator[0]
 for c in p['clips']:
  ref=animator.props.get('anims/'+c['name']);require(type(ref)is SubRef,'World damage local animation');kind,props=doc.sub[ref.id];require(kind=='Animation'and not props.get('loop',False)and all(k in['length','resource_name','step','loop']or k.startswith('tracks/')for k in props),'World damage nonloop clip profile');duration=f.f32(props.get('length',1));groups={}
  for k,v in props.items():
   if k.startswith('tracks/'):part=k.split('/');require(len(part)==3 and part[1].isdigit(),'World damage track syntax');groups.setdefault(int(part[1]),{})[part[2]]=v
  require(set(groups)==set(range(len(groups))),'World damage contiguous source tracks');first=len(m['keys']);seen=set()
  for track,t in groups.items():
   fields(t,'type path interp loop_wrap imported enabled keys','world damage track');require(t['type']=='value'and type(t['path'])is NodePathValue and type(t['interp'])is int and t['interp']==1 and t['loop_wrap']is True and t['imported']is False and t['enabled']is True,'World damage track profile');target=[v for v in p['targets']if v['path']==t['path'].path];require(len(target)==1,'World damage unknown property opcode');target=target[0];require(target['id']not in seen,'World damage duplicate property target');seen.add(target['id']);k=t['keys'];fields(k,'times transitions update values','world damage track keys');times=[f.f32(v)for v in k['times']];require(type(k['update'])is int and k['update']in[0,1]and k['transitions']==[1]*len(times)and len(times)==len(k['values'])and times and times==sorted(set(times))and times[0]>=0 and times[-1]<=f.f32(duration*1.001),'World damage key domain including source end tolerance');require(k['update']==1 or times==[0.],'World damage unsupported interpolated curve')
   for time,value in zip(times,k['values']):
    if target['type']==4:require(type(value)is Color,'World damage color Variant');values=[f.f32(v)for v in[value.r,value.g,value.b,value.a]]
    else:require(type(value)is(bool if target['type']==2 else float),'World damage numeric/Boolean Variant');values=[f.f32(value),0.,0.,0.]
    m['keys'].append(dict(target=target['id'],mode=k['update'],time=time,type=target['type'],value=values))
  m['clips'].append(dict(id=c['id'],name=c['name'],duration=duration,first=first,count=len(m['keys'])-first))
 default_prob=int(match(r'get\("heal_prob", (\d+)\)',status,'passive status field default'))
 for spec in p['statuses']:
  d=ex.yaml(spec['path']);require(set(d)<=set('priority healing messages status_bubble effects_by_char ignore_reinflict exclusive_status'.split())and set(d['healing'])<=set('persistent passive_heal heal_prob'.split()),'World damage unsupported source status fields');effects=d['effects_by_char'];require(set(effects)<=set('party_member any enemy'.split()),'World damage unsupported character status selector');member=effects.get('party_member',effects.get('any',{}));require(set(member)<=set('hp_scroll_multiplier info_plate_color incapacitated'.split()),'World damage unsupported party effects');passive=d['healing'].get('passive_heal',False);flags=int(d['healing'].get('persistent',False))|int(passive)<<1|int(d.get('exclusive_status',False))<<2|int(member.get('incapacitated',False))<<3;plate=member.get('info_plate_color','FFFFFF');require(re.fullmatch('[0-9A-F]{6}',plate),'World damage plate color');m['statuses'].append(dict(id=spec['id'],flags=flags,priority=d['priority'],scroll=f.f32(member.get('hp_scroll_multiplier',1.)),plate=int(plate+'FF',16),prob=d['healing'].get('heal_prob',default_prob)if passive else 0))
 validate(m);proof=dict(commit=PIN,recipe_sha256=hashlib.sha256(Path(file).read_bytes()).hexdigest(),sources=p['sources'],symbols=symbols,engine_commit=p['engine_commit'],engine_sources=p['engine_sources'],scope=p['scope']);return p,m,proof

def validate(m):
 fields(m,'strings integers numbers clips keys statuses','world damage model');require(type(m['strings'])is list and len(m['strings'])==4,'World damage text fields')
 for v in m['strings']:text(v)
 require(type(m['integers'])is list and len(m['integers'])==7,'World damage integer policy');a=m['integers'];integer(a[0],1,65535);integer(a[1],1,65535);require(a[0]!=a[1],'World damage duplicate lifecycle clip');integer(a[2],0,1000000);integer(a[3],0,15);integer(a[4],2,256);integer(a[5],0,a[4]-1);integer(a[6],0,15)
 require(type(m['numbers'])is list and len(m['numbers'])==18,'World damage real policy')
 for v in m['numbers']:require(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1e6,'World damage finite binary64 policy')
 v=m['numbers'];require(0<=v[0]<=100 and 0<v[1]<=60 and all(0<=v[i]<=1 for i in[4,5])and 0<v[6]<=60 and 0<=v[9]<v[10]<=10000 and all(0<v[i]<=60 for i in[13,14,15])and all(v[i]>=0 for i in[11,12,16,17]),'World damage policy domain')
 require(type(m['clips'])is list and len(m['clips'])==2 and {c['id']for c in m['clips']}==set(a[:2])and 0<len(m['keys'])<=128,'World damage clip/key bounds');first=0
 for c in m['clips']:
  fields(c,'id name duration first count','world damage clip');text(c['name']);real(c['duration'],.000001,60);integer(c['count'],1,64);require(type(c['first'])is int and c['first']==first and first+c['count']<=len(m['keys']),'World damage key ownership');seen={}
  for k in m['keys'][first:first+c['count']]:
   fields(k,'target mode time type value','world damage key');integer(k['target'],1,9);integer(k['mode'],0,1);real(k['time'],0,f.f32(c['duration']*1.001));require(type(k['type'])is int and k['type']==(1 if k['target']in[1,3,9]else 2 if k['target']==7 else 4)and type(k['value'])is list and len(k['value'])==4,'World damage property type/shape')
   for v in k['value']:real(v,0,1)
   require(k['type']==4 or k['value'][1:]==[0.,0.,0.],'World damage unused scalar lanes');require(k['type']!=2 or k['value'][0]in[0.,1.],'World damage Boolean value');require(k['mode']!=0 or k['time']==0,'World damage constant profile')
   if k['target']in seen:old=seen[k['target']];require(old['mode']==k['mode']==1 and old['time']<k['time'],'World damage sorted unique property times')
   seen[k['target']]=k
  require(set(seen)==(set(range(1,7))if c['id']==a[0]else set(range(1,10))),'World damage clip target coverage');first+=c['count']
 require(first==len(m['keys'])and len({c['name']for c in m['clips']})==2,'World damage unowned keys/duplicate names');require(type(m['statuses'])is list and len(m['statuses'])==2 and len({s['id']for s in m['statuses']})==len(m['statuses'])and m['strings'][3]in{s['id']for s in m['statuses']},'World damage status identity coverage')
 for s in m['statuses']:
  fields(s,'id flags priority scroll plate prob','world damage status');text(s['id']);integer(s['flags'],0,15);integer(s['priority'],0,1000000);real(s['scroll'],.001,100);integer(s['plate']);integer(s['prob'],0,100);require(s['flags']&2 or s['prob']==0,'World damage passive field guard')
 require(next(s for s in m['statuses']if s['id']==m['strings'][3])['flags']&12==12,'World damage unconscious effect binding');return m

def encode(m):
 validate(m);pool=bytearray()
 def S(v):raw=v.encode('ascii');span=(len(pool),len(raw));pool.extend(raw);return span
 policy=struct.pack('<16I20d',*[n for v in m['strings']for n in S(v)],*m['integers'],0,*m['numbers'],0.,0.)
 clips=b''.join(struct.pack('<4If3I',c['id'],*S(c['name']),c['first'],c['duration'],c['count'],0,0)for c in m['clips']);keys=b''.join(struct.pack('<2IfI4f',k['target'],k['mode'],k['time'],k['type'],*k['value'])for k in m['keys']);statuses=b''.join(struct.pack('<4If7I',*S(s['id']),s['flags'],s['priority'],s['scroll'],s['plate'],s['prob'],0,0,0,0,0)for s in m['statuses']);return f.pack_container(MAGIC,0,[pool,policy,clips,keys,statuses],STRIDES,LIMIT)

def parse(blob):
 scene,sections=f.unpack_container(blob,MAGIC,STRIDES,LIMIT,capability=1);pool,policy,clips,keys,statuses=sections;require(scene==0 and 0<len(pool)<=4096 and len(policy)==224 and len(clips)==64 and 0<len(keys)<=4096 and len(statuses)==96,'World damage section bounds');used=bytearray(len(pool))
 def S(a,b):require(0<b<=512 and a<=len(pool)and b<=len(pool)-a and not any(used[a:a+b]),'World damage text span/alias');used[a:a+b]=b'\1'*b;return pool[a:a+b].decode('ascii')
 p=struct.unpack('<16I20d',policy);require(not any(policy[60:64])and not any(policy[208:224]),'World damage policy reserved');m=dict(strings=[S(*p[i:i+2])for i in range(0,8,2)],integers=list(p[8:15]),numbers=list(p[16:34]),clips=[],keys=[],statuses=[])
 for at in range(0,len(clips),32):v=struct.unpack_from('<4If3I',clips,at);require(not any(v[6:]),'World damage clip reserved');m['clips'].append(dict(id=v[0],name=S(v[1],v[2]),first=v[3],duration=v[4],count=v[5]))
 for at in range(0,len(keys),32):v=struct.unpack_from('<2IfI4f',keys,at);m['keys'].append(dict(target=v[0],mode=v[1],time=v[2],type=v[3],value=list(v[4:])))
 for at in range(0,len(statuses),48):v=struct.unpack_from('<4If7I',statuses,at);require(not any(v[7:]),'World damage status reserved');m['statuses'].append(dict(id=S(v[0],v[1]),flags=v[2],priority=v[3],scroll=v[4],plate=v[5],prob=v[6]))
 require(all(used),'World damage unowned text bytes');return validate(m)

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--out',type=Path,required=True);out=a.parse_args().out.resolve();require(out.is_relative_to(ROOT/'build')and out!=ROOT/'build'and not out.exists(),'Fresh world damage candidate under build required');_,m,p=source();blob=encode(m);require(parse(blob)==m,'World damage roundtrip');out.mkdir(parents=True);(out/'world-damage.encwdm').write_bytes(blob);(out/'model.json').write_text(json.dumps(m,indent=2)+'\n');p.update(bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest());(out/'receipt.json').write_text(json.dumps(p,indent=2)+'\n');print('World damage candidate:',len(blob),'bytes;',len(m['keys']),'property keys;not formal scene admission')
if __name__=='__main__':main()
