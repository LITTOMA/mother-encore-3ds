from pathlib import Path
import hashlib,json,subprocess,sys,time
root=Path(__file__).resolve().parents[2];expected='8f25cba4b518c68ffee1253d4832360c7916cf8f514b98618c8e995b5366dfec'
references=[p for p in(root/'build/emulator-regression').glob('*/reference.json')if hashlib.sha256(p.read_bytes()).hexdigest()==expected];assert references
base=root/'build/full-port/item-admission-bb-official';game=base/'verified-artifact/3ds/encore-native/encore-native.3dsx';receipt=base/'verified-artifact/build-receipt.json';meta=json.loads(receipt.read_bytes());identity=meta['artifacts']['encore-native.3dsx']
assert meta['source_commit']=='bb743a4fee0c60ca9ffe2e794e46f2b94ed331e0'and game.stat().st_size==identity['bytes']==265060576 and hashlib.sha256(game.read_bytes()).hexdigest()==identity['sha256']=='a51308775df4f125a0d09dfa434079a619733bf98298ce19f944cb89383a24cf'
emulator=root/'build/azahar-cdb/bin/Release/azahar.exe';assert hashlib.sha256(emulator.read_bytes()).hexdigest()=='bd50cebf6427410df090855c4f5ba7cd1991b7fe1ff7f7f12f2ac49e187da525'
out=root/'build/emulator-regression/item-admission-bb-opening-1';assert not out.exists()
command=[sys.executable,'-X','utf8',str(root/'build/full-port/run_current_opening.py'),'--emulator',str(emulator),'--game',str(game),'--receipt',str(receipt),'--scenario',str(root/'tests/emulator/opening-field-exit.json'),'--original-reference',str(references[0]),'--out',str(out),'--repetitions','2','--dsp-component','C:/Users/litto/AppData/Roaming/Azahar/sdmc/3ds/dspfirm.cdc','--require-audio']
record=dict(command=command,scope='Two ordinary fresh cold new games, original90 expectations unchanged. No save, memory, RNG, flag or inventory injection. Current new-menu resources checked; formal catacombs, GPU, perceptual audio and hardware remain unverified.')
(base/'opening-command.json').write_text(json.dumps(record,indent=2)+'\n');start=time.monotonic()
with(base/'opening.stdout.log').open('wb')as stdout,(base/'opening.stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
record=dict(returncode=result.returncode,seconds=time.monotonic()-start,receipt=str(out/'receipt.json'))
(base/'opening-completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record),flush=True)
if result.returncode:print((base/'opening.stderr.log').read_text(errors='replace')[-3500:])
assert result.returncode==0
