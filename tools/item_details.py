#!/usr/bin/env python3
"""Pinned item-description IR, genuine texture conversion and ENCITD01 compiler.

Offline JSON and receipts are never game inputs. This resource describes only
the admitted item descriptions; it grants no item-use or status-healing action.
"""
from __future__ import annotations
import argparse, csv, io, math, re, shutil, struct, subprocess, sys, zlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.extract_battle_entry import Extractor, PIN, require, node
from tools.drawer_program import canonical, digest, fields, read_json, write_json, safe_path

IR = 'content/native-item-details.json'
RECIPE = 'content/item-details-assets.json'
REVIEW = 'reports/item-details-source/source-review.json'
RECEIPT = 'content/asset-receipts/graphics/ui/item-details/source.json'
PACK = 'romfs/data/opening.encdetails'
ICON = 'Graphics/UI/Ailments/Asthma.png'
OUTPUT = 'graphics/ui/item-details/asthma-status.t3x'
NAMES = ('Strings', 'Definitions', 'Locales', 'Presentations', 'Tokens', 'Resources', 'Parameters')
FORMATS = (None, '<5I', '<8I', '<4I', '<4I', '<7I32s2I', '<If')
STRIDES = (1, 20, 32, 16, 16, 68, 8)
HEADER = 176
TOKEN_KINDS = {'Text': 1, 'Nickname': 2, 'Doses': 3, 'InlineImage': 4, 'Newline': 5, 'ItemValue': 6}
PARAMETERS = ('LineHeight', 'ImageBaseline', 'ENNameLimit', 'ZHNameLimit')


def table(ex, path):
    result = {}
    for row in csv.DictReader(io.StringIO(ex.text(path))):
        require(row['key'] not in result, 'Duplicate details translation key')
        result[row['key']] = row
    return result


def tokenize(raw):
    require(isinstance(raw, str) and '\0' not in raw, 'Details text type')
    raw = raw.replace('\\n', '\n').replace('[BR]', '\n')
    result = []
    for part in re.split(r'(\[Ninten\]|\[Asthma\]|\[ItemValue\]|%s|\n)', raw):
        if not part:
            continue
        kind = {'[Ninten]': 'Nickname', '[Asthma]': 'InlineImage', '[ItemValue]': 'ItemValue',
                '%s': 'Doses', '\n': 'Newline'}.get(part, 'Text')
        require(kind != 'Text' or not any(c in part for c in '[]%{}'), 'Unreviewed details text control')
        result.append(dict(kind=kind, value=part if kind == 'Text' else 0, color=0))
    require(result and len(result) <= 128, 'Details token capacity')
    return result


