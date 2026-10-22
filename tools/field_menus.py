#!/usr/bin/env python3
"""Source-reviewed START Goods/Status/Map/Options data and bounded RGBA maps.

No scene interpreter or generated-file patching. Extraction is explicit;
compile and stage rederive the candidate from the pinned read-only source.
"""
from __future__ import annotations
import argparse,csv,hashlib,io,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require,node
from tools.drawer_program import canonical
IR=ROOT/'content/native-field-menus.json'
PACK=Path('ui/field.encmenus')
KEYS=('MENU_TITLE_GOODS','MENU_TITLE_STATUS','CONTROLS_MAP','MENU_TITLE_OPTIONS','INVENTORY_ACTION_EQUIP','INVENTORY_ACTION_UNEQUIP','INVENTORY_ACTION_USE','INVENTORY_ACTION_SORT','INVENTORY_ACTION_DROP','INVENTORY_ACTION_SORT_MANUAL','INVENTORY_ACTION_SORT_AUTO','INVENTORY_ACTION_DROP_CONFIRM','INVENTORY_ACTION_USE_TARGET','MENU_YES','MENU_NO',None,'EQUIP_EMPTY','MENU_SKILLS','SKILLS_FIELD','SKILLS_BATTLE','SKILLS_PSI','STAT_HP','STAT_PP','STAT_LEVEL','STAT_EXP',None,'STAT_OFFENSE','STAT_DEFENSE','STAT_SPEED','STAT_IQ','STAT_GUTS','EQUIP_WEAPON','EQUIP_BODY','EQUIP_ARMS','EQUIP_OTHER','EQUIP_NONE','OPTIONS_FILESETTINGS','OPTIONS_SETTINGS','OPTIONS_TITLESCREEN','OPTIONS_QUIT','OPTIONS_TITLESCREEN_CONFIRM','OPTIONS_QUIT_CONFIRM','MENU_TEXTSPEED','MENU_FLAVORS','MENU_PROMPTS','OPTIONS_AUDIO','OPTIONS_VIDEO','OPTIONS_CONTROLS','OPTIONS_LANGUAGE','OPTIONS_VOLUME_MASTER','OPTIONS_VOLUME_MUSIC','OPTIONS_VOLUME_SFX','OPTIONS_RESOLUTION','OPTIONS_FULLSCREEN','OPTIONS_INTEGER_SCALING','OPTIONS_VSYNC','OPTIONS_ON','OPTIONS_OFF','MAP_404','MAP_FASTSCROLL','MAP_CLOSE','MAP_MARKERS','CONTROLS_INTERACT','CONTROLS_SKILL','CONTROLS_MENU','CONTROLS_SCOPE','CONTROLS_RUN','CONTROLS_PREV','CONTROLS_NEXT','CONTROLS_MAP','CONTROLS_PRESS_BUTTON',None,None,'MENU_BACK','CONTROLS_DEFAULT','CONTROLS_DEFAULT_CONFIRM')
ADAPT={15:('Key Items','关键道具'),25:('Next EXP','升级所需经验'),71:('Display Area','显示范围'),72:('3DS: fixed screen, 1:1 pixels, VSync on.','3DS：固定屏幕，原始像素，开启垂直同步。')}

