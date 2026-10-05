"""Bind already reviewed native House spans to original YAML phrase keys."""
import re,yaml
from localization_assets import BASE,UPSTREAM,load,require

def unescape(s):
 return re.sub(r'\\([abfnrtv\\\'\"])',lambda m:{'a':'\a','b':'\b','f':'\f','n':'\n','r':'\r','t':'\t','v':'\v','\\':'\\',"'":"'",'"':'"'}[m[1]],s)
def condition(s):
 # Current CTR reviewed input binding is a gamepad, as existing Pillow frontend.
 return re.sub(r'\[if input:gamepad\](.*?)(?:\[else\](.*?))?\[/if\]',lambda m:m[1],s,flags=re.S)
def source_plain(s,labels=None,item=None):
 s=condition(unescape(s));s=re.sub(r'\[(?:Ninten|PartyLead|ItemReceiver)\]','[Ninten]',s,flags=re.I);s=re.sub(r'\[(ui_select|ui_accept|ui_toggle)\]',lambda m:(labels or {'ui_toggle':'B'})[m[1].lower()],s,flags=re.I)
 if item:
  s=re.sub(r'\[ItemName\]',item['names']['en'],s,flags=re.I);s=re.sub(r'\[ItemArt1\]',item['articles']['en'].split(',')[1],s,flags=re.I)
 s=re.sub(r'\[(?:@|(?:WAIT|W)(?:@|BR)?|BR|/?color|/?c|D(?::[\d.]+)?|DELAY(?::[\d.]+)?)\]','',s,flags=re.I)
 return re.sub(r'\s','',s)
def native_plain(parts):
 substitutions={2:'[Ninten]',5:'[EarnedCash]',6:'[BankCash]',7:'[CurrentCash]',11:'[FavFood]'}
 return re.sub(r'\s','',''.join(t['text']if t['kind']==1 else substitutions.get(t['kind'],'')for p in parts for t in p['tokens']))
def bindings(catalog,source,bind,tr):
 house=load(BASE/'content/native-house.json');groups=[dict(first_segment=n['first_segment'],segment_count=n['segment_count'],source_path=n['dialogue_path'])for n in house['npcs']if n['segment_count']]+house['dialogues']
 from tools.story_input_bindings import load as input_bindings
 labels=input_bindings(BASE)
 basement=load(BASE/'content/native-basement-progression.json')
 result=[]
 for group in groups:
  first,count,path=group['first_segment'],group['segment_count'],group['source_path'];doc=yaml.safe_load(source(path));item=basement['key_items'][0] if path=='Data/Dialogue/'+basement['door']['opened']+'.yaml' else None;expected=native_plain(house['segments'][first:first+count]);matches=[]
  for label,phrase in doc.items():
   if isinstance(phrase,dict)and phrase.get('text')and source_plain(tr(phrase['text']),labels,item)==expected:matches.append((label,phrase))
  require(len(matches)==1,f'House source phrase ambiguous {first}/{count} {path}: {matches}')
  label,phrase=matches[0];key=phrase['text'];identity=f'house/{first}/{count}'
  bind(identity,key,tr(key),path+':'+str(label))
  if item:
   bind('house.item.name/'+str(first)+'/'+str(count),item['name_key'],item['names']['en'],path+':'+str(label)+':source key context')
   bind('house.item.article/'+str(first)+'/'+str(count),item['article_key'],item['articles']['en'],path+':'+str(label)+':source key context')
  def sized(value):return str(len(value.encode('utf-8')))+':'+value
  signature=''.join(str(part['flags'])+'/'+sized(part['speaker'])+sized(part['voice'])+str(len(part['tokens']))+'/'+''.join(str(token['kind'])+'/'+sized(token['text'])for token in part['tokens'])for part in house['segments'][first:first+count])
  bind('house.expected/'+str(first)+'/'+str(count),signature,signature,path+':'+str(label)+':checked native token projection')
  # Inherited speaker is visible in the existing reviewed span. Resolve the
  # explicit original name key, or the nearest preceding source name state.
  speaker=phrase.get('name','');seen=''
  for lab,ph in doc.items():
   if isinstance(ph,dict)and 'name'in ph:seen=ph['name']
   if lab==label:break
  speaker=speaker or seen
  expected_speaker=house['segments'][first]['speaker']
  if expected_speaker:
   require(tr(speaker)==expected_speaker,f'House speaker source mismatch {identity} {speaker!r}/{expected_speaker!r}')
   bind('house.speaker/'+str(first)+'/'+str(count),speaker,expected_speaker,path+':'+str(label)+':name')
  result.append(dict(first=first,count=count,key=key,speaker_key=speaker,source=path,phrase=str(label)))
 return result