def build(root=ROOT):
    root = Path(root); ex = Extractor(root)
    text = ex.text('Scripts/global/text_tools.gd')
    description = ex.text('Scripts/UI/Reusables/Description.gd')
    item_script = ex.text('Scripts/global/Item.gd')
    for snippet in ('TextTools.replace_text(item.get_data().get("description"))',
                    'TextTools.get_item_doses_phrase(item)', 'if item_phrase: text %= item_phrase',
                    'TextTools.add_line_breaks(_text, _text_label)'):
        require(snippet in description, 'Details source update changed: ' + snippet)
    for snippet in ('var sep := _tr("WORD_SEPARATOR")', 'phrase.split("\\n")',
                    'font.get_string_size(strip_bbcode(result_line + sep + words[i])).x > max_length',
                    'var nb_uses: int = item.get_data().get("doses", 1)',
                    'item.doses in range(0, nb_uses)',
                    'return name.substr(0, max_length)',
                    'img_regex.sub(source, "——", true)',
                    'res://Graphics/UI/Ailments/%s.png',
                    'return plural if number >= 2 else singular'):
        require(snippet in text, 'Details source semantics changed: ' + snippet)
    require('self.doses = get_data().get("doses", 1)' in item_script, 'Details default doses changed')
    color = re.findall(r'const DIALOG_HINT_COLOR := "([0-9a-f]{6})"', text)
    require(len(color) == 1, 'Details hint color selector')
    hint = int.from_bytes(bytes.fromhex(color[0]) + b'\xff', 'little')
    font = ex.text('Fonts/EBMain_la.tres')
    require('extra_spacing_top = -1' in font and 'extra_spacing_bottom = -1' in font,
            'Details main font spacing changed')
    for p in re.findall(r'path="res://([^"\n]+)"', font):
        ex.data(p)
    scene = ex.text('Nodes/Ui/Description.tscn')
    label = node(scene, 'HBox/MarginContainer/Desc')
    require(label['scroll_active'] is False and label['fit_content_height'] is True
            and label['margin_right'] == 213 and label['margin_bottom'] == 29,
            'Details source label geometry changed')
    ailment_font = ex.text('Graphics/UI/Ailments/Ailments.tres')
    baseline = re.findall(r'^ascent = (\d+(?:\.\d+)?)$', ailment_font, re.M)
    require(len(baseline) == 1 and float(baseline[0]) == 1 and '\nheight =' not in ailment_font,
            'Details inline image font default height/ascent')
    # TextTools lowercases the status path; the pinned asset uses title case.
    # Admit the unique case-folded match explicitly for case-sensitive builds.
    matching = [p for p in ex.inventory['files'] if p.casefold() == ICON.casefold()]
    require(matching == [ICON], 'Ambiguous inline asthma path casing')
    size = ex.png_size(ICON); ex.data(ICON + '.import'); ex.text('LICENSE')
    items = table(ex, 'Translations/TranslatedText/items - sheet.csv')
    menus = table(ex, 'Translations/TranslatedText/menus - sheet.csv')
    native_items = read_json(root / 'content/native-items.json')
    require([(d['id'], d['source']) for d in native_items['definitions']]
            == [(1, 'BaseballCap'), (2, 'AsthmaSpray')], 'Unsupported details item scope')
    definitions = []; presentations = []
    for definition_index, d in enumerate(native_items['definitions']):
        doc = ex.yaml('Data/Items/' + d['source'] + '.yaml')
        boosts = doc.get('boost', {})
        value = next((v for v in boosts.values() if v > 0),
                     next((doc[k] for k in ('PPrecover', 'HPrecover') if doc.get(k, 0) > 0), 0))
        require(type(value) is int and value >= 0, 'Details item value')
        definitions.append(dict(id=definition_index, source=d['source'], raw_description=d['description'],
                                max_doses=doc.get('doses', 1), item_value=value))
        for lang, column in (('en', 'en'), ('zh_Hans_CN', 'zh_CN')):
            raw = items[doc['description']][column]
            tokens = tokenize(raw)
            if d['source'] == 'AsthmaSpray':
                require([t['kind'] for t in tokens if t['kind'] != 'Text']
                        == ['Nickname', 'InlineImage', 'Doses'], 'Spray details controls changed')
            else:
                require([t['kind'] for t in tokens if t['kind'] != 'Text']
                        == ['Newline', 'ItemValue'], 'Cap details controls changed')
            presentations.append(dict(definition=definition_index, locale=lang, raw=raw, tokens=tokens))
    locales = []
    for lang, column in (('en', 'en'), ('zh_Hans_CN', 'zh_CN')):
        total = menus['INVENTORY_ITEM_USES_TOTAL'][column]
        left = menus['INVENTORY_ITEM_USES_LEFT'][column]
        articles = menus['ARTICLES_NUMBERS'][column]
        require(articles == '[plur_num:{0}::s]' and total.count('{value}') == 1
                and left.count('{value}') == 1, 'Unreviewed details dose phrase grammar')
        total = total.replace('{value}', '%s')
        singular = left.replace('{value}', '%s').replace('{v0}', '')
        plural = left.replace('{value}', '%s').replace('{v0}', 's')
        require(not any(c in total + singular + plural for c in '[]{}'), 'Unreviewed dose controls')
        locales.append(dict(code=lang, separator=menus['WORD_SEPARATOR'][column], total=total,
                            left_singular=singular, left_plural=plural, image_measure='——',
                            base_color=0xffffffff, hint_color=hint))
    name_limits = [len(menus['LONGEST_POSSIBLE_NAME'][c]) for c in ('en', 'zh_CN')]
    # Main native font's checked 12px height plus RichTextLabel's default 1px
    # line separation, as used by the existing admitted Items layout.
    height = native_items['parameters']['LabelSize'][1] + 1
    return dict(schema=1, kind='encore.native-item-details.source-ir', commit=PIN,
                scope='Checked BaseballCap/AsthmaSpray descriptions only; en/zh, nickname, inline asthma and actual doses; no item action',
                sources=dict(sorted(ex.sources.items())),
                dependencies={'content/native-items.json': digest(root / 'content/native-items.json')},
                definitions=definitions, locales=locales, presentations=presentations,
                resources=[dict(id=1, source=ICON, path=OUTPUT, kind=1, width=size[0], height=size[1], columns=1, rows=1)],
                # Godot3.6.2 BitmapFont defaultheight1 minus ascent1 gives
                # descent0. Default INLINE_ALIGN_BASELINE places image at
                # lineHeight - (imageFontDescent + imageHeight), not at ascent.
                parameters=dict(zip(PARAMETERS, [height, max(native_items['parameters']['LabelSize'][1],size[1])-size[1], *name_limits])))


