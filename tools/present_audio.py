"""Source-admitted House present clip audio; shared receipt sounds stay singletons."""
from pathlib import Path
from tools.drawer_program import read_json, fields, require, safe_path


def bindings(root):
    from tools.house_presents import load
    root = Path(root)
    ir = load(root)
    config = read_json(root / 'content/present-audio-binding.json')
    fields(config, ('schema', 'kind', 'commit', 'license_review', 'assets'), 'Present audio binding')
    require(type(config['schema']) is int and config['schema'] == 1 and config['kind'] == 'encore.present-audio.source-binding' and
            config['commit'] == ir['commit'] and type(config['license_review']) is str and config['license_review'].strip(),
            'Present audio identity/license')
    expected = {o['sound'] for o in ir['objects']}
    require(type(config['assets']) is list and len(config['assets']) == len(expected), 'Present audio coverage')
    paths, ids, outputs = set(), set(), set()
    for row in config['assets']:
        fields(row, ('source', 'identity', 'pcm', 'gain_db', 'conversion'), 'Present audio row')
        fields(row['identity'], ('kind', 'value'), 'Present audio identity')
        require(row['identity']['kind'] == 'stable' and type(row['identity']['value']) is int and
                0 < row['identity']['value'] < 2 ** 32 and row['identity']['value'] not in ids, 'Present audio stable ID')
        require(safe_path(row['source']) and row['source'] in expected and row['source'] not in paths and
                row['source'] in ir['sources'] and row['source'] + '.import' in ir['sources'], 'Present audio source')
        require(row['conversion'] is None and type(row['gain_db']) in (int, float) and row['gain_db'] == 0 and safe_path(row['pcm']) and
                row['pcm'].startswith('sound/effects/') and row['pcm'].endswith('.pcm') and row['pcm'] not in outputs,
                'Present audio conversion/path/gain')
        paths.add(row['source']); ids.add(row['identity']['value']); outputs.add(row['pcm'])
    require(paths == expected, 'Present audio incomplete coverage')
    return config['assets']
