#!/usr/bin/env python3
"""Pinned field item-use source IR and independent ENCIUSE1 resource.

Only the reviewed status-only consume action is admitted. JSON/reviews remain
offline; native gameplay consumes the checked binary. No generated C++ content.
"""
from __future__ import annotations
import argparse, csv, io, math, re, struct, sys, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, require, node, properties
from tools.drawer_program import canonical, digest, fields, read_json, write_json, safe_path

IR = 'content/native-item-use.json'
REVIEW = 'reports/item-use-source/source-review.json'
PACK = 'romfs/data/opening.encuse'
SCENE = 'Nodes/Ui/Inventory/InventoryUI.tscn'
ITEM = 'Data/Items/AsthmaSpray.yaml'
AILMENT = 'Data/StatusAilments/asthma.yaml'
SOUND = 'Audio/Sound effects/EB/eat.wav'
NAMES = ('Strings', 'Rules', 'Statuses', 'Targets', 'Locales', 'Layouts', 'Parameters', 'Sounds')
FORMATS = (None, '<10I', '<5I', '<I', '<5I', '<I8f', '<I4f', '<2I')
STRIDES = (1, 40, 20, 4, 20, 36, 20, 8)
HEADER = 192
SOUNDS = ('Open', 'Move', 'Confirm', 'Back', 'Restricted', 'Heal', 'Close')
LAYOUTS = ('Inventory', 'Action', 'Targets', 'Message', 'Description', 'TargetTitle', 'Divider')
PARAMETERS = ('Grid', 'GridOrigin', 'TargetOrigin', 'TextInset', 'LabelSize', 'CursorOffsets', 'ActionOrigin', 'SourceViewport', 'PlatformViewport', 'SubmenuPlacement', 'SubmenuPoint', 'CursorCenter', 'MessageAlignment', 'MessagePatch')

def table(ex, path):
    rows = {}
    for row in csv.DictReader(io.StringIO(ex.text(path))):
        require(row['key'] not in rows, 'Duplicate item-use translation key')
        rows[row['key']] = row
    return rows

def checked_body(text, start, end):
    require(text.count(start) == 1 and text.count(end) == 1, 'Item-use source method selector')
    return text[text.index(start):text.index(end)]