def recipe(ir):
    return dict(schema=1, kind='encore.item-details.asset-recipe', commit=PIN,
                sources=ir['sources'], resources=ir['resources'],
                license_review='Pinned upstream LICENSE permits game-related forks/modifications; original art retains upstream terms, not MIT.',
                format='rgba8', compression='none')


def extract(root=ROOT):
    root = Path(root); ir = build(root)
    write_json(root / IR, ir); write_json(root / RECIPE, recipe(ir))
    write_json(root / REVIEW, dict(schema=1, commit=PIN, ir_sha256=digest(root / IR), sources=ir['sources'],
        semantics=['No use action enabled; item identity, old raw description, flags and save unchanged',
                   'Description.set_item translates and replaces nickname/status/item value, then formats current doses',
                   'Nickname scalar length follows locale LONGEST_POSSIBLE_NAME; en7/zh6',
                   'Doses outside range0..max-1 show original total phrase;0/1/2 show remaining with original plural suffix',
                   'TextTools.add_line_breaks measures inline image as two em dashes; EN ordinary space; ZH empty separator',
                   'Original inline texture uses BitmapFont defaultheight1/ascent1/descent0; Godot3.6.2 default baseline alignment gives lineheight12-imageheight12=0 vertical offset',
                   'Nearest tex3ds conversion; main text EBMain_la; explicit unique casefold mapping fixes original lowercase status path versus titlecase asset'],
        unverified=['Manual tests not run', '3DS device display/input validation pending']))
    return ir


def load(root=ROOT):
    root = Path(root); ir = read_json(root / IR)
    require(canonical(ir) == canonical(build(root)), 'Stale/unreviewed item details IR')
    review = read_json(root / REVIEW)
    fields(review, ('schema', 'commit', 'ir_sha256', 'sources', 'semantics', 'unverified'), 'Details review')
    require(review['schema'] == 1 and review['commit'] == PIN and review['sources'] == ir['sources']
            and review['ir_sha256'] == digest(root / IR), 'Details review mismatch')
    require(read_json(root / RECIPE) == recipe(ir), 'Details recipe mismatch')
    return ir


def compile_assets(tex3ds, root=ROOT):
    root = Path(root); ir = load(root); tex3ds = Path(tex3ds)
    require(tex3ds.is_file(), 'Need genuine tex3ds')
    target = root / 'romfs' / OUTPUT; target.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(tex3ds), '-f', 'rgba8', '-z', 'none', '-o', str(target),
                    str(root / 'upstream/MOTHER-Encore' / ICON)], check=True)
    receipt = dict(schema=1, commit=PIN, ir_sha256=digest(root / IR), producer_sha256=digest(root / 'tools/item_details.py'),
                   tex3ds_sha256=digest(tex3ds), outputs={OUTPUT: dict(bytes=target.stat().st_size, sha256=digest(target), crc32=zlib.crc32(target.read_bytes()))})
    write_json(root / RECEIPT, receipt)
    return receipt


