#!/usr/bin/env python3
"""Pinned Pause -> Equip frontend, original texture recipe and ENCFIE01.

JSON is offline source IR. This slice grants only singleton Ninten equipment;
known Pause commands other than Equip are represented but explicitly disabled.
"""
from __future__ import annotations
import argparse,csv,io,math,re,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require,node,animation
from tools.drawer_program import canonical,digest,fields,read_json,write_json,safe_path
IR='content/native-field-equipment.json'
RECIPE='content/field-equipment-assets.json'
REVIEW='reports/field-equipment-source/source-review.json'
RECEIPT='content/asset-receipts/graphics/ui/equipment/source.json'
PACK='romfs/data/opening.encfield'
NIL=0xffffffff
NAMES=('Strings','Parameters','Bindings','Commands','Slots','Equipment','Resources','Layouts','Clips','Keys')
FORMATS=(None,'<If','<III','<IIII','<IIII','<III7i','<7I32s2I','<7I10f4I','<IIIf','<3f')
STRIDES=(1,8,12,16,16,40,68,84,16,12)
HEADER=224
PARAMETERS=('ReferenceWidth','ReferenceHeight','PlatformWidth','PlatformHeight','PauseColumns','SlotPitch','ListPitch','StatPitch','CursorMoveSeconds','CursorFps','CursorFrame0','CursorFrame1','CursorFrame2','CursorFrame3','MainLineHeight','NumberLineHeight','NumberSpacing','LoopAround','PauseColumnPitch','PauseRowPitch','ListRows','BoostWeight0','BoostWeight1','BoostWeight2','BoostWeight3','BoostWeight4','BoostWeight5','BoostWeight6','OpenMask','ConfirmMask','CancelMask','ScopeMask','OwnerId')
BINDINGS=('PauseTitle','EquipTitle','None','Empty','StatMaxHP','StatMaxPP','StatOffense','StatDefense','StatSpeed','StatIQ','StatGuts','Owner','MainFont','NumberFont','PauseOpenSound','PauseCloseSound','EquipOpenSound','EquipCloseSound','MoveSound','ConfirmSound','RestrictedSound','ClearSound','EquipSound','BackSound','Level','CashPattern','CashRight')
ROLES=('PausePanel','PauseInside','PauseTitle','PauseCommand','PauseCash','EquipmentPanel','StatsPanel','Owner','EquipTitle','Portrait','SlotPanel','SlotLabel','SlotItem','ListPanel','ListItem','StatLabel','StatValue','StatProjected','StatIcon','DescriptionPanel','DescriptionText','Cursor','BoostEmpty','BoostBetter','BoostLower','PauseCursor','SlotCursor','CandidateCursor','CashLabel','CashValue','LevelLabel','LevelValue','PortraitEquipped','PortraitSuitable','PortraitBetter','PortraitLower','CashCents')
CLIPS=('PauseOpen','PauseClose','EquipOpen','EquipClose','DescriptionOpen','DescriptionClose')
STATS=('maxhp','maxpp','offense','defense','speed','iq','guts')
ASSETS=(('panel','Graphics/UI/Overworld/flavours/defaultempty.png',None,[1,1]),('inside','Graphics/UI/Overworld/flavours/defaultbox_inside.png',None,[1,1]),('description','Graphics/UI/Overworld/flavours/defaultbox.png',None,[1,1]),('ninten','Graphics/UI/Inventory/characters/ninten.png',None,[1,1]),('cursor','Graphics/UI/Inventory/cursor.png',None,[3,1]),('boost-empty','Graphics/UI/EquipMenu/empty.png',None,[1,1]),('boost-better','Graphics/UI/EquipMenu/red_arrow.png',None,[1,1]),('boost-lower','Graphics/UI/EquipMenu/blue_arrow.png',None,[1,1]),('portrait-equipped','Graphics/UI/Inventory/modifiers.png',[15,10,9,7],[1,1]),('portrait-suitable','Graphics/UI/Inventory/modifiers.png',[0,10,13,7],[1,1]),('portrait-better','Graphics/UI/Inventory/modifiers2.png',[0,0,11,7],[1,1]),('portrait-lower','Graphics/UI/Inventory/modifiers2.png',[0,7,11,7],[1,1]),('cents','Graphics/UI/Overworld/cents.png',None,[1,1]))
KINDS=('Container','Sprite','Rectangle','NinePatch','Text')

