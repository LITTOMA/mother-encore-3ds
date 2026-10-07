#include "../platform/ctr/podunk_player_effect_owners.hpp"
#include "manual_require.hpp"
#include <cstdio>
#include <cstring>
// This manual ARM entry checks only the unbound-owner rejection. A positive
// lifecycle needs the actual process ObjectDB, source Tree and loaded GPU.
// It cannot be fabricated by a reader fixture or a callback returning true.
int main(int argc, char **argv) {
  if (argc != 2 || std::strcmp(argv[1], "--unbound-host")) {
    std::fprintf(stderr, "usage: player_effect_owners_manual --unbound-host\n");
    return 2;
  }
  encore::ctr::PodunkConcretePlayerEffectOwners owner;
  std::string error;
  encore::upstream::FieldObjectId untouched = 123;
  MANUAL_REQUIRE(!owner.duplicate_sprite(0, untouched, error));
  MANUAL_REQUIRE(untouched == 123);
  MANUAL_REQUIRE(!owner.play(0, "", error));
  MANUAL_REQUIRE(!owner.owns(0));
  MANUAL_REQUIRE(!owner.accepts({}));
  MANUAL_REQUIRE(!owner.collect_retired(error));
  return 0;
}
