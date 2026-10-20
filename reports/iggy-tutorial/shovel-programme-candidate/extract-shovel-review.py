from pathlib import Path
import json,hashlib,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools.extract_battle_entry import Extractor,PIN
ex=Extractor(r);source='Data/Dialogue/Podunk/cutscenes/gravedigger_getshovel.yaml';doc=ex.yaml(source)
for n in ['Scripts/UI/DialogueBox.gd','Scripts/Main/actor.gd','Scripts/Main/npc.gd','Maps/podunk/podunk.tscn','Nodes/Overworld/Objects/DroppedItem.tscn','Scripts/Main/DroppedItem.gd','Scripts/Main/ItemHolder.gd','Scripts/Main/FlaggableObject.gd','Scripts/misc/sparkles.gd','Translations/TranslatedText/dialogue_Podunk_cutscenes - sheet.csv']:ex.data(n)
p=dict(schema=1,kind='encore.graveyard-shovel.reviewed-source',commit=PIN,scene=2,source=source,npc='Objects/NPCS/npc18',boundary='Cutscenes/Cutscene Area7',object='Objects/Items/Item',documents={source:doc},sources=ex.sources,scope='Ten-label source review/candidate only. Source sets got_shovel before explicit deferred object deletion at3, erases Iggy before returned_shovel at11, restores only remaining Ninten. No item grant. Preserve initial/camera idle handoffs and half-second path WaitAction. No shipping root/object/renderer/pickup/frontend/quest claim.')
path=r/'content/graveyard-shovel.json';assert not path.exists();path.write_bytes((json.dumps(p,indent=2,ensure_ascii=False)+'\n').encode('utf8'))
from tools.graveyard_shovel import read
actual,_=read();assert actual==p;print('Actual original shovel review:',len(doc),'labels;',len(ex.sources),'source fingerprints; no programme or shipping claim')
