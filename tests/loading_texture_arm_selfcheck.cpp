// Standalone official-SDK parity check; not part of the game executable.
#include "platform/ctr/loading_texture.hpp"
#include <cstdarg>
#include <cstring>

namespace {
FILE* log_file = nullptr;
unsigned checked = 0;
uint64_t bytes_compared = 0, callbacks = 0, previous = 0, total = 0;
bool progress_valid = true;
#ifndef SELF_CHECK_LOG_PATH
#define SELF_CHECK_LOG_PATH "sdmc:/encore-loading-texture-selfcheck.log"
#endif
void message(const char* format, ...) {
    va_list args;
    va_start(args, format); std::vprintf(format, args); va_end(args);
    if (log_file) {
        va_start(args, format); std::vfprintf(log_file, format, args); va_end(args);
        std::fflush(log_file);
    }
    gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
}
void observe(void*, const encore::LoadProgress& progress) {
    ++callbacks;
    if (progress.phase != encore::LoadPhase::Texture || progress.completed < previous ||
        progress.completed - previous > 8192 || (total && total != progress.total)) progress_valid = false;
    previous = progress.completed;
    total = progress.total;
}
ssize_t diagnose_read(void* context, void* destination, size_t size) {
    auto& input = *static_cast<encore::ctr::loading_texture_detail::Input*>(context);
    const ssize_t result = encore::ctr::loading_texture_detail::read(context, destination, size);
    message("DIAG read request=%lu result=%ld failed=%u eof=%u error=%u\n",
        static_cast<unsigned long>(size), static_cast<long>(result), unsigned(input.failed),
        unsigned(std::feof(input.file) != 0), unsigned(std::ferror(input.file) != 0));
    return result;
}
void diagnose_import(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (!file) { message("DIAG open failed\n"); return; }
    std::setvbuf(file, nullptr, _IOFBF, 8192);
    if (std::fseek(file, 0, SEEK_END) != 0) { std::fclose(file); return; }
    const long length = std::ftell(file);
    std::rewind(file);
    encore::ctr::loading_texture_detail::Input input{file, static_cast<uint64_t>(length)};
    C3D_Tex texture{};
    auto metadata = Tex3DS_TextureImportCallback(&texture, nullptr, false, diagnose_read, &input);
    message("DIAG SDK import=%u failed=%u completed=%llu length=%ld\n", unsigned(metadata != nullptr),
        unsigned(input.failed), static_cast<unsigned long long>(input.completed), length);
    if (metadata) { Tex3DS_TextureFree(metadata); C3D_TexDelete(&texture); }
    std::fclose(file);
}
bool compare(const char* path) {
    using namespace encore::ctr;
    auto original = C2D_SpriteSheetLoad(path);
    if (!original) { message("FAIL original: %s\n", path); return false; }
    previous = total = 0;
    LoadingSpriteSheet progressive = nullptr;
    {
        encore::ScopedLoadProgress observer(observe);
        progressive = loading_sprite_sheet_load(path);
    }
    bool same = progressive && progress_valid && total > 0 && previous == total;
    if (!same) {
        message("DIAG load=%u progress=%u read=%llu total=%llu\n", unsigned(progressive != nullptr),
            unsigned(progress_valid), static_cast<unsigned long long>(previous), static_cast<unsigned long long>(total));
        if (!progressive) diagnose_import(path);
    }
    const auto count = C2D_SpriteSheetCount(original);
    if (count != loading_sprite_sheet_count(progressive)) message("DIAG counts %lu/%lu\n",
        static_cast<unsigned long>(count), static_cast<unsigned long>(loading_sprite_sheet_count(progressive)));
    same = same && count > 0 && count == loading_sprite_sheet_count(progressive);
    for (size_t index = 0; same && index < count; ++index) {
        const auto first = C2D_SpriteSheetGetImage(original, index);
        const auto second = loading_sprite_sheet_get_image(progressive, index);
        same = first.tex && second.tex && first.subtex && second.subtex;
        if (!same) break;
        const auto& a = *first.subtex;
        const auto& b = *second.subtex;
        same = a.width == b.width && a.height == b.height && a.left == b.left &&
            a.top == b.top && a.right == b.right && a.bottom == b.bottom;
        if (!same) message("DIAG subtexture %lu mismatch\n", static_cast<unsigned long>(index));
        if (!same || index) continue;
        same = first.tex->dim == second.tex->dim && first.tex->fmt == second.tex->fmt &&
            first.tex->size == second.tex->size && first.tex->param == second.tex->param &&
            first.tex->border == second.tex->border && first.tex->lodParam == second.tex->lodParam;
        if (!same) message("DIAG params dim=%08lx/%08lx fmt=%u/%u size=%lu/%lu param=%08lx/%08lx border=%08lx/%08lx lod=%08lx/%08lx\n",
            static_cast<unsigned long>(first.tex->dim), static_cast<unsigned long>(second.tex->dim), unsigned(first.tex->fmt), unsigned(second.tex->fmt),
            static_cast<unsigned long>(first.tex->size), static_cast<unsigned long>(second.tex->size),
            static_cast<unsigned long>(first.tex->param), static_cast<unsigned long>(second.tex->param),
            static_cast<unsigned long>(first.tex->border), static_cast<unsigned long>(second.tex->border),
            static_cast<unsigned long>(first.tex->lodParam), static_cast<unsigned long>(second.tex->lodParam));
        u32 first_size = 0, second_size = 0;
        const auto first_bytes = C3D_Tex2DGetImagePtr(first.tex, -1, &first_size);
        const auto second_bytes = C3D_Tex2DGetImagePtr(second.tex, -1, &second_size);
        if (first_size != second_size) message("DIAG texture byte sizes=%lu/%lu\n", static_cast<unsigned long>(first_size), static_cast<unsigned long>(second_size));
        if (first_size == second_size && std::memcmp(first_bytes, second_bytes, first_size) != 0) {
            const auto a = static_cast<const unsigned char*>(first_bytes);
            const auto b = static_cast<const unsigned char*>(second_bytes);
            for (u32 offset = 0; offset < first_size; ++offset) if (a[offset] != b[offset]) {
                message("DIAG first texture mismatch offset=%lu byte=%02x/%02x\n", static_cast<unsigned long>(offset), a[offset], b[offset]);
                break;
            }
        }
        same = same && first_size == second_size && std::memcmp(first_bytes, second_bytes, first_size) == 0;
        if (same) bytes_compared += first_size;
    }
    C2D_SpriteSheetFree(original);
    loading_sprite_sheet_free(progressive);
    if (!same) { message("FAIL parity: %s\n", path); return false; }
    ++checked;
    message("PASS %u: %s\n", checked, path);
    return true;
}
bool run() {
    FILE* paths = std::fopen("romfs:/texture-paths.txt", "r");
    if (!paths) { message("FAIL texture path list\n"); return false; }
    char path[512];
    bool passed = true;
    while (std::fgets(path, sizeof(path), paths)) {
        const auto length = std::strlen(path);
        if (!length || path[length - 1] != '\n') { passed = false; break; }
        path[length - 1] = '\0';
        if (!compare(path)) { passed = false; break; }
    }
    passed = passed && !std::ferror(paths) && checked > 0;
    std::fclose(paths);
    using namespace encore::ctr;
    auto missing = loading_sprite_sheet_load("romfs:/does-not-exist.t3x");
    auto truncated = loading_sprite_sheet_load("romfs:/truncated.t3x");
    passed = passed && !missing && !truncated;
    loading_sprite_sheet_free(missing); loading_sprite_sheet_free(truncated);
    return passed;
}
}

int main() {
    gfxInitDefault(); consoleInit(GFX_BOTTOM, nullptr); gfxSetDoubleBuffering(GFX_BOTTOM, false);
    log_file = std::fopen(SELF_CHECK_LOG_PATH, "w");
    message("Progressive T3X SDK parity\n");
    const bool gpu_ready = C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    const Result romfs = romfsInit();
    const bool passed = gpu_ready && R_SUCCEEDED(romfs) && run();
    message("%s\nTextures: %u\nBytes: %llu\nCallbacks: %llu\nSTART exits\n",
        passed ? "ALL CHECKS PASS" : "SELF CHECK FAILED", checked,
        static_cast<unsigned long long>(bytes_compared), static_cast<unsigned long long>(callbacks));
    if (log_file) { std::fclose(log_file); log_file = nullptr; }
    while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
    if (R_SUCCEEDED(romfs)) romfsExit();
    if (gpu_ready) C3D_Fini();
    gfxExit();
    return passed ? 0 : 1;
}