def build(root=ROOT):
    root = Path(root); ex = Extractor(root)
    item = ex.yaml(ITEM); ailment = ex.yaml(AILMENT)
    inventory = ex.text('Scripts/global/Inventory.gd')
    actor = ex.text('Scripts/global/Character.gd')
    pm = ex.text('Scripts/global/PartyMember.gd')
    action = ex.text('Nodes/Ui/Inventory/ActionSelect.gd')
    target = ex.text('Nodes/Ui/Inventory/TargetCharaSelect.gd')
    status = ex.text('Scripts/Main/Status.gd')
    ex.text('Scripts/global/Item.gd');inventory_ui=ex.text('Scripts/UI/Inventory/InventoryUI.gd')
    ex.text('Scripts/global/text_tools.gd'); ex.text('Nodes/Ui/Inventory/DialogBox.gd')
    require(item['actions'] == [{'name': 'INVENTORY_ACTION_USE', 'function': 'consume'}]
            and item['status_heals'] == ['asthma'] and item['transform'] == ''
            and item['slot'] == '' and item['HPrecover'] == item['PPrecover'] == 0
            and all(type(v) is int and v == 0 for v in item['boost'].values())
            and not item.get('reusable', False) and not item.get('target_all', False)
            and 'can_consume' not in item and 'can_use' not in item,
            'Unreviewed item-use effect/action/target scope')
    require(type(item['doses']) is int and 0 < item['doses'] <= 65535,
            'Item-use source initial dose bounds')
    require(ailment['healing'] == {'persistent': False}
            and ailment['can_get'] == {'ninten': True}, 'Unreviewed status persistence/owner policy')
    require('cant_receive_item' not in canonical(ailment), 'Unreviewed status item-receive blocker')
    consume = checked_body(inventory, 'func consume_item(', '#####################################################################\n######################### INVENTORY HOLDERS')
    snippets = ('var is_reusable = item_data.get("reusable", false)',
                'for status in status_heals:', 'if receiver.has_status(status):',
                'receiver.remove_status(status)', 'performed_actions[Item.ItemActions.HEAL_FAIL].append(status)',
                'receiver.apply_boosts(item_data["boost"], performed_actions)',
                'if !is_reusable:\n\t\treduce_or_drop_item(item)')
    require(all(s in consume for s in snippets)
            and consume.index('receiver.remove_status(status)') < consume.index('reduce_or_drop_item(item)'),
            'Item-use source heal/decrement ordering changed')
    reduce = checked_body(inventory, 'func reduce_or_drop_item(', 'func get_items(')
    require('if item.doses > 1:\n\t\titem.doses -= 1' in reduce
            and 'return drop_item(item)' in reduce and 'transform' not in reduce,
            'Item-use source final-dose deletion changed')
    require('get_combined_status_effect("cant_receive_item")' in actor
            and 'return has_status(Status.AILMENT_UNCONSCIOUS)' in actor
            and 'return item_data.get("can_use", globaldata.characters).has(get_name())' in pm,
            'Item-use source character admission changed')
    hp_refresh=checked_body(pm,'func _refresh_hp_from_status():','func _refresh_status_from_hp():')
    refresh=re.search(r'elif !is_unconscious\(\) and _hp == 0:\n\t\tset_hp\((\d+)\)',hp_refresh)
    require(refresh is not None and 'func remove_status(ailment: String):\n\t.remove_status(ailment)\n\t_refresh_hp_from_status()' in pm
            and 'HP: _hp = int(clamp(new_value, 0, get_max_hp()))' in actor
            and 'func set_hp(new_value: int):\n\tset_stat(HP, new_value)' in actor,
            'Item-use source remove_status/HP refresh clamp changed')
    require('Item.ItemActions.HEAL_FAIL:' in target
            and 'Status.get_status_message(cur_action[0], "heal_overworld_fail")' in target
            and 'if success:\n\t\taudioManager.play_sfx(load("res://' + SOUND + '"), "menu")' in target
            and 'emit_signal("back", true)' in target
            and 'emit_signal("back", false)' in target,
            'Item-use source feedback/back boundary changed')
    require('for party_mem in global.get_party_in_natural_order():' in action
            and 'title = action_name + "_TARGET"' in action
            and '(!character.is_unconscious() or item_action.get("target_unconscious", false))' in action,
            'Item-use source ordered target selection changed')
    require('if get_data()["healing"].get("passive_heal", false):' in status
            and 'dict["passive_healing_turns"] = battle_turns' in status
            and 'new_status.battle_turns = sts.get("passive_healing_turns", 0)' in actor,
            'Item-use status saved default changed')
    initial = ex.yaml('Data/save_new_game.yaml')
    require(initial['party'] == ['ninten'], 'Item-use active target scope changed')
    native = read_json(root / 'content/native-items.json')
    require([(d['id'], d['source']) for d in native['definitions']]
            == [(1, 'BaseballCap'), (2, 'AsthmaSpray')], 'Item-use checked definition identity')
    definition = next(i for i,d in enumerate(native['definitions']) if d['source'] == 'AsthmaSpray')
    menus = table(ex, 'Translations/TranslatedText/menus - sheet.csv')
    messages = ailment['messages']; heal = messages['heal_overworld']; fail = messages['heal_overworld_fail']
    locales = [dict(code=locale, action=menus[item['actions'][0]['name']][column],
                    title=menus[item['actions'][0]['name'] + '_TARGET'][column],
                    heal=menus[heal][column], fail=menus[fail][column])
               for locale,column in (('en','en'),('zh_Hans_CN','zh_CN'))]
    scene = ex.text(SCENE); root_node=node(scene,'.'); panel=node(scene,'Inventory')
    grid = node(scene,'Inventory/CenterContainer/Items/GridContainer')
    first = node(scene,'Inventory/CenterContainer/Items/GridContainer/Item1')
    second = node(scene,'Inventory/CenterContainer/Items/GridContainer/Item2')
    third = node(scene,'Inventory/CenterContainer/Items/GridContainer/Item3')
    target_box=node(scene,'ActionSelect/TargetCharaSelect')
    target_title=node(scene,'ActionSelect/TargetCharaSelect/ToWhomLabel')
    target_list=node(scene,'ActionSelect/TargetCharaSelect/MarginContainer/VBoxContainer')
    target_next=node(scene,'ActionSelect/TargetCharaSelect/MarginContainer/VBoxContainer/CharaLabel2')
    text_label=node(scene,'Bottom/DialogBox/TextLabel')
    message_box=node(scene,'Bottom/DialogBox')
    description=node(scene,'Inventory/DescriptionPanel')
    action_box=node(scene,'ActionSelect'); action_list=node(scene,'ActionSelect/MarginContainer/VBoxContainer')
    item_cursor=node(scene,'Inventory/Arrow');target_cursor=node(scene,'ActionSelect/TargetCharaSelect/arrow')
    submenu_point=node(scene,'Inventory/Arrow/Position2D')
    center=node(scene,'Inventory/CenterContainer');items_center=node(scene,'Inventory/CenterContainer/Items')
    divider=node(scene,'Inventory/CenterContainer/Divider')
    style_body=re.search(r'^\[sub_resource type="StyleBoxTexture" id=17\]\n(.*?)(?=^\[)',scene,re.M|re.S)[1]
    require('region_rect = Rect2( 0, 0, 24, 24 )' in style_body,'Item-use action panel source region changed')
    style=properties(style_body.replace('region_rect = Rect2( 0, 0, 24, 24 )\n',''))
    require(root_node['margin_left']==31 and root_node['margin_right']==288
            and root_node['margin_bottom']==132 and grid['columns']==2
            and first['rect_min_size']==[100,12], 'Item-use original menu geometry changed')
    # 1:1 native viewport centering is an explicit offline adapter operation.
    dx=(400-320)/2;dy=(240-180)/2;x=root_node['margin_left']+dx;y=dy
    panel_width=root_node['margin_right']-root_node['margin_left']
    layouts = [dict(role='Inventory',rect=[x,y+panel['margin_top'],panel_width,root_node['margin_bottom']-panel['margin_top']]),
               dict(role='Action',rect=[x+action_box['margin_left'],y+action_box['margin_top'],
                                        action_list['margin_right']+style['margin_right'],first['rect_min_size'][1]+style['margin_top']+style['margin_bottom']]),
               dict(role='Targets',rect=[x+action_box['margin_left']+target_box['margin_left'],y+action_box['margin_top'],
                                         target_box['margin_right']-target_box['margin_left'],target_box['margin_bottom']]),
               dict(role='Message',rect=[x,y+root_node['margin_bottom'],panel_width,48]),
               dict(role='Description',rect=[x,y+panel['margin_top']+description['margin_top'],
                                             panel_width+description['margin_right'],description['margin_bottom']-description['margin_top']]),
               dict(role='TargetTitle',rect=[x+action_box['margin_left']+target_box['margin_left']+target_title['margin_left'],y+action_box['margin_top']+target_title['margin_top'],target_title['margin_right']-target_title['margin_left'],target_title['margin_bottom']-target_title['margin_top']]),
               dict(role='Divider',rect=[x+divider['margin_left'],y+panel['margin_top']+center['margin_top']+divider['margin_top'],divider['margin_right']-divider['margin_left'],divider['margin_bottom']-divider['margin_top']])]
    for l in layouts:l['color']=divider['color'] if l['role']=='Divider' else target_title['custom_colors/font_color'] if l['role']=='TargetTitle' else [1,1,1,1]
    placement=re.search(r'var side := ([-\d.]+) if _get_selected_pos_x\(\) == 1 else ([-\d.]+)',inventory_ui)
    pitch=re.search(r'submenu_position.x = submenu_position.x \+ (\d+(?:\.\d+)?) \* side',inventory_ui)
    ceiling=re.search(r'const MAX_SUBMENU_POSITION := (\d+(?:\.\d+)?)',inventory_ui)
    require(placement is not None and pitch is not None and ceiling is not None
            and 'var submenu_position = $Inventory/Arrow/Position2D.global_position' in inventory_ui
            and 'submenu_position.y = min(MAX_SUBMENU_POSITION, submenu_position.y)' in inventory_ui,
            'Item-use source dynamic submenu anchor changed')
    cursor_size=ex.png_size('Graphics/UI/Inventory/cursor.png');require(cursor_size==[24,8], 'Item-use source cursor extent changed')
    parameters = dict(Grid=[grid['columns'],16/grid['columns'],second['margin_left'],third['margin_top']],
                      GridOrigin=[items_center['margin_left']+grid['margin_left'],center['margin_top']+items_center['margin_top']+grid['margin_top'],0,0],TargetOrigin=[target_list['margin_left'],target_list['margin_top'],target_next['margin_top'],0],
                      TextInset=[text_label['margin_left'],text_label['margin_top'],-text_label['margin_right'],-text_label['margin_bottom']],
                      LabelSize=[*first['rect_min_size'],0,0],CursorOffsets=[*item_cursor['cursor_offset'],*target_cursor['cursor_offset']],
                      ActionOrigin=[action_list['margin_left'],action_list['margin_top'],0,0],
                      SourceViewport=[320,180,0,0],PlatformViewport=[400,240,0,0],
                      SubmenuPlacement=[float(pitch[1]),float(placement[2]),float(placement[1]),float(ceiling[1])],
                      SubmenuPoint=[*submenu_point['position'],root_node['margin_left'],root_node.get('margin_top',0)],
                      CursorCenter=[-cursor_size[0]/3/6,cursor_size[1]/2,0,0],
                      MessageAlignment=[text_label['align'],text_label['valign'],int(text_label['clip_text']),int(text_label.get('autowrap',False))],
                      MessagePatch=[message_box['patch_margin_'+side] for side in ('left','top','right','bottom')])
    manager=ex.text('Scripts/global/audioManager.gd');cursor=ex.text('Scripts/UI/cursor.gd')
    audio=dict(re.findall(r'"([a-z0-9_]+)": load\("res://([^"]+)"\)',manager))
    require(all(s in cursor for s in ("play_sfx('cursor1')","play_sfx('cursor2')","play_sfx('back')")),
            'Item-use source cursor feedback changed')
    sounds=[dict(event=event,name=name,source=SOUND if event=='Heal' else audio[name])
            for event,name in zip(SOUNDS,('menu_open','cursor1','cursor2','back','restricted','','menu_close'))]
    for s in sounds:ex.data(s['source']);ex.data(s['source']+'.import')
    # Validate retained source art/font/audio, but use the already checked field
    # equipment/description resources rather than duplicating their conversions.
    for source in ('Graphics/UI/Overworld/flavours/defaultbox.png',
                   'Graphics/UI/Overworld/flavours/defaultbox_title.png',
                   'Graphics/UI/Inventory/cursor.png', 'Fonts/EBMain_la.tres',
                   'Fonts/BottleRocket.tres', SOUND, 'LICENSE'):
        ex.data(source)
    result=dict(schema=1,kind='encore.native-item-use.source-ir',commit=PIN,
                scope='Field status-only consume: selected UID, target selection, cure/no-effect, dose/drop, original feedback and return; other item actions remain unavailable.',
                sources=dict(sorted(ex.sources.items())),
                dependencies={'content/native-items.json':digest(root/'content/native-items.json')},
                rules=[dict(definition=definition,source='AsthmaSpray',status=item['status_heals'][0],max_doses=item['doses'],
                            reusable=item.get('reusable',False),heal_message=heal,fail_message=fail,success_sound=SOUND,heal_hp=0,heal_pp=0)],
                statuses=[dict(id=item['status_heals'][0],persistent=ailment['healing']['persistent'],
                               passive_healing=ailment['healing'].get('passive_heal',False),default_saved_turns=0,refresh_hp_value=int(refresh[1]))],
                targets=initial['party'],locales=locales,layouts=layouts,parameters=parameters,sounds=sounds)
    lower(result);return result

