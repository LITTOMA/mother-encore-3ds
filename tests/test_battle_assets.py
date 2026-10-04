import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zlib

from PIL import Image
from tools import battle_assets as assets


class BattleAssetsTests(unittest.TestCase):
    def make_image(self):
        # Transparent RGB is intentionally nonzero and distinct from opaque RGB.
        image = Image.new('RGBA', (4, 2))
        image.putdata([(255, 255, 255, 255), (0, 255, 0, 255),
                       (255, 255, 255, 0), (0, 255, 0, 0),
                       (0, 0, 0, 0), (0, 0, 255, 130),
                       (0, 255, 0, 255), (255, 255, 255, 255)])
        return image

    def test_lossless_frame_reordering_and_transparent_rgb(self):
        image = self.make_image()
        decoded = assets.decode_indexed(assets.encode_indexed(image, (2, 1)))
        self.assertEqual((decoded['width'], decoded['height'], decoded['frames']), (2, 2, 2))
        pixels = [decoded['palette'][index] for index in decoded['pixels']]
        self.assertEqual(pixels, list(image.crop((0, 0, 2, 2)).getdata()) + list(image.crop((2, 0, 4, 2)).getdata()))
        self.assertIn((255, 255, 255, 0), decoded['palette'])
        self.assertIn((255, 255, 255, 255), decoded['palette'])

    def test_palette_limit(self):
        image = Image.new('RGBA', (257, 1))
        image.putdata([(i % 256, i // 256, 0, 255) for i in range(257)])
        with self.assertRaisesRegex(ValueError, 'palette'):
            assets.encode_indexed(image, (1, 1))

    def test_invalid_grids(self):
        for grid in [(0, 1), (1, 0), (3, 1), (1, 3), (-1, 1)]:
            with self.subTest(grid=grid), self.assertRaises(ValueError):
                assets.encode_indexed(self.make_image(), grid)

    def test_every_truncation_rejected(self):
        raw = assets.encode_indexed(self.make_image(), (2, 1))
        for size in range(len(raw)):
            with self.subTest(size=size), self.assertRaises(ValueError):
                assets.decode_indexed(raw[:size])

    def test_trailing_byte_rejected(self):
        with self.assertRaisesRegex(ValueError, 'length'):
            assets.decode_indexed(assets.encode_indexed(self.make_image(), (1, 1)) + b'\0')

    def test_bad_version_magic_dimensions_and_crc(self):
        raw = assets.encode_indexed(self.make_image(), (1, 1))
        for offset, value in [(0, 0), (8, 2), (12, 0), (12, 1025), (16, 0), (20, 0), (20, 257), (24, 0), (24, 257), (32, 0)]:
            data = bytearray(raw)
            struct.pack_into('<I', data, offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                assets.decode_indexed(data)

    def test_crc_correct_invalid_palette_reference_rejected(self):
        raw = bytearray(assets.encode_indexed(self.make_image(), (1, 1)))
        raw[-1] = 255
        struct.pack_into('<I', raw, 32, zlib.crc32(raw[assets.HEADER.size:]))
        with self.assertRaisesRegex(ValueError, 'reference'):
            assets.decode_indexed(raw)

    def test_shader_branch_order_and_unblended_color(self):
        old = (255, 255, 255, 255)
        new = (0, 0, 255, 130)
        screen = (37, 81, 151, 255)
        self.assertEqual(assets.shader_transition(old, screen, old, new), new)
        self.assertEqual(assets.shader_transition((0, 255, 0, 255), screen, old, new), screen)
        for pixel in [(255, 255, 255, 0), (0, 255, 0, 0), (0, 0, 0, 0), (1, 2, 3, 128)]:
            self.assertEqual(assets.shader_transition(pixel, screen, old, new), pixel)

    def make_recipe(self, root):
        source = root / 'source.png'
        self.make_image().save(source)
        font = root / 'font.ttf'
        font.write_bytes(b'font fixture, not executable')
        return dict(schema=1, commit='reviewed', game_version='fixture', licence_review='fixture only',
                    sources={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in [source, font]},
                    resources=[dict(id=0, name='fixture', source='source.png', kind='texture', size=[4, 2],
                                    grid=[1, 1], output='graphics/battle/lamp/fixture.t3x', fix_alpha_edges=False)],
                    font=dict(source='font.ttf', id=1, size=16, first=32, last=126))

    def test_source_pin_and_changes(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            recipe = self.make_recipe(root)
            lock = dict(commit='reviewed', game_version='fixture')
            assets.validate_source(root, recipe, lock)
            (root / 'source.png').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'Changed'):
                assets.validate_source(root, recipe, lock)

    def test_unknown_recipe_fields_kinds_paths_crop(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            recipe = self.make_recipe(root)
            lock = dict(commit='reviewed', game_version='fixture')
            for key, value in [('kind', 'execute_script'), ('output', '../outside.t3x'), ('grid', [3, 1]),
                               ('id', 1), ('crop', [0, 0, 5, 2]), ('unknown', 1)]:
                changed = copy.deepcopy(recipe)
                changed['resources'][0][key] = value
                with self.subTest(key=key), self.assertRaises(ValueError):
                    assets.validate_source(root, changed, lock)

    def test_unreviewed_version_commit_permission(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            recipe = self.make_recipe(root)
            lock = dict(commit='reviewed', game_version='fixture')
            for key, value in [('schema', 2), ('game_version', 'new'), ('commit', 'different'), ('licence_review', '')]:
                changed = copy.deepcopy(recipe)
                changed[key] = value
                with self.subTest(key=key), self.assertRaises(ValueError):
                    assets.validate_source(root, changed, lock)

    def test_compiled_source_receipt(self):
        # This project deliberately fails if its required source-backed content
        # has not been compiled; a missing asset is not a skipped passing test.
        assets.verify(assets.ROOT / 'upstream/MOTHER-Encore')
        receipt = json.loads(assets.receipt_path(assets.OUT, assets.ROOT).read_text())
        transition = assets.decode_indexed((assets.OUT / 'transition.bpx').read_bytes())
        self.assertEqual((transition['width'], transition['height'], transition['frames']), (320, 180, 25))
        self.assertEqual(set(transition['palette']), {(0, 0, 0, 0), (0, 255, 0, 0), (255, 255, 255, 0),
                                                    (0, 255, 0, 255), (255, 255, 255, 255)})
        self.assertEqual(receipt['font_metrics']['version']['hash'], '3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8')
        self.assertEqual(len(receipt['glyphs']), 95)
        for glyph, native in zip(receipt['glyphs'], receipt['font_metrics']['advances']):
            self.assertEqual(glyph['advance'], native)

    def test_actual_renderer_expansion_repeat_and_reference_sampling(self):
        # Compile the actual platform helper with no-op GPU bindings. This tests
        # its CPU compositor and checked stream loader, not GPU rendering.
        stub = r'''#pragma once
#include <cstdint>
#include <cstdlib>
#include <sys/types.h>
using u16=uint16_t;using u32=uint32_t;
enum {GPU_RGBA8,GPU_NEAREST,GPU_CLAMP_TO_EDGE,GPU_ALWAYS,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_GREATER,GPU_CLAMP_TO_BORDER,GPU_LA8};
inline int last_source=0,last_dest=0;inline bool alpha_enabled=false;
struct C3D_Tex {void*data=nullptr;int fmt=0;u16 width=0,height=0;size_t size=0;u32 border=0;};
struct C3D_TexCube;
struct Tex3DS_SubTexture {u16 width,height;float left,top,right,bottom;};
using Tex3DS_Texture=void*;using decompressCallback=ssize_t(*)(void*,void*,size_t);
inline Tex3DS_Texture Tex3DS_TextureImportCallback(C3D_Tex*,C3D_TexCube*,bool,decompressCallback,void*){return nullptr;}
inline Tex3DS_Texture Tex3DS_TextureImport(const void*,size_t,C3D_Tex*,C3D_TexCube*,bool){return nullptr;}
inline void Tex3DS_TextureFree(Tex3DS_Texture){}
inline size_t Tex3DS_GetNumSubTextures(Tex3DS_Texture){return 0;}
inline const Tex3DS_SubTexture* Tex3DS_GetSubTexture(Tex3DS_Texture,size_t){return nullptr;}
struct C2D_Image {C3D_Tex*tex;const Tex3DS_SubTexture*subtex;};
struct C2D_ImageTint{};using C2D_SpriteSheet=void*;
inline bool C3D_TexInit(C3D_Tex*t,u16 w,u16 h,int){t->width=w;t->height=h;t->size=size_t(w)*h*4;t->data=std::calloc(1,t->size);return t->data;}
inline void C3D_TexDelete(C3D_Tex*t){std::free(t->data);}
inline void C3D_TexSetFilter(C3D_Tex*,int,int){}inline void C3D_TexSetWrap(C3D_Tex*,int,int){}inline void C3D_TexFlush(C3D_Tex*){}
inline C2D_SpriteSheet C2D_SpriteSheetLoad(const char*){return nullptr;}inline void C2D_SpriteSheetFree(C2D_SpriteSheet){}
inline size_t C2D_SpriteSheetCount(C2D_SpriteSheet){return 0;}inline C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet,size_t){return {nullptr,nullptr};}
inline bool Tex3DS_SubTextureRotated(const Tex3DS_SubTexture*){return false;}
inline void C2D_PlainImageTint(C2D_ImageTint*,u32,float){}
inline bool C2D_DrawImageAt(C2D_Image,float,float,float,const C2D_ImageTint*,float,float){return true;}
inline void C2D_DrawRectSolid(float,float,float,float,float,u32){}
inline void C2D_Flush(){}inline void C2D_Prepare(){}
inline void C3D_FrameSync(){}inline int GSPGPU_FlushDataCache(const void*,u32){return 0;}
inline void C3D_AlphaTest(bool on,int,int){alpha_enabled=on;}inline void C3D_AlphaBlend(int,int,int source,int dest,int,int){last_source=source;last_dest=dest;}
'''
        source = r'''#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <cassert>
#define private public
#include "platform/ctr/battle_renderer.hpp"
#undef private
int main(int argc,char**argv){
 assert(argc==3);std::string error;BattleRenderer renderer;
 std::vector<BattleRenderer::Resource> resources={{argv[1],2,2,2,1,1},{argv[2],2,2,2,1,1}};
 const uint32_t base=0xff112233,new_color=0x8200ff00,old_color=0xffffffff;
 assert(renderer.load(resources,4,4,error));renderer.draw_surface(0,0,4,4,true);assert(last_source==GPU_SRC_ALPHA&&last_dest==GPU_ONE_MINUS_SRC_ALPHA&&alpha_enabled);std::fill(renderer.surface_.begin(),renderer.surface_.end(),base);
 assert(renderer.compose_transition(0,0,old_color,new_color,1,1));
 const uint32_t expected[]={new_color,new_color,base,base,new_color,new_color,base,base,
                           0,0,0x80030201,0x80030201,0,0,0x80030201,0x80030201};
 for(unsigned i=0;i<16;++i)assert(renderer.surface_[i]==expected[i]);
 assert(!renderer.compose_transition(0,1,old_color,new_color));
 assert(!renderer.compose_transition(99,0,old_color,new_color));
 BattleRenderer::BackgroundLayer layer;layer.resource=1;layer.width=2;layer.height=2;layer.opacity=1;layer.repeat=true;
 assert(renderer.set_background({layer},error));assert(renderer.compose_background(0,0));
 const uint32_t colors[]={0xff00000a,0xff000014,0xff00001e,0xff000028};
 for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)assert(renderer.surface_[y*4+x]==colors[(y%2)*2+x%2]);
 layer.repeat=false;assert(renderer.set_background({layer},error));assert(renderer.compose_background(0,0));
 for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)assert(renderer.surface_[y*4+x]==colors[std::min(y,1u)*2+std::min(x,1u)]);
 auto second=layer;second.opacity=0.5f;
 assert(renderer.set_background({layer,second},error));assert(renderer.compose_background(2.25f,base));renderer.upload_surface();
 auto* gpu=static_cast<uint32_t*>(renderer.surface_texture_.data);
 const std::vector<uint32_t> linear_upload(gpu,gpu+renderer.surface_texture_.size/4);
 assert(renderer.compose_background(2.25f,base,true)&&renderer.direct_surface_);renderer.upload_surface();
 assert(std::equal(linear_upload.begin(),linear_upload.end(),gpu));
 assert(!renderer.compose_transition(0,0,old_color,new_color));
 assert(renderer.set_background({layer},error));assert(renderer.compose_background(0,base,true)&&!renderer.direct_surface_);renderer.upload_surface();
#ifdef ENCORE_FRAME_PROFILE
 const auto diag=renderer.background_diagnostics();
 assert(!diag.region_ready&&!diag.region_used&&!diag.direct_texture&&diag.samples==0&&diag.skipped==0);
 assert(diag.prepared_bytes==renderer.background_.prepared_bytes());
 const std::vector<uint32_t> before_diagnostics(gpu,gpu+renderer.surface_texture_.size/4);
 renderer.background_diagnostics();assert(std::equal(before_diagnostics.begin(),before_diagnostics.end(),gpu));
 layer.repeat=true;layer.amplitude_x=0.1f;layer.frequency_x=0.5f;layer.speed_x=1;
 layer.compression_amplitude_y=0.1f;layer.compression_frequency_y=0.6f;layer.compression_speed_y=1;
 second=layer;second.amplitude_x=0;second.opacity=0.5f;
 assert(renderer.set_background({layer,second},error));assert(renderer.compose_background(1.25f,base,true));
 const auto used=renderer.background_diagnostics();
 assert(used.region_ready&&used.region_used&&used.direct_texture&&used.samples>0&&used.samples+used.skipped==32);
 assert(used.preparation==encore::RegionBackgroundKernel::PreparationStatus::Ready);
 assert(renderer.compose_background(10000.f,base,true));const auto fallback=renderer.background_diagnostics();
 assert(fallback.region_ready&&!fallback.region_used&&fallback.direct_texture&&fallback.samples==0&&fallback.skipped==0);
 assert(!renderer.compose_background(std::numeric_limits<float>::quiet_NaN(),base,true));
 const auto failed=renderer.background_diagnostics();assert(!failed.region_used&&!failed.direct_texture&&failed.samples==0&&failed.skipped==0);
 renderer.free();const auto cleared=renderer.background_diagnostics();
 assert(!cleared.region_ready&&!cleared.region_used&&!cleared.direct_texture&&cleared.prepared_bytes==0);
#endif
 renderer.free();assert(renderer.load(resources,2,2,error));std::fill(renderer.surface_.begin(),renderer.surface_.end(),base);
 assert(renderer.compose_transition(0,0,old_color,new_color));
 assert(renderer.surface_[0]==new_color&&renderer.surface_[1]==base&&renderer.surface_[2]==0&&renderer.surface_[3]==0x80030201);
 renderer.free();assert(renderer.load(resources,1,1,error));assert(!renderer.compose_transition(0,0,old_color,new_color));renderer.free();
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / '3ds.h').write_text(stub)
            (root / 'citro2d.h').write_text('#pragma once\n#include "3ds.h"\n')
            (root / 'test.cpp').write_text(source)
            mask = Image.new('RGBA', (2, 2))
            mask.putdata([(255, 255, 255, 255), (0, 255, 0, 255), (0, 0, 0, 0), (1, 2, 3, 128)])
            background = Image.new('RGBA', (2, 2))
            background.putdata([(i, 0, 0, 255) for i in [10, 20, 30, 40]])
            (root / 'mask.bpx').write_bytes(assets.encode_indexed(mask, (1, 1)))
            (root / 'bg.bpx').write_bytes(assets.encode_indexed(background, (1, 1)))
            command = ['c++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror',
                            '-I' + str(root), '-I' + str(assets.ROOT), '-I' + str(assets.ROOT / 'include'), str(root / 'test.cpp'),
                            '-o', str(root / 'test')]
            for profile in (False, True):
                subprocess.run(command + (['-DENCORE_FRAME_PROFILE'] if profile else []),
                               check=True, capture_output=True, text=True)
                subprocess.run([str(root / 'test'), str(root / 'mask.bpx'), str(root / 'bg.bpx')],
                               check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
