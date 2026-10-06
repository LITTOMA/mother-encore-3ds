#!/usr/bin/env python3
"""Pinned ordinary Minnie Storage frontend and genuine tex3ds producer.

Only the already admitted singleton Ninten inventory definitions are usable.
JSON is offline IR; the console receives ENCSTG01 and original textures.
"""
from __future__ import annotations
import argparse,csv,io,json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require,node
from tools.drawer_program import canonical,digest,fields,read_json,write_json,safe_path
IR='content/native-storage.json'
RECIPE='content/storage-assets.json'
REVIEW='reports/storage-source/source-review.json'
RECEIPT='content/asset-receipts/graphics/ui/storage/source.json'
SCENE='Nodes/Ui/Storage.tscn'
SCRIPT='Scripts/UI/Storage.gd'
ROLES=['Container','Panel','OnHandList','StoredList','Title','Counter','Portrait','ItemLabel','Cursor','Description','Prompt','Yes','No','Separator','Scrollbar','Equipped','ItemIcon','IconFrame','DescriptionText','ScrollThumb','Highlight','PortraitEquipped','PortraitSuitable','PortraitBetter','PortraitLower','PortraitFull','QuestionCursor']
PARAMETERS=['StorageCapacity','InventoryCapacity','Rows','RowPitch','WarnSeconds','EquippedScoreDelta','LoopAround','CursorSeconds','ReferenceWidth','ReferenceHeight','PlatformWidth','PlatformHeight','CursorX','CursorY','CursorWidth','CursorHeight','CursorFps','CursorFrame0','CursorFrame1','CursorFrame2','CursorFrame3','CursorMoveSeconds','BoostWeight0','BoostWeight1','BoostWeight2','BoostWeight3','BoostWeight4','BoostWeight5','BoostWeight6','TextLineHeight','CounterCharacterSpacing']
BINDINGS=['TitleEn','TitleZh','UnequipEn','UnequipZh','EquipEn','EquipZh','StorageFullEn','StorageFullZh','InventoryFullEn','InventoryFullZh','YesEn','YesZh','NoEn','NoZh','Owner','MoveSound','ConfirmSound','RestrictedSound','ClearSound','EquipSound','CounterPattern','CounterFont','MainFont']
STATS=['maxhp','maxpp','offense','defense','speed','iq','guts']
ASSETS=[('panel','Graphics/UI/Overworld/flavours/defaultempty.png',None,[1,1]),
        ('inside','Graphics/UI/Overworld/flavours/defaultbox_inside.png',None,[1,1]),
        ('description','Graphics/UI/Overworld/flavours/defaultbox.png',None,[1,1]),
        ('counter','Graphics/UI/Overworld/flavours/defaultbox_inside_light.png',None,[1,1]),
        ('ninten','Graphics/UI/Inventory/characters/ninten.png',None,[1,1]),
        ('cursor','Graphics/UI/Inventory/cursor.png',None,[3,1]),
        ('equipped','Graphics/UI/Inventory/modifiers.png',[2,19,8,7],[1,1]),
        ('separator','Graphics/UI/dashedline.png',None,[1,1]),
        ('scroll-bg','Graphics/UI/Overworld/flavours/defaultbox_inside_scrollbar.png',None,[1,1]),
        ('scroll-thumb','Graphics/UI/Overworld/flavours/defaultscroll.png',None,[1,1]),
        ('icon-frame','Graphics/UI/Inventory/item_icon.png',None,[1,1]),
        ('portrait-equipped','Graphics/UI/Inventory/modifiers.png',[15,10,9,7],[1,1]),
        ('portrait-suitable','Graphics/UI/Inventory/modifiers.png',[0,10,13,7],[1,1]),
        ('portrait-better','Graphics/UI/Inventory/modifiers2.png',[0,0,11,7],[1,1]),
        ('portrait-lower','Graphics/UI/Inventory/modifiers2.png',[0,7,11,7],[1,1]),
        ('portrait-full','Graphics/UI/Inventory/modifiers.png',[15,19,9,7],[1,1])]

