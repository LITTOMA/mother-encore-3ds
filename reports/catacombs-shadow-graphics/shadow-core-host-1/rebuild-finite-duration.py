import hashlib,json,subprocess
from pathlib import Path
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');out=r/'build/full-port/shadow-core-host-1';sources=['runtime/field_steps.cpp','runtime/shadow_graphics.cpp'];flags=['-std=c++17','-O1','-g','-Wall','-Wextra','-Wpedantic','-Wno-misleading-indentation','-fno-rtti','-fno-exceptions','-fno-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Iinclude'];objects=[]
for source in sources:
 target=out/(source.replace('/','_')+'.o')
 with(out/('finite-duration-'+source.replace('/','_')+'.log')).open('wb')as output:subprocess.run(['g++',*flags,'-c',str(r/source),'-o',str(target)],stdout=output,stderr=subprocess.STDOUT,check=True)
 objects.append(str(target))
subprocess.run(['ar','rcs',str(out/'libencore-shadow.a'),*objects],check=True)
old=json.loads((out/'inputs-initial-117.json').read_bytes());changed=[p for p,h in old.items()if hashlib.sha256((r/p).read_bytes()).hexdigest()!=h];assert sorted(changed)==sorted(sources)
current={p:hashlib.sha256((r/p).read_bytes()).hexdigest()for p in old};(out/'inputs.json').write_text(json.dumps(current,indent=2)+'\n');(out/'finite-duration-completion.json').write_text(json.dumps(dict(recompiled=sources,all_current_runtime_headers_match=True,reason='Reject nonfinite frame duration before narrowing double tofloat;no earlier execution test claimed',initial_scope_label_correction='Original builder template scope label said116;actual compiled_sources117 and source/object lists prove117;original log unchanged'))+'\n');print('Two changed current sources recompiled;all117 current core inputs accounted for')
