#!/usr/bin/env python3
"""Scoped original zoo_vm ShopUI rules/layout/assets. JSON is offline source IR."""
from __future__ import annotations
import argparse,csv,io,json,re,struct,subprocess,sys,zlib,math
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,stable,read,sha,require
from tools.extract_battle_entry import Extractor,node
IR=ROOT/'content/native-field-shop.json';REVIEW=ROOT/'reports/field-shop/source-review.json';PACK=ROOT/'romfs/data/zoo.encshop';RECEIPT=ROOT/'content/asset-receipts/graphics/ui/shop/source.json'
SCRIPT='Scripts/UI/Shop/ShopUI.gd';SCENE='Nodes/Ui/Shop/ShopUI.tscn'
def write(p,d):
 p.parent.mkdir(parents=True,exist_ok=True)
 with p.open('w',encoding='utf8',newline='\n')as f:json.dump(d,f,ensure_ascii=False,indent=2);f.write('\n')
def extract():
 ex=Extractor(ROOT);script=ex.text(SCRIPT);scene=ex.text(SCENE);ui=ex.text('Scripts/global/uiManager.gd');inv=ex.text('Scripts/global/Inventory.gd');item=ex.text('Scripts/global/Item.gd');list_script=ex.text('Scripts/UI/Reusables/ItemListMenu.gd');modal=ex.text('Scripts/UI/Reusables/DescriptionModal.gd');texttools=ex.text('Scripts/global/text_tools.gd');ex.text('Scripts/UI/Reusables/Description.gd');ex.text('Scripts/UI/Reusables/DescriptionWithYesNo.gd');ex.text('Scripts/UI/cursor.gd');ex.text('Scripts/global/global.gd');ex.text('Scripts/global/globalData.gd');ex.text('LICENSE')
 for s in ['for partyMem in global.get_party_in_natural_order():','_menu_buy.item_list.append(Item.new(item_name))','_current_character.inv.add_item_by_name(_selected_item.item_name)','globaldata.cash -= item_data.get("cost", 0)','return item.get_data().value * item.doses','_current_character.inv.drop_item(item)','_callback.call_func(_last_purchased.item_name if _last_purchased else "")','_desc_no_dialog.warn(tr("TRANSACTION_FULL"), 1)','_enter_sell()'] :require(s in script,'Unknown original shop operation '+s)
 require('randomize()'in item and 'used_uids.append(new_uid)'in item and 'item.equipped = false'in inv,'Unknown Item default UID or removal')
 offers=ex.yaml('Data/Shops/zoo_vm.yaml');require(offers==['Hamburger','SportsDrink','EyeDrops'],'Changed source shop order')
 phrase=ex.yaml('Data/Dialogue/Reusable/vendingmachine.yaml');require(phrase['1']['open_shop']is None and 'func open_shop(shop = null, can_sell = true'in ui,'Unreviewed null shop/default sell binding')
 baseline=read(ROOT/'content/native-field-item-definitions.json');names=sorted(set(r['item_name']for r in baseline['definitions'])|set(offers));policies=[];supplement=[]
 table={}
 for path in ['Translations/TranslatedText/items - sheet.csv','Translations/TranslatedText/menus - sheet.csv']:
  for r in csv.DictReader(io.StringIO(ex.text(path))):table[r['key']]=r
 keys={'SHOP_SHOP','SHOP_BUY','SHOP_SELL','SHOP_ASK_BUY','SHOP_ASK_SELL','SHOP_ASK_SELL_FOR_CASH','TRANSACTION_FULL','MENU_YES','MENU_NO','WORD_SEPARATOR'}
 for name in names:
  path='Data/Items/'+name+'.yaml';raw=ex.yaml(path);require(raw['transform']=='' and 'use_at_shop_dialog'not in raw,'Unsupported shop transform/dialog '+name)
  if name in offers:require(not raw['keyitem']and raw['slot']=='','Original shop equipment offer requires broader consumer')
  policies.append(dict(id=stable(path),source=path,name=name,doses=raw.get('doses',1),cost=raw['cost'],value=raw['value'],key=raw['keyitem'],name_key=raw['name'],description_key=raw['description'],article_key=raw['article'],slot=raw['slot'],sha256=ex.sources[path]));keys.update(raw[k]for k in ('name','description','article'))
  if name not in {r['item_name']for r in baseline['definitions']}:supplement.append(dict(source=path,definition=raw))
 texts=[]
 for k in sorted(keys):
  r=table.get(k,{});texts.append(dict(key=k,en=(r.get('en')or k),zh=(r.get('zh_CN')or r.get('en')or k)))
 audio=ex.text('Scripts/global/audioManager.gd');sounds=[]
 for name in ['cursor1','cursor2','restricted','back','cash']:
  source=re.search(r'"'+name+r'": load\("res://([^\"]+)"\)',audio)[1];ex.data(source);ex.data(source+'.import');sounds.append(source)
 textures=[];indices={}
 def tex(p,flavor=False):
  if p not in indices:
   ex.data(p);require('flags/filter=false'in ex.text(p+'.import'),'Shop source texture filter');w,h=ex.png_size(p);require(0<w<=1024 and 0<h<=1024,'Shop source texture extent');indices[p]=len(textures);textures.append(dict(source=p,path='graphics/ui/shop/'+str(len(textures))+'.t3x',width=w,height=h,flavor=flavor))
  require(textures[indices[p]]['flavor']==flavor,'Shop conflicting material');return indices[p]
 ext={int(i):p for p,i in re.findall(r'\[ext_resource path="res://([^\"]+)"[^\n]* id=(\d+)\]',scene)}
 panels=[]
 for path in ['ShopBox/Header','ShopBox/Header/Title','ShopBox/BuyMenu','ShopBox/SellMenu','CharacterSelect','BuySellDialog','DescBox']:
  v=node(scene,path);rect=[v.get('margin_'+k,0)for k in ['left','top','right','bottom']]
  if path in ['ShopBox/BuyMenu','ShopBox/SellMenu']:rect=[0,v['margin_top'],node(scene,'ShopBox')['margin_right']-node(scene,'ShopBox')['margin_left'],node(scene,'ShopBox')['margin_bottom']-node(scene,'ShopBox')['margin_top']]
  panels.append(dict(node=path,rect=rect,patch=[v['patch_margin_'+k]for k in ['left','top','right','bottom']],texture=tex(ext[v['texture']['ExtResource']],True)))
 cash=ex.text('Nodes/Ui/Shop/CashBoxShop.tscn');c=node(cash,'.');cext={int(i):p for p,i in re.findall(r'\[ext_resource path="res://([^\"]+)"[^\n]* id=(\d+)\]',cash)};panels.append(dict(node='Cash',rect=[node(scene,'ShopBox/Header/CashBox')['margin_left'],c['margin_top'],node(scene,'ShopBox/Header/CashBox')['margin_right'],c['margin_bottom']],patch=[c['patch_margin_'+k]for k in ['left','top','right','bottom']],texture=tex(cext[c['texture']['ExtResource']],True)))
 desc=ex.text('Nodes/Ui/Description.tscn');confirm=ex.text('Nodes/Ui/DescriptionWithYesNo.tscn');portrait=ex.text('Nodes/Ui/Inventory/portrait.tscn');arrow=ex.text('Nodes/Ui/arrow.tscn');scroll=ex.text('Nodes/Ui/Reusables/Scrollbar.tscn');ex.text('Scripts/UI/Reusables/Scrollbar.gd');ex.text('Nodes/Ui/HighlightLabel.gd');ex.text('Nodes/Ui/Indicator.tscn')
 # Resource role order is explicit source IR, not inferred file naming in C++.
 roles={}
 for role,p,flavor in [('icon_panel','Graphics/UI/Inventory/item_icon.png',True),('cursor','Graphics/UI/Inventory/cursor.png',False),('dollar_left','Graphics/UI/ATM/dollar_left.png',False),('dollar_right','Graphics/UI/ATM/dollar_right.png',False),('modifiers','Graphics/UI/Inventory/modifiers.png',False),('scroll_bg','Graphics/UI/Overworld/flavours/defaultbox_inside_scrollbar.png',True),('scroll_thumb','Graphics/UI/Overworld/flavours/defaultscroll.png',True)]:roles[role]=tex(p,flavor)
 for p in policies:
  icon='Graphics/Objects/Items/'+p['name']+'.png';roles['item:'+str(p['id'])]=tex(icon,False)if (ex.upstream/icon).is_file()else 0xffffffff
 portraits=[]
 for name in re.findall(r'Party(?:Member|NPC)\.([A-Z_]+): preload\("res://(Graphics/UI/Inventory/characters/[^\"]+)"\)',script[:script.index('# Nodes')]):
  portraits.append(dict(character=name[0],texture=tex(name[1],True)))
 shader=ex.text('Shaders/MenuFlavors.tres');colors=[]
 for i in range(1,9):colors.append([int(round(float(c)*255))for c in re.search(r'shader_param/OLDCOLOR'+str(i)+r' = Color\( ([^)]+) \)',shader)[1].split(',')])
 fonts=['Fonts/EBMain.tres','Fonts/EBMain_la.tres','Fonts/BottleRocket_align.tres','Fonts/BottleRocket.tres']
 for p in fonts:
  f=ex.text(p)
  for q in re.findall(r'path="res://([^\"]+)"',f):ex.data(q)
 def margins(t,p):v=node(t,p);return[v.get('margin_'+k,0)for k in ('left','top','right','bottom')]
 buy=node(scene,'ShopBox/BuyMenu');sell=node(scene,'ShopBox/SellMenu');row=node(scene,'ShopBox/BuyMenu/MarginContainer/VBoxContainer/ShopItem');nameslot=node(scene,'ShopBox/BuyMenu/MarginContainer/VBoxContainer/ShopItem/ItemLabel');priceslot=node(scene,'ShopBox/BuyMenu/MarginContainer/VBoxContainer/ShopItem/PriceLabel')
 # Flat vector positions are a binary schema. Every value is taken from source.
 layout=margins(scene,'ShopBox')+margins(scene,'ShopBox/BuyMenu/MarginContainer')+[row.get('margin_bottom',13),node(scene,'ShopBox/BuyMenu/MarginContainer/VBoxContainer')['custom_constants/separation']]+margins(scene,'BuySellDialog/VBoxContainer')+margins(scene,'CharacterSelect/CharacterPortraits/Party1')+[node(scene,'CharacterSelect/CharacterPortraits')['custom_constants/separation']]+margins(desc,'HBox')+margins(desc,'HBox/TextureRect')+margins(desc,'HBox/MarginContainer/Desc')+margins(confirm,'HBox/MarginContainer/Desc')+list(node(scene,'ShopBox/BuyMenu/arrow')['position'])+list(node(scene,'ShopBox/BuyMenu/arrow').get('cursor_offset',[0,0]))+list(node(scene,'BuySellDialog/arrow')['position'])+list(node(confirm,'arrow')['position'])+[node(scene,'.')['margin_top'],nameslot['margin_right'],priceslot['margin_right'],node(scene,'BuySellDialog/VBoxContainer/NoLabel')['margin_top']]
 aux=[margins(cash,'HBoxContainer/Amount'),margins(cash,'HBoxContainer/DollarLeft'),margins(cash,'HBoxContainer/DollarRight'),margins(scene,'ShopBox/BuyMenu/Scrollbar'),margins(scroll,'ScrollBG'),margins(scroll,'ScrollBG/Thumb'),[node(scroll,'ScrollBG')['patch_margin_'+k]for k in ('left','top','right','bottom')],[node(scroll,'ScrollBG/Thumb')['patch_margin_'+k]for k in ('left','top','right','bottom')],margins(scroll,'UpArrow'),margins(scroll,'DownArrow'),[node(scene,'BuySellDialog/VBoxContainer/YesLabel').get('margin_bottom',12),node(scene,'BuySellDialog/VBoxContainer/NoLabel')['margin_bottom']-node(scene,'BuySellDialog/VBoxContainer/NoLabel')['margin_top'],node(desc,'HBox')['custom_constants/separation'],node(desc,'HBox/MarginContainer')['custom_constants/margin_top']],[1,1,1,1],[float(v)for v in re.search(r'bg_color = Color\( ([^)]+) \)',scene)[1].split(',')]]
 aux.extend([[node(scene,'ShopBox/Header/Title/Label').get('align',0),node(scene,'ShopBox/Header/Title/Label').get('valign',0),nameslot.get('align',0),nameslot.get('valign',0)],[priceslot.get('align',0),priceslot.get('valign',0),node(cash,'HBoxContainer/Amount').get('align',0),node(cash,'HBoxContainer/Amount').get('valign',0)]])
 aux.extend([margins(cash,'HBoxContainer'),[node(scroll,'UpArrow/arrow')['rotation'],node(scroll,'DownArrow/arrow')['rotation'],*node(scroll,'UpArrow/arrow')['position']],margins(scene,'ShopBox/BuyMenu/Separator'),[float(c)for c in re.search(r'self_modulate = Color\( ([^)]+) \)',scroll)[1].split(',')]])
 d=dict(schema=1,kind='encore.field-shop.source-ir' ,commit=PIN,name='zoo_vm',offers=[stable('Data/Items/'+n+'.yaml')for n in offers],policies=policies,supplemental_definitions=supplement,texts=texts,sounds=sounds,lines=int(re.search(r'const LINES_PER_PAGE = (\d+)',list_script)[1]),cash_digits=int(re.search(r'pad_zeros\((\d+)\)',script)[1]),warning_seconds=float(re.search(r'warn\(tr\("TRANSACTION_FULL"\), (\d+)\)',script)[1]),can_sell=True,buy_loop=buy['loop_around'],sell_loop=sell.get('loop_around',False),text_keys=[node(scene,'ShopBox/Header/Title/Label')['text'],node(scene,'BuySellDialog/VBoxContainer/YesLabel')['text'],node(scene,'BuySellDialog/VBoxContainer/NoLabel')['text']]+['SHOP_ASK_BUY','SHOP_ASK_SELL','SHOP_ASK_SELL_FOR_CASH','TRANSACTION_FULL','MENU_YES','MENU_NO'],panels=panels,layout=layout,aux=aux,fonts=fonts,textures=textures,roles=roles,portraits=portraits,cursor=dict(frames=[0,1,2,1],speed=float(re.search(r'"speed": ([\d.]+)',arrow)[1]),size=[8,8]),portrait_limit=int(re.search(r'for i in (\d+): #',script)[1]),source_colors=colors,threshold=float(re.search(r'distance\(curr_pixel, OLDCOLOR1\) < ([.0-9]+)',shader)[1]),sources=dict(sorted(ex.sources.items())),semantics=['Source zoo_vm offers in order; normal null-shop dialogue permits selling','Opening constructs three Item preview UIDs and reseeds shared source RNG; each purchase constructs one different UID','Exact selected natural-order member inventory; no first-available substitution','Full inventory warns for source 1s without blocking item cursor; cash shortage yes enters sell','Resale multiplies source value by actual doses and drop clears equipped; callback returns last purchased name or empty','Original equipment purchase branches unreachable for this three nonequipment offer set; future equip offers rejected','Descriptions retain source controls and require checked TextTools/ItemDetails composition; no fake generic labels'],unverified=['Manual tests not run','Shared item/Session extensions and Main/SceneHost integration pending','Emulator/hardware unverified'])
 validate(d);write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unverified=d['unverified']));return d