def table(ex,path):
    out={}
    for row in csv.DictReader(io.StringIO(ex.text(path))):
        require(row['key'] not in out,'Duplicate Storage translation key');out[row['key']]=row
    return out

def rect(props):
    x,y=props.get('margin_left',0),props.get('margin_top',0)
    return [x,y,props.get('margin_right',0)-x,props.get('margin_bottom',0)-y]

def build(root=ROOT):
    ex=Extractor(root);script=ex.text(SCRIPT);scene=ex.text(SCENE)
    inventory=ex.text('Scripts/global/Inventory.gd');itemscript=ex.text('Scripts/global/Item.gd')
    listscript=ex.text('Scripts/UI/Reusables/ItemListMenu.gd')
    yesno=ex.text('Scripts/UI/Reusables/DescriptionWithYesNo.gd')
    text=ex.text('Scripts/global/text_tools.gd');audio=ex.text('Scripts/global/audioManager.gd')
    for p in ('Nodes/Ui/DescriptionWithYesNo.tscn','Nodes/Ui/Description.tscn','Nodes/Ui/InventorySelect.tscn',
              'Nodes/Ui/Inventory/InventorySelect.gd','Nodes/Ui/Inventory/portrait.tscn','Nodes/Ui/Inventory/portrait.gd',
              'Nodes/Ui/HighlightLabel.tscn','Nodes/Ui/HighlightLabel.gd','Nodes/Ui/arrow.tscn',
              'Nodes/Ui/Reusables/Scrollbar.tscn','Scripts/UI/Reusables/Description.gd','Scripts/UI/Reusables/DescriptionModal.gd',
              'Scripts/UI/cursor.gd','Scripts/global/PartyMember.gd','Scripts/global/Character.gd','Scripts/global/globalData.gd','LICENSE'):
        ex.text(p)
    save=ex.yaml('Data/save_new_game.yaml')
    require(save['party']==['ninten'] and save.get('storage',[])==[], 'Storage singleton/empty initial storage changed')
    require('var god_mode := false' in script and '_storage_holder.inv.sort_auto()' in script,'Storage normal mode/sort entry changed')
    require('const _MAX_STORAGE_SIZE := 64' in inventory and 'const _MAX_INVENTORY_SIZE := 16' in inventory,'Storage capacity changed')
    require('drop_item(item)\n\t\t\tif target_inv.get_type() != InvType.STORAGE_GOD:\n\t\t\t\ttarget_inv.add_item(item)' in inventory and 'item.equipped = false' in inventory,'Storage transfer identity/unequip changed')
    require('var item := Item.new(item_name)' not in inventory[inventory.index('func transfer_item('):inventory.index('# Transform an item')],'Storage transfer allocates a new identity')
    require('return _has_function("equip")' in itemscript and 'return tr(a.get_data()["sorting_name"]) < tr(b.get_data()["sorting_name"])' in inventory,'Storage equipment/sort comparator changed')
    for snippet in ('_ask_user(question_str, item_id, "_unequip_and_store", [_current_char, item_pos])',
                    '_withdraw_item(_current_char, item_pos)','_ask_user(question_str, item_id, "_equip_after_withdrawal", [ _current_char, item_uid])',
                    '_current_char.can_equip_item(item)','_storage_holder.inv.sort_auto()',
                    '_switch_to_panel(_current_panel_is_storage, true)','_desc_panel.warn(question_str, 1)',
                    'character.equip_item(character.inv.get_item_from_uid(item_uid))'):
        require(snippet in script,'Storage reviewed control flow changed: '+snippet)
    require(script.index('_withdraw_item(_current_char, item_pos)',script.index('func _on_selected_storage('))<script.index('_ask_user(question_str, item_id, "_equip_after_withdrawal"'),'Storage withdrawal must precede equip question')
    require('if answer_is_yes:' in yesno and '_on_answer(false)' in yesno and 'emit_signal("closed")' in yesno,'Storage prompt cancel/callback changed')
    require('const LINES_PER_PAGE = 6' in listscript and 'if loop_around:' in listscript,'Storage list page contract changed')
    require('get_item_or_skill_articles(item_or_skill)' in text and 'article_str.split(",")' in text,'Storage item/article interpolation changed')
    menus=table(ex,'Translations/TranslatedText/menus - sheet.csv');items=table(ex,'Translations/TranslatedText/items - sheet.csv')
    native_items=read_json(Path(root)/'content/native-items.json');defs=native_items['definitions']
    require([(d['id'],d['source']) for d in defs]==[(1,'BaseballCap'),(2,'AsthmaSpray')],'Storage unsupported item definitions')
    from tools.item_use import load as load_item_use
    item_use=load_item_use(root)
    reduction=re.findall(r'func reduce_or_drop_item\(item: Item\) -> bool:\n\tif item.doses > (\d+):\n\t\titem.doses -= (\d+)',inventory)
    require(len(reduction)==1 and reduction[0][0]==reduction[0][1]=='1','Storage dose reduction source changed')
    policies=[];equipment=[];translations={};articles=[]
    for d in defs:
        doc=ex.yaml('Data/Items/'+d['source']+'.yaml')
        if d['source']=='AsthmaSpray':
            from tools.drawer_item import validate_item
            validate_item(doc)
        require(doc['keyitem'] is False and doc['transform']=='' and not doc.get('battle_action'),'Unsupported Storage item policy')
        boosts=[doc['boost'][stat] for stat in STATS]
        require(all(type(v) is int and -65535<=v<=65535 for v in boosts),'Storage boost schema')
        total=sum(v*c for v,c in zip(boosts,[2,2,3,2,1,1,1]))
        if doc.get('status_heals'):score=800000
        elif doc['actions']==[{'function':'equip'}]:
            require(doc['can_use']==['ninten'] and doc['slot']=='other','Storage equipment ownership/slot changed')
            score=200000+10000+total
        else:raise ValueError('Unreviewed Storage sorting category')
        doses=doc.get('doses',1);minimum=doses
        use_rules=[r for r in item_use['rules'] if r['source']==d['source']]
        if use_rules:
            require(len(use_rules)==1 and use_rules[0]['max_doses']==doses and not use_rules[0]['reusable'],'Storage consumable source binding changed')
            minimum=int(reduction[0][0])
        policies.append(dict(definition_id=d['id'],source_item=d['source'],doses=doses,min_doses=minimum,max_count=1,sort_rank_en=score,sort_rank_zh=score))
        equipment.append(dict(definition_id=d['id'],boosts=boosts))
        translations[d['source']]={lang:{k:items[doc[k]][col] for k in ('name','sorting_name','article')} for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]}
        articles.append({lang:translations[d['source']][lang]['article'].split(',')[1] for lang in ('en','zh_Hans_CN')})
    require(articles[0]==articles[1],'Storage prompts require per-item article expansion')
    binding={}
    pairs=[('Title','MENU_TITLE_STORAGE'),('Unequip','TRANSACTION_ASK_UNEQUIP'),('Equip','SHOP_ASK_EQUIP'),('StorageFull','TRANSACTION_STORAGE_FULL'),('InventoryFull','TRANSACTION_FULL'),('Yes','MENU_YES'),('No','MENU_NO')]
    for prefix,key in pairs:
        for lang,col,suffix in [('en','en','En'),('zh_Hans_CN','zh_CN','Zh')]:
            value=menus[key][col].replace('{i1}',articles[0][lang])
            require(re.findall(r'\{[^}]*\}',value) in ([],['{item}']) and '[' not in value and '\0' not in value,'Unreviewed Storage text controls')
            binding[prefix+suffix]=value
    binding['Owner']='ninten';binding['CounterPattern']='%s\u202f/\u202f%s';binding['CounterFont']='Fonts/BottleRocket.tres';binding['MainFont']='Fonts/EBMain_la.tres'
    for p in ('Fonts/BottleRocket.tres','Fonts/BottleRocket.ttf','Fonts/BottleRocket_ja.ttf','Fonts/BottleRocket_ko.ttf','Fonts/BottleRocket_zh_cn.otf','Fonts/EBMain_la.tres','Fonts/EBMain.ttf'):ex.data(p)
    for key,event in [('MoveSound','cursor1'),('ConfirmSound','cursor2'),('RestrictedSound','restricted'),('ClearSound','clear'),('EquipSound','equip')]:
        m=re.findall(r'"'+event+r'": load\("res://([^"\n]+)"\)',audio)
        require(len(m)==1,'Ambiguous Storage sound');binding[key]=m[0];ex.data(m[0]);ex.data(m[0]+'.import')
    # Storage's description cursor and list both use the source back feedback.
    # Reuse the separately checked Items Close binding, admitting that equality
    # here rather than embedding a path or inventing a Storage-only sound ID.
    back=re.findall(r'"back": load\("res://([^"\n]+)"\)',audio)
    close=[sound for sound in native_items['sounds'] if sound['event']=='Close']
    require(len(back)==len(close)==1 and back[0]==close[0]['path'],'Storage back sound differs from checked Items Close binding')
    ex.data(back[0]);ex.data(back[0]+'.import')
    require('play_sfx(\'back\')' in ex.text('Scripts/UI/cursor.gd'),'Storage cancel feedback source changed')
    resources=[]
    for i,(name,path,crop,grid) in enumerate(ASSETS):
        size=ex.png_size(path);ex.data(path+'.import')
        resources.append(dict(id=i+1,name=name,source=path,size=size,crop=crop,grid=grid,output='graphics/ui/storage/'+name+'.t3x'))
    counter_spacing=re.findall(r'^extra_spacing_char = (-?\d+)$',ex.text('Fonts/BottleRocket.tres'),re.M)
    require(len(counter_spacing)==1,'Storage counter character spacing')
    params=dict(zip(PARAMETERS,[64,16,6,14,1,-100000,1,.2,320,180,400,240,-5,2,8,8,5,0,1,2,1,.1,2,2,3,2,1,1,1,native_items['parameters']['LabelSize'][1],int(counter_spacing[0])]))
    require('const TWEEN_LENGTH := 0.1' in ex.text('Scripts/UI/cursor.gd'),'Storage cursor movement timing')
    require(node(scene,'StorageBox/ItemsOnHand')['loop_around'] and node(scene,'StorageBox/ItemsStored')['loop_around'],'Storage list looping changed')
    require(rect(node(scene,'StorageBox'))==[28,4,264,125] and rect(node(scene,'DescContainer'))==[28,132,264,48],'Storage reference layout changed')
    layouts=[]
    def layout(role,kind,r,resource=None,patch=None,frame=0):
        layouts.append(dict(id=len(layouts)+1,role=role,parent=None,kind=kind,resource=resource,frame=frame,flags=0,anchor=[0,0],rect=r,color=[1,1,1,1],patch=patch or [0,0,0,0]))
    layout('Panel','NinePatch',[28,4,264,126],0,[8]*4)
    layout('OnHandList','NinePatch',[28,29,132,101],1,[9]*4)
    layout('StoredList','NinePatch',[160,29,132,101],1,[9]*4)
    layout('Panel','NinePatch',[162,12,36,13],3,[2]*4)
    layout('Panel','NinePatch',[194,4,118,28],1,[6,5,6,6])
    layout('Title','Text',[202,12,103,12]);layout('Counter','Text',[164,11,32,11])
    layout('Portrait','Sprite',[34,4,21,21],4)
    layout('Description','NinePatch',[28,132,264,48],2,[9]*4)
    layout('Prompt','Text',[66,142,176,28]);layout('Yes','Text',[267,142,24,13]);layout('No','Text',[267,156,24,13])
    layout('ItemLabel','Text',[45,37,97,13]);layout('ItemLabel','Text',[177,37,97,13])
    layout('Cursor','Sprite',[45-5-8/6-4,37+2,8,8],5);layout('Cursor','Sprite',[177-5-8/6-4,37+2,8,8],5)
    layout('Scrollbar','NinePatch',[147,44,4,70],8,[2]*4);layout('Scrollbar','NinePatch',[279,44,4,70],8,[2]*4)
    layout('Equipped','Sprite',[39,40,8,7],6)
    layout('Separator','Sprite',[159,5,2,123],7)
    layout('ItemIcon','Container',[40,145,20,20]);layout('IconFrame','NinePatch',[38,143,24,24],10,[2]*4)
    layout('DescriptionText','Text',[66,144,213,28])
    layout('ScrollThumb','NinePatch',[146,44,6,12],9,[2]*4);layout('ScrollThumb','NinePatch',[278,44,6,12],9,[2]*4)
    layout('Highlight','Rectangle',[45,37,97,13]);layout('Highlight','Rectangle',[177,37,97,13])
    layout('PortraitEquipped','Sprite',[40,5,9,7],11);layout('PortraitSuitable','Sprite',[38,25,13,7],12)
    layout('PortraitBetter','Sprite',[39,5,11,7],13);layout('PortraitLower','Sprite',[39,5,11,7],14);layout('PortraitFull','Sprite',[40,25,9,7],15)
    # Source cursor.gd centers at labelGlobal+offset+(-width/6,height/2).
    # Store top-left for the checked 8x8 source frame; second row adds14px.
    layout('QuestionCursor','Sprite',[267-4-8/6-4,142+1,8,8],5)
    ui=ex.text('Scripts/global/uiManager.gd')
    flavors=re.findall(r'(\[[^\n]+\]),\s*# Plain',ui)
    require(len(flavors)==1,'Storage plain flavor');palette=json.loads(flavors[0]);require(len(palette)==8,'Storage flavor palette')
    for l in layouts:
        if l['role']=='Highlight':l['color']=[int(palette[4][p:p+2],16)/255 for p in (0,2,4)]+[1]
        if l['role'] in ('ItemLabel','Cursor','Scrollbar','ScrollThumb','Highlight','Equipped'):
            # Parent provides checked pane ownership. Rectangles remain absolute
            # source viewport coordinates; renderer must not add the parent rect.
            l['parent']=1 if l['rect'][0]<160 else 2
    return dict(schema=1,kind='encore.native-storage.source-ir',commit=PIN,scope='Original ordinary Minnie Storage; singleton Ninten; BaseballCap and acquired partially consumed AsthmaSpray; transfer/equipment prompts/persistent UID; rich description and god storage unsupported',sources=dict(sorted(ex.sources.items())),dependencies={'content/native-items.json':digest(Path(root)/'content/native-items.json'),'content/native-item-use.json':digest(Path(root)/'content/native-item-use.json')},parameters=params,bindings=binding,policies=policies,equipment=equipment,translations=translations,resources=resources,layouts=layouts)

