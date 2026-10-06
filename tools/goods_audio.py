"""Original Goods open/close sounds, with shared menu identities kept intact."""
from pathlib import Path
import hashlib
from tools.drawer_program import read_json, fields, require, safe_path
from tools.field_goods import load


def bindings(root):
    root = Path(root)
    # load() checks complete source IR/review and inventory dependency identity.
    ir = load()
    config = read_json(root / 'content/goods-audio-binding.json')
    fields(config, ('schema', 'kind', 'commit', 'license_review', 'assets'), 'Goods audio binding')
    require(type(config['schema']) is int and config['schema'] == 1 and
            config['kind'] == 'encore.goods-audio.source-binding' and
            config['commit'] == ir['commit'] and type(config['license_review']) is str and
            config['license_review'].strip(), 'Goods audio identity/license')
    # Five sound selectors are checked by the source Goods schema; only Open
    # and Close require new bank assets. Move/Confirm/Back retain old sources.
    expected = {ir['sounds'][0], ir['sounds'][4]}
    require(type(config['assets']) is list and len(config['assets']) == len(expected),
            'Goods audio coverage')
    paths, ids, outputs = set(), set(), set()
    for row in config['assets']:
        fields(row, ('source', 'identity', 'pcm', 'gain_db', 'conversion'), 'Goods audio row')
        fields(row['identity'], ('kind', 'value'), 'Goods audio identity')
        require(row['identity']['kind'] == 'stable' and type(row['identity']['value']) is int and
                0 < row['identity']['value'] < 2**32 and row['identity']['value'] not in ids,
                'Goods audio stable ID')
        p = row['source']
        require(safe_path(p) and p in expected and p not in paths and
                p in ir['sources'] and p + '.import' in ir['sources'], 'Goods audio source')
        for path in (p, p + '.import'):
            require(hashlib.sha256((root / 'upstream/MOTHER-Encore' / path).read_bytes()).hexdigest()
                    == ir['sources'][path], 'Changed Goods audio source/import: ' + path)
        require(row['conversion'] is None and type(row['gain_db']) in (int, float) and
                row['gain_db'] == 0 and safe_path(row['pcm']) and
                row['pcm'].startswith('sound/effects/') and row['pcm'].endswith('.pcm') and
                row['pcm'] not in outputs, 'Goods audio conversion/path/gain')
        paths.add(p)
        ids.add(row['identity']['value'])
        outputs.add(row['pcm'])
    require(paths == expected, 'Goods audio incomplete coverage')
    return config['assets']
