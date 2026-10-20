from pathlib import Path
import subprocess,sys,time,json
r=Path(__file__).resolve().parents[2]
steps=[('story',['tools/field_story.py','compile']),('room',['tools/podunk_field.py','compile']),('session-extract',['tools/native_session.py','extract']),('session-compile',['tools/native_session.py','compile']),('restore-extract',['tools/native_restore.py','extract']),('restore-compile',['tools/native_restore.py','compile']),('new-game',['tools/new_game_assets.py','reuse-session','--base','7d2e552f86c9ef0c995c85c301e3a2f26a78f32a']),('player-assets',['tools/field_player_assets.py','assets','--from-atlases','build/iggy-tutorial/swing-graphics-candidate-2']),('audio',['tools/gameplay_audio.py']),('localization',['tools/localization_assets.py','compile']),('menus',['tools/field_menus.py','extract']),('catalog',['tools/resource_catalog.py','compile']),('stage',['tools/stage_native_romfs.py','--verify-only'])]
results=[]
for name,args in steps:
 begin=time.time();log=r/('build/full-port/iggy-release-final-'+name+'.log')
 with log.open('wb')as f:p=subprocess.run([sys.executable,*args],cwd=r,stdout=f,stderr=subprocess.STDOUT)
 results.append(dict(step=name,returncode=p.returncode,seconds=time.time()-begin,log=str(log.relative_to(r))))
 (r/'build/full-port/iggy-release-regeneration.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf8')
 print(name,p.returncode,round(time.time()-begin,2),flush=True)
 if p.returncode:raise SystemExit(p.returncode)