def recipe(ir):
    return dict(schema=1,kind='encore.storage.asset-recipe',commit=PIN,sources=ir['sources'],resources=ir['resources'],licence_review='Pinned upstream LICENSE permits game-related forks/modifications; original art remains under upstream terms, not MIT.')

def extract(root=ROOT):
    ir=build(root);write_json(Path(root)/IR,ir);write_json(Path(root)/RECIPE,recipe(ir))
    write_json(Path(root)/REVIEW,dict(schema=1,commit=PIN,ir_sha256=digest(Path(root)/IR),sources=ir['sources'],scope=ir['scope'],semantics=['Normal storage64; Ninten inventory16; six rows; entry and deposit storage sort; source scores descending and translated sorting-name ascending on ties','Transfer same object UID/doses, source drop unequips; no constructor/random draw','Store equipped only after YES; withdraw first and optionally equip after YES; NO/cancel preserves withdrawal','Source back sound is admitted equal to checked Items Close; storage/cursor cancellation consumes that independent binding','Equipment boost/stat data retained in binary; no item use or status healing claimed','Source Control rectangles at 1:1; 400x240 adapts by centering320x180 coordinates; pointer rect follows source cursor offset/size conversion; 5fps original0,1,2,1 strip and0.1secondquart-out movement','Original BottleRocket counter face/outline uses checked Introduction catalogue and source extra_spacing_char; EBMain text line-height derived from admitted Items source metrics; parent links identify pane without additional transforms'],unsupported=['God-mode storage','Other party members/items','Rich item descriptions; source controls remain explicitly gated','Equal sorting-score definitions beyond current scope','Scrollbar rotated up/down arrow ornaments, InventorySelect character-bar backing and question-label highlight ornament are not drawn; paging, thumb, portrait/status, confirmation cursor and repeat controls remain functional'],unverified=['Manual tests not run','3DS device visual/input/audio validation pending']))
    return ir

