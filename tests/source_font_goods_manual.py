"""Manual-only exact Goods/font provenance gate cases with genuine resources."""
import copy
import json
from unittest.mock import patch


def manual_goods_font_provenance_cases(romfs):
    from tools import source_fonts
    files = source_fonts.stage_files(romfs)
    assert files and 'fonts/source-fonts.encfont' in {p.as_posix() for p in files}
    original = json.loads
    receipt = source_fonts.ROOT / 'content/asset-receipts/fonts/source.json'
    expected = original(receipt.read_bytes())
    assert expected['field_goods_sha256'] == source_fonts.sha(
        source_fonts.ROOT / 'content/native-field-goods.json')
    assert expected['field_goods_producer_sha256'] == source_fonts.sha(
        source_fonts.ROOT / 'tools/field_goods.py')
    for key in ('field_goods_sha256', 'field_goods_producer_sha256'):
        bad = copy.deepcopy(expected); bad[key] = '0' * 64
        def decode(raw, *args, **kwargs):
            value = original(raw, *args, **kwargs)
            return bad if isinstance(value, dict) and 'field_goods_sha256' in value else value
        with patch.object(source_fonts.json, 'loads', side_effect=decode):
            try:
                source_fonts.stage_files(romfs)
            except ValueError:
                continue
            raise AssertionError('changed Goods font provenance accepted')


def manual_unreviewed_source_font_hinting_cases():
    from pathlib import Path
    from tempfile import TemporaryDirectory
    from tools.source_fonts import data_settings
    with TemporaryDirectory() as temporary:
        root = Path(temporary)
        f = root / 'fixture.tres'
        for property_line in ('hinting = 0', 'antialiased = 7', 'unknown = true'):
            f.write_text('[sub_resource type="DynamicFontData" id=1]\n'
                         'font_path = "res://font.otf"\n' + property_line + '\n'
                         '[resource]\n', encoding='utf-8')
            try:
                data_settings(root, 'fixture.tres', ['font.otf'])
            except ValueError:
                continue
            raise AssertionError('unknown source font data property accepted')
