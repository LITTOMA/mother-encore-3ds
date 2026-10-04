#!/usr/bin/env python3
"""Compiler/source negatives and checked preview fixtures; never edits RomFS."""
import argparse,copy,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import startup_settings_assets as settings

def rejected(action):
 try:action()
 except (ValueError,TypeError,KeyError,AssertionError):return
 raise AssertionError('Unsupported preview source/IR accepted')

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--prepare-only',action='store_true');args=ap.parse_args()
 source=(ROOT/'upstream/MOTHER-Encore/Scripts/UI/NamingScreen/TextSpeed.gd').read_text()
 assert settings.preview_minimum(source)==5
 for candidate in [source.replace('> 5','>= 5'),source.replace('> 5','> 6',1),source.replace('/Slow.text','/Other.text'),source.replace('and len','or len',1),source.replace('> 5','> 1025')]:rejected(lambda:settings.preview_minimum(candidate))
 assert settings.preview_minimum(source.replace('> 5','> 0'))==0
 recipe=json.loads((ROOT/'content/native-startup-settings.json').read_text())
 for value in [None,True,False,-1,1025,1.5,float('nan'),float('inf'),float('-inf'),'5']:
  altered=copy.deepcopy(recipe);altered['preview_minimum_characters']=value;rejected(lambda:settings.encode(altered))
 missing=copy.deepcopy(recipe);del missing['preview_minimum_characters'];rejected(lambda:settings.encode(missing))
 for version in [None,True,1,3,2.0,'2']:
  old=copy.deepcopy(recipe);old['schema']=version;rejected(lambda:settings.encode(old))
 if not args.prepare_only:
  print('Settings preview compiler/source negatives passed');return
 out=ROOT/'build/settings-preview';out.mkdir(parents=True,exist_ok=True)
 fixtures={'source':recipe}
 for name,threshold,labels in [('five',5,['Five5']*3),('six',5,['Sixsix']*3),('mixed',5,['Sixsix','Sixsix','Five5']),('five-threshold4',4,['Five5']*3),('source-threshold0',0,None)]:
  altered=copy.deepcopy(recipe);altered['preview_minimum_characters']=threshold
  if labels:
   for row,text in zip(altered['panels'][0]['labels'],labels):row['text']=text
  fixtures[name]=altered
 for name,data in fixtures.items():
  blob=settings.encode(data);assert struct.unpack_from('<I',blob,len(blob)-4)[0]==data['preview_minimum_characters'];(out/(name+'.encsettings')).write_bytes(blob)
 print('Settings preview compiler/source negatives and fixtures passed')
if __name__=='__main__':main()