def build(root=ROOT):
    root=Path(root);ex=Extractor(root)
    snippets={
      'Scripts/UI/Pausemenu.gd':['$InventoryUI.open(global.get_party_in_natural_order()[0])','3: #Status','4: #Map','5: #Options'],
      'Nodes/Ui/Inventory/ActionSelect.gd':['item.get_data().value != 0','global.party.size() > 1','ACTION_SUB_SORTAUTO:','ACTION_SUB_SORTMANUAL:','_current_char.unequip(_current_item)'],
      'Scripts/global/Inventory.gd':['sorting_scores[0] > sorting_scores[1]','reduce_or_drop_item(item)','_items.sort_custom(self, "_sort_auto")'],
      'Scripts/UI/MapScreen/MapScreen.gd':['global.get_player().position / 16','_toggle_markers()','1 + int(Input.is_action_pressed("ui_toggle"))'],
      'Scripts/UI/Options/OptionsUI.gd':['TITLE_SCREEN_ONLY','VOLUME_GRADES','global.save_settings()'],
      'Scripts/UI/statsmenu.gd':['_character.get_stat(_character.MAXHP)','panel_skills.activate()'],
      'Scripts/UI/SkillsMenuUI.gd':['SkillTabs.FIELD','current_character.has_field_skill(skill_id)'],
    }
    for path,checks in snippets.items():
        code=ex.text(path)
        for check in checks:require(check in code,'Unreviewed field menu behavior: '+path+': '+check)
    for path in ('Nodes/Ui/Inventory/InventoryUI.tscn','Scripts/UI/Inventory/InventoryUI.gd','Nodes/Ui/Inventory/InventorySelect.gd','Nodes/Ui/Inventory/TargetCharaSelect.gd','Nodes/Ui/Inventory/SortTypeSelect.gd','Nodes/Ui/Inventory/ConfirmationSelect.gd','Nodes/Ui/StatsMenuUI.tscn','Scripts/global/PartyMember.gd','Scripts/global/text_tools.gd','Scripts/UI/Options/ControlsUI.gd','Scripts/Main/RoomTypes/AreaRoom.gd','Scripts/global/uiManager.gd','Scripts/global/audioManager.gd','LICENSE'):
        ex.text(path)
    translations={}
    for file in ('menus','items','fieldskills','battleskills','locations'):
        for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/'+file+' - sheet.csv'))):
            if not row.get('key'):continue
            translations[row['key']]=dict(en=row['en'],zh=row.get('zh_CN') or row['en'])
    def label(key,ui=False):
        require(key in translations,'Missing reviewed translation '+str(key))
        result={lang:v.replace('\\n','\n').replace('[BR]','\n') for lang,v in translations[key].items()}
        if ui:
            inputs={name:'{button:'+str(i)+'}' for i,name in enumerate(('ui_accept','ui_cancel','ui_select','ui_scope','ui_toggle','ui_focus_prev','ui_focus_next','ui_map'))}
            for lang,value in result.items():
                depth=0
                def token(match):
                    nonlocal depth
                    name=match[1]
                    if name=='color':require(depth==0,'Nested source hint wrapper');depth=1;return ''
                    if name=='/color':require(depth==1,'Unbalanced source hint wrapper');depth=0;return ''
                    if name=='Ninten':return '{owner}'
                    require(name in inputs,'Unreviewed field skill text token: '+name);return inputs[name]
                result[lang]=re.sub(r'\[([^\]]+)\]',token,value)
                require(depth==0,'Unclosed source hint wrapper')
        require(all('['not in v and ']'not in v and '\u200b'not in v for v in result.values()),'Unreviewed field menu text controls: '+key)
        return result
    labels=[label(key) if key else dict(zip(('en','zh'),ADAPT[i])) for i,key in enumerate(KEYS)]
    from tools.source_settings import choices
    settings=choices(ex)
    setting_labels=[[label('MENU_'+key) for key in settings['speed_names']],[label('FLAVOR_'+v.upper()) for v in settings['flavors']], [label('MENU_'+v.upper()) for v in settings['prompts']]]
    # The admitted session includes only these normal and key identities. Check
    # every source action; unsupported effects cannot become enabled menu rows.
    session=json.loads((root/'content/native-session.json').read_text(encoding='utf8'))
    identities=list(dict.fromkeys([i['item_id'] for i in session['defaults']['characters'][0]['inventory']+session['defaults']['key_items']]+[i['item_id']for i in session['acquisitions']+session['key_acquisitions']]))
    item_definitions={d['source']:d['id']-1 for d in json.loads((root/'content/native-items.json').read_text(encoding='utf8'))['definitions']}
    from tools.item_consumption import load as consume_rules
    consumption=consume_rules()
    effects={row['id']:row for row in consumption['effects']}
    items=[]
    for identity in identities:
        item=ex.yaml('Data/Items/'+identity+'.yaml');actions=item.get('actions',[])
        require(len(actions)<=1 and not item.get('transform'),'Unreviewed field item transform/actions')
        function=actions[0]['function'] if actions else ''
        action={'':0,'equip':1,'consume':2,'use':3}.get(function)
        require(action is not None and (action!=3 or item.get('map_for')=='Podunk'),'Unreviewed field item action')
        result=dict(en='',zh='')
        if action==2:
            require(identity in effects,'Missing reviewed original consumption effect')
            effect=effects[identity]
            require(item.get('status_heals',[])==([effect['cure']]if effect['cure']else[])
                    and item['HPrecover']==effect['hp']and not item['PPrecover']and not any(item['boost'].values()),
                    'Unreviewed field consumable')
            if effect['cure']:
                status=ex.yaml('Data/StatusAilments/'+effect['cure']+'.yaml');result=label(status['messages']['heal_overworld_fail'])
        base=100000;equipped=base
        if not item['keyitem']:
            if item.get('battle_action',{}).get('skill'):
                require('score += SCORE_CAT_BATTLE_ITEMS'in ex.text('Scripts/global/Inventory.gd'),'Field battle item sort source changed')
                base=600000;equipped=base
            elif item.get('status_heals'):base=800000;equipped=base
            elif item.get('slot'):
                require(item['slot'] in ('weapon','body','arms','other'),'Unreviewed sort slot')
                slot=4-('weapon','body','arms','other').index(item['slot']);weights=(2,2,3,2,1,1,1)
                boost=sum(item['boost'][name]*weight for name,weight in zip(('maxhp','maxpp','offense','defense','speed','iq','guts'),weights))
                base=200000+slot*10000+boost;equipped=100000+slot*10000+boost
            elif item.get('HPrecover',0)>0:
                require(action==2 and not item['PPrecover']and not any(item['boost'].values()),'Unreviewed field food sort')
                base=1000000+item['HPrecover'];equipped=base
            else:raise ValueError('Unreviewed source sort category')
        require(item['keyitem'] or identity in item_definitions,'Missing checked ordinary item description binding')
        items.append(dict(id=identity,key=int(item['keyitem']),drop=int(not item['keyitem'] and item['value']!=0),action=action,doses=item.get('doses',1),score=base,equipped_score=equipped,slot=('weapon','body','arms','other').index(item['slot']) if item.get('slot') else 0xffffffff,definition=0xffffffff if item['keyitem'] else item_definitions[identity],name=label(item['name']),description=label(item['description']) if item['keyitem'] else dict(en='',zh=''),sorting=label(item['sorting_name']),result=result))
    skills=[]
    for path in sorted((root/'upstream/MOTHER-Encore/Data/FieldSkills').glob('*.yaml')):
        identity=path.stem;s=ex.yaml('Data/FieldSkills/'+path.name)
        if 'usable' in s and not s['usable'].get('ninten',False):continue
        require(set(s)<= {'name','description','battle_skill','usable','flag','is_skill_button'},'Unreviewed field skill condition')
        skills.append(dict(id=identity,flag=s.get('flag',''),battle_skill=s.get('battle_skill',''),weapon='',weapons=[],name=label(s['name']),description=label(s['description'],True),category=0,cost=0))
    known=list(dict.fromkeys([s for row in session['levels'] for s in row['skills']]+[s['skill_id']for s in session['skill_acquisitions']]))
    party=ex.text('Scripts/global/PartyMember.gd');weapon_body=re.findall(r'const WEAPONS : Dictionary = \{(.*?)^\}',party,re.M|re.S);require(len(weapon_body)==1,'Missing source weapon skill conditions')
    weapons={key:json.loads(values) for key,values in re.findall(r'"([^"]+)"\s*:\s*(\[[^\]]*\])',weapon_body[0],re.S)}
    psi_names={s['source']:dict(en=s['names']['en'],zh=s['names']['zh_Hans_CN']) for s in json.loads((root/'content/native-field-equipment.json').read_text(encoding='utf8'))['skills']}
    for identity in known:
        s=ex.yaml('Data/BattleSkills/'+identity+'.yaml')
        if s['skill_type'] not in ('skill','psi'):continue
        desc=label(s['description'],True)
        if s.get('description_ext'):
            extra=label(s['description_ext']);desc={lang:desc[lang]+'\n'+extra[lang] for lang in desc}
        weapon=s.get('required_weapon','');require(not weapon or weapon in weapons,'Unreviewed source skill weapon')
        skills.append(dict(id=identity,flag='',battle_skill=identity,weapon=weapon,weapons=weapons[weapon] if weapon else [],name=psi_names.get(identity,label(s['name'])),description=desc,category=1 if s['skill_type']=='skill' else 2,cost=s.get('sp_cost',0) if s['skill_type']=='skill' else s.get('pp_cost',0)))
    images=['Graphics/UI/Maps/Podunk_map.png','Graphics/UI/Maps/404_map.png','Graphics/UI/Maps/player_map_locator.png']
    markers=[];scene=ex.text('Nodes/Ui/MapScreen/Markers/PodunkMapMarkers.tscn');map_scene=ex.text('Nodes/Ui/MapScreen/Maps/Podunk.tscn');ex.text('Nodes/Ui/MapScreen/Maps/404.tscn');ex.text('Nodes/Ui/MapScreen/MapScreen.tscn');ex.text('Nodes/Ui/MapScreen/MapArrows.tscn');ex.text('Scripts/UI/MapScreen/MapArrows.gd')
    ext={int(i):p for p,i in re.findall(r'\[ext_resource path="res://([^"]+)"[^\n]*id=(\d+)\]',scene)}
    for name,body in re.findall(r'\[node name="([^"]+)" type="(?:Label|TextureRect)" parent="\."\]\n(.*?)(?=\[node|\Z)',scene,re.S):
        values=node(scene,name);texture=values.get('texture');image=0xffffffff;txt=dict(en='',zh='')
        if texture:
            source=ext[texture['ExtResource']]
            if source not in images:images.append(source)
            image=images.index(source)
        else:txt=label(values['text'])
        markers.append(dict(image=image,x=values.get('margin_left',0),y=values.get('margin_top',0),flip=values.get('flip_v',False),label=txt))
    map_ext={int(i):p for p,i in re.findall(r'\[ext_resource path="res://([^"]+)"[^\n]*id=(\d+)\]',map_scene)}
    for name in re.findall(r'\[node name="([^"]+)" type="TextureRect" parent="\."\]',map_scene):
        values=node(map_scene,name);source=map_ext[values['texture']['ExtResource']]
        if source not in images:images.append(source)
        markers.append(dict(image=images.index(source),x=values.get('margin_left',0),y=values.get('margin_top',0),flip=values.get('flip_v',False),label=dict(en='',zh='')))
    for p in images:ex.data(p);ex.data(p+'.import')
    field_scene=ex.text('Maps/podunk/podunk.tscn');home_scene=ex.text('Maps/podunk/Nintens House.tscn')
    def map_offset(scene):
        values=re.findall(r'^_player_map_offset = Vector2\( ([\d.-]+), ([\d.-]+) \)',scene,re.M);require(len(values)==1,'Ambiguous source map player offset');return [float(v)for v in values[0]]
    # Platform adaptation is explicit data, separate from source game rules.
    require({row['id']for row in items if row['action']==2}==set(effects),'Field consumption effect coverage differs from admitted items')
    return dict(schema=3,consumption=consumption,commit=PIN,sources=ex.sources,labels=labels,items=items,skills=skills,images=images,markers=markers,setting_labels=setting_labels,
      geometry=[[20,0,280,180],[32,6,256,16],[40,31,240,98],[32,133,256,29],[32,165,256,12],[32,32,8,8]],rows=7,columns=2,row_pitch=14,column_pitch=123,line_height=14,
      volume_db=[40*math.log((v+1)/11) for v in range(11)],logical_buttons=[1,2,8,2048,1024,512,256,4],button_names=['A','B','START','Y','X','L','R','SELECT'],default_volume=[10,10,10],display=0,
      locales=['en','zh_Hans_CN'],locale_names=[label('OPTIONS_LANGUAGE_EN'),label('OPTIONS_LANGUAGE_ZH_HANS_CN')],map_item='MapPodunk',map_offset=map_offset(field_scene),map_home=map_offset(home_scene),map_divisor=16,map_speed=60,map_fast=2,description_toggle=True,display_sizes=[[400,240],[320,180]],display_names=['400x240','320x180'],capture_cancel=dict(en='Assigned buttons swap bindings. D-pad Left: Back.',zh='已占用按键会交换绑定。方向键左：返回'))