def verify_receipt(ir, root=ROOT, source=None):
    root = Path(root); r = read_json(root / RECEIPT)
    fields(r, ('schema', 'commit', 'ir_sha256', 'producer_sha256', 'tex3ds_sha256', 'outputs'), 'Details receipt')
    require(r['schema'] == 1 and r['commit'] == PIN and r['ir_sha256'] == digest(root / IR)
            and r['producer_sha256'] == digest(root / 'tools/item_details.py')
            and re.fullmatch('[0-9a-f]{64}', r['tex3ds_sha256']) is not None, 'Details receipt provenance')
    fields(r['outputs'], (OUTPUT,), 'Details receipt outputs')
    out = r['outputs'][OUTPUT]; fields(out, ('bytes', 'sha256', 'crc32'), 'Details texture receipt')
    texture = (root / 'romfs' if source is None else Path(source)) / OUTPUT
    require(texture.stat().st_size == out['bytes'] and digest(texture) == out['sha256']
            and zlib.crc32(texture.read_bytes()) == out['crc32'], 'Details texture receipt mismatch')
    return r


def lower(ir, receipt):
    fields(ir, ('schema','kind','commit','scope','sources','dependencies','definitions','locales','presentations','resources','parameters'), 'Details IR')
    require(type(ir['schema']) is int and ir['schema'] == 1 and ir['kind'] == 'encore.native-item-details.source-ir'
            and ir['commit'] == PIN, 'Details IR schema/pin')
    fields(ir['parameters'], PARAMETERS, 'Details parameters')
    pool = bytearray(b'\0'); offsets = {'': 0}
    def string(value):
        require(isinstance(value, str) and '\0' not in value and len(value.encode('utf-8')) <= 4096, 'Details string')
        if value not in offsets:
            offsets[value] = len(pool); pool.extend(value.encode('utf-8') + b'\0')
        return offsets[value]
    t = {n: [] for n in NAMES}
    for d in ir['definitions']:
        fields(d, ('id','source','raw_description','max_doses','item_value'), 'Details definition IR')
        t['Definitions'].append([d['id'], string(d['source']), string(d['raw_description']), d['max_doses'], d['item_value']])
    for l in ir['locales']:
        fields(l, ('code','separator','total','left_singular','left_plural','image_measure','base_color','hint_color'), 'Details locale IR')
        t['Locales'].append([string(l['code']), string(l['separator']), string(l['total']), string(l['left_singular']),
                             string(l['left_plural']), string(l['image_measure']), l['base_color'], l['hint_color']])
    locale_indices = {l['code']: i for i, l in enumerate(ir['locales'])}
    for p in ir['presentations']:
        fields(p, ('definition','locale','raw','tokens'), 'Details presentation IR')
        require(p['locale'] in locale_indices and p['tokens'] == tokenize(p['raw']), 'Details raw/token binding')
        first = len(t['Tokens'])
        for token in p['tokens']:
            fields(token, ('kind','value','color'), 'Details token IR')
            require(token['kind'] in TOKEN_KINDS, 'Details token kind IR')
            value = string(token['value']) if token['kind'] == 'Text' else token['value']
            t['Tokens'].append([TOKEN_KINDS[token['kind']], value, token['color'], 0])
        t['Presentations'].append([p['definition'], locale_indices[p['locale']], first, len(t['Tokens']) - first])
    for r in ir['resources']:
        fields(r, ('id','source','path','kind','width','height','columns','rows'), 'Details resource IR')
        t['Resources'].append([r['id'], string(r['path']), r['kind'], r['width'], r['height'], r['columns'], r['rows'],
                               bytes.fromhex(receipt['outputs'][r['path']]['sha256']),
                               receipt['outputs'][r['path']]['bytes'], receipt['outputs'][r['path']]['crc32']])
    t['Parameters'] = [[i + 1, ir['parameters'][n]] for i, n in enumerate(PARAMETERS)]
    t['Strings'] = bytes(pool); validate(t)
    return t


