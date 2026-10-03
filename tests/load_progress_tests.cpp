#include "encore/audio_data.hpp"
#include "encore/crc32.hpp"
#include "encore/load_progress.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
unsigned checks = 0;
void check(bool ok, const char* why) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); }
}
uint32_t bitwise_crc(uint32_t crc, const uint8_t* bytes, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    return crc;
}
void put32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = uint8_t(value >> (i * 8));
}
struct Capture {
    std::vector<encore::LoadProgress> events;
    bool recursive = false;
    static void receive(void* context, const encore::LoadProgress& progress) {
        auto& capture = *static_cast<Capture*>(context);
        capture.events.push_back(progress);
        if (capture.recursive)
            encore::report_load_progress(encore::LoadPhase::Scene, 0, 0);
    }
};
void check_checksum_progress(const Capture& capture, uint64_t bytes, bool complete) {
    check(!capture.events.empty(), "checksum reports exist across translation units");
    check(capture.events.front().completed == 0, "operation begins at zero");
    uint64_t previous = 0;
    for (const auto& progress : capture.events) {
        check(progress.phase == encore::LoadPhase::Checksum, "PCM checksum phase");
        check(progress.total == bytes, "PCM expected total stays fixed");
        check(progress.completed >= previous && progress.completed <= bytes,
              "PCM progress is monotonic and bounded");
        check(progress.completed - previous <= 8192, "PCM yields at most every 8 KiB");
        previous = progress.completed;
    }
    check(complete ? previous == bytes : previous < bytes,
          "progress ends at processed byte count without fabricating completion");
}
class TemporaryFile {
public:
    TemporaryFile() : path_(std::filesystem::temp_directory_path() /
        ("encore-load-progress-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin")) {}
    ~TemporaryFile() { std::filesystem::remove(path_); }
    std::string path() const { return path_.string(); }
    void write(const std::vector<uint8_t>& bytes) {
        std::ofstream output(path_, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        output.close();
        check(bool(output), "write temporary fixture");
    }
private:
    std::filesystem::path path_;
};
std::vector<uint8_t> bank_fixture() {
    // A valid metadata fixture large enough to cross an 8 KiB read boundary.
    constexpr uint32_t count = 8, strings = 64 + count * 96;
    std::vector<uint8_t> bytes(strings);
    const std::string magic = "ENCAUD01";
    std::copy(magic.begin(), magic.end(), bytes.begin());
    put32(bytes, 8, 1); put32(bytes, 20, count); put32(bytes, 24, 96);
    put32(bytes, 28, strings); put32(bytes, 40, 0xc2a00000u); // -80 dB
    for (uint32_t i = 0; i < count; ++i) {
        const size_t record = 64 + i * 96;
        put32(bytes, record, i + 1); bytes[record + 4] = 1;
        put32(bytes, record + 36, 8000); bytes[record + 40] = 1;
        put32(bytes, record + 44, 1); put32(bytes, record + 52, 2);
        for (unsigned field = 0; field < 2; ++field) {
            const std::string value = (field ? "res://" : "") + std::string(600, 'a' + i);
            put32(bytes, record + 60 + field * 8, uint32_t(bytes.size()));
            put32(bytes, record + 64 + field * 8, uint32_t(value.size()));
            bytes.insert(bytes.end(), value.begin(), value.end()); bytes.push_back(0);
        }
    }
    put32(bytes, 12, uint32_t(bytes.size()));
    put32(bytes, 32, uint32_t(bytes.size()) - strings);
    put32(bytes, 16, bitwise_crc(0xffffffffu, bytes.data(), bytes.size()) ^ 0xffffffffu);
    return bytes;
}
}

