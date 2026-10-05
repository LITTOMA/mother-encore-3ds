"""Validate the runtime asset categories before creating a console RomFS."""
from pathlib import PurePath, PurePosixPath

EXTENSIONS = {
    'graphics': {'.t3x', '.bpx'},
    'sound': {'.pcm', '.encaudio', '.encmusic'},
    'fonts': {'.t3x', '.encfont'},
    'data': {
        '.encbars', '.encbattle', '.encchoices', '.enccontinue',
        '.encfx', '.enchouse', '.encinput', '.encintro', '.encinspect', '.encitems', '.encload',
        '.enclocale', '.encmigration', '.encnewgame', '.encphone',
        '.encprompts', '.encresources', '.encrestore', '.encroom', '.encround',
        '.encsavemenu', '.encsession', '.encsettings', '.enctitlelocale',
    },
}


def check_layout(files):
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
        if category not in EXTENSIONS or path.suffix not in EXTENSIONS[category]:
            raise ValueError('Unclassified runtime resource: ' + name)
        if category == 'sound' and (len(path.parts) < 3 or
                path.parts[1] not in ('music', 'effects', 'banks') or
                (path.parts[1] == 'banks') != (path.suffix in ('.encaudio', '.encmusic'))):
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