def table(ex,path):
    result={}
    for row in csv.DictReader(io.StringIO(ex.text(path))):
        require(row['key'] not in result,'Field duplicate translation');result[row['key']]=row
    return result

def build(root=ROOT):
    root=Path(root);ex=Extractor(root)
    pause_path='Nodes/Ui/Pause menu.tscn';equip_path='Nodes/Ui/EquipMenu/EquipMenuUI.tscn'
    pause=ex.text(pause_path);equip=ex.text(equip_path)
    ps=ex.text('Scripts/UI/Pausemenu.gd');es=ex.text('Nodes/Ui/EquipMenu/EquipMenuUI.gd')
    inv=ex.text('Scripts/global/Inventory.gd');party=ex.text('Scripts/global/PartyMember.gd')
    cursor=ex.text('Scripts/UI/cursor.gd');arrow=ex.text('Nodes/Ui/arrow.tscn')
    for path in ('Scripts/global/Item.gd','Scripts/global/Character.gd','Scripts/global/uiManager.gd','Scripts/global/controlsManager.gd','Nodes/Ui/HighlightLabel.tscn','Nodes/Ui/HighlightLabel.gd','Nodes/Ui/InventorySelect.tscn','Nodes/Ui/Inventory/InventorySelect.gd','Nodes/Ui/Inventory/portrait.tscn','Nodes/Ui/Inventory/portrait.gd','Nodes/Ui/EquipMenu/StatsLabel.tscn','Nodes/Ui/CashBoxPause.tscn','Nodes/Ui/Description.tscn','Scripts/UI/Reusables/Description.gd','project.godot','LICENSE'):ex.text(path)
    for snippet in ('event.is_action_pressed("ui_select")','2: #Equip','$EquipMenuUI.open(global.get_party_in_natural_order()[0])','_arrow.set_cursor_from_index(0, false)','audioManager.play_sfx_by_name("menu_open2", "menu_open")','audioManager.play_sfx_by_name("menu_close2", "menu_close")'):
        require(snippet in ps,'Field Pause source changed: '+snippet)
    for snippet in ('_current_character.get_items_for_slot( _get_current_slot(), true, false).empty()','var items = _current_character.get_items_for_slot(current_slot, true, true)','if _current_character.get_equipped_item(current_slot) or items.empty():','_item_list.append("")','_current_character.unequip_slot(_get_current_slot())','_current_character.equip_item(_get_selected_item())','current_value - equipped_item_boost.get(stat, 0) + selected_item_boost.get(stat, 0)','projected_value == current_value','_cursor_list.cursor_index = 0','anim_to_play = "Open"','audioManager.play_sfx_by_name("clear", "menu")','audioManager.play_sfx_by_name("equip", "menu")'):
        require(snippet in es,'Field Equip source changed: '+snippet)
    require('const SLOTS := ["weapon", "body", "arms", "other"]' in inv,'Field slots changed')
    require('return item_data.get("can_use", globaldata.characters).has(get_name())' in party,'Field equip owner condition changed')
    require('if !(only_unequipped and item.equipped) and !(only_suitable and !is_suitable):' in party,'Field equip candidates changed')
    require('const TWEEN_LENGTH := 0.1' in cursor and 'Tween.TRANS_QUART' in cursor and 'Tween.EASE_OUT' in cursor,'Field cursor easing changed')
    require('"speed": 5.0' in arrow and '[ SubResource( 1 ), SubResource( 2 ), SubResource( 3 ), SubResource( 2 ) ]' in arrow,'Field cursor source strip changed')
    from tools.items_presentation_bindings import load as load_items_binding
    identity=load_items_binding(root)['item']
    native=read_json(root/'content/native-items.json')
    require(native['owner']==identity['owner_id'] and type(native['owner']) is int and 1<=native['owner']<=8192,'Field Items owner identity mismatch')
    require([(d['id'],d['source']) for d in native['definitions']]==[(1,'BaseballCap'),(2,'AsthmaSpray')],'Field Items identity scope changed')
    cap=ex.yaml('Data/Items/BaseballCap.yaml')
    require(cap['actions']==[{'function':'equip'}] and cap['can_use']==[identity['party']] and cap['slot']=='other' and cap['keyitem'] is False and cap['transform']=='' and not cap.get('battle_action'),'Field unsupported equipment')
    boosts=[cap['boost'][key] for key in STATS]
    require(boosts==[0,0,0,5,0,0,0],'Field equipment source stats changed')
    require(ex.yaml('Data/save_new_game.yaml')['party']==[identity['party']],'Field singleton source changed')
    menus=table(ex,'Translations/TranslatedText/menus - sheet.csv')
    def text(key):return {lang:menus[key][col] for lang,col in (('en','en'),('zh_Hans_CN','zh_CN'))}
    bindings={}
    keys=('MENU_MENU','MENU_TITLE_EQUIP','EQUIP_NONE','EQUIP_EMPTY','STAT_MAXHP','STAT_MAXPP','STAT_OFFENSE','STAT_DEFENSE','STAT_SPEED','STAT_IQ','STAT_GUTS')
    for name,key in zip(BINDINGS[:11],keys):bindings[name]=text(key)
    for name,value in (('Owner',identity['party']),('MainFont','Fonts/EBMain_la.tres'),('NumberFont','Fonts/BottleRocket.tres')):bindings[name]={'en':value,'zh_Hans_CN':value}
    audio=ex.text('Scripts/global/audioManager.gd')
    events=('menu_open2','menu_close2','menu_open','menu_close','cursor1','cursor2','restricted','clear','equip','back')
    for name,event in zip(BINDINGS[14:24],events):
        matches=re.findall(r'"'+event+r'": load\("res://([^"\n]+)"\)',audio)
        require(len(matches)==1,'Field ambiguous audio binding');path=matches[0];ex.data(path);ex.data(path+'.import');bindings[name]={'en':path,'zh_Hans_CN':path}
    bindings['Level']=text('STAT_LEVEL');bindings['CashPattern']=text('$_LEFT');bindings['CashRight']=text('$_RIGHT')
    # English's source zero-width no-right-currency placeholder is a control,
    # not a visible glyph; preserve its empty rendering without font synthesis.
    require(bindings['CashRight']['en']=='\u200b' and bindings['CashRight']['zh_Hans_CN']=='','Field currency placeholder changed')
    bindings['CashRight']['en']=''
    for path in ('Fonts/EBMain_la.tres','Fonts/EBMain.tres','Fonts/BottleRocket.tres'):
        font=ex.text(path)
        for ref in re.findall(r'path="res://([^"\n]+)"',font):ex.data(ref)
    spacing=re.findall(r'^extra_spacing_char = (-?\d+)$',ex.text('Fonts/BottleRocket.tres'),re.M);require(len(spacing)==1,'Field numeric spacing')
    # These masks are reviewed native adapter bindings, not Godot keycodes.
    params=dict(zip(PARAMETERS,[320,180,400,240,2,24,13,12,.1,5,0,1,2,1,native['parameters']['LabelSize'][1],12,int(spacing[0]),1,71,15,7,2,2,3,2,1,1,1,8,1,2,512,native['owner']]))
    commands=[]
    for i,(name,key) in enumerate(zip(('Goods','PSI','Equip','Status','Map','Options'),('MENU_GOODS','MENU_PSI','MENU_EQUIP','MENU_STATUS','MENU_MAP','MENU_OPTIONS'))):
        require(node(pause,'menu/Commands/Items/'+name)['text']==key,'Field command translation binding')
        commands.append(dict(id=i+1,labels=text(key),enabled=i==2))
    slots=[dict(id=i+1,source=source,labels=text('EQUIP_'+source.upper())) for i,source in enumerate(('weapon','body','arms','other'))]
    for part in ('SlotArrow','ItemListPanel/ItemArrow'):
        require(node(equip,'EquipMenu/Box/Panels/Slots/'+part)['loop_around'],'Field slot/list wrapping changed')
    require(node(pause,'menu/Commands/Items')['columns']==2 and node(pause,'menu/Commands/Items')['custom_constants/hseparation']==10 and node(pause,'menu/Commands/Items')['custom_constants/vseparation']==3,'Field Pause grid changed')
    resources=[]
    for i,(name,path,crop,grid) in enumerate(ASSETS):
        size=ex.png_size(path);ex.data(path+'.import');resources.append(dict(id=i+1,name=name,source=path,size=size,crop=crop,grid=grid,output='graphics/ui/equipment/'+name+'.t3x'))
    layouts=[]
    def layout(role,kind,rect,resource=None,patch=None,color=None):
        layouts.append(dict(id=len(layouts)+1,role=role,parent=None,kind=kind,resource=resource,frame=0,flags=1,anchor=[0,0],rect=rect,color=color or [1,1,1,1],patch=patch or [0,0,0,0]))
    # Source viewport pixel rectangles. Container auto-sizing is explicitly
    # adapted to a bounded singleton; no screen scaling or fabricated font.
    layout('PausePanel','NinePatch',[7,7,156,66],0,[8]*4);layout('PauseInside','NinePatch',[15,15,140,50],1,[8]*4)
    layout('PauseTitle','Text',[18,3,120,12]);layout('PauseCommand','Text',[15,15,61,12]);layout('PauseCursor','Sprite',[8.666667,17,8,8],4)
    layout('CashLabel','Text',[16,65,24,12],color=[0,0,0,1]);layout('CashValue','Text',[40,65,94,12],color=[0,0,0,1]);layout('CashCents','Sprite',[134,66,10,9],12);layout('PauseCash','Text',[144,65,15,12],color=[0,0,0,1])
    layout('EquipmentPanel','NinePatch',[28,0,264,133],2,[4]*4);layout('StatsPanel','NinePatch',[40,23,140,110],1,[8]*4)
    layout('Owner','Text',[48,27,66,12]);layout('LevelLabel','Text',[48,40,66,12]);layout('LevelValue','Text',[114,40,26,12])
    layout('EquipTitle','Text',[220,5,65,12]);layout('Portrait','Sprite',[34,0,21,21],3)
    layout('SlotPanel','NinePatch',[186,23,106,110],1,[8]*4);layout('SlotLabel','Text',[196,30,86,12],color=[.74902,.705882,.803922,1]);layout('SlotItem','Text',[201,41,86,12])
    layout('SlotCursor','Sprite',[194.666667,43,8,8],4);layout('ListPanel','Rectangle',[191,29,96,97],color=[.0784314,.0666667,.0901961,1]);layout('ListItem','Text',[201,30,81,12]);layout('CandidateCursor','Sprite',[194.666667,32,8,8],4)
    layout('StatLabel','Text',[48,53,69,12]);layout('StatValue','Text',[114,53,26,12]);layout('StatProjected','Text',[151,53,21,12]);layout('StatIcon','Container',[143,57,8,8])
    layout('BoostEmpty','Sprite',[0,0,8,8],5);layout('BoostBetter','Sprite',[0,0,8,8],6);layout('BoostLower','Sprite',[0,0,8,8],7)
    layout('DescriptionPanel','NinePatch',[28,132,264,48],2,[8]*4);layout('DescriptionText','Text',[66,144,213,28])
    for role,rect,res in [('PortraitEquipped',[40,1,9,7],8),('PortraitSuitable',[38,21,13,7],9),('PortraitBetter',[39,1,11,7],10),('PortraitLower',[39,1,11,7],11)]:layout(role,'Sprite',rect,res)
    clips=[]
    for role,scene,path,rid,target,base in [('PauseOpen',pause,pause_path,1,'menu:rect_position',0),('PauseClose',pause,pause_path,3,'menu:rect_position',0),('EquipOpen',equip,equip_path,3,'EquipMenu/Box:rect_position:y',0),('EquipClose',equip,equip_path,2,'EquipMenu/Box:rect_position:y',0),('DescriptionOpen',equip,equip_path,7,'../EquipMenu/Description:rect_position:y',132),('DescriptionClose',equip,equip_path,6,'../EquipMenu/Description:rect_position:y',132)]:
        a=animation(scene,rid,path,role);tracks=[track for track in a['tracks'] if track['path']==target]
        require(len(tracks)==1 and tracks[0]['type']=='value' and tracks[0]['interp']==1 and tracks[0]['keys']['update']==0,'Field animation source mapping')
        keys=tracks[0]['keys'];values=[v[1] if isinstance(v,list) else v for v in keys['values']]
        clips.append(dict(role=role,duration=a['length'],source=path,source_resource_id=rid,source_base=base,source_values=values,keys=[dict(time=t,value=v-base,ease=e) for t,v,e in zip(keys['times'],values,keys['transitions'])]))
    return dict(schema=1,kind='encore.field-equipment.source-ir',commit=PIN,scope='Original Pause -> Equip for singleton Ninten and admitted BaseballCap; other known commands disabled; current UID and seven derived stats committed transactionally; no battle item-use or broader party capability',sources=dict(sorted(ex.sources.items())),dependencies={p:digest(root/p) for p in ('content/native-items.json','content/items-presentation-bindings.json')},parameters=params,bindings=bindings,commands=commands,slots=slots,equipment=[dict(definition=0,source='BaseballCap',slot=3,boosts=boosts)],resources=resources,layouts=layouts,clips=clips)