int main() {
    const std::string standard = "123456789";
    const auto* standard_bytes = reinterpret_cast<const uint8_t*>(standard.data());
    check(encore::crc32(nullptr, 0) == 0, "CRC empty known vector");
    check(encore::crc32(standard_bytes, standard.size()) == 0xcbf43926u,
          "IEEE CRC32 standard known vector");
    const std::string sentence = "The quick brown fox jumps over the lazy dog";
    check(encore::crc32(reinterpret_cast<const uint8_t*>(sentence.data()), sentence.size())
          == 0x414fa339u, "CRC second known vector");
    std::vector<uint8_t> data(65537);
    uint32_t random = 0x628adb31u;
    for (auto& value : data) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        value = uint8_t(random);
    }
    for (size_t length : {0u, 1u, 2u, 15u, 16u, 17u, 255u, 256u, 257u, 8191u, 8192u, 8193u, 65537u}) {
        const uint32_t expected = bitwise_crc(0xffffffffu, data.data(), length) ^ 0xffffffffu;
        check(encore::crc32(data.data(), length) == expected, "table CRC equals bitwise oracle");
        check(encore::upstream::audio_crc32(data.data(), length) == expected,
              "audio API preserves CRC identity");
        for (size_t chunk : {1u, 3u, 16u, 255u, 8192u}) {
            uint32_t crc = 0xffffffffu;
            for (size_t offset = 0; offset < length; offset += chunk)
                crc = encore::crc32_update(crc, data.data() + offset, std::min(chunk, length - offset));
            check((crc ^ 0xffffffffu) == expected, "incremental CRC chunk equivalence");
        }
        check(encore::crc32_update(0x12345678u, data.data(), length) ==
              bitwise_crc(0x12345678u, data.data(), length), "arbitrary accumulator equivalence");
    }
    for (size_t split = 0; split <= 256; ++split) {
        auto crc = encore::crc32_update(0xffffffffu, data.data(), split);
        crc = encore::crc32_update(crc, data.data() + split, 256 - split);
        check((crc ^ 0xffffffffu) == encore::crc32(data.data(), 256), "every short split boundary");
    }

    Capture outer, inner;
    encore::clear_load_progress_observer();
    encore::report_load_progress(encore::LoadPhase::Scene, 1, 1);
    {
        encore::ScopedLoadProgress scope(Capture::receive, &outer);
        encore::report_load_progress(encore::LoadPhase::Texture, 9, 4);
        check(outer.events.back().completed == 4, "known total bounds callback count");
        encore::report_load_progress(encore::LoadPhase::FileRead, 9, 0);
        check(outer.events.back().completed == 9 && outer.events.back().total == 0,
              "unknown total retains byte count without percent");
        {
            encore::ScopedLoadProgress nested(Capture::receive, &inner);
            inner.recursive = true;
            encore::report_load_progress(encore::LoadPhase::Scene, 1, 1);
            check(inner.events.size() == 1, "recursive callback dispatch suppressed");
        }
        encore::report_load_progress(encore::LoadPhase::Scene, 1, 1);
        check(outer.events.size() == 3, "nested observer restores prior callback and context");
        {
            encore::ScopedLoadProgress disabled(nullptr);
            encore::report_load_progress(encore::LoadPhase::Scene, 1, 1);
        }
        check(outer.events.size() == 3, "null scoped observer disables reports");
    }
    encore::report_load_progress(encore::LoadPhase::Scene, 1, 1);
    check(outer.events.size() == 3, "scope exit clears initial observer");
    encore::set_load_progress_observer(Capture::receive, &outer);
    encore::clear_load_progress_observer();
    encore::report_load_progress(encore::LoadPhase::Scene, 1, 1);
    check(outer.events.size() == 3, "explicit clear makes reports no-op");

    TemporaryFile temporary;
    data.resize(8192 * 4 + 12); temporary.write(data);
    encore::upstream::AudioAsset asset;
    asset.channels = 2; asset.frames = uint32_t(data.size() / 4);
    asset.pcm_bytes = uint32_t(data.size());
    asset.pcm_crc = bitwise_crc(0xffffffffu, data.data(), data.size()) ^ 0xffffffffu;
    encore::upstream::AudioPcmStream stream;
    std::string error;
    Capture pcm;
    {
        encore::ScopedLoadProgress scope(Capture::receive, &pcm);
        check(stream.open(asset, temporary.path().c_str(), error), "valid PCM CRC accepted");
    }
    check_checksum_progress(pcm, asset.pcm_bytes, true);
    check(pcm.events.size() == 6, "one callback per PCM chunk plus initial report");
    pcm.events.clear();
    auto corrupt = asset; corrupt.pcm_crc ^= 1;
    {
        encore::ScopedLoadProgress scope(Capture::receive, &pcm);
        check(!stream.open(corrupt, temporary.path().c_str(), error) && stream.is_open(),
              "bad PCM CRC rejected while retaining previous validated stream");
    }
    check_checksum_progress(pcm, asset.pcm_bytes, true);
    stream.close();
    pcm.events.clear(); data.pop_back(); temporary.write(data);
    {
        encore::ScopedLoadProgress scope(Capture::receive, &pcm);
        check(!stream.open(asset, temporary.path().c_str(), error), "short PCM rejected");
    }
    check_checksum_progress(pcm, asset.pcm_bytes, false);
    pcm.events.clear(); data.push_back(0); data.push_back(0); temporary.write(data);
    {
        encore::ScopedLoadProgress scope(Capture::receive, &pcm);
        check(!stream.open(asset, temporary.path().c_str(), error), "long PCM rejected");
    }
    check_checksum_progress(pcm, asset.pcm_bytes, true);

    auto bytes = bank_fixture(); temporary.write(bytes);
    encore::upstream::AudioBank bank;
    Capture metadata;
    {
        encore::ScopedLoadProgress scope(Capture::receive, &metadata);
        check(bank.load_file(temporary.path().c_str(), error), "chunked audio bank read validates");
    }
    check(bank.count() == 8, "chunked metadata retains all records");
    bool read_complete = false, crc_complete = false;
    uint64_t read_previous = 0, crc_previous = 0;
    for (const auto& progress : metadata.events) {
        uint64_t& previous = progress.phase == encore::LoadPhase::FileRead ? read_previous : crc_previous;
        check(progress.completed >= previous && progress.completed - previous <= 8192,
              "metadata read and CRC each yield within 8 KiB");
        previous = progress.completed;
        if (progress.phase == encore::LoadPhase::FileRead && progress.total == bytes.size())
            read_complete = progress.completed == bytes.size();
        if (progress.phase == encore::LoadPhase::Checksum && progress.total == bytes.size())
            crc_complete = progress.completed == bytes.size();
    }
    check(read_complete && crc_complete, "metadata read and CRC report exact end counts");
    bytes.back() ^= 1;
    check(!bank.load(bytes.data(), bytes.size(), error) && bank.count() == 8,
          "bank CRC rejection preserves previous metadata");
    std::cout << "Loading progress and CRC: " << checks << " checks passed\n";
}
