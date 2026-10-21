import hashlib,json,subprocess,sys,time
from pathlib import Path
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tools.audio_asset import parse_bank
n=sys.argv[1];script=r/'build/full-port/key-ndsp-host-1.sh';bank=r/'romfs/sound/banks/opening.encaudio';asset=next(a for a in parse_bank(bank.read_bytes())['assets']if a['stable_id']==21)
paths=[*sorted((r/'include').rglob('*.hpp')),*sorted((r/'platform/ctr').glob('*.hpp')),*sorted((r/'tests/fixtures/audio_ndsp').glob('*')),*[r/p for p in['runtime/dungeon_key_media.cpp','runtime/dungeon_key_clocks.cpp','runtime/dungeon_key_state.cpp','runtime/dungeon_keys.cpp','runtime/audio_data.cpp','runtime/content.cpp','runtime/file_io.cpp','tests/dungeon_key_ndsp_tests.cpp','platform/ctr/audio_player.cpp']],*sorted((r/'build/full-port/key-media-fixture-1').rglob('*')),bank,r/'romfs'/asset['pcm_path'],script]
def hashes():return{p.relative_to(r).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()for p in paths if p.is_file()}
before=hashes();start=time.time();prefix=r/('build/full-port/key-ndsp-host-'+n)
with prefix.with_suffix('.log').open('wb')as out,prefix.with_suffix('.stderr.log').open('wb')as err:
 result=subprocess.run(['wsl','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/key-ndsp-host-1.sh'],cwd=r,stdout=out,stderr=err,timeout=240)
after=hashes();proof=dict(command=result.args,exit=result.returncode,seconds=time.time()-start,inputs=before,unchanged=before==after,stdout_sha256=hashlib.sha256(prefix.with_suffix('.log').read_bytes()).hexdigest(),stderr_sha256=hashlib.sha256(prefix.with_suffix('.stderr.log').read_bytes()).hexdigest());prefix.with_suffix('.proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({k:v for k,v in proof.items()if k!='inputs'}));sys.exit(result.returncode if before==after else 23)
