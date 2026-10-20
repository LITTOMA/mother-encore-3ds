from pathlib import Path
import hashlib,json,subprocess,shutil,sys,os
r=Path(__file__).resolve().parents[2];source=r/'build/iggy-tutorial/area-entry-probe-2';out=r/'build/iggy-tutorial/area-entry-probe-3'
assert not out.exists();out.mkdir();exe=r/'build/godot-reference/Godot_v3.6.2-stable_win64.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd'
for name in ['input.json','project.godot','probe.gd']:shutil.copyfile(source/name,out/name)
script=(source/'probe_node.gd').read_text();script=script.replace('var rect\n','var rect\nvar rshape\n').replace('var rshape=CollisionShape2D.new()','rshape=CollisionShape2D.new()')
assert script.count('    record("idle")')==1
script=script.replace('    record("idle")','''    if tick==24:
        shape.disabled=true
        record("disable-inside-idle")
    if tick==28:
        shape.disabled=false
        record("reenable-idle")
    if tick==32:
        rshape.disabled=true
        record("disable-target-idle")
    if tick==36:
        rshape.disabled=false
        record("reenable-target-idle")
    record("idle")''').replace('if tick>=21:','if tick>=41:')
(out/'probe_node.gd').write_text(script,encoding='utf8');startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
env=dict(os.environ,APPDATA=str(out/'user-roaming'))
with(out/'godot.log').open('wb')as f:subprocess.run([str(exe),'--no-window','--path',str(out),'-s','probe.gd','--fixed-fps','60','--audio-driver','Dummy'],cwd=out,stdout=f,stderr=subprocess.STDOUT,startupinfo=startup,env=env,timeout=60,check=True)
data=json.loads((out/'result.json').read_bytes());assert data['engine']['hash']=='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'
for row in data['rows']:
 if row['phase']not in('physics','idle'):print(row['tick'],row['phase'],row['disabled'],row['overlaps'])
print('Rows',len(data['rows']))