def recipe(ir):return dict(schema=1,kind='encore.field-equipment.asset-recipe',commit=PIN,sources=ir['sources'],resources=ir['resources'],licence_review='Pinned upstream LICENSE permits game-related modifications; art/font/audio retain upstream notices, not MIT.')
def extract(root=ROOT):
    root=Path(root);ir=build(root);write_json(root/IR,ir);write_json(root/RECIPE,recipe(ir))
    write_json(root/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),sources=ir['sources'],scope=ir['scope'],semantics=['Stable numeric OwnerId and source party name come from the checked Items identity adapter; they are separate from string-pool offsets','Pause six commands in original row-major order; only Equip admitted; source cancel/select closes Pause; Equip cancel returns Pause','Four original slots; empty unsuitable slot restricted; candidates suitable unequipped in inventory order; None last iff slot equipped or candidate list empty','Confirm equipment/None by persistent UID without consuming or recreating items; seven projected stats current minus slot boost plus selected boost','Original EBMain/BottleRocket faces, source numeric spacing, original PNG textures, 5fps 0/1/2/1 cursor and0.1sec quart-out movement; Pause cash uses EBMain right alignment and source black text','Source Y animation keys/easing retained; description absolute132px normalized to relative offsets, source timeline remains in IR','Source320x180 rectangles at1:1 centered for400x240; bounded singleton auto-size adaptation recorded explicitly; no claim of exact Godot container pixel parity','Native START/A/B/L masks map source select/accept/cancel/scope; no keycode copied into C++','English source U+200B right-currency placeholder is explicitly rendered empty; no new glyph invented'],unsupported=['Other Pause commands','Other characters/equipment/items','Battle item action and asthma statuses','Character tabs/backing ornaments and Pause info plates/audio muffle remain outside this slice'],unverified=['Tests retained but not run','Emulator and physical3DS visual/input/audio/save acceptance pending']))
    return ir

