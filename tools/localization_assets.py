#!/usr/bin/env python3
"""Lossless upstream locale catalog and explicitly source-bound native text IDs.

No runtime English-text reverse map; the checked bindings retain the original key,
source node/phrase, and expected legacy value. Legacy gameplay packs are immutable.
"""
from __future__ import annotations
import argparse,csv,hashlib,json,os,re,struct,sys,zlib
from pathlib import Path
LANE=Path(__file__).resolve().parents[1]
BASE=Path(os.environ.get('ENCORE_SOURCE_ROOT',str(LANE))).resolve()
sys.path.insert(0,str(BASE))
from tools.extract_battle_entry import node,require
import yaml
UPSTREAM=BASE/'upstream/MOTHER-Encore'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(Path(p).read_text())
def write(p,x):Path(p).parent.mkdir(parents=True,exist_ok=True);Path(p).write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def extract():
 from localization_house import unescape,bindings as house_bindings
 sources={}; tables={}; records={}; superseded=[]
 def source(p):
  sources[p]=sha(UPSTREAM/p);return (UPSTREAM/p).read_text()
 global_source=source('Scripts/global/global.gd')
 locales=json.loads(re.search(r'const LANGUAGES := (\[[^\n]+\])',global_source)[1])
 disabled=json.loads(re.search(r'const LANGUAGES_DISABLED := (\[[^\n]*\])',global_source)[1])
 fallback=re.search(r'const LANGUAGE_DEFAULT := "([^"]+)"',global_source)[1]
 for f in sorted((UPSTREAM/'Translations/TranslatedText').glob('*.csv')):
  path=f.relative_to(UPSTREAM).as_posix();source(path); rows=list(csv.DictReader(f.open(encoding='utf-8-sig',newline='')));tables[f.stem]=rows
  for line,row in enumerate(rows,2):
   key=row.pop('key')
   if key in records:superseded.append(records[key])
   if not key:continue
   # Godot import canonicalizes zh_CN to zh_Hans_CN (native probe retained).
   values={('zh_Hans_CN' if k=='zh_CN' else k):v for k,v in row.items()}
   require(set(values)<=set(locales),'Unknown upstream CSV locale')
   records[key]=dict(key=key,source=path,line=line,values=values)
 def tr(key,locale='en'):
  r=records.get(key);return unescape((r['values'].get(locale)or r['values'].get(fallback)or key)if r else key)
 bindings=[]
 def bind(identity,key,expected,origin):
  require(tr(key)==expected,f'Source binding mismatch {identity}: {key!r} {tr(key)!r} != {expected!r}')
  bindings.append(dict(identity=identity,key=key,expected=expected,origin=origin))
 naming=load(BASE/'content/native-new-game.json'); sequence=yaml.safe_load(source('Data/NamingSequences/intro.yaml'))['scenario']
 for i,step in enumerate(sequence[:6]):
  bind(f'naming.prompt/{i}',step['prompt'],naming['fields'][i]['prompt'],'Data/NamingSequences/intro.yaml:'+str(i))
 for i,key in enumerate([sequence[0]['prompt'],'NAME_BLOCKED','NAME_DUPLICATED','SYMBOL_BULLET_NAMING','SYMBOL_DOT','KEYBOARD_PANEL1_NAME','KEYBOARD_PANEL0_NAME']):bind(f'naming.text/{i}',key,naming['texts'][i],'Maps/Naming screen.tscn')
 for i,key in enumerate(['MENU_DONT_CARE','MENU_BACKSPACE','MENU_OK']):bind(f'naming.command/{i+1}',key,naming['panels'][0][85+i]['value'],'Maps/Naming screen.tscn')
 scene=source('Maps/Naming screen.tscn');settings=load(BASE/'content/native-startup-settings.json')
 for i,name in enumerate(['TextSpeed','MenuFlavor','ButtonPrompts','End']):
  path='CanvasLayer/Settings/VBoxContainer/'+name;bind(f'settings.row/{i}',node(scene,path)['text'],settings['rows'][i]['text'],'Maps/Naming screen.tscn:'+path)
 for category,names,keys in [('speed',settings['choices']['speed_names'],['MENU_'+x for x in settings['choices']['speed_names']]),('flavor',settings['choices']['flavors'],['FLAVOR_'+x.upper()for x in settings['choices']['flavors']]),('prompt',settings['choices']['prompts'],['MENU_'+x.upper()for x in settings['choices']['prompts']])]:
  for i,key in enumerate(keys):bind(f'settings.{category}/{i}',key,settings[category+'_labels'][i],'Maps/Naming screen.tscn')
 for panel,(name,children) in enumerate([('TextSpeed',['Fast','Medium','Slow']),('Flavors',settings['choices']['flavors']),('ButtonPrompts',settings['choices']['prompts'])]):
  for i,child in enumerate(children):
   path='CanvasLayer/'+name+'/VBoxContainer/'+child;bind(f'settings.panel/{panel}/{i}',node(scene,path)['text'],settings['panels'][panel]['labels'][i]['text'],'Maps/Naming screen.tscn:'+path)
 for key in ['certainty']:
  # Explicit original Label in source scene, validated against expected data.
  matches=[]
  for m in re.finditer(r'^\[node name="([^"]+)"[^\n]*parent="([^"]+)"[^\n]*\]\n(.*?)(?=^\[|\Z)',scene,re.M|re.S):
   if 'Confirmation' in m[2]:
    t=re.search(r'^text = "([^"]+)"',m[3],re.M)
    if t and tr(t[1])==settings[key]['text']:matches.append((t[1],m[2]+'/'+m[1]))
  require(len(matches)==1,'Ambiguous settings certainty source');bind('settings.certainty',matches[0][0],settings[key]['text'],'Maps/Naming screen.tscn:'+matches[0][1])
 for i,label in enumerate(settings['confirmation_choices']):
  key=node(scene,'CanvasLayer/ConfirmationRight/Surely/VBoxContainer/'+['Label','Label2'][i])['text'];bind(f'settings.confirm/{i}',key,label['text'],'Maps/Naming screen.tscn:Confirmation')
 mechanism=source('Scripts/global/text_tools.gd')
 hangul=source('Scripts/languages/KoreanHangul.gd')
 vowel_rows=re.search(r'return (\[.*?\])\[type\] if .*? else (\[.*?\])\[type\]',mechanism)
 for category,group in [('vowel',1),('consonant',2)]:
  for i,value in enumerate(json.loads(vowel_rows[group])):bind(f'text.particle.{category}/{i}',value,value,'Scripts/global/text_tools.gd:_get_korean_particle')
 for i,value in enumerate(re.findall(r'last_char (?:in|not in) "([^"]+)"',hangul)):
  bind('text.korean.endings/'+str(i),value,value,'Scripts/languages/KoreanHangul.gd:ends_with_vowel')
 hint=re.search(r'const DIALOG_HINT_COLOR := "([^"]+)"',mechanism)[1]
 delay=re.search(r'var duration := tag_params\[0\]\.to_float\(\) if tag_params else ([0-9.]+)',mechanism)[1]
 bind('text.hint_color',hint,hint,'Scripts/global/text_tools.gd:DIALOG_HINT_COLOR')
 bind('text.default_delay',delay,delay,'Scripts/global/text_tools.gd:_replace_tags:delay')
 adapter=load(BASE/'content/pillow-input.json');require(adapter['action']=='ui_toggle'and adapter['input_type']=='gamepad','CTR toggle adapter changed')
 bind('input.ui_toggle',adapter['label'],adapter['label'],'content/pillow-input.json:label')
 house=house_bindings(records,source,bind,tr)
 from localization_battle import add as battle_bindings
 battles=battle_bindings(records,locales,source,bind,tr)
 # Checked current menu bindings. Literal display names never identify actors.
 battle_ui=load(BASE/'content/native-battle.json')
 for i,action in enumerate(battle_ui['menu']['actions']):bind('battle.menu/'+str(i),action['label_key'],battle_ui['presentation']['translations_en'][action['label_key']],'content/native-battle.json:menu/actions/'+str(i))
 items=load(BASE/'content/native-items.json')
 for i,item in enumerate(items['definitions']):
  path='Data/Items/'+item['source']+'.yaml';definition=yaml.safe_load(source(path));bind('item.name/'+str(i),definition['name'],item['name'],path+':name')
  item_value=next((value for value in definition['boost'].values()if value>0),0)
  key='@native.item.description/'+str(i);values={l:tr(definition['description'],l).replace('[ItemValue]',str(item_value)).replace('[BR]','\n')for l in locales};records[key]=dict(key=key,source=path+'#'+definition['description'],line=i+1,values=values);bind('item.description/'+str(i),key,item['description'],path+':description:'+definition['description'])
 # Source list is authoritative; compiled resources not referenced by project
 # are inventoried as unused, never merged under ambiguous duplicate keys.
 project=source('project.godot');legacy=[]
 for f in sorted((UPSTREAM/'Translations/TranslatedText').glob('*.translation')):
  p=f.relative_to(UPSTREAM).as_posix();sources[p]=sha(f);legacy.append(dict(source=p,sha256=sha(f),bytes=f.stat().st_size,referenced=p in project))
 require(not any(x['referenced']for x in legacy),'Legacy compiled translation now requires semantic import')
 for p in ['Scripts/global/text_tools.gd','Scripts/UI/AbstractDialogueBox.gd','Fonts/EBMain_la.tres','Fonts/EBMain_ja.tres','Fonts/EBMain_ko.tres','Fonts/EBMain_zh_cn.tres']:source(p)
 locale_rows=[]
 for locale in locales:
  font={'ja':'Fonts/EBMain_ja.tres','ko':'Fonts/EBMain_ko.tres','zh_Hans_CN':'Fonts/EBMain_zh_cn.tres'}.get(locale,'Fonts/EBMain_la.tres')
  unsupported=sorted({tag for r in records.values()if r['key'].startswith('@native.battle/')for tag in re.findall(r'\[(decline):[^\]]*\]',r['values'].get(locale,''))})
  locale_rows.append(dict(native_ready=not unsupported and locale in ['en','zh_Hans_CN'],native_blocker='Source custom-name declension is not yet implemented'if unsupported else('Native presentation validation pending'if locale not in ['en','zh_Hans_CN']else ''),code=locale,csv_code='zh_CN'if locale=='zh_Hans_CN'else locale,name=tr('OPTIONS_LANGUAGE_'+locale.upper()),enabled=locale not in disabled,font=font))
 return dict(schema=1,commit=load(BASE/'upstream.lock')['commit'],fallback=fallback,locales=locale_rows,records=sorted(superseded+list(records.values()),key=lambda r:r['key']),bindings=sorted(bindings,key=lambda r:r['identity']),sources=sources,unused_compiled=legacy,house_bindings=house,battle_bindings=battles)