def load(root=ROOT):
    root=Path(root);ir=read_json(root/IR)
    require(canonical(ir)==canonical(build(root)), 'Stale/unreviewed item-use IR')
    review=read_json(root/REVIEW)
    fields(review,('schema','commit','ir_sha256','sources','semantics','unverified'),'Item-use review')
    require(review['schema']==1 and review['commit']==PIN and review['sources']==ir['sources']
            and review['ir_sha256']==digest(root/IR), 'Item-use review binding changed')
    return ir

def lower(ir):
    fields(ir,('schema','kind','commit','scope','sources','dependencies','rules','statuses','targets','locales','layouts','parameters','sounds'),'Item-use IR')
    require(ir['schema']==1 and ir['kind']=='encore.native-item-use.source-ir' and ir['commit']==PIN, 'Item-use IR version/pin')
    pool=bytearray(b'\0');strings={'':0}
    def string(value):
        require(isinstance(value,str) and '\0' not in value and len(value.encode('utf-8'))<=4096,'Item-use string')
        if value not in strings:strings[value]=len(pool);pool.extend(value.encode('utf-8')+b'\0')
        return strings[value]
    t={name:[] for name in NAMES}
    for r in ir['rules']:
        fields(r,('definition','source','status','max_doses','reusable','heal_message','fail_message','success_sound','heal_hp','heal_pp'),'Item-use rule')
        t['Rules'].append([r['definition'],string(r['source']),string(r['status']),r['max_doses'],int(r['reusable']),
                           string(r['heal_message']),string(r['fail_message']),string(r['success_sound']),r['heal_hp'],r['heal_pp']])
    for s in ir['statuses']:
        fields(s,('id','persistent','passive_healing','default_saved_turns','refresh_hp_value'),'Item-use status')
        t['Statuses'].append([string(s['id']),int(s['persistent']),int(s['passive_healing']),s['default_saved_turns'],s['refresh_hp_value']])
    t['Targets']=[[string(s)] for s in ir['targets']]
    for l in ir['locales']:
        fields(l,('code','action','title','heal','fail'),'Item-use locale')
        t['Locales'].append([string(l[k]) for k in ('code','action','title','heal','fail')])
    require([l['role'] for l in ir['layouts']]==list(LAYOUTS),'Item-use layout roles')
    for i,l in enumerate(ir['layouts']):
        fields(l,('role','rect','color'),'Item-use layout');t['Layouts'].append([i+1,*l['rect'],*l['color']])
    fields(ir['parameters'],PARAMETERS,'Item-use parameters')
    t['Parameters']=[[i+1,*ir['parameters'][key]] for i,key in enumerate(PARAMETERS)]
    require([s['event'] for s in ir['sounds']]==list(SOUNDS),'Item-use sound events')
    for i,s in enumerate(ir['sounds']):
        fields(s,('event','name','source'),'Item-use sound');t['Sounds'].append([i+1,string(s['source'])])
    t['Strings']=bytes(pool);validate(t);return t