def load(root=ROOT):
    root=Path(root);ir=read_json(root/IR);require(canonical(ir)==canonical(build(root)),'Field stale/unreviewed source IR');review=read_json(root/REVIEW)
    fields(review,('schema','commit','ir_sha256','sources','scope','semantics','unsupported','unverified'),'Field review')
    require(review['schema']==1 and review['commit']==PIN and review['ir_sha256']==digest(root/IR) and review['sources']==ir['sources'] and review['scope']==ir['scope'],'Field review binding')
    require(read_json(root/RECIPE)==recipe(ir),'Field changed texture recipe');return ir

def compile_assets(tex3ds,root=ROOT):
    from PIL import Image
    root=Path(root);ir=load(root);require(tex3ds and Path(tex3ds).is_file(),'Field requires genuine tex3ds');tmp=root/'build/field-equipment-assets';tmp.mkdir(parents=True,exist_ok=True);outputs={}
    for r in ir['resources']:
        source=root/'upstream/MOTHER-Encore'/r['source'];target=root/'romfs'/r['output'];target.parent.mkdir(parents=True,exist_ok=True);size=r['size']
        if r['crop']:
            x,y,w,h=r['crop'];require(x>=0 and y>=0 and w>0 and h>0 and x+w<=size[0] and y+h<=size[1],'Field source crop');converted=tmp/(r['name']+'.png');Image.open(source).convert('RGBA').crop((x,y,x+w,y+h)).save(converted);source=converted;size=[w,h]
        subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(source)],check=True)
        outputs[r['output']]=dict(size=size,bytes=target.stat().st_size,sha256=digest(target),crc32=zlib.crc32(target.read_bytes())&NIL)
    receipt=dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),producer_sha256=digest(root/'tools/field_equipment.py'),tex3ds_sha256=digest(tex3ds),outputs=outputs);write_json(root/RECEIPT,receipt);return receipt

