#include "encore/podunk_player_character.hpp"
#include <cassert>
using namespace encore::upstream;
// Manual-only source: not registered or executed during development.
int main() {
  PodunkPlayerCharacter owner;
  bool value = true;
  std::string sprite = "preserved", e;
  assert(!owner.character_effect(1, "unknown", value, e));
  assert(value && !e.empty());
  assert(!owner.is_incapacitated(1, value, e));
  assert(value && !e.empty());
  assert(!owner.get_sprite(1, sprite, e));
  assert(sprite == "preserved" && !e.empty());
  PlayerReadyHost host;
  owner.bind(host);
  assert(!host.character_incapacitated(1, value, e));
  assert(value);
}
