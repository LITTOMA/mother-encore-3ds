from pathlib import Path
import sys,json,hashlib,time,cProfile,pstats
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools import link_phone_content as phone
from tools.extract_native_content import Extractor
from tools.extract_house import build
start=time.monotonic();profile=cProfile.Profile();profile.enable()
room,_=Extractor(ROOT).run();house=build(ROOT);audio=phone.build_audio(ROOT,room)
old=json.loads((ROOT/phone.REPORT).read_bytes());new=phone.report(ROOT,room,house,audio)
profile.disable();profile.dump_stats(str(ROOT/'build/full-port/phone-root-profile.pstats'))
pstats.Stats(profile).sort_stats('cumulative').print_stats(22)
diff={}
for section in old.keys()|new.keys():
 a,b=old.get(section),new.get(section)
 if a==b:continue
 if isinstance(a,dict)and isinstance(b,dict):
  diff[section]={k:{'old':a.get(k),'new':b.get(k)}for k in a.keys()|b.keys()if a.get(k)!=b.get(k)}
 else:diff[section]={'old':a,'new':b}
proof=dict(seconds=time.monotonic()-start,changes=diff,audio_rows=len(audio['assets']))
target=ROOT/'build/full-port/phone-root-report-difference.json'
target.write_text(json.dumps(proof,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(json.dumps(proof,ensure_ascii=False,indent=2))