def validate(t):
    require(set(t) == set(NAMES), 'Details sections')
    pool = t['Strings']
    require(isinstance(pool, bytes) and 1 <= len(pool) <= 65536 and pool[0] == pool[-1] == 0, 'Details strings')
    decoded = pool.decode('utf-8')
    require(all(ord(c) >= 32 or c in '\0\n' for c in decoded) and '\x7f' not in decoded, 'Details string controls')
    def string(off):
        require(type(off) is int and 0 <= off < len(pool) and (off == 0 or pool[off - 1] == 0), 'Details string offset')
        value = pool[off:pool.index(0, off)].decode('utf-8')
        require(len(value.encode('utf-8')) <= 4096, 'Details string capacity')
        return value
    limits = ((2,2), (2,2), (4,4), (1,1024), (1,1), (4,4))
    for n, f, (lo, hi) in zip(NAMES[1:], FORMATS[1:], limits):
        require(lo <= len(t[n]) <= hi, 'Details ' + n + ' capacity')
        for row in t[n]:
            integers = row[:7] + row[8:] if n == 'Resources' else row[:1] if n == 'Parameters' else row
            require(all(type(v) is int and 0 <= v <= 0xffffffff for v in integers), 'Details ' + n + ' integer')
            try: struct.pack(f, *row)
            except (struct.error, TypeError, OverflowError) as error: raise ValueError('Details ' + n + ' record') from error
    ids = set()
    for d in t['Definitions']:
        require(d[0] not in ids and string(d[1]) and string(d[2])
                and 1 <= d[3] <= 65535 and d[4] <= 65535, 'Details definition')
        ids.add(d[0])
    for i, l in enumerate(t['Locales']):
        require(string(l[0]) == ('en', 'zh_Hans_CN')[i] and string(l[1]) in ('', ' ')
                and string(l[5]) and (l[6] >> 24) == (l[7] >> 24) == 255, 'Details locale')
        for off in l[2:5]:
            value = string(off)
            require(value.count('%s') == 1 and '%' not in value.replace('%s','')
                    and not any(c in value for c in '[]{}\n'), 'Details dose pattern')
    for r in t['Resources']:
        require(r[0] > 0 and safe_path(string(r[1])) and string(r[1]).startswith('graphics/')
                and string(r[1]).endswith('.t3x') and r[2] == 1 and 0 < r[3] <= 4096
                and 0 < r[4] <= 4096 and r[5] == r[6] == 1 and len(r[7]) == 32 and any(r[7])
                and 0 < r[8] <= 1024*1024, 'Details resource')
    require(len({r[0] for r in t['Resources']}) == len(t['Resources']), 'Details resource identity')
    for token in t['Tokens']:
        kind, value, color, reserved = token
        require(kind in TOKEN_KINDS.values() and color <= 1 and reserved == 0, 'Details token')
        if kind == 1:
            s = string(value); require(s and not any(c in s for c in '[]%{}\n'), 'Details literal controls')
        elif kind == 4: require(value < len(t['Resources']), 'Details inline image index')
        else: require(value == 0, 'Details dynamic token value')
    cursor = 0; seen = set()
    for d, locale, first, count in t['Presentations']:
        require(d in ids and locale < 2 and (d,locale) not in seen and first == cursor
                and 0 < count <= 128 and first + count <= len(t['Tokens']), 'Details presentation')
        cursor += count; seen.add((d, locale))
        kinds = [token[0] for token in t['Tokens'][first:first+count]]
        max_doses = next(row[3] for row in t['Definitions'] if row[0] == d)
        require(kinds.count(TOKEN_KINDS['Nickname']) <= 1 and kinds.count(TOKEN_KINDS['Doses']) == (1 if max_doses > 1 else 0),
                'Details duplicate/policy token binding')
    require(cursor == len(t['Tokens']) and seen == {(d,l) for d in ids for l in (0,1)}, 'Details presentation coverage')
    for i, (ident, value) in enumerate(t['Parameters']):
        require(type(value) in (int,float) and ident == i + 1 and math.isfinite(value)
                and (-128 <= value <= 128 if i == 1 else 0 < value <= 128), 'Details parameter')
        if i >= 2: require(value == int(value), 'Details nickname scalar limit')


