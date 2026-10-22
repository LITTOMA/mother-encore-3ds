"""Reviewed catalog identities/art for the seven original catacombs pickups.

This producer extends the existing item catalog. Acquisition conservation and
field effects are separate consumers; this module does not admit a scene.
"""
from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import io
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import dropped_items as dropped
from tools.extract_battle_entry import Extractor, PIN, require

IR = Path('content/catacombs-item-catalog.json')
REVIEW = Path('compatibility/reviews/catacombs-item-catalog-v0410.json')
RECEIPT = Path('content/asset-receipts/graphics/items/catacombs/source.json')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate catalog field')
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


def canonical(value):
    return json.dumps(value, sort_keys=True, ensure_ascii=False, separators=(',', ':'), allow_nan=False)


def derive():
    recipe, model, proof = dropped.source()
    ex = Extractor(ROOT)
    ex.text('LICENSE')
    translations = {}
    for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/items - sheet.csv'))):
        require(row['key'] not in translations, 'Duplicate catalog translation')
        translations[row['key']] = row
    entries = []
    templates = model['templates']
    require(len(templates) == 3 and len(model['bindings']) == 7, 'Catalog source coverage changed')
    # Existing stable definitions occupy 1..4. New IDs append in the original
    # scene's first-appearance order, independently of resource table offsets.
    for index, template in enumerate(templates):
        document = ex.yaml(template['yaml'])
        require(document == template['document'], 'Catalog pickup definition changed')
        common = set('name sorting_name description article keyitem cost value actions transform slot HPrecover PPrecover boost'.split())
        extra = set(document) - common
        require(extra in ({'is_food'}, {'status_heals'}) and set(document) == common | extra,
                'Catalog unsupported item fields')
        require(document['keyitem'] is False and document['transform'] == document['slot'] == ''
                and type(document['HPrecover']) is int and document['HPrecover'] >= 0
                and document['PPrecover'] == 0
                and set(document['boost']) == set('maxhp maxpp offense defense speed iq guts'.split())
                and all(type(v) is int and v == 0 for v in document['boost'].values())
                and len(document['actions']) == 1
                and set(document['actions'][0]) == {'name', 'function'}
                and document['actions'][0]['function'] == 'consume',
                'Catalog unsupported item mechanism')
        require((extra == {'is_food'} and document['is_food'] is True and document['HPrecover'] > 0)
                or (extra == {'status_heals'} and document['status_heals'] == ['poisoned']
                    and document['HPrecover'] == 0), 'Catalog unreviewed healing effect')
        source = template['source']
        require(re.fullmatch('[A-Za-z][A-Za-z0-9]*', source) is not None,
                'Catalog unsafe source identity')
        source_texture = template['texture']
        size = ex.png_size(source_texture)
        ex.data(source_texture + '.import')
        require(size == [20, 20] and template['doses'] == 1, 'Catalog source icon/dose changed')
        labels = {}
        for locale, column in [('en', 'en'), ('zh', 'zh_CN')]:
            labels[locale] = {}
            for field in ('name', 'sorting_name', 'description', 'article'):
                key = document[field]
                require(key in translations and type(translations[key][column]) is str,
                        'Catalog missing source translation')
                labels[locale][field] = translations[key][column].replace('\\n', '\n')
        grants = [{'owner': v['id'], 'flag': v['flag'], 'kind': v['flag_kind']}
                  for v in model['bindings'] if v['template'] == template['id']]
        require(grants and all(v['kind'] == 'object' for v in grants), 'Catalog unknown acquisition')
        entries.append(dict(id=5 + index, source=source, template=template['id'], document=document,
                            labels=labels, grants=grants, texture=source_texture, size=size,
                            output='graphics/items/catacombs/' + source.lower() + '.t3x'))
    require(len({v['source'] for v in entries}) == 3
            and len({g['flag'] for v in entries for g in v['grants']}) == 7,
            'Catalog duplicate item or pickup identity')
    return dict(schema=1, kind='encore.catacombs-item-catalog', commit=PIN,
                dropped_recipe_sha256=digest(dropped.RECIPE), sources=ex.sources,
                entries=entries,
                scope='Three original item definitions appended after stable IDs 1..4; seven distinct object-flag acquisitions. No new scene or save admission.',
                permission='Original game art under the pinned upstream game-related derivative permission; no fonts or third-party binaries copied.')


