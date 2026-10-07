#include "encore/field_node_tree.hpp"
#include "manual_require.hpp"
using namespace encore::upstream;
// Borrow an actual checked source factory, not an invented node/Ready fixture.
// Standalone manual coverage; intentionally not registered or executed.
void field_native_sort_children_manual(FieldNodeTreeRuntime&tree,
                                      FieldObjectId tilemap,
                                      FieldObjectId wrong_class) {
  std::string error;
  const auto*descriptor=tree.descriptor(tilemap);
  const auto*state=tree.state(tilemap);
  MANUAL_REQUIRE(descriptor&&state&&descriptor->native_class=="TileMap");
  const auto source_flags=descriptor->flags;
  const bool original=bool(state->flags&128);
  MANUAL_REQUIRE(tree.set_sort_children(tilemap,!original,error));
  MANUAL_REQUIRE(bool(tree.state(tilemap)->flags&128)==!original);
  MANUAL_REQUIRE(tree.descriptor(tilemap)->flags==source_flags);
  MANUAL_REQUIRE(tree.set_sort_children(tilemap,original,error));
  MANUAL_REQUIRE(!tree.set_sort_children(0,true,error));
  const auto*other=tree.state(wrong_class);
  const auto*other_descriptor=tree.descriptor(wrong_class);
  MANUAL_REQUIRE(other&&other_descriptor&&
                 other_descriptor->native_class!="YSort"&&
                 other_descriptor->native_class!="TileMap");
  const auto other_flags=other->flags;
  MANUAL_REQUIRE(!tree.set_sort_children(wrong_class,true,error));
  MANUAL_REQUIRE(tree.state(wrong_class)->flags==other_flags);
}