def validate(d):
 require(set(d)==set('schema kind commit name offers policies supplemental_definitions texts sounds lines cash_digits warning_seconds can_sell buy_loop sell_loop text_keys panels layout aux fonts textures roles portraits cursor portrait_limit source_colors threshold sources semantics unverified'.split()),'Unknown/missing Shop source IR field')
 require(len(d['aux'])==19 and all(len(v)==4 and all(type(x)in(int,float)and math.isfinite(x)for x in v)for v in d['aux']),'Shop bounded auxiliary geometry')
 require(d['schema']==1 and d['kind']=='encore.field-shop.source-ir'and d['commit']==PIN and d['name']=='zoo_vm','Shop source IR identity');require(len(d['offers'])==3 and len(d['policies'])==19 and len(d['panels'])==8 and len(d['sounds'])==5 and len(d['layout'])==47,'Shop coverage/layout schema');require(d['lines']==6 and d['cash_digits']==6 and d['warning_seconds']==1 and d['can_sell']is True,'Shop source tuning')
 require(all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1000000 for v in d['layout']),'Shop bounded geometry')
 ids=set()
 for p in d['policies']:
  require(set(p)==set('id source name doses cost value key name_key description_key article_key slot sha256'.split())and p['id']==stable(p['source'])and p['id']not in ids and type(p['key'])is bool and 0<p['doses']<=65535 and 0<=p['cost']<=2147483647 and 0<=p['value']<=2147483647,'Shop definition policy');ids.add(p['id'])
 require(len(d['text_keys'])==9 and all(k in {t['key']for t in d['texts']}for k in d['text_keys'])and len(d['fonts'])==4 and len(d['source_colors'])==8 and len(d['cursor']['size'])==2 and 0<d['cursor']['speed']<=1000 and type(d['portrait_limit'])is int and 0<d['portrait_limit']<=64,'Shop source render role policy')
 for t in d['textures']:require(set(t)==set('source path width height flavor'.split())and type(t['flavor'])is bool and 0<t['width']<=1024 and 0<t['height']<=1024 and t['path'].startswith('graphics/ui/shop/')and '..'not in t['path'],'Shop source GPU policy')
 require(all(i in ids for i in d['offers']),'Shop unknown offer');return d
