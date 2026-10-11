"""One-shot dependency migration for unchanged, reviewed UI asset recipes.

Only accepts dependency JSON changes confined to provenance/dependencies, or
byte-identical relocated binaries. It cannot approve changed game content.
Run after the original room/house/round/session extractors, before compilation.
"""
import argparse, copy, hashlib, json, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.runtime_layout import source_path, load as load_layout
FILES=('content/native-new-game.json','content/native-house-button-prompts.json','content/native-continue.json','content/native-title-locales.json')
def sha(b):return hashlib.sha256(b).hexdigest()
def old_file(base,path):return subprocess.check_output(['git','show',base+':'+path],cwd=ROOT)
def semantic(document):
    value=copy.deepcopy(document)
    value.pop('dependencies',None);value.pop('provenance',None)
    return value

def migrate(base):
    pending=[]
    for name in FILES:
        path=ROOT/name;current=json.loads(path.read_text(encoding='utf-8'));previous=json.loads(old_file(base,name))
        if semantic(current)!=semantic(previous):raise ValueError('UI asset semantics changed: '+name)
        dependencies={}
        for dep,expected in previous['dependencies'].items():
            old=old_file(base,dep)
            if sha(old)!=expected:raise ValueError('Baseline dependency fingerprint mismatch: '+dep)
            physical=source_path(dep);new=(ROOT/physical).read_bytes()
            if old!=new:
                if not dep.endswith('.json') or semantic(json.loads(old))!=semantic(json.loads(new)):
                    raise ValueError('Non-metadata dependency change rejected: '+dep)
            dependencies[physical]=sha(new)
        current['dependencies']=dependencies
        pending.append((path,(json.dumps(current,indent=2,ensure_ascii=False)+'\n').encode('utf-8')))
    for path,blob in pending:path.write_bytes(blob);print('Migrated unchanged UI recipe dependencies:',path.relative_to(ROOT))
def projected_producer(old, current, paths):
    """Only quoted resource path substitutions may change a texture producer."""
    expected=old.decode('utf-8')
    for before,after in paths.items():
        for prefix in ('','romfs/'):
            for quote in ("'",'"'):
                expected=expected.replace(quote+prefix+before+quote,quote+prefix+after+quote)
    if expected.encode('utf-8')!=current:
        raise ValueError('Texture producer has changes beyond exact resource paths')

def migrate_receipts(base):
    pending=[]
    for tool,ir,receipt in (
        ('tools/item_details.py','content/native-item-details.json','content/asset-receipts/graphics/ui/item-details/source.json'),
        ('tools/field_equipment.py','content/native-field-equipment.json','content/asset-receipts/graphics/ui/equipment/source.json')):
        previous=json.loads(old_file(base,receipt));current=json.loads((ROOT/receipt).read_text(encoding='utf-8'))
        old_producer=old_file(base,tool);new_producer=(ROOT/tool).read_bytes()
        projected_producer(old_producer,new_producer,load_layout())
        if previous['producer_sha256']!=sha(old_producer) or previous['ir_sha256']!=sha((ROOT/ir).read_bytes()):
            raise ValueError('Baseline texture source/producer mismatch: '+receipt)
        expected=copy.deepcopy(previous);expected['producer_sha256']=sha(new_producer)
        if current not in (previous,expected):raise ValueError('Unreviewed texture receipt change: '+receipt)
        for name,output in previous['outputs'].items():
            data=(ROOT/'romfs'/name).read_bytes()
            if len(data)!=output['bytes'] or sha(data)!=output['sha256']:
                raise ValueError('Texture output changed: '+name)
        pending.append((ROOT/receipt,json.dumps(expected,indent=2,ensure_ascii=False)+'\n'))
    for path,value in pending:path.write_text(value,encoding='utf-8');print('Migrated path-only texture producer:',path.relative_to(ROOT))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--base',required=True);p.add_argument('--receipts-only',action='store_true');args=p.parse_args()
    if args.receipts_only:migrate_receipts(args.base)
    else:migrate(args.base)