def encode(t):
    validate(t); data = bytearray(HEADER)
    for i, n in enumerate(NAMES):
        block = t[n] if i == 0 else b''.join(struct.pack(FORMATS[i], *r) for r in t[n])
        while len(data) % 4: data.append(0)
        struct.pack_into('<HHIII', data, 64 + i * 16, i+1, STRIDES[i], len(data), len(block)//STRIDES[i], len(block))
        data.extend(block)
    struct.pack_into('<8s6I20s12x', data, 0, b'ENCITD01', 1, len(data), 0, 7, 1, 1, bytes.fromhex(PIN))
    struct.pack_into('<I', data, 16, zlib.crc32(data)); return bytes(data)


def parse_pack(blob):
    require(HEADER <= len(blob) <= 1024*1024, 'Details size')
    magic, version, size, crc, n, caps, rules, pin = struct.unpack_from('<8s6I20s', blob)
    require((magic,version,size,n,caps,rules,pin.hex()) == (b'ENCITD01',1,len(blob),7,1,1,PIN)
            and not any(blob[52:64]), 'Details header/pin')
    copy = bytearray(blob); struct.pack_into('<I', copy, 16, 0)
    require(zlib.crc32(copy) == crc, 'Details CRC')
    t = {}; end = HEADER
    for i,n in enumerate(NAMES):
        kind,stride,off,count,amount = struct.unpack_from('<HHIII', blob, 64+i*16)
        require(kind == i+1 and stride == STRIDES[i] and count > 0 and amount == count*stride
                and off == (end+3)//4*4 and off+amount <= len(blob) and not any(blob[end:off]), 'Details directory/span')
        block = blob[off:off+amount]; end = off+amount
        t[n] = bytes(block) if i == 0 else list(struct.iter_unpack(FORMATS[i],block))
    require(end == len(blob), 'Details trailing bytes'); validate(t); return t


def glyph_request(ir):
    return {l['code']: ''.join(p['raw'] for p in ir['presentations'] if p['locale'] == l['code'])
            + l['total'] + l['left_singular'] + l['left_plural'] + l['image_measure'] + '0123456789'
            for l in ir['locales']}


def native_probe(godot, root=ROOT):
    """Record the actual pinned TextTools/font algorithm, not a replacement.

    In particular this exposes source empty-separator wrapping of BBCode in
    Chinese. Consumers must not claim identical source breaks without reading
    this evidence. No game script/autoload or runtime fixture is executed.
    """
    root = Path(root); ir = load(root); work = root / 'build/item-details-native-probe'
    project = work / 'project'; project.mkdir(parents=True, exist_ok=True)
    for path in ir['sources']:
        if path.startswith('Fonts/') or path in (ICON, 'Graphics/UI/Ailments/Ailments.tres'):
            target = project / path; target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(root / 'upstream/MOTHER-Encore' / path, target)
    examples = []
    for presentation in ir['presentations']:
        loc = next(l for l in ir['locales'] if l['code'] == presentation['locale'])
        definition = next(d for d in ir['definitions'] if d['id'] == presentation['definition'])
        for nickname in ('Ninten', 'ABCDEFGHI'):
            limit = ir['parameters']['ENNameLimit' if loc['code'] == 'en' else 'ZHNameLimit']
            for doses in range(definition['max_doses'] + 1):
                phrase = (loc['total'] if doses == definition['max_doses'] else
                          loc['left_singular'] if doses == 1 else loc['left_plural']) % doses
                raw = presentation['raw'].replace('\\n','\n').replace('[BR]','\n')
                raw = raw.replace('[Ninten]',nickname[:limit]).replace('[ItemValue]',str(definition['item_value']))
                raw = raw.replace('[Asthma]','[font=res://Graphics/UI/Ailments/Ailments.tres][img]res://' + ICON + '[/img][/font]')
                if definition['max_doses'] > 1: raw = raw % phrase
                examples.append(dict(definition=definition['id'],locale=loc['code'],nickname=nickname,doses=doses,
                                     separator=loc['separator'],text=raw,width=213))
    write_json(project / 'request.json', examples)
    source = (root / 'upstream/MOTHER-Encore/Scripts/global/text_tools.gd').read_text(encoding='utf-8')
    functions = source[source.index('static func add_line_breaks('):source.index('static func get_text_delay(')]
    script = '''extends SceneTree
func _init():
 var version = Engine.get_version_info()
 assert(version.hash == "3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8")
 var file = File.new()
 assert(file.open("res://request.json",File.READ) == OK)
 var request = JSON.parse(file.get_as_text()).result
 file.close()
 var font = load("res://Fonts/EBMain_la.tres")
 var labels = []
 var image_font = load("res://Graphics/UI/Ailments/Ailments.tres")
 var output = {"engine":version,"font_height":font.get_height(),"font_ascent":font.get_ascent(),"image_font_height":image_font.get_height(),"image_font_ascent":image_font.get_ascent(),"image_font_descent":image_font.get_descent(),"em_dash_width":font.get_string_size("——").x,"cases":[]}
 for example in request:
  OS.set_environment("DETAILS_WORD_SEPARATOR",example.separator)
  var label = RichTextLabel.new()
  label.rect_size = Vector2(example.width,29)
  label.bbcode_enabled = true
  label.fit_content_height = true
  label.scroll_active = false
  label.add_font_override("normal_font",font)
  get_root().add_child(label)
  var wrapped = add_line_breaks(example.text,label)
  label.bbcode_text = wrapped
  output.cases.append({"input":example,"wrapped_bbcode":wrapped,"stripped":strip_bbcode(wrapped)})
  labels.append(label)
 for _i in range(4): yield(self,"idle_frame")
 for i in range(labels.size()):
  output.cases[i]["content_height"] = labels[i].get_content_height()
  output.cases[i]["parsed_text"] = labels[i].get_text()
 assert(file.open("res://reference.json",File.WRITE) == OK)
 file.store_string(JSON.print(output,"  "))
 file.close()
 print("ITEM_DETAILS_NATIVE_REFERENCE_COMPLETE")
 quit()
static func _tr(_message):
 return OS.get_environment("DETAILS_WORD_SEPARATOR")
'''
    (project / 'project.godot').write_text('[application]\nconfig/name="Pinned item details text/font probe"\n',encoding='utf-8')
    (project / 'probe.gd').write_text(script + '\n' + functions,encoding='utf-8')
    subprocess.run([str(godot),'--path',str(project),'--script','probe.gd'],check=True)
    reference = read_json(project / 'reference.json')
    write_json(root / 'reports/item-details-source/native-reference.json', reference)
    return reference


def compile_pack(root=ROOT):
    root = Path(root); ir = load(root); receipt = verify_receipt(ir, root)
    blob = encode(lower(ir, receipt)); parse_pack(blob)
    target = root / PACK; target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(blob)
    print('Item details compile: %d checked bytes; independent ENCITD01, descriptions only' % len(blob))


def stage_files(source):
    source = Path(source); ir = load(ROOT); receipt = verify_receipt(ir, ROOT, source)
    blob = (source / 'data/opening.encdetails').read_bytes()
    require(blob == encode(lower(ir, receipt)), 'Staged details pack differs from reviewed source/receipt')
    parse_pack(blob)
    return {Path('data/opening.encdetails'): blob, Path(OUTPUT): (source / OUTPUT).read_bytes()}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=['extract', 'assets', 'compile', 'verify', 'probe'])
    p.add_argument('--tex3ds', type=Path)
    p.add_argument('--godot', type=Path)
    args = p.parse_args()
    if args.action == 'extract': extract()
    elif args.action == 'assets': compile_assets(args.tex3ds)
    elif args.action == 'compile': compile_pack()
    elif args.action == 'probe': native_probe(args.godot)
    else:
        ir = load(); receipt = verify_receipt(ir)
        require((ROOT / PACK).read_bytes() == encode(lower(ir,receipt)), 'Details generated pack differs')
        parse_pack((ROOT / PACK).read_bytes()); print('Item details verify: checked source/receipt/format')

if __name__ == '__main__': main()
