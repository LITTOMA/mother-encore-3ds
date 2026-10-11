"""Explicit authoring-to-runtime path projection; no runtime fallback aliases."""
import json
from pathlib import Path, PurePosixPath
ROOT = Path(__file__).resolve().parents[1]

def load(root=ROOT):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result: raise ValueError('Duplicate layout key: ' + key)
            result[key] = value
        return result
    value = json.loads((Path(root)/'content/runtime-layout.json').read_text(), object_pairs_hook=unique)
    if set(value) != {'schema','kind','paths'} or type(value['schema']) is not int or value['schema'] != 1 or value['kind'] != 'encore.runtime-layout':
        raise ValueError('Unsupported runtime layout')
    paths = value['paths']
    if type(paths) is not dict or not paths or len(paths) > 256: raise ValueError('Invalid layout paths')
    from tools.romfs_layout import check_layout
    destinations = set()
    for old, new in paths.items():
        if type(old) is not str or type(new) is not str or not old.startswith('data/') or len(old.split('/')) != 2 or PurePosixPath(old).name != PurePosixPath(new).name:
            raise ValueError('Invalid layout mapping')
        check_layout([new])
        if new in destinations: raise ValueError('Duplicate layout destination')
        destinations.add(new)
    return paths

def runtime_path(value, root=ROOT):
    text = str(value)
    if not text.startswith('data/'): return text
    paths = load(root)
    if text not in paths: raise ValueError('Unmapped legacy resource: ' + text)
    return paths[text]

def source_path(value, root=ROOT):
    text = str(value)
    return 'romfs/' + runtime_path(text[6:], root) if text.startswith('romfs/') else text
