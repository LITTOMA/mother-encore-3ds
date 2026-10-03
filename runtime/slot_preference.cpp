#include "encore/slot_preference.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <utility>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace encore::upstream {
namespace {
constexpr uint8_t magic[8] = {'E', 'N', 'C', 'S', 'L', 'O', 'T', '1'};
bool reject(std::string& error, const char* message) { error = message; return false; }
uint32_t checksum(const uint8_t* bytes, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
uint32_t get32(const uint8_t* p) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t(p[i]) << (8 * i);
    return value;
}
void put32(uint8_t* p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(value >> (8 * i));
}
bool valid_path(const char* path, std::string& error) {
    if (!path || !*path) return reject(error, "Empty slot preference path");
    const std::string value(path);
    if (value.size() > 1000 || value.back() == '/' || value.back() == '\\')
        return reject(error, "Invalid slot preference path");
    for (unsigned char c : value)
        if (c < 32 || c == 127) return reject(error, "Invalid slot preference path character");
    return true;
}
// Bound storage reads before allocation, including oversized/corrupt files.
class LocalPreferenceFileOps final : public SessionSaveFileOps {
public:
    SessionFileReadResult read(const std::string& path, size_t limit,
                              std::vector<uint8_t>& output, std::string& error) override {
        errno = 0; FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            if (errno == ENOENT) { error.clear(); return SessionFileReadResult::Missing; }
            error = "Cannot open slot preference"; return SessionFileReadResult::Error;
        }
        if (limit > slot_preference_bytes) limit = slot_preference_bytes;
        uint8_t bytes[slot_preference_bytes + 1];
        const size_t size = std::fread(bytes, 1, limit + 1, f);
        bool ok = !std::ferror(f);
        if (std::fclose(f)) ok = false;
        if (!ok || size > limit) {
            error = size > limit ? "Slot preference exceeds file bounds" : "Cannot read complete slot preference";
            return SessionFileReadResult::Error;
        }
        output.assign(bytes, bytes + size); error.clear(); return SessionFileReadResult::Ok;
    }
    bool write_new(const std::string& path, const std::vector<uint8_t>& bytes,
                   std::string& error) override {
        if (bytes.size() != slot_preference_bytes) return reject(error, "Invalid slot preference write size");
#if defined(_WIN32)
        const int descriptor = _open(path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
        FILE* f = descriptor < 0 ? nullptr : _fdopen(descriptor, "wb");
#else
        const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        FILE* f = descriptor < 0 ? nullptr : ::fdopen(descriptor, "wb");
#endif
        if (!f) {
            error = "Cannot exclusively create slot preference; check directory, SD, or stale temporary file";
            if (descriptor >= 0) {
#if defined(_WIN32)
                _close(descriptor);
#else
                ::close(descriptor);
#endif
                std::string cleanup_error;
                if (!remove(path, cleanup_error)) error += "; " + cleanup_error;
            }
            return false;
        }
        bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        if (std::fflush(f)) ok = false;
        if (std::fclose(f)) ok = false;
        if (!ok) {
            error = "Slot preference write/flush/close failed";
            std::string cleanup_error;
            if (!remove(path, cleanup_error)) error += "; " + cleanup_error;
            return false;
        }
        error.clear(); return true;
    }
    bool replace(const std::string& from, const std::string& to, std::string& error) override {
        // libctru rename may remove the destination before failing. The caller
        // retains a verified backup and attempts exclusive primary recreation.
        if (std::rename(from.c_str(), to.c_str())) return reject(error, "Cannot replace slot preference");
        error.clear(); return true;
    }
    bool remove(const std::string& path, std::string& error) override {
        if (std::remove(path.c_str()) && errno != ENOENT) return reject(error, "Cannot remove temporary slot preference");
        error.clear(); return true;
    }
};
bool verify(SessionSaveFileOps& ops, const std::string& path,
            const std::vector<uint8_t>& expected, std::string& error) {
    std::vector<uint8_t> actual;
    if (ops.read(path, slot_preference_bytes, actual, error) != SessionFileReadResult::Ok) {
        if (error.empty()) error = "Slot preference disappeared during verification";
        return false;
    }
    if (actual != expected) return reject(error, "Slot preference verification failed");
    return true;
}
void cleanup(SessionSaveFileOps& ops, const std::string& path, std::string& error) {
    std::string detail;
    if (!ops.remove(path, detail)) error += "; " + detail;
}
bool restore(SessionSaveFileOps& ops, const std::string& primary,
             const std::vector<uint8_t>& previous, std::string& error) {
    std::vector<uint8_t> actual; std::string detail;
    const auto state = ops.read(primary, slot_preference_bytes, actual, detail);
    if (state == SessionFileReadResult::Ok && actual == previous) return true;
    if (state == SessionFileReadResult::Missing && ops.write_new(primary, previous, detail) &&
        verify(ops, primary, previous, detail)) {
        error += "; prior preference restored"; return true;
    }
    error += "; prior preference retained at " + primary + ".bak; primary recovery required";
    if (!detail.empty()) error += ": " + detail;
    return false;
}
} // namespace