def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Shop source review stale');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed source shop '+p)
 return d
def encode(d,receipt):
 validate(d);out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 s(d['name']);u(d['lines'],d['cash_digits'],int(d['can_sell']),int(d['buy_loop']),int(d['sell_loop']));f(d['warning_seconds'],d['threshold']);u(*d['offers'])
 for p in d['policies']:
  u(p['id'],p['doses'],p['cost'],p['value'],int(p['key']))
  for k in ['source','name','name_key','description_key','article_key','slot']:s(p[k])
  out.extend(bytes.fromhex(p['sha256']))
 u(len(d['texts']))
 for t in d['texts']:
  for k in ['key','en','zh']:s(t[k])
 for v in d['sounds']:s(v)
 u(len(d['textures']))
 for i,t in enumerate(d['textures']):
  a=receipt['outputs'][i];require(a['path']==t['path'],'Shop receipt order');s(t['source']);s(t['path']);u(t['width'],t['height'],a['bytes'],int(t['flavor']));out.extend(bytes.fromhex(a['sha256']))
 for p in d['panels']:s(p['node']);f(*p['rect'],*p['patch']);u(p['texture'])
 f(*d['layout']);u(*(int.from_bytes(bytes(c),'little')for c in d['source_colors']))
 u(len(d['roles']))
 for k,v in d['roles'].items():s(k);u(v)
 u(len(d['portraits']))
 for p in d['portraits']:s(p['character']);u(p['texture'])
 for p in d['fonts']:s(p)
 u(d['portrait_limit']);f(d['cursor']['speed']);u(*d['cursor']['size'],len(d['cursor']['frames']),*d['cursor']['frames'])
 u(len(d['aux']))
 for v in d['aux']:f(*v)
 for k in d['text_keys']:s(k)
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s3I',out,0,b'ENCSHP01',1,len(out),0,1,1,len(d['offers']),bytes.fromhex(PIN),len(d['policies']),len(d['panels']),len(d['layout']));struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)
def compile_assets(tool):
 d=load();require(tool and Path(tool).is_file(),'Actual tex3ds required')
 def convert(t):
  p=ROOT/'romfs'/t['path'];p.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tool),'-f','rgba8','-z','none','-o',str(p),str(ROOT/'upstream/MOTHER-Encore'/t['source'])],check=True);return dict(path=t['path'],bytes=p.stat().st_size,sha256=sha(p))
 with ThreadPoolExecutor(max_workers=4)as pool:outputs=list(pool.map(convert,d['textures']))
 r=dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(ROOT/'tools/field_shop.py'),tex3ds_sha256=sha(tool),outputs=outputs);write(RECEIPT,r);raw=encode(d,r);PACK.write_bytes(raw);return raw
