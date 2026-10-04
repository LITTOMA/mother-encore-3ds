"""CI-only LibYAML safe parser selection, enabled by explicit PYTHONPATH.

Both loaders use PyYAML's safe constructor and resolver. Production source
hashes, recipe checks and fresh extraction comparisons still run normally.
Ordinary developer invocations retain PyYAML's default Python SafeLoader.
"""
import yaml

if yaml.__version__ != '6.0.3' or not getattr(yaml, '__with_libyaml__', False):
    raise SystemExit('CI safe parser requires pinned PyYAML 6.0.3 with LibYAML')

yaml.SafeLoader = yaml.CSafeLoader
