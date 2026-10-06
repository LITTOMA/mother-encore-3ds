"""Validate the runtime asset categories before creating a console RomFS."""
from pathlib import PurePath, PurePosixPath
if __package__:
    from .resource_catalog import SUFFIXES
else:
    from resource_catalog import SUFFIXES

# Catalog roles are the authoritative binary type registry. A new typed
# resource must not need a second independent extension list to reach RomFS.
BANK_TYPES = {SUFFIXES[role] for role in ('Audio', 'MusicRegions')}
FONT_TYPES = {SUFFIXES['SourceFonts']}

EXTENSIONS = {
    'graphics': {'.t3x', '.bpx'},
    'sound': {'.pcm'} | BANK_TYPES,
    'fonts': {'.t3x'} | FONT_TYPES,
    'data': (set(SUFFIXES.values()) - BANK_TYPES - FONT_TYPES) | {'.encfx', '.encresources'},
}

# FieldSceneBundle owns its internal typed packs, which are selected through
# that checked bundle rather than separate top-level catalog entries. Derive
# their categories from the same producer recipe used to build/admit it.
def bundle_extensions():
    from pathlib import Path
    import json
    recipe = json.loads((Path(__file__).resolve().parents[1] /
                         'content/podunk-bundle-recipe.json').read_text(encoding='utf-8'))
    result = {}
    for row in recipe['packs']:
        path = PurePosixPath(row['path'])
        if path.parts[0] != 'data' or not path.suffix:
            raise ValueError('Invalid typed bundle pack category: ' + row['path'])
        result.setdefault('data', set()).add(path.suffix)
    result['shaders'] = {'.shbin'}
    return result


def check_layout(files):
    types = {k: set(v) for k, v in EXTENSIONS.items()}
    for category, suffixes in bundle_extensions().items():
        types.setdefault(category, set()).update(suffixes)
    for entry in files:
        name = entry.as_posix() if isinstance(entry, PurePath) else str(entry)
        path = PurePosixPath(name)
        if '\\' in name or ':' in name or path.is_absolute() or any(
                part in ('', '.', '..') for part in name.split('/')) or len(path.parts) < 2:
            raise ValueError('Unsafe RomFS layout path: ' + name)
        category = path.parts[0]
        if any(part.endswith('-preview') for part in path.parts[:-1]):
            raise ValueError('Legacy preview directory in RomFS: ' + name)
        if category == 'licenses':
            # release_notices separately checks the exact notice list and bytes.
            continue
        if category not in types or path.suffix not in types[category]:
            raise ValueError('Unclassified runtime resource: ' + name)
        if category == 'sound' and (len(path.parts) < 3 or
                path.parts[1] not in ('music', 'effects', 'banks') or
                (path.parts[1] == 'banks') != (path.suffix in BANK_TYPES)):
            raise ValueError('Unclassified sound resource: ' + name)


def checked_inventory(root):
    from pathlib import Path
    root = Path(root)
    if not root.is_dir() or root.is_symlink():
        raise ValueError('Missing/unsafe RomFS directory')
    entries = list(root.rglob('*'))
    if any(entry.is_symlink() for entry in entries):
        raise ValueError('Symlink in RomFS inventory')
    files = {entry.relative_to(root) for entry in entries if entry.is_file()}
    check_layout(files)
    return files


def check_catalog(path):
    if __package__:
        from .resource_catalog import decode
    else:
        from resource_catalog import decode
    from pathlib import Path
    catalog=decode(Path(path).read_bytes())
    check_layout(row['path'] for row in catalog['bindings'])
    print('Checked registered RomFS categories:',len(catalog['bindings']))


if __name__ == '__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog',required=True)
    check_catalog(parser.parse_args().catalog)