def encode(d):
 raw=bytearray()
 def number(x):raw.extend(struct.pack('<I',x))
 def text(x):b=x.encode('utf-8');number(len(b));raw.extend(b)
 text(d['fallback']);number(len(d['locales']))
 for l in d['locales']:
  for k in ['code','csv_code','name','font']:text(l[k])
  number(int(l['enabled']));number(int(l['native_ready']));text(l['native_blocker'])
 number(len(d['records']))
 for r in d['records']:
  text(r['key']);text(r['source']);number(r['line'])
  from localization_house import unescape
  for l in d['locales']:text(unescape(r['values'].get(l['code'],'')))
 number(len(d['bindings']))
 for r in d['bindings']:
  for k in ['identity','key','expected','origin']:text(r[k])
 return struct.pack('<8s4I',b'ENCL10N1',1,len(raw)+24,zlib.crc32(raw),0)+raw
def stage_files(root):
 d=extract();blob=encode(d);path=Path('data/localization.enclocale');actual=(Path(root)/path).read_bytes();require(actual==blob,'Locale catalog does not match current source keys/bindings');return {path:actual}

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['compile','verify']);a=p.parse_args();d=extract();b=encode(d);dest=LANE/'romfs/data/localization.enclocale'
 if a.action=='compile':write(LANE/'content/native-localization.json',d);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(b)
 else:require(dest.read_bytes()==b,'Locale catalog is stale')
 print('Locale catalog:',len(d['locales']),'locales;',len(d['records']),'keys;',len(d['bindings']),'bindings;',len(b),'bytes')
