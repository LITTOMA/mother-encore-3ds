"""Artificial checkpoint CLI admission; no original progression claim."""
from pathlib import Path
import subprocess
import sys
import tempfile

exe = str(Path(sys.argv[1]).resolve())
parent = Path(sys.argv[2]).resolve()
parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='bridge-fixture-cli-', dir=parent) as temp:
    folder = Path(temp)
    blobs = {}
    for name, options in (
        ('base', []),
        ('melody', ['--post-phone-melody']),
        ('map', ['--with-podunk-map']),
        ('both', ['--post-phone-melody', '--with-podunk-map']),
        ('reverse', ['--with-podunk-map', '--post-phone-melody']),
    ):
        target = folder / (name + '.encsave')
        result = subprocess.run([exe, str(target), *options], capture_output=True)
        assert result.returncode == 0, result.stdout + result.stderr
        blobs[name] = target.read_bytes()
        assert 0 < len(blobs[name]) <= 1048576
    assert blobs['base'] != blobs['melody']
    assert blobs['base'] != blobs['map']
    assert blobs['melody'] != blobs['both']
    assert blobs['both'] == blobs['reverse']
    rejected = folder / 'rejected.encsave'
    for options in (
        ['--unknown'],
        ['--with-podunk-map', '--with-podunk-map'],
        ['--post-phone-melody', '--post-phone-melody'],
        ['--with-podunk-map', '--post-phone-melody', '--unknown'],
    ):
        assert subprocess.run([exe, str(rejected), *options], capture_output=True).returncode == 1
        assert not rejected.exists()
    assert subprocess.run([exe], capture_output=True).returncode == 1
print('PASS five validated fixture variants and five rejected argument cases')
