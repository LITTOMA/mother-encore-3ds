"""Check and stage the reviewed redistribution notices for every console package."""
import hashlib
import json
from pathlib import Path


OFFICIAL_FILES = frozenset((
    'COPYING.NEWLIB', 'COPYING.RUNTIME', 'COPYING3',
    'Godot-3.6.2-LICENSE.txt', 'MOTHER-Encore-0.4.1.0-LICENSE.txt',
    'PCG-APACHE-2.0.txt', 'PCG-NOTICE.txt',
    'citro2d-1.7.0-LICENSE.txt', 'citro3d-1.7.1-LICENSE.txt',
    'libctru-2.7.0-README.txt', 'Fusion-Pixel-LICENSE-OFL.txt',
    'Galmuri-LICENSE-OFL.md',
    'CMake-3.31.10-Copyright.txt',
))
PROJECT_FILES = frozenset(('README.md', 'SOURCE_FONT_NOTICES.md', 'license-sources.json'))


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate license manifest field')
        result[key] = value
    return result


def _read(root, path):
    candidate = root / path
    if candidate.is_symlink() or not candidate.is_file():
        raise ValueError('Missing or linked redistribution notice: ' + str(path))
    if root.resolve() not in candidate.resolve().parents:
        raise ValueError('Notice escapes source root: ' + str(path))
    data = candidate.read_bytes()
    if not data or len(data) > 1024 * 1024:
        raise ValueError('Empty or oversized redistribution notice: ' + str(path))
    data.decode('utf-8')
    return data


def stage_files(root):
    """Return checked bytes under licenses/; never collect arbitrary local files."""
    root = Path(root)
    directory = root / 'docs/licenses'
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('Missing or linked notice directory')
    expected = OFFICIAL_FILES | PROJECT_FILES
    if {p.name for p in directory.iterdir()} != expected:
        raise ValueError('Unreviewed or missing redistribution notice file')
    files = {Path('licenses') / name: _read(root, Path('docs/licenses') / name)
             for name in sorted(expected)}
    manifest = json.loads(files[Path('licenses/license-sources.json')], object_pairs_hook=_unique_object)
    if (not isinstance(manifest, dict) or set(manifest) != {'schema', 'checked_on', 'files'} or
            type(manifest.get('schema')) is not int or manifest['schema'] != 1 or
            not isinstance(manifest.get('checked_on'), str) or not manifest['checked_on'] or
            not isinstance(manifest.get('files'), list)):
        raise ValueError('Unsupported license source manifest')
    seen = set()
    for entry in manifest['files']:
        if not isinstance(entry, dict) or set(entry) != {'file', 'sha256', 'source_url', 'source_revision', 'scope', 'verification'}:
            raise ValueError('Malformed license source entry')
        if (entry['source_revision'] is not None and not isinstance(entry['source_revision'], str) or
                any(not isinstance(entry[field], str) or not entry[field] for field in ('scope', 'verification'))):
            raise ValueError('Malformed license source provenance')
        name = entry.get('file')
        if not isinstance(name, str) or name not in OFFICIAL_FILES or name in seen:
            raise ValueError('Unknown or duplicate license source entry')
        if not isinstance(entry.get('source_url'), str) or not entry['source_url'].startswith('https://'):
            raise ValueError('Missing official license source URL')
        digest = hashlib.sha256(files[Path('licenses') / name]).hexdigest()
        if entry.get('sha256') != digest:
            raise ValueError('Redistribution notice fingerprint mismatch: ' + name)
        seen.add(name)
    if seen != OFFICIAL_FILES:
        raise ValueError('Incomplete official license source manifest')
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        files[Path('licenses') / name] = _read(root, Path(name))
    return files
