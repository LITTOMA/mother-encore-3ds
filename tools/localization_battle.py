"""Recover and validate typed battle text contexts without runtime string matching."""
import re,itertools,yaml
from localization_assets import BASE,load,require

def add(records,locales,source,bind,tr):
 enemies=[]
 for enemy in ['lamp','doll','pillow']:
  d=yaml.safe_load(source('Data/Battlers/'+enemy+'.yaml'));enemies.append(d)
 telepathy=yaml.safe_load(source('Data/BattleSkills/telepathy.yaml'))
 def articles(key,lang,name):return [x.replace('{0}',name)for x in tr(key,lang).split(',')]
 def context(actor,target,enemy,lang,live,value=0,stat=''):
  player='[Ninten]'if live else 'Ninten';ename=tr(enemy['name'],lang)
  aname=player if actor=='player'else ename;tname=player if target=='player'else ename
  akey='ARTICLES_NINTEN'if actor=='player'else enemy['article'];tkey='ARTICLES_NINTEN'if target=='player'else enemy['article']
  legacy_stat=stat
  stat=stat.replace('STAT_','INLINE_STAT_',1)if stat else ''
  d=dict(name=aname,target=tname,value=str(value),stat=tr(legacy_stat if lang=='en'else stat,lang)if stat else '',skill=tr(telepathy['name'],lang),skillLevel='')
  for prefix,key,n in [('n',akey,aname),('t',tkey,tname),('s',telepathy['article'],tr(telepathy['name'],lang)),('i',telepathy['article'],tr(telepathy['name'],lang)),('v','ARTICLES_NUMBERS',str(value)),('st',stat+'_ARTICLE'if stat else '',tr(stat,lang)if stat else '')]:
   d.update({prefix+str(i):a for i,a in enumerate(articles(key,lang,n))})
  # Godot .format(array,"{n_}") only changes present array indices; absent
  # indices remain visible markers. Preserve them and reject unsupported later.
  return d
 def fmt(raw,ctx):return re.sub(r'\{([^}]+)\}',lambda m:ctx.get(m[1],m[0]),raw)
 stat_keys=[k for k in records if k.startswith('STAT_')]
 result=[]
 for path in ['content/native-round.json','content/doll-round.json','content/pillow-round.json']:
  ir=load(BASE/path);battle=ir['binding']['battle_id']
  for i,row in enumerate(ir['texts']):
   if row['role']==0:continue
   key=row['key']or row['source_text'];raw=tr(key);require(raw==row['source_text'],'Battle source key changed '+key)
   role=row['role'];candidates=[]
   if role in (0,1,2,4,5):candidates=[('player','enemy',enemies[0],0,'')]
   else:
    actor='player'if(role in (9,10,12)or(role==3 and key=='ATTACK_DIALOG'))else'enemy'
    target='enemy'if actor=='player'else'player'
    if role==8:actor,target='player','enemy'
    values=[0]
    if role==9:values=[x['exp']for x in enemies]
    elif role==10:values=[ir.get('encounter',{}).get('promoted_level',0)]
    elif role==11:values=[g['after']-g['before']for g in ir.get('growth',[])if g['text']==i]
    stats=stat_keys if role==11 else ['']
    for enemy,value,stat in itertools.product(enemies,values,stats):
     c=(actor,target,enemy,value,stat)
     if fmt(raw,context(*c[:3],'en',False,*c[3:]))==row['text']:candidates.append(c)
   require(candidates,'Unreviewed battle context '+path+':'+str(i))
   chosen=candidates[0];values={}
   for lang in locales:
    values[lang]=row['text']if lang=='en'else fmt(tr(key,lang),context(*chosen[:3],lang,True,*chosen[3:]))
   synthetic=f'@native.battle/{battle}/{i}';records[synthetic]=dict(key=synthetic,source=path+'#'+row['key'],line=i+1,values=values)
   bind(f'battle/{battle}/{i}',synthetic,row['text'],path+':texts/'+str(i)+':'+row['key'])
   # Preserve original source key and source template explicitly in sidecar.
   result.append(dict(battle=battle,index=i,key=row['key'],source_text=row['source_text'],expected=row['text'],actor=chosen[0],target=chosen[1],enemy=chosen[2]['name'],value=chosen[3],stat=chosen[4],derived_key=synthetic))
 return result