def load(root=ROOT):
    ir=read_json(Path(root)/IR);require(canonical(ir)==canonical(build(root)),'Stale/unreviewed Storage source IR')
    review=read_json(Path(root)/REVIEW)
    fields(review,('schema','commit','ir_sha256','sources','scope','semantics','unsupported','unverified'),'Storage review')
    require(review['schema']==1 and review['commit']==PIN and review['ir_sha256']==digest(Path(root)/IR) and review['sources']==ir['sources'] and review['scope']==ir['scope'],'Storage review mismatch')
    require(read_json(Path(root)/RECIPE)==recipe(ir),'Unreviewed Storage texture recipe');return ir

def compile_assets(root,tex3ds):
    from PIL import Image
    root=Path(root);ir=load(root);outputs={};tmp=root/'build/storage-assets';tmp.mkdir(parents=True,exist_ok=True)
    require(Path(tex3ds).is_file(),'Need genuine tex3ds')
    for row in ir['resources']:
        source=root/'upstream/MOTHER-Encore'/row['source'];target=root/'romfs'/row['output'];target.parent.mkdir(parents=True,exist_ok=True)
        args=[str(tex3ds),'-f','rgba8','-z','none','-o',str(target)]
        size=row['size']
        if row['crop']:
            x,y,w,h=row['crop'];require(x>=0 and y>=0 and w>0 and h>0 and x+w<=size[0] and y+h<=size[1],'Storage crop')
            converted=tmp/(row['name']+'.png');Image.open(source).convert('RGBA').crop((x,y,x+w,y+h)).save(converted);source=converted;size=[w,h]
        # One tex3ds subtexture retains the whole original strip; columns/rows
        # in ENCSTG01 select lossless regions, as in the existing Items renderer.
        subprocess.run(args+[str(source)],check=True)
        outputs[row['output']]=dict(bytes=target.stat().st_size,sha256=digest(target),size=size)
    receipt=dict(schema=1,commit=PIN,ir_sha256=digest(root/IR),producer_sha256=digest(root/'tools/storage_assets.py'),tex3ds_sha256=digest(tex3ds),outputs=outputs)
    write_json(root/RECEIPT,receipt);return receipt

