"""Owned core fixtures; not a release resource or a preplayed game save."""
import copy
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEGACY = ROOT / 'tests/fixtures/item-admission-legacy'
sys.path.insert(0, str(ROOT))
from tools import catacombs_item_catalog as catalog, native_items, native_session
from tools.extract_battle_entry import Extractor


def build(output):
    output = Path(output).resolve()
    allowed = ROOT / 'build'
    if not output.is_relative_to(allowed) or output == allowed or output.is_symlink():
        raise ValueError('Owned catalog fixture must be below build')
    data = catalog.load()
    legacy_proof=catalog.read(LEGACY/'source.json')
    for path in ('content/native-items.json','content/native-session.json'):
        record=legacy_proof['files'][path]
        if catalog.digest(ROOT/record['fixture'])!=record['sha256']:
            raise ValueError('Historical catalog baseline changed')
    original = native_session.read(LEGACY/'native-session.json')
    if original['schema'] != 11:
        raise ValueError('Legacy source baseline changed; review the catalog fixture')
    candidate = copy.deepcopy(original)
    candidate['schema'] = 12
    for row in data['entries']:
        for grant in row['grants']:
            candidate['acquisitions'].append(dict(item_id=row['source'], doses=1, max_count=1,
                                                  flag_id=grant['flag'], flag_policy=2))
        candidate['storage_policies'].append(dict(item_id=row['source'], doses=1,
                                                  total_count=len(row['grants']), required=False,
                                                  boosts=[0] * 7))
        candidate['field_items'].append(dict(item_id=row['source'], doses=1, drop=True, consume=True))
    old_items=catalog.read(LEGACY/'native-items.json')
    item_ir = catalog.append_item(Extractor(ROOT),old_items)
    legacy12 = copy.deepcopy(original)
    legacy12['schema'] = 12
    files = {'candidate.encitems': native_items.encode(native_items.lower(item_ir)),
             'candidate.encsession': native_session.encode(candidate),
             'legacy12.encsession': native_session.encode(legacy12),
             'legacy11.encsession': native_session.encode(original),
             'legacy.encitems': native_items.encode(native_items.lower(old_items))}
    for filename,path in [('legacy11.encsession','romfs/session/opening.encsession'),('legacy.encitems','romfs/ui/opening.encitems')]:
        if hashlib.sha256(files[filename]).hexdigest()!=legacy_proof['files'][path]['sha256']:
            raise ValueError('Historical catalog binary bytes changed')
    bad = []

    def reject(label, mutate):
        value = copy.deepcopy(candidate)
        mutate(value)
        bad.append(label)
        files['bad-%d.encsession' % (len(bad) - 1)] = native_session.encode(value)

    reject('duplicate source flag', lambda v: v['acquisitions'][-1].update(flag_id=v['acquisitions'][-2]['flag_id']))
    reject('inconsistent doses for same identity', lambda v: v['acquisitions'][-1].update(doses=2))
    reject('grant count beyond one source pickup', lambda v: v['acquisitions'][-1].update(max_count=2))
    reject('missing grouped storage policy', lambda v: v['storage_policies'].pop())
    reject('storage total below sum of source flags', lambda v: v['storage_policies'][-1].update(total_count=1))
    reject('storage total above sum of source flags', lambda v: v['storage_policies'][-1].update(total_count=3))
    reject('unsupported flag domain', lambda v: v['acquisitions'][-1].update(flag_policy=3))
    reject('malformed object flag path', lambda v: v['acquisitions'][-1].update(flag_id='../outside/flag'))
    reject('legacy version rejects duplicate item identities', lambda v: v.update(schema=11))
    reject('required policy cannot aggregate acquisitions', lambda v: v['storage_policies'][-1].update(required=True))
    reject('duplicate storage policy', lambda v: v['storage_policies'].append(copy.deepcopy(v['storage_policies'][-1])))
    reject('missing field item policy', lambda v: v['field_items'].pop())
    unknown = bytearray(files['candidate.encsession'])
    struct.pack_into('<I', unknown, 8, 13)
    struct.pack_into('<I', unknown, 20, 13)
    bad.append('unknown next version')
    files['bad-%d.encsession' % (len(bad) - 1)] = bytes(unknown)
    wrong_cap = bytearray(files['candidate.encsession'])
    struct.pack_into('<I', wrong_cap, 20, 11)
    bad.append('v12 mismatched capability')
    files['bad-%d.encsession' % (len(bad) - 1)] = bytes(wrong_cap)
    files['negative-count.txt'] = str(len(bad)).encode()
    manifest = dict(schema=1, negative_cases=bad,
                    sources={p: catalog.digest(ROOT / p) for p in
                             ['tools/native_session.py', 'tools/catacombs_item_catalog.py',
                              'tests/prepare_catacombs_item_catalog.py',
                               'content/catacombs-item-catalog.json',
                               'tests/fixtures/item-admission-legacy/native-session.json',
                               'tests/fixtures/item-admission-legacy/native-items.json']},
                    files={p: hashlib.sha256(v).hexdigest() for p, v in files.items()})
    files['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    marker = output / '.owned-catacombs-catalog-fixture'
    if output.exists():
        if not marker.is_file() or marker.read_text() != '1\n' or {p.name for p in output.iterdir()} != set(files) | {marker.name}:
            raise ValueError('Unowned or unexpected catalog fixture files')
        if any(not (output / p).is_file() or (output / p).is_symlink() or (output / p).read_bytes() != v for p, v in files.items()):
            raise ValueError('Changed catalog fixture')
    else:
        output.mkdir(parents=True)
        marker.write_text('1\n')
        for path, blob in files.items():
            (output / path).write_bytes(blob)
    return manifest


if __name__ == '__main__':
    print(json.dumps(build(sys.argv[1]), indent=2))