def validate(t):
    require(set(t)==set(NAMES),'Item-use sections');pool=t['Strings']
    require(isinstance(pool,bytes) and 1<=len(pool)<=65536 and pool[0]==pool[-1]==0,'Item-use string pool')
    starts={};i=0
    while i<len(pool):
        end=pool.index(0,i);text=pool[i:end].decode('utf-8');require(not any(ord(c)<32 or ord(c)==127 or c in '[]' for c in text),'Item-use text controls');starts[i]=text;i=end+1
    def string(i):require(type(i) is int and i in starts,'Item-use string reference');return starts[i]
    require(len(t['Rules'])==1 and 1<=len(t['Statuses'])<=16 and 1<=len(t['Targets'])<=4
            and 1<=len(t['Locales'])<=16 and len(t['Layouts'])==len(LAYOUTS) and len(t['Parameters'])==len(PARAMETERS)
            and len(t['Sounds'])==len(SOUNDS),'Item-use section counts')
    statuses=set()
    for r in t['Statuses']:
        require(len(r)==5 and string(r[0]) and r[0] not in statuses and r[1] in (0,1) and r[2]==r[3]==0
                and type(r[4]) is int and 0<r[4]<=2147483647,'Item-use status bounds');statuses.add(r[0])
    for r in t['Rules']:
        require(len(r)==10 and type(r[0]) is int and r[0]>=0 and string(r[1]) and r[2] in statuses
                and type(r[3]) is int and 1<=r[3]<=65535 and r[4] in (0,1)
                and string(r[5])!=string(r[6]) and safe_path(string(r[7])) and r[8]==r[9]==0,'Item-use rule bounds')
    require(len({string(r[0]) for r in t['Targets']})==len(t['Targets']),'Item-use duplicate targets')
    require(len({string(r[0]) for r in t['Locales']})==len(t['Locales']),'Item-use duplicate locales')
    for r in t['Locales']:
        require(len(r)==5 and all(string(v) for v in r),'Item-use locale empty')
        for value in r[3:]:
            text=string(value);require(text.count('{target}')==1 and not any(c in text.replace('{target}','') for c in '{}%'),'Item-use target template')
    for name in ('Layouts','Parameters'):
        for i,r in enumerate(t[name]):
            require(len(r)==(9 if name=='Layouts' else 5) and r[0]==i+1 and all(type(v) in (int,float) and math.isfinite(v) and -8192<=v<=8192 for v in r[1:]),'Item-use geometry')
            if name=='Layouts':require(r[1]>=0 and r[2]>=0 and r[3]>0 and r[4]>0 and all(0<=v<=1 for v in r[5:]),'Item-use empty rectangle/color')
    for i,r in enumerate(t['Sounds']):require(len(r)==2 and r[0]==i+1 and safe_path(string(r[1])),'Item-use sound path')
    require(t['Sounds'][SOUNDS.index('Heal')][1]==t['Rules'][0][7],'Item-use heal sound binding')