def verify_receipt(ir,root=ROOT,source=None):
    root=Path(root);source=Path(source) if source is not None else root/'romfs';r=read_json(root/RECEIPT)
    fields(r,('schema','commit','ir_sha256','producer_sha256','tex3ds_sha256','outputs'),'Field receipt')
    require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==digest(root/IR) and r['producer_sha256']==digest(root/'tools/field_equipment.py') and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256']),'Field receipt source identity')
    require(set(r['outputs'])=={a['output'] for a in ir['resources']},'Field receipt set')
    for a in ir['resources']:
        o=r['outputs'][a['output']];fields(o,('size','bytes','sha256','crc32'),'Field receipt output');raw=(source/a['output']).read_bytes()
        require(o['size']==(a['crop'][2:] if a['crop'] else a['size']) and type(o['bytes']) is int and o['bytes']==len(raw)>0 and digest(source/a['output'])==o['sha256'] and zlib.crc32(raw)&NIL==o['crc32'],'Field changed texture bytes')
    return r

def lower(ir,receipt):
    pool=bytearray(b'\0');offsets={'':0};out={name:[] for name in NAMES}
    def string(value):
        require(type(value) is str and '\0' not in value and len(value.encode())<=4096,'Field text bound')
        if value not in offsets:offsets[value]=len(pool);pool.extend(value.encode()+b'\0')
        return offsets[value]
    out['Parameters']=[[i+1,ir['parameters'][name]] for i,name in enumerate(PARAMETERS)]
    out['Bindings']=[[i+1,string(ir['bindings'][name]['en']),string(ir['bindings'][name]['zh_Hans_CN'])] for i,name in enumerate(BINDINGS)]
    out['Commands']=[[c['id'],string(c['labels']['en']),string(c['labels']['zh_Hans_CN']),int(c['enabled'])] for c in ir['commands']]
    out['Slots']=[[s['id'],string(s['source']),string(s['labels']['en']),string(s['labels']['zh_Hans_CN'])] for s in ir['slots']]
    out['Equipment']=[[e['definition'],string(e['source']),e['slot'],*e['boosts']] for e in ir['equipment']]
    for r in ir['resources']:
        a=receipt['outputs'][r['output']];out['Resources'].append([r['id'],string(r['output']),1,*a['size'],*r['grid'],bytes.fromhex(a['sha256']),a['bytes'],a['crc32']])
    for l in ir['layouts']:
        out['Layouts'].append([l['id'],ROLES.index(l['role'])+1,NIL if l['parent'] is None else l['parent'],KINDS.index(l['kind'])+1,NIL if l['resource'] is None else l['resource'],l['frame'],l['flags'],*l['anchor'],*l['rect'],*l['color'],*l['patch']])
    for i,c in enumerate(ir['clips']):
        start=len(out['Keys']);out['Keys'].extend([[k['time'],k['value'],k['ease']] for k in c['keys']]);out['Clips'].append([i+1,start,len(c['keys']),c['duration']])
    out['Strings']=bytes(pool);validate(out);return out

