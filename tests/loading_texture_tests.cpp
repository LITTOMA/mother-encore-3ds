// Assertions are the test oracle even in Release builds.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "platform/ctr/loading_texture.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct TextureMetadata {
    Tex3DS_SubTexture subtextures[2] = {
        {71, 53, .125f, .875f, .5f, .25f},
        {29, 37, .25f, .125f, .75f, .875f}
    };
};
namespace {
int live_textures = 0, live_metadata = 0, texture_deletes = 0, imports = 0;
C3D_Tex* imported_texture = nullptr;
std::vector<encore::LoadProgress> reports;
constexpr size_t payload_size = 3 * 8192 + 29;
constexpr size_t small_payload_size = 512;
void observe(void*, const encore::LoadProgress& progress) {
    reports.push_back(progress);
    // This executes while import is in flight and may stand in for drawing UI.
    // It deliberately does not access the texture being imported.
}
void write_file(const std::string& path, const std::vector<unsigned char>& bytes) {
    FILE* file = std::fopen(path.c_str(), "wb");
    assert(file);
    if (!bytes.empty()) assert(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
    assert(std::fclose(file) == 0);
}
}

Tex3DS_Texture Tex3DS_TextureImportCallback(C3D_Tex* texture, C3D_TexCube* cube,
        bool vram, decompressCallback callback, void* context) {
    ++imports;
    assert(!cube && !vram);
    uint32_t size = 0;
    if (callback(context, &size, sizeof(size)) != sizeof(size) ||
        (size != payload_size && size != small_payload_size)) return nullptr;
    imported_texture = texture;
    // libctru's decoder prefetches 16384 bytes even for a complete 512-byte
    // payload. It accepts the useful short count and rejects only missing data.
    const size_t requested = size == small_payload_size ? 16384 : size;
    texture->data = std::malloc(requested);
    assert(texture->data);
    texture->size = size;
    ++live_textures;
    // Deliberately ask for the whole image at once, as uncompressed Tex3DS does.
    if (callback(context, texture->data, requested) != size) {
        C3D_TexDelete(texture);
        return nullptr;
    }
    ++live_metadata;
    return new TextureMetadata;
}
Tex3DS_Texture Tex3DS_TextureImport(const void* bytes,size_t length,C3D_Tex* texture,C3D_TexCube* cube,bool vram){
    struct Input{const uint8_t* bytes;size_t length,offset;};Input input{static_cast<const uint8_t*>(bytes),length,0};
    const auto read=[](void* context,void* target,size_t count)->ssize_t{auto& i=*static_cast<Input*>(context);count=std::min(count,i.length-i.offset);std::memcpy(target,i.bytes+i.offset,count);i.offset+=count;return ssize_t(count);};
    return Tex3DS_TextureImportCallback(texture,cube,vram,read,&input);
}
void Tex3DS_TextureFree(Tex3DS_Texture metadata) { assert(metadata); --live_metadata; delete metadata; }
void C3D_TexDelete(C3D_Tex* texture) {
    assert(texture->data && live_textures > 0);
    std::free(texture->data);
    texture->data = nullptr;
    --live_textures;
    ++texture_deletes;
}
void C3D_TexSetWrap(C3D_Tex* texture, int s, int t) { texture->wrap_s = s; texture->wrap_t = t; }
size_t Tex3DS_GetNumSubTextures(Tex3DS_Texture) { return 2; }
const Tex3DS_SubTexture* Tex3DS_GetSubTexture(Tex3DS_Texture metadata, size_t index) {
    return index < 2 ? &metadata->subtextures[index] : nullptr;
}

int main(int argc, char** argv) {
    assert(argc == 2);
    using namespace encore::ctr;
    const std::string base = argv[1], good = base + "/good.bin", bad = base + "/bad.bin";
    std::vector<unsigned char> data(sizeof(uint32_t) + payload_size);
    const uint32_t size = payload_size;
    std::memcpy(data.data(), &size, sizeof(size));
    for (size_t i = sizeof(size); i < data.size(); ++i) data[i] = static_cast<unsigned char>(i * 37);
    write_file(good, data);
    encore::ScopedLoadProgress observer(observe);
    std::string error = "stale";
    auto sheet = loading_sprite_sheet_load(good.c_str(), &error);
    assert(error.empty());
    assert(sheet && live_textures == 1 && live_metadata == 1);
    const auto acquired=loading_sprite_sheet_acquire(good.c_str(),&error);
    assert(acquired==sheet&&sheet->references==2&&live_textures==1);
    loading_sprite_sheet_free(acquired);assert(sheet->references==1&&live_textures==1);
    const auto from_memory=loading_sprite_sheet_acquire_memory(good.c_str(),{},error);
    assert(from_memory==sheet&&sheet->references==2);loading_sprite_sheet_free(from_memory);
    assert(loading_sprite_sheet_count(sheet) == 2);
    const auto image = loading_sprite_sheet_get_image(sheet, 0);
    assert(image.tex == imported_texture && image.tex == &sheet->texture);
    assert(image.subtex == Tex3DS_GetSubTexture(sheet->metadata, 0));
    assert(image.subtex->width == 71 && image.subtex->height == 53 && image.subtex->top == .875f);
    assert(std::memcmp(image.tex->data, data.data() + sizeof(size), payload_size) == 0);
    assert(image.tex->border == 0 && image.tex->wrap_s == GPU_CLAMP_TO_BORDER && image.tex->wrap_t == GPU_CLAMP_TO_BORDER);
    const auto rotated = loading_sprite_sheet_get_image(sheet, 1);
    assert(rotated.tex == image.tex && rotated.subtex->top < rotated.subtex->bottom);
    assert(!loading_sprite_sheet_get_image(sheet, 2).subtex);
    assert(reports.size() == 6 && reports.front().completed == 0 && reports.back().completed == data.size());
    uint64_t previous = 0;
    for (const auto& progress : reports) {
        assert(progress.phase == encore::LoadPhase::Texture && progress.total == data.size());
        assert(progress.completed >= previous && progress.completed - previous <= 8192);
        previous = progress.completed;
    }
    loading_sprite_sheet_free(sheet);
    assert(live_textures == 0 && live_metadata == 0 && texture_deletes == 1);

    // Import failure after GPU allocation must clean up exactly once.
    data.resize(data.size() - 17);
    write_file(bad, data);
    reports.clear();
    assert(!loading_sprite_sheet_load(bad.c_str(), &error));
    assert(error.find("Texture decode/allocation failed: ") == 0);
    assert(live_textures == 0 && live_metadata == 0 && texture_deletes == 2);
    assert(reports.back().completed == data.size());
    // Header truncation and invalid header fail before GPU allocation.
    data.resize(2);
    write_file(bad, data);
    assert(!loading_sprite_sheet_load(bad.c_str()) && texture_deletes == 2);
    data.assign(8, 0);
    write_file(bad, data);
    assert(!loading_sprite_sheet_load(bad.c_str()) && texture_deletes == 2);
    write_file(bad, {});
    const int before_empty = imports;
    assert(!loading_sprite_sheet_load(bad.c_str(), &error) && imports == before_empty);
    assert(error.find("Texture file is empty or not seekable: ") == 0);
    assert(!loading_sprite_sheet_load((base + "/missing").c_str(), &error));
    assert(error.find("Texture file could not be opened: ") == 0);
    assert(!loading_sprite_sheet_load(nullptr, &error));
    assert(error == "Texture path is null");
    assert(loading_sprite_sheet_count(nullptr) == 0);
    const auto empty = loading_sprite_sheet_get_image(nullptr, 0);
    assert(!empty.tex && !empty.subtex);
    loading_sprite_sheet_free(nullptr);

    // An actual stdio error is distinct from EOF, and failures remain sticky.
    FILE* write_only = std::fopen(bad.c_str(), "wb");
    assert(write_only);
    loading_texture_detail::Input input{write_only, 8};
    unsigned char output[8]{};
    assert(loading_texture_detail::read(&input, output, sizeof(output)) == -1 && input.failed);
    std::clearerr(write_only);
    assert(loading_texture_detail::read(&input, output, sizeof(output)) == -1);
    assert(std::fclose(write_only) == 0);
    assert(live_textures == 0 && live_metadata == 0);

    // Regression: a clean short read during decoder read-ahead is not an I/O
    // error. Real ARM shadow.t3x uses precisely this 512/16384 payload pattern.
    data.resize(sizeof(uint32_t) + small_payload_size);
    const uint32_t small_size = small_payload_size;
    std::memcpy(data.data(), &small_size, sizeof(small_size));
    for (size_t i = sizeof(small_size); i < data.size(); ++i) data[i] = static_cast<unsigned char>(i * 17);
    write_file(good, data);
    reports.clear();
    sheet = loading_sprite_sheet_load(good.c_str());
    assert(sheet && live_textures == 1 && live_metadata == 1);
    assert(std::memcmp(loading_sprite_sheet_get_image(sheet, 0).tex->data,
        data.data() + sizeof(small_size), small_payload_size) == 0);
    assert(reports.back().completed == data.size());
    loading_sprite_sheet_free(sheet);
    data.pop_back(); // Same prefetch, but the SDK cannot finish decoding now.
    write_file(bad, data);
    assert(!loading_sprite_sheet_load(bad.c_str()));
    assert(live_textures == 0 && live_metadata == 0);
    std::puts("PASS loading texture: bounded reads, identical payload/metadata, stable texture, failures and cleanup");
}