def encode(t):
    validate(t);data=bytearray(HEADER)
    for i,name in enumerate(NAMES):
        block=t[name] if i==0 else b''.join(struct.pack(FORMATS[i],*r) for r in t[name])
        while len(data)%4:data.append(0)
        struct.pack_into('<HHIII',data,64+i*16,i+1,STRIDES[i],len(data),len(block)//STRIDES[i],len(block));data.extend(block)
    struct.pack_into('<8s6I20s12x',data,0,b'ENCIUSE1',1,len(data),0,len(NAMES),1,1,bytes.fromhex(PIN))
    struct.pack_into('<I',data,16,zlib.crc32(data));return bytes(data)

def parse_pack(blob):
    require(HEADER<=len(blob)<=1024*1024,'Item-use size');header=struct.unpack_from('<8s6I20s',blob)
    require(header==(b'ENCIUSE1',1,len(blob),header[3],len(NAMES),1,1,bytes.fromhex(PIN)) and not any(blob[52:64]),'Item-use header')
    copy=bytearray(blob);struct.pack_into('<I',copy,16,0);require(zlib.crc32(copy)==header[3],'Item-use CRC')
    t={};end=HEADER
    for i,name in enumerate(NAMES):
        kind,stride,off,count,amount=struct.unpack_from('<HHIII',blob,64+i*16)
        require(kind==i+1 and stride==STRIDES[i] and count>0 and amount==count*stride
                and off==(end+3)//4*4 and off+amount<=len(blob) and not any(blob[end:off]),'Item-use directory/span')
        block=blob[off:off+amount];end=off+amount;t[name]=bytes(block) if i==0 else list(struct.iter_unpack(FORMATS[i],block))
    require(end==len(blob),'Item-use trailing bytes');validate(t);return t

def glyph_request(ir):
    return {l['code']:''.join(l[k].replace('{target}','') for k in ('action','title','heal','fail'))
            for l in ir['locales']}

def extract(root=ROOT):
    root=Path(root);ir=build(root);write_json(root/IR,ir)
    write_json(root/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),sources=ir['sources'],
        semantics=['UID selected from actual inventory, source action then natural-order target selection.',
                   'Remove the actual asthma status if present; absent status reports HEAL_FAIL and still consumes a dose.',
                   'Non-reusable source reduce_or_drop_item decrements while doses>1 and drops the final item; no implicit transform or RNG.',
                   'Success alone emits original eat.wav; source heal/fail feedback substitutes the actual target nickname.',
                   'Asthma has no passive-heal counter in source serialization; saved missing counter defaults to zero.',
                   'PartyMember.remove_status calls HP refresh; curing at HP zero without unconscious uses the source recovery value clamped to the actual derived MAXHP, while HEAL_FAIL does not refresh HP.'],
        unverified=['Manual positive/negative tests not run.', '3DS cross-build/packaging and emulator/hardware UI/audio have not been verified for this slice.']))
    return ir

def compile_pack(root=ROOT):
    root=Path(root);blob=encode(lower(load(root)));parse_pack(blob);target=root/PACK;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
    print('Field item consume: %d bytes, checked source status/dose/feedback only'%len(blob));return blob

def stage_files(source_root):
    source_root=Path(source_root);expected=encode(lower(load(ROOT)))
    relative=Path(PACK).relative_to('romfs');actual=(source_root/relative).read_bytes()
    require(actual==expected,'Item-use staging resource differs from checked source');parse_pack(actual)
    return {relative:actual}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
    if a.action=='extract':extract()
    elif a.action=='compile':compile_pack()
    else:
        expected=encode(lower(load()));actual=(ROOT/PACK).read_bytes();require(actual==expected,'Item-use pack differs from source');parse_pack(actual);print('Field item consume: source/format admitted')
if __name__=='__main__':main()
