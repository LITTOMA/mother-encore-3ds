#!/usr/bin/env python3
"""Generate an explicit, isolated basement interaction test checkpoint.

The C++ shared-core test validates and encodes the snapshot. It is an artificial
precondition with a retained key, not a played save or original progress claim.
"""
import argparse, hashlib, json, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
RESOURCES=('session/opening.encsession','world/house/opening.encroom','world/house/opening.enchouse',
           'world/house/opening.encpresent','ui/opening.encitems','battle/lamp/opening.encround')
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--generator',type=Path,required=True);p.add_argument('--build-receipt',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);a=p.parse_args();out=a.out.resolve()
    if out.exists() or not out.is_relative_to(ROOT/'build'):raise ValueError('Fresh owned build output required')
    generator=a.generator.resolve()
    if not generator.is_file() or not generator.is_relative_to(ROOT/'build'):raise ValueError('Use the compiled shared-core test generator in build')
    source=json.loads(a.build_receipt.read_bytes())['source_commit']
    if not re.fullmatch('[0-9a-f]{40}',source):raise ValueError('Genuine build receipt lacks exact source identity')
    # The input/observer wrappers may evolve independently. Fixture gameplay and
    # resource inputs must still match the tested genuine artifact's source.
    subprocess.run(['git','diff','--exit-code',source,'--','runtime','include','platform','romfs','content'],cwd=ROOT,check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    session=json.loads((ROOT/'content/native-session.json').read_bytes())
    presents=json.loads((ROOT/'content/house-presents.json').read_bytes())
    indices=[i for i,o in enumerate(presents['objects']) if o['item']=='MapPodunk' and o['program'] is not None]
    if len(indices)!=1:raise ValueError('Map fixture requires one checked source present')
    out.mkdir(parents=True);save=out/'initial.encsave'
    process=subprocess.run([str(generator),str(ROOT/'romfs'),str(save)],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=60)
    if len(process.stdout)>4*1024*1024:raise ValueError('Fixture generator log budget exceeded')
    (out/'generator.log').write_bytes(process.stdout)
    if process.returncode or not save.is_file() or not 0<save.stat().st_size<=1024*1024:
        raise ValueError('Shared-core fixture generation failed; inspect retained log')
    manifest=dict(schema=1,kind='encore.emulator-save-fixture',source_commit=source,upstream_commit=session['commit'],
                  file=save.name,sha256=sha(save),resources={name:sha(ROOT/'romfs'/name) for name in RESOURCES},
                  base_key_count=len(session['defaults']['key_items'])+1,present_index=indices[0],
                  scope='Artificial shared-core-validated snapshot starts beside the basement with KeyBasement already held. Not a played save or evidence of naturally obtaining the key. LOAD, unlock, pickup and reentry use normal game input; no memory writes.',
                  execution=dict(generator_sha256=sha(generator),producer_sha256=sha(__file__),log_sha256=sha(out/'generator.log')))
    (out/'fixture.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode('utf8'))
    print('Checked artificial save fixture generated; original acquisition history is outside this checkpoint')
if __name__=='__main__':main()