def dump(path,value):path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes((json.dumps(value,ensure_ascii=False,indent=2)+'\n').encode('utf8'))

def encode(ir,root=ROOT):
    from PIL import Image
    require(type(ir['schema'])is int and ir['schema']in(2,3),'Unsupported field menu resource version')
    require((ir['schema']==3)==('consumption'in ir),'Missing/unowned field menu consumption extension')
    p=bytearray(bytes.fromhex(PIN))
    def put(fmt,*v):p.extend(struct.pack('<'+fmt,*v))
    def text(v):b=v.encode('utf8');require(len(b)<=4096 and '\0'not in v,'Field text bound');put('I',len(b));p.extend(b)
    def label(v):text(v['en']);text(v['zh'])
    def array(a,f):put('I',len(a));[f(v)for v in a]
    array(ir['labels'],label)
    def item(v):text(v['id']);put('8I',*[v[k]for k in ('key','drop','action','doses','score','equipped_score','slot','definition')]);[label(v[k])for k in ('name','description','sorting','result')]
    array(ir['items'],item)
    def skill(v):[text(v[k])for k in ('id','flag','battle_skill','weapon')];put('2I',v['category'],v['cost']);label(v['name']);label(v['description']);array(v['weapons'],text)
    array(ir['skills'],skill)
    def image(path):
        im=Image.open(Path(root)/'upstream/MOTHER-Encore'/path).convert('RGBA');require(0<im.width<=1024 and 0<im.height<=1024,'Map dimensions');raw=im.tobytes();put('3I',im.width,im.height,len(raw));p.extend(raw)
    array(ir['images'],image)
    def marker(v):put('I2fI',v['image'],v['x'],v['y'],int(v['flip']));label(v['label'])
    array(ir['markers'],marker)
    for g in ir['geometry']:put('4f',*g)
    put('2I3f',ir['rows'],ir['columns'],ir['row_pitch'],ir['column_pitch'],ir['line_height'])
    array(ir['volume_db'],lambda v:put('d',v));array(ir['logical_buttons'],lambda v:put('I',v));array(ir['button_names'],text)
    put('4I',*ir['default_volume'],ir['display']);array(ir['locales'],text);array(ir['locale_names'],label);text(ir['map_item']);put('5fI',*ir['map_offset'],ir['map_divisor'],ir['map_speed'],ir['map_fast'],ir['description_toggle'])
    for group in ir['setting_labels']:array(group,label)
    array(ir['display_names'],text);label(ir['capture_cancel']);put('2f',*ir['map_home']);put('4I',*ir['display_sizes'][0],*ir['display_sizes'][1])
    if ir['schema']==3:
        from tools.item_consumption import encode_extension
        require('consumption'in ir,'Field menu consume policy missing')
        p.extend(encode_extension(ir['consumption']))
    return struct.pack('<8s6I',b'ENCFMN01',ir['schema'],32+len(p),zlib.crc32(p),ir['schema'],1,0)+p