def encode(t):
    validate(t);blob=bytearray(HEADER);struct.pack_into('<8s6I20s12x',blob,0,b'ENCFIE01',2,0,0,1,1,len(NAMES),bytes.fromhex(PIN))
    for i,name in enumerate(NAMES):
        while len(blob)%4:blob.append(0)
        data=t[name] if i==0 else b''.join(struct.pack(FORMATS[i],*r) for r in t[name]);struct.pack_into('<4I',blob,64+i*16,i+1,len(blob),len(t[name]),STRIDES[i]);blob.extend(data)
    struct.pack_into('<I',blob,12,len(blob));struct.pack_into('<I',blob,16,zlib.crc32(blob)&NIL);return bytes(blob)

def parse_pack(blob):
    require(isinstance(blob,(bytes,bytearray)) and HEADER<=len(blob)<=1024*1024,'Field pack size')
    magic,schema,size,crc,caps,rules,count,pin=struct.unpack_from('<8s6I20s',blob)
    require(magic==b'ENCFIE01' and schema==2 and caps==rules==1 and size==len(blob) and count==len(NAMES) and pin.hex()==PIN and not any(blob[52:64]),'Field version/capabilities/pin')
    c=bytearray(blob);c[16:20]=b'\0'*4;require(zlib.crc32(c)&NIL==crc,'Field pack CRC');out={};end=HEADER
    for i,name in enumerate(NAMES):
        kind,start,num,stride=struct.unpack_from('<4I',blob,64+i*16)
        require(kind==i+1 and stride==STRIDES[i] and start==(end+3)//4*4 and not any(blob[end:start]) and start+num*stride<=len(blob),'Field directory/overlap')
        data=blob[start:start+num*stride];out[name]=bytes(data) if i==0 else [list(struct.unpack_from(FORMATS[i],data,j*stride)) for j in range(num)];end=start+num*stride
    require(end==len(blob),'Field trailing data');validate(out);return out

def validate(t):
    fields(t,NAMES,'Field sections');pool=t['Strings'];require(type(pool) is bytes and 0<len(pool)<=65536 and pool[0]==pool[-1]==0,'Field string pool');starts=set();p=0
    while p<len(pool):starts.add(p);end=pool.find(b'\0',p);require(end>=p,'Field unterminated string');pool[p:end].decode('utf-8');p=end+1
    def string(offset):require(type(offset) is int and offset in starts,'Field string reference');return pool[offset:pool.index(0,offset)].decode('utf-8')
    def integer(v,lo=0,hi=NIL):return type(v) is int and lo<=v<=hi
    def finite(v):return type(v) in (int,float) and math.isfinite(v) and abs(v)<=2000000
    require(len(t['Parameters'])==len(PARAMETERS) and len(t['Bindings'])==len(BINDINGS) and len(t['Commands'])==6 and len(t['Slots'])==4 and len(t['Equipment'])==1 and 1<=len(t['Resources'])<=32 and 1<=len(t['Layouts'])<=64 and len(t['Clips'])==6 and 1<=len(t['Keys'])<=32,'Field section count')
    params={}
    for i,p in enumerate(t['Parameters']):require(len(p)==2 and p[0]==i+1 and finite(p[1]),'Field parameter identity/value');params[PARAMETERS[i]]=p[1]
    require(1<=params['OwnerId']<=8192 and params['OwnerId']==int(params['OwnerId']),'Field owner ID')
    for name in ('ReferenceWidth','ReferenceHeight','PlatformWidth','PlatformHeight'):require(1<=params[name]<=1024 and params[name]==int(params[name]),'Field viewport')
    require(params['PauseColumns']==2 and params['LoopAround']==1 and 1<=params['ListRows']<=16 and params['ListRows']==int(params['ListRows']),'Field layout topology')
    for name in ('SlotPitch','ListPitch','StatPitch','MainLineHeight','NumberLineHeight','PauseColumnPitch','PauseRowPitch'):require(0<params[name]<=128,'Field positive layout pitch')
    require(0<params['CursorMoveSeconds']<=2 and 0<params['CursorFps']<=60 and -16<=params['NumberSpacing']<=16,'Field cursor/font tuning')
    for i in range(4):require(integer(int(params['CursorFrame'+str(i)]),0,2) and params['CursorFrame'+str(i)]==int(params['CursorFrame'+str(i)]),'Field cursor frame')
    for i in range(7):require(0<params['BoostWeight'+str(i)]<=100,'Field stat score')
    masks=[params[name] for name in ('OpenMask','ConfirmMask','CancelMask','ScopeMask')]
    require(all(v==int(v) and 0<v<=65535 and int(v)&(int(v)-1)==0 for v in masks) and len(set(masks))==4,'Field input mapping')
    for i,b in enumerate(t['Bindings']):
        require(len(b)==3 and b[0]==i+1,'Field binding identity')
        values=[string(o) for o in b[1:]]
        require(all(len(v)<=4096 for v in values),'Field binding text size')
        if 12<=i+1<=24:require(values[0]==values[1] and safe_path(values[0]),'Field path binding')
        else:require(all(not any(c in v for c in '\n\r[]{}') for v in values),'Field text controls')
    for i,c in enumerate(t['Commands']):require(len(c)==4 and c[0]==i+1 and string(c[1]) and string(c[2]) and c[3]==int(i==2),'Field command grants')
    slot_names=set()
    for i,s in enumerate(t['Slots']):require(len(s)==4 and s[0]==i+1 and safe_path(string(s[1])) and string(s[1]) not in slot_names and string(s[2]) and string(s[3]),'Field slot identity');slot_names.add(string(s[1]))
    for e in t['Equipment']:require(len(e)==10 and integer(e[0],0,15) and safe_path(string(e[1])) and integer(e[2],0,3) and all(integer(v,-65535,65535) for v in e[3:]),'Field equipment policy')
    resources=set()
    for i,r in enumerate(t['Resources']):
        path=string(r[1]);require(len(r)==10 and r[0]==i+1 and path.startswith('graphics/ui/equipment/') and path.endswith('.t3x') and safe_path(path) and path not in resources and r[2]==1 and all(integer(v,1,1024) for v in r[3:7]) and r[3]%r[5]==r[4]%r[6]==0 and type(r[7]) is bytes and len(r[7])==32 and any(r[7]) and integer(r[8],1,1024*1024) and integer(r[9]),'Field resource');resources.add(path)
    roles=set()
    for i,l in enumerate(t['Layouts']):
        require(len(l)==21 and l[0]==i+1 and integer(l[1],1,len(ROLES)) and l[1] not in roles and l[2]==NIL and integer(l[3],1,len(KINDS)) and (l[4]==NIL or integer(l[4],0,len(t['Resources'])-1)) and integer(l[5],0,1023) and l[6]==1 and all(finite(v) for v in l[7:17]) and l[11]>=0 and l[12]>=0 and all(0<=v<=1 for v in l[13:17]) and all(integer(v,0,64) for v in l[17:]),'Field layout');roles.add(l[1])
        if l[3] in (2,4):require(l[4]!=NIL and l[5]<t['Resources'][l[4]][5]*t['Resources'][l[4]][6],'Field texture layout')
        else:require(l[4]==NIL,'Field nontexture resource')
    required=set(range(1,len(ROLES)+1))-{22};require(required<=roles,'Field missing layout role')
    end=0
    for i,c in enumerate(t['Clips']):
        require(len(c)==4 and c[0]==i+1 and c[1]==end and integer(c[2],2,8) and finite(c[3]) and 0<c[3]<=2 and c[1]+c[2]<=len(t['Keys']),'Field animation clip');previous=-1
        for k in t['Keys'][c[1]:c[1]+c[2]]:require(len(k)==3 and all(finite(v) for v in k) and 0<=k[0]<=c[3] and k[0]>previous and abs(k[1])<=512 and 0<k[2]<=4,'Field animation key');previous=k[0]
        require(t['Keys'][c[1]][0]==0 and abs(t['Keys'][c[1]+c[2]-1][0]-c[3])<.0001,'Field animation boundary');end+=c[2]
    require(end==len(t['Keys']),'Field orphan animation keys')

def compile_pack(root=ROOT):
    root=Path(root);ir=load(root);receipt=verify_receipt(ir,root);blob=encode(lower(ir,receipt));parse_pack(blob);target=root/PACK;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob);print('Field equipment compile: %d checked bytes; six known commands / Equip only'%len(blob))
def stage_files(source):
    source=Path(source);ir=load();receipt=verify_receipt(ir,ROOT,source);blob=(source/'data/opening.encfield').read_bytes();require(blob==encode(lower(ir,receipt)),'Field staged pack differs from source');parse_pack(blob);return {Path('data/opening.encfield'):blob,**{Path(r['output']):(source/r['output']).read_bytes() for r in ir['resources']}}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('extract','assets','compile','verify'));p.add_argument('--tex3ds',type=Path);a=p.parse_args()
    if a.action=='extract':extract();print('Field equipment source extract: admitted')
    elif a.action=='assets':compile_assets(a.tex3ds);print('Field equipment genuine assets: admitted')
    elif a.action=='compile':compile_pack()
    else:
        ir=load();receipt=verify_receipt(ir);blob=(ROOT/PACK).read_bytes();require(blob==encode(lower(ir,receipt)),'Field generated pack differs');parse_pack(blob);print('Field equipment source/format verify: admitted')
if __name__=='__main__':main()