bool encode_slot_preference(uint32_t max_slots, uint32_t slot,
                            std::vector<uint8_t>& output, std::string& error) {
    if (!max_slots || slot > max_slots) return reject(error, "Invalid slot preference range");
    std::vector<uint8_t> candidate(slot_preference_bytes);
    std::memcpy(candidate.data(), magic, sizeof(magic));
    put32(candidate.data() + 8, slot_preference_schema);
    put32(candidate.data() + 12, slot);
    put32(candidate.data() + 16, checksum(candidate.data(), 16));
    output = std::move(candidate); error.clear(); return true;
}
bool decode_slot_preference(const uint8_t* bytes, size_t size, uint32_t max_slots,
                            uint32_t& output, std::string& error) {
    if (!max_slots) return reject(error, "Invalid slot preference range");
    if (!bytes || size != slot_preference_bytes || std::memcmp(bytes, magic, sizeof(magic)))
        return reject(error, "Invalid slot preference format/size");
    if (get32(bytes + 8) != slot_preference_schema) return reject(error, "Unsupported slot preference schema");
    if (get32(bytes + 16) != checksum(bytes, 16)) return reject(error, "Slot preference checksum mismatch");
    const auto slot = get32(bytes + 12);
    if (slot > max_slots) return reject(error, "Slot preference outside configured range");
    output = slot; error.clear(); return true;
}
SlotPreferenceReadResult read_slot_preference(const char* path, uint32_t max_slots,
                                             uint32_t& output, std::string& error,
                                             SessionSaveFileOps* operations) {
    if (!valid_path(path, error)) return SlotPreferenceReadResult::Error;
    if (!max_slots) { error = "Invalid slot preference range"; return SlotPreferenceReadResult::Error; }
    LocalPreferenceFileOps local; SessionSaveFileOps& ops = operations ? *operations : local;
    std::vector<uint8_t> bytes;
    const auto status = ops.read(path, slot_preference_bytes, bytes, error);
    if (status == SessionFileReadResult::Missing) { error.clear(); return SlotPreferenceReadResult::Missing; }
    if (status == SessionFileReadResult::Error) return SlotPreferenceReadResult::Error;
    return decode_slot_preference(bytes.data(), bytes.size(), max_slots, output, error)
        ? SlotPreferenceReadResult::Ok : SlotPreferenceReadResult::Error;
}
bool write_slot_preference(const char* path, uint32_t max_slots, uint32_t slot,
                           std::string& error, SessionSaveFileOps* operations) {
    if (!valid_path(path, error)) return false;
    std::vector<uint8_t> bytes;
    if (!encode_slot_preference(max_slots, slot, bytes, error)) return false;
    LocalPreferenceFileOps local; SessionSaveFileOps& ops = operations ? *operations : local;
    const std::string primary(path), temporary = primary + ".tmp";
    const std::string backup = primary + ".bak", backup_temporary = primary + ".bak.tmp";
    std::vector<uint8_t> previous;
    const auto old = ops.read(primary, slot_preference_bytes, previous, error);
    if (old == SessionFileReadResult::Error) return false;
    if (old == SessionFileReadResult::Ok) {
        uint32_t checked;
        if (!decode_slot_preference(previous.data(), previous.size(), max_slots, checked, error)) {
            error = "Refusing to replace invalid slot preference: " + error; return false;
        }
    }
    if (!ops.write_new(temporary, bytes, error)) return false;
    if (!verify(ops, temporary, bytes, error)) { cleanup(ops, temporary, error); return false; }
    if (old == SessionFileReadResult::Ok) {
        if (!ops.write_new(backup_temporary, previous, error)) { cleanup(ops, temporary, error); return false; }
        if (!verify(ops, backup_temporary, previous, error)) {
            cleanup(ops, backup_temporary, error); cleanup(ops, temporary, error); return false;
        }
        if (!ops.replace(backup_temporary, backup, error)) {
            cleanup(ops, backup_temporary, error); cleanup(ops, temporary, error); return false;
        }
        if (!verify(ops, backup, previous, error)) { cleanup(ops, temporary, error); return false; }
    }
    if (!ops.replace(temporary, primary, error)) {
        const bool recovered = old != SessionFileReadResult::Ok || restore(ops, primary, previous, error);
        if (recovered) cleanup(ops, temporary, error);
        return false;
    }
    error.clear(); return true;
}
} // namespace encore::upstream
