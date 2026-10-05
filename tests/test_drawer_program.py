#!/usr/bin/env python3
"""Manual-only source graph and ENCDRP01 admission negative coverage."""
import copy, json, struct, sys, tempfile, unittest, zlib
from unittest.mock import patch
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.drawer_program import ROOT, IR, PATH, SOUND, PIN, HEADER, encode,parse_pack,lower,commands_for,text_segments,validate_document
from tools.drawer_item import IR as ITEM_IR, OUTPUT, append_item,validate_item,validate_description
from tools.drawer_audio import bindings as audio_bindings

class DrawerProgramTests(unittest.TestCase):
    def fixture(self):
        pool=bytearray(b'\0');positions={}
        def add(s):positions[s]=len(pool);pool.extend(s.encode()+b'\0');return positions[s]
        item=add('AsthmaSpray');flag=add('got_asthma_spray');sound=add(SOUND);source=add(PATH);inspection=add('Objects/interact_dialog6')
        return dict(Strings=bytes(pool),Templates=[[1,item,3,0]],Binding=[[source,inspection]],Commands=[
            [1,62,0,0,0],[2,0,0,0,0],[3,flag,1,17,0],[4,0,14,0,0],[8,8,0,0,0],
            [1,63,0,0,0],[2,0,0,0,0],[8,8,0,0,0],[5,0,0,0,0],[1,64,0,0,0],
            [6,sound,0,0,0],[7,flag,1,0,0],[2,0,0,0,0],[9,0,0,0,0],
            [1,65,0,0,0],[2,0,0,0,0],[9,0,0,0,0],[1,66,0,0,0],[2,0,0,0,0],[9,0,0,0,0]])
    def change(self,blob,offset,fmt,value):
        b=bytearray(blob);struct.pack_into(fmt,b,offset,value);struct.pack_into('<I',b,16,0);struct.pack_into('<I',b,16,zlib.crc32(b));return bytes(b)
    def test_full_checked_stream_and_dormant_phrase(self):
        t=self.fixture();parsed=parse_pack(encode(t))
        self.assertEqual(parsed['Commands'],[tuple(row) for row in t['Commands']])
        ir=json.loads((ROOT/IR).read_text(encoding='utf-8'))
        commands,labels=commands_for(ir['document'],ir['texts'])
        self.assertEqual(labels,{'0':0,'1':5,'2':8,'3':14,'4':17})
        self.assertEqual([c['opcode'] for c in commands[8:14]],['GrantItem','ShowText','PlaySound','SetFlag','AwaitText','End'])
    def test_missing_unknown_header_versions_and_source(self):
        blob=encode(self.fixture())
        damaged=[blob[:HEADER-1],blob[:-1],self.change(blob,8,'I',2),self.change(blob,24,'I',2),self.change(blob,28,'I',2),self.change(blob,20,'I',3),self.change(blob,32,'B',0),self.change(blob,52,'I',1)]
        for b in damaged:
            with self.subTest(size=len(b)),self.assertRaises(ValueError):parse_pack(b)
    def test_crc_directory_overlap_reserved_and_utf8(self):
        b=encode(self.fixture());bad=bytearray(b);bad[-1]^=1
        for blob in (bad,self.change(b,100,'I',128),self.change(b,98,'H',24),self.change(b,112,'H',5)):
            with self.assertRaises(ValueError):parse_pack(blob)
        t=self.fixture();t['Strings']=t['Strings'][:-1]+b'\xff\0'
        with self.assertRaises(UnicodeDecodeError):parse_pack(encode(t))
    def test_all_opcode_operand_paths_reject_unknown(self):
        # Each of the nine opcode forms gets a reserved/invalid operand case.
        for pc,column,value in ((0,1,0),(1,1,1),(2,2,2),(3,1,2),(8,1,99),(10,2,1),(11,2,2),(4,1,20),(13,1,1),(0,0,10),(0,4,1)):
            t=self.fixture();t['Commands'][pc][column]=value
            with self.subTest(pc=pc,column=column),self.assertRaises(ValueError):parse_pack(encode(t))
    def test_graph_cycle_fallthrough_and_text_gate_fail_closed(self):
        for pc,row in ((4,[8,0,0,0,0]),(7,[8,5,0,0,0]),(19,[2,0,0,0,0]),(1,[9,0,0,0,0]),(6,[1,63,0,0,0]),(14,[2,0,0,0,0])):
            t=self.fixture();t['Commands'][pc]=row
            with self.subTest(pc=pc),self.assertRaises(ValueError):parse_pack(encode(t))
    def test_template_and_binding_fail_closed(self):
        for row in ([0,1,3,0],[1,1,0,0],[1,1,3,2],[1,2,3,0]):
            t=self.fixture();t['Templates'][0]=row
            with self.assertRaises(ValueError):parse_pack(encode(t))
        for binding in ([],[[0,0]],[[1,1],[1,1]]):
            t=self.fixture();t['Binding']=binding
            with self.assertRaises(ValueError):parse_pack(encode(t))
    def test_source_unknown_fields_and_branch_priority(self):
        d=json.loads((ROOT/IR).read_text(encoding='utf-8'))['document']
        for mutation in ('extra','reorder','dropdormant','effect','boolean'):
            x=copy.deepcopy(d)
            if mutation=='extra':x['2']['commands']=['invented']
            elif mutation=='reorder':x['0']['if'].reverse()
            elif mutation=='dropdormant':del x['1']
            elif mutation=='effect':x['2']['item']='DifferentItem'
            else:x['0']['if'][0]['flags']['got_asthma_spray']=1
            with self.assertRaises(ValueError):validate_document(x)
    def test_receiver_token_scope_and_unknown_controls(self):
        self.assertEqual(text_segments('[@][ItemReceiver] received it.'),[[dict(kind=2,text=''),dict(kind=1,text=' received it.')]])
        for text in ('[@][ReceiverArt4]','[@][ItemReceiver:Other]','[@][Unknown]','No bullet'):
            with self.assertRaises(ValueError):text_segments(text)
    def test_link_rejects_missing_and_wrong_external_text(self):
        ir=json.loads((ROOT/IR).read_text(encoding='utf-8'))
        house=dict(commit=PIN,dialogues=[dict(id=r['id'],source_path=PATH) for r in ir['texts']])
        self.assertEqual(len(lower(ir,house)['Commands']),20)
        for mutation in ('missing','wrongpath','duplicate','unknowncommand'):
            x=copy.deepcopy(ir);h=copy.deepcopy(house)
            if mutation=='missing':h['dialogues'].pop()
            elif mutation=='wrongpath':h['dialogues'][0]['source_path']='Data/Dialogue/other.yaml'
            elif mutation=='duplicate':h['dialogues'].append(h['dialogues'][0])
            else:x['commands'][0]['ignored']=1
            with self.assertRaises(ValueError):lower(x,h)

    def test_item_source_property_and_description_boundaries(self):
        item=json.loads((ROOT/ITEM_IR).read_text(encoding='utf-8'))
        validate_item(item['document'])
        for field,value in (('unknown',0),('doses',1),('keyitem',True),('actions',[]),('status_heals',[]),('cost',0)):
            doc=copy.deepcopy(item['document']);doc[field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):validate_item(doc)
        for row in item['translations'].values():validate_description(row['description'])
        for raw in ('[Ninten] [Asthma]', '[Ninten] [Other] %s', '[Ninten] [Asthma] %s %s', '[ItemReceiver] [Asthma] %s'):
            with self.assertRaises(ValueError):validate_description(raw)

    def test_item_extension_preserves_initial_inventory_and_raw_controls(self):
        item=json.loads((ROOT/ITEM_IR).read_text(encoding='utf-8'))
        baseline=dict(definitions=[dict(id=1,source='BaseballCap')],resources=[dict(id=1,path='graphics/fixture.t3x')],dependencies={},scope='Fixture',initial_inventory=[dict(id=1,definition=0,equipped=True,doses=1)])
        receipt=dict(resource=dict(id=1,path=OUTPUT,kind=1,width=20,height=20,columns=1,rows=1,sha256='1'*64))
        class Ex:root=ROOT
        with patch('tools.drawer_item.load',return_value=item),patch('tools.drawer_item.verify_receipt',return_value=receipt),patch('tools.drawer_item.digest',return_value='1'*64):
            out=append_item(Ex(),baseline)
            self.assertEqual(out['initial_inventory'],baseline['initial_inventory'])
            self.assertEqual(len(baseline['definitions']),1)
            definition=out['definitions'][1]
            self.assertEqual((definition['id'],definition['flags'],definition['can_use']),(2,2,0))
            self.assertEqual(definition['description'],item['translations']['en']['description'])
            self.assertEqual(definition['icon'],1)
            with self.assertRaises(ValueError):append_item(Ex(),out)

    def audio_config(self):
        return dict(schema=1,kind='encore.drawer-audio.source-binding',commit=PIN,license_review='Fixture source licence review',
                    assets=[dict(source=SOUND,identity=dict(kind='stable',value=1401),pcm='sound/effects/item-received.pcm',gain_db=0.0,conversion=None)])

    def admit_audio(self,config,source_ir=None):
        ir=source_ir or dict(commit=PIN,commands=[dict(opcode='PlaySound',a=SOUND)],sources={SOUND:'1'*64,SOUND+'.import':'2'*64})
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'content').mkdir()
            (root/'content/drawer-audio-binding.json').write_text(json.dumps(config),encoding='utf-8')
            with patch('tools.drawer_audio.load',return_value=ir):return audio_bindings(root)

    def test_audio_checked_source_binding_and_integer_gain(self):
        config=self.audio_config();self.assertEqual(self.admit_audio(config),config['assets'])
        config['assets'][0]['gain_db']=0;self.assertEqual(self.admit_audio(config),config['assets'])

    def test_audio_unknown_fields_versions_and_missing_coverage(self):
        changes=[lambda c:c.update(extra=1),lambda c:c.update(schema=2),lambda c:c.update(schema=True),
                 lambda c:c.update(kind='unknown'),lambda c:c.update(commit='0'*40),lambda c:c.update(license_review=True),
                 lambda c:c.update(license_review=' '),lambda c:c.update(assets=[]),lambda c:c.update(assets=True),
                 lambda c:c['assets'][0].update(extra=1),lambda c:c['assets'][0]['identity'].update(extra=1)]
        for i,change in enumerate(changes):
            config=self.audio_config();change(config)
            with self.subTest(case=i),self.assertRaises(ValueError):self.admit_audio(config)
        ir=dict(commit=PIN,commands=[dict(opcode='PlaySound',a=SOUND)],sources={SOUND:'1'*64})
        with self.assertRaises(ValueError):self.admit_audio(self.audio_config(),ir)
        other='Audio/Sound effects/fixture-other.mp3'
        ir=dict(commit=PIN,commands=[dict(opcode='PlaySound',a=SOUND),dict(opcode='PlaySound',a=other)],
                sources={SOUND:'1'*64,SOUND+'.import':'2'*64,other:'3'*64,other+'.import':'4'*64})
        with self.assertRaises(ValueError):self.admit_audio(self.audio_config(),ir)
        config=self.audio_config();second=copy.deepcopy(config['assets'][0]);second['source']=other;second['identity']['value']=1402
        config['assets'].append(second)
        with self.assertRaises(ValueError):self.admit_audio(config,ir) # Separate source cannot overwrite the same PCM.

    def test_audio_source_identity_duplication_and_unknown_sound(self):
        for source in ('Audio/Sound effects/Unknown.mp3','Audio/../Sound effects/Item Received.mp3',True,1401,['invalid']):
            config=self.audio_config();config['assets'][0]['source']=source
            with self.subTest(source=source),self.assertRaises(ValueError):self.admit_audio(config)
        for value in (True,1.0,0,-1,2**32,'1401'):
            config=self.audio_config();config['assets'][0]['identity']['value']=value
            with self.subTest(value=value),self.assertRaises(ValueError):self.admit_audio(config)
        config=self.audio_config();config['assets'].append(copy.deepcopy(config['assets'][0]))
        with self.assertRaises(ValueError):self.admit_audio(config)
        config=self.audio_config();config['assets'][0]['identity']['kind']='path'
        with self.assertRaises(ValueError):self.admit_audio(config)

    def test_audio_conversion_gain_and_destination_fail_closed(self):
        for conversion in (False,0,'pcm16',{},dict(sample_rate=8000)):
            config=self.audio_config();config['assets'][0]['conversion']=conversion
            with self.subTest(conversion=conversion),self.assertRaises(ValueError):self.admit_audio(config)
        for gain in (False,True,1.0,'0',None):
            config=self.audio_config();config['assets'][0]['gain_db']=gain
            with self.subTest(gain=gain),self.assertRaises(ValueError):self.admit_audio(config)
        for pcm in ('sound/effects/../item.pcm','sound/effects//item.pcm','sound/effects/./item.pcm','/sound/effects/item.pcm',
                    'sound/effects/item.wav','sound/music/item.pcm','sound/effects/bad\n.pcm','sound/effects/item\\file.pcm',True,0,None):
            config=self.audio_config();config['assets'][0]['pcm']=pcm
            with self.subTest(pcm=pcm),self.assertRaises(ValueError):self.admit_audio(config)

if __name__=='__main__':unittest.main()