def compile_pack():
 d=load();r=read(RECEIPT);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['producer_sha256']==sha(ROOT/'tools/field_shop.py'),'Shop texture receipt stale')
 for t in r['outputs']:p=ROOT/'romfs'/t['path'];require(p.stat().st_size==t['bytes']and sha(p)==t['sha256'],'Shop actual texture integrity')
 raw=encode(d,r);PACK.write_bytes(raw);return raw
def stage_files(source):
 d=load();r=read(RECEIPT);raw=encode(d,r);root=Path(source);require((root/'data/zoo.encshop').read_bytes()==raw,'Stale staged shop binary');out={Path('data/zoo.encshop'):raw}
 for t in r['outputs']:p=root/t['path'];require(p.stat().st_size==t['bytes']and sha(p)==t['sha256'],'Stale staged shop texture');out[Path(t['path'])]=p.read_bytes()
 return out
def glyph_requests():
 d=load();return{p:''.join(t[k]for t in d['texts']for k in ('en','zh'))+'0123456789'for p in d['fonts']}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','pack']);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 try:print(len(extract()['policies'])if a.action=='extract'else len(compile_assets(a.tex3ds)if a.action=='compile'else compile_pack()))
 except(ValueError,KeyError,TypeError,OSError,struct.error,subprocess.CalledProcessError)as e:sys.exit('SHOP ERROR: '+str(e))
