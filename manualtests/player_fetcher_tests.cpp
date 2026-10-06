#include "../tests/manual_require.hpp"
#include "encore/player_fetcher.hpp"
#include <fstream>
#include <iterator>
namespace encore::upstream {
// Explicit manual format negatives; no lifecycle or source process is run.
void manual_player_fetcher_format(const char*path,const PlayerInitializationData&p,
                                 const FieldNodeTreeData&t,
                                 const FieldGlobalConstructorData&g){
 std::ifstream stream(path,std::ios::binary);
 std::vector<uint8_t>raw((std::istreambuf_iterator<char>(stream)),{});
 MANUAL_REQUIRE(raw.size()>=128);std::string error;PlayerFetcherData good;
 MANUAL_REQUIRE(good.load(raw.data(),raw.size(),p,t,g,error));
 auto reject=[&](std::vector<uint8_t>b){PlayerFetcherData d;MANUAL_REQUIRE(!d.load(b.data(),b.size(),p,t,g,error));MANUAL_REQUIRE(!d.valid());};
 for(auto offset:{size_t(8),size_t(24),size_t(28),size_t(32),size_t(36),size_t(40),size_t(60),size_t(124)}){auto b=raw;b[offset]^=0x40;reject(std::move(b));}
 auto b=raw;b.back()^=1;reject(std::move(b));b=raw;b.resize(127);reject(std::move(b));
 PlayerInitializationData absent_player;FieldNodeTreeData absent_tree;FieldGlobalConstructorData absent_global;PlayerFetcherData refused;
 MANUAL_REQUIRE(!refused.load(raw.data(),raw.size(),absent_player,t,g,error));
 MANUAL_REQUIRE(!refused.load(raw.data(),raw.size(),p,absent_tree,g,error));
 MANUAL_REQUIRE(!refused.load(raw.data(),raw.size(),p,t,absent_global,error));
 // A failed transactional load does not replace an earlier admitted data owner.
 b=raw;b[28]^=1;MANUAL_REQUIRE(!good.load(b.data(),b.size(),p,t,g,error));MANUAL_REQUIRE(good.valid());
}
}
