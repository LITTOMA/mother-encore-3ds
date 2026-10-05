#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// Schema identities, independent of filenames and saved game identities.
enum class ResourceRole : uint32_t {
    Room=1, Blackbars=2, Battle=3, Round=4, House=5, Items=6, Audio=7,
    Phone=8, Choices=9, SaveMenu=10, Session=11, Settings=12, Prompts=13,
    Continue=14, Restore=15, SessionMigration=16, NewGame=17,
    Localization=18, TitleLocale=19, SourceFonts=20, Input=21,
    LoadingIndicator=22, EncounterBattle=23, EncounterRound=24, Introduction=25,
    HouseInspections=26, DrawerProgram=27, Storage=28, ItemDetails=29
};

class ResourceCatalog {
public:
    bool load(const uint8_t*, size_t, std::string&);
    bool load_file(const char*, std::string&);
    bool valid() const { return valid_; }
    const std::string& path(ResourceRole) const;
    std::string companion_path(std::string_view battle_path) const;
    bool verify_files(const char* prefix, std::string& error) const;
private:
    struct Binding { uint32_t id; ResourceRole role; std::string path; uint32_t size, crc; };
    struct Encounter { uint32_t battle, round; };
    bool valid_=false;
    std::vector<Binding> bindings_;
    std::vector<Encounter> encounters_;
};
}