def verify_receipt(root=ROOT):
    root=Path(root);ir=load(root);r=read_json(root/RECEIPT)
    fields(r,('schema','commit','ir_sha256','producer_sha256','tex3ds_sha256','outputs'),'Storage receipt')
    require(r['schema']==1 and r['commit']==PIN and r['ir_sha256']==digest(root/IR) and r['producer_sha256']==digest(root/'tools/storage_assets.py') and re.fullmatch('[0-9a-f]{64}',r['tex3ds_sha256']),'Storage texture producer identity')
    require(set(r['outputs'])=={a['output'] for a in ir['resources']},'Storage receipt output set')
    for a in ir['resources']:
        output=r['outputs'][a['output']];fields(output,('bytes','sha256','size'),'Storage output')
        require(output['size']==(a['crop'][2:] if a['crop'] else a['size']) and type(output['bytes']) is int and output['bytes']>0 and (root/'romfs'/a['output']).stat().st_size==output['bytes'] and digest(root/'romfs'/a['output'])==output['sha256'],'Changed Storage texture')
    return r

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--tex3ds',type=Path);args=p.parse_args()
    try:
        if args.action=='extract':extract()
        elif args.action=='compile':compile_assets(ROOT,args.tex3ds)
        else:verify_receipt()
        print('Storage source/assets '+args.action+': admitted')
    except (ValueError,KeyError,TypeError,OSError) as error:
        print('Storage source/assets rejected:',error,file=sys.stderr);return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
