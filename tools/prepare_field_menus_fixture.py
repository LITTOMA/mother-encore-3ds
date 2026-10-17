#!/usr/bin/env python3
"""Run the checked C++ menu fixture producer and retain its exact provenance.

The snapshot is an artificial admitted boundary, never a played-progress save.
"""
import argparse,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.run_emulator_semantics import sha,validate_save_fixture,is_within

def prepare(producer,receipt_path,out):
    producer=Path(producer).resolve();out=Path(out).resolve()
    if out.exists() or not is_within(out,ROOT/'build') or not producer.is_file() or not is_within(producer,ROOT/'build'):raise ValueError('Use a built producer and fresh directory within build')
    receipt=json.loads(Path(receipt_path).read_text(encoding='utf8'))
    commit=receipt['source_commit']
    if len(commit)!=40 or any(c not in '0123456789abcdef'for c in commit):raise ValueError('Cross-build source identity rejected')
    subprocess.run(['git','diff','--exit-code',commit,'--','runtime','include','platform','romfs','content','tools/emulator_field_menus_fixture.cpp'],cwd=ROOT,check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    out.mkdir(parents=True);save=out/'field-menus.encsave';log=out/'generator.log'
    with log.open('wb')as stream:result=subprocess.run([str(producer),str(save)],cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,timeout=30)
    if result.returncode or not save.is_file() or not 0<save.stat().st_size<=1024*1024 or b'Artificial admitted menu save:'not in log.read_bytes():raise RuntimeError('C++ fixture validation failed; inspect retained generator.log')
    resources=['session/opening.encsession','world/house/opening.encroom','world/house/opening.enchouse','world/house/opening.encpresent','ui/opening.encitems','battle/lamp/opening.encround','ui/field.encmenus','ui/opening.encdetails','ui/opening.encsettings','ui/opening.encfield']
    fixture=dict(schema=1,kind='encore.emulator-menu-fixture',source_commit=commit,upstream_commit='7d9246600fffe518408f5830d4848635019005a3',file=save.name,sha256=sha(save),resources={name:sha(ROOT/'romfs'/name)for name in resources},scope='Artificial native-validated level-2 checkpoint with all admitted acquisition/skill flags; encode/decode/revalidate in C++; not a naturally played route',execution=dict(generator_sha256=sha(ROOT/'tools/emulator_field_menus_fixture.cpp'),producer_sha256=sha(producer),log_sha256=sha(log)))
    manifest=out/'fixture.json';manifest.write_text(json.dumps(fixture,indent=2)+'\n',encoding='utf8');validate_save_fixture(manifest,receipt);return manifest

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--producer',type=Path,required=True);p.add_argument('--receipt',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(prepare(a.producer,a.receipt,a.out))
if __name__=='__main__':main()
