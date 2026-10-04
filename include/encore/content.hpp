#pragma once
#include "encore/crc32.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace encore {
constexpr uint32_t kPackVersion=1, kRulesVersion=1, kCapabilities=0x0f;
constexpr uint32_t kMaxFlags=32, kContentFamily=0x454e0001;
constexpr size_t kMaxPackBytes=4*1024*1024;
enum class Op : uint8_t { End=0, Say=1, SetFlag=2, IfFlag=3, Jump=4, Wait=5, Teleport=6, Battle=7 };
enum class ObjectKind : uint8_t { Guide=0, Door=1, Terminal=2, Sign=3 };
struct Instruction { Op op=Op::End; int32_t a=0,b=0,c=0; };
struct Program { uint32_t id=0; std::vector<Instruction> code; };
struct Object { uint32_t id=0; ObjectKind kind=ObjectKind::Guide; int16_t x=0,y=0; uint32_t program=0,name=0; };
struct Map {
    uint32_t id=0,title=0; uint16_t width=0,height=0;
    std::vector<uint8_t> tiles; std::vector<Object> objects;
    bool blocked(int x, int y) const;
};
struct Enemy { uint32_t id=0,name=0; int32_t hp=0,attack=0,defense=0,reward=0; };
struct Content {
    uint32_t family=0,rules=0,payload_crc=0,start_map=0;
    int32_t spawn_x=0,spawn_y=0;
    std::vector<std::string> strings; std::vector<Map> maps;
    std::vector<Program> programs; std::vector<Enemy> enemies; std::vector<uint32_t> flag_ids;
    // Validates bytes and all cross-references before replacing the current content.
    bool load(const uint8_t* bytes, size_t size, std::string& error);
    bool load_file(const char* path, std::string& error);
    const std::string& text(uint32_t index) const { return strings.at(index); }
};
bool read_file(const char* path, std::vector<uint8_t>& out, size_t limit, std::string& error);
bool write_file_atomic(const char* path, const std::vector<uint8_t>& data, std::string& error);
}