def load(root=ROOT):
    ir=json.loads((Path(root)/'content/native-field-menus.json').read_text(encoding='utf8'));require(canonical(ir)==canonical(build(root)),'Field menus IR differs from reviewed source/adapter');return ir
def stage_files(source):
    raw=(Path(source)/PACK).read_bytes();require(raw==encode(load()),'Stale staged field menu data');return {PACK:raw}
def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=('extract','compile','verify'));a=p.parse_args()
    ir=build() if a.action=='extract' else load()
    if a.action=='extract':dump(IR,ir);dump(ROOT/'reports/field-menus-source/source-review.json',dict(commit=PIN,sources=ir['sources'],scope='START six commands and all submenus for the currently admitted singleton, inventory and skill set. PC window-only controls are fixed 3DS capabilities. Game assets remain covered by the upstream game-related-use permission; original LICENSE retained.',mechanisms=['Typed ordinary item descriptions reuse the checked ItemDetails binding; exact source key line breaks and field skill input/nickname templates; unknown tags reject', 'Goods equip/unequip, consume even on heal failure, protected keys, manual swap and score/name auto sort; singleton hides Give as source does','Status derived live stats, EXP threshold, equipment and source learned/flagged skill filters','Map possession, missing page, source map/player offsets, scroll/fast scroll and markers','File settings, platform audio/display/input/language persistence, explicit title/quit confirmation']))
    raw=encode(ir);out=ROOT/'romfs'/PACK
    if a.action=='verify':require(out.read_bytes()==raw,'Stale field menus pack')
    else:out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(raw)
    print('Field menus: checked pinned source and',len(raw),'bytes')
if __name__=='__main__':main()
