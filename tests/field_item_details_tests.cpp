#include "encore/field_item_definitions.hpp"
#include "encore/item_details.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
// Manual only: execute explicitly against generated source packs.
int main(int argc, char **argv) {
  assert(argc == 3);
  std::string error;
  encore::upstream::FieldItemDefinitions defs;
  assert(defs.load_file(argv[1], error));
  encore::upstream::ItemDetailsData details;
  assert(details.load_file(argv[2], error));
  auto view = details.view();
  assert(view.field_family());
  assert(view.bind_field_items(defs, error));
  assert(!view.bind_items({}, error));
  const auto *eye = defs.definition("EyeDrops");
  assert(eye);
  encore::upstream::ItemDetailsComposition out;
  const auto measure = [](std::string_view s) { return float(s.size()); };
  assert(view.compose(eye->id, 1, "Ninten", "en", 213, measure, out, error));
  bool icon = false;
  for (const auto &a : out.atoms)
    icon |= a.kind == encore::upstream::ItemDetailsTokenKind::InlineImage;
  assert(icon);
  assert(!view.compose(eye->id, 2, "Ninten", "en", 213, measure, out, error));
  assert(!view.compose(0, 1, "Ninten", "en", 213, measure, out, error));
  assert(
      !view.compose(eye->id, 1, "Ninten", "unknown", 213, measure, out, error));
  std::ifstream file(argv[2], std::ios::binary);
  std::vector<uint8_t> raw{std::istreambuf_iterator<char>(file), {}};
  const auto original = raw;
  raw.at(0) ^= 1;
  encore::upstream::ItemDetailsData rejected;
  assert(!rejected.load(raw.data(), raw.size(), error));
  raw = original;
  raw.push_back(0);
  assert(!rejected.load(raw.data(), raw.size(), error));
  assert(!rejected.load(original.data(), 175, error));
}