def load(path=None):
    path = ROOT / IR if path is None else Path(path)
    candidate = read(path)
    require(canonical(candidate) == canonical(derive()), 'Unreviewed catalog data or source binding')
    review = read(ROOT / REVIEW)
    require(canonical(review) == canonical(dict(schema=1, commit=PIN, ir_sha256=digest(path),
                           producer_sha256=digest(__file__), sources=candidate['sources'],
                           scope=candidate['scope'], permission=candidate['permission'])),
            'Catalog review mismatch')
    return candidate


def receipt(source_root=None):
    data = load()
    proof = read(ROOT / RECEIPT)
    require(set(proof) == set('schema commit ir_sha256 producer_sha256 tex3ds_sha256 outputs'.split())
            and type(proof['schema']) is int and proof['schema'] == 1 and proof['commit'] == PIN
            and proof['ir_sha256'] == digest(ROOT / IR)
            and proof['producer_sha256'] == digest(__file__)
            and type(proof['tex3ds_sha256']) is str
            and re.fullmatch('[0-9a-f]{64}', proof['tex3ds_sha256']) is not None,
            'Catalog texture provenance rejected')
    require(type(proof['outputs']) is dict
            and set(proof['outputs']) == {v['output'] for v in data['entries']},
            'Catalog output coverage rejected')
    base = ROOT / 'romfs' if source_root is None else Path(source_root)
    for relative, record in proof['outputs'].items():
        path = base / relative
        require(type(record) is dict and type(record.get('bytes')) is int
                and not path.is_symlink() and path.is_file()
                and record == dict(bytes=path.stat().st_size, sha256=digest(path)),
                'Catalog texture bytes changed')
    return proof


def append_item(ex, ir):
    data = load()
    proof = receipt()
    result = copy.deepcopy(ir)
    require([v['id'] for v in result['definitions']] == list(range(1, 5)),
            'Catalog stable prefix changed')
    for row in data['entries']:
        require(row['id'] == len(result['definitions']) + 1
                and all(v['source'] != row['source'] for v in result['definitions']),
                'Catalog duplicate or reordered stable definition')
        icon = len(result['resources'])
        result['resources'].append(dict(id=max(v['id'] for v in result['resources']) + 1,
                                       path=row['output'], kind=1, width=row['size'][0],
                                       height=row['size'][1], columns=1, rows=1,
                                       sha256=proof['outputs'][row['output']]['sha256']))
        document = row['document']
        result['definitions'].append(dict(id=row['id'], source=row['source'],
                                          name=row['labels']['en']['name'],
                                          description=row['labels']['en']['description'],
                                          icon=icon, equipment_slot=0xffffffff,
                                          heal_hp=0, heal_pp=0,
                                          max_hp_boost=0, max_pp_boost=0, flags=2,
                                          can_use=0))
    for path in (IR, REVIEW, RECEIPT):
        result['dependencies'][path.as_posix()] = digest(ROOT / path)
    result['scope'] += '; reviewed catacombs item identities/art; effects and session acquisition require independent admission'
    return result


def stage_files(source_root):
    proof = receipt(source_root)
    return {Path(p): (Path(source_root) / p).read_bytes() for p in proof['outputs']}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('extract', 'assets', 'verify'))
    parser.add_argument('--tex3ds', type=Path)
    parser.add_argument('--tex3ds-wrapper', type=Path)
    args = parser.parse_args()
    if args.action == 'extract':
        data = derive()
        write(ROOT / IR, data)
        write(ROOT / REVIEW, dict(schema=1, commit=PIN, ir_sha256=digest(ROOT / IR),
                                 producer_sha256=digest(__file__), sources=data['sources'],
                                 scope=data['scope'], permission=data['permission']))
    elif args.action == 'assets':
        data = load()
        require(args.tex3ds is not None and args.tex3ds.is_file(), 'Genuine tex3ds required')
        outputs = {}
        for row in data['entries']:
            target = ROOT / 'romfs' / row['output']
            target.parent.mkdir(parents=True, exist_ok=True)
            command = [sys.executable, str(args.tex3ds_wrapper)] if args.tex3ds_wrapper else [str(args.tex3ds)]
            subprocess.run(command + ['-f', 'rgba8', '-z', 'none', '-o', str(target),
                                      str(ROOT / 'upstream/MOTHER-Encore' / row['texture'])],
                           cwd=ROOT, check=True)
            outputs[row['output']] = dict(bytes=target.stat().st_size, sha256=digest(target))
        write(ROOT / RECEIPT, dict(schema=1, commit=PIN, ir_sha256=digest(ROOT / IR),
                                  producer_sha256=digest(__file__), tex3ds_sha256=digest(args.tex3ds),
                                  outputs=outputs))
        receipt()
    else:
        receipt()


if __name__ == '__main__':
    main()
