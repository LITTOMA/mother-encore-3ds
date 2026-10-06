#include "manual_require.hpp"
#include "podunk_ui_host.hpp"
#include <cmath>
using namespace encore::upstream;
using namespace encore::ctr;
// Manual host case: caller supplies the actual Registry-constructed UI owner
// after constructor publication and before UiManager's source Ready. It must
// also supply the real signal owner; this case does not manufacture one.
void field_ui_constructor_flavor_manual(PodunkUiHost &host,
                                        const FieldUiManagerData &source) {
  MANUAL_REQUIRE(source.valid());
  std::string error;
  FieldGlobalExternalState before;
  MANUAL_REQUIRE(host.state(before, error));
  MANUAL_REQUIRE(!before.ready);
  auto *runtime = host.ui();
  MANUAL_REQUIRE(runtime && runtime->ready_cursor() == 0);
  const FieldUiMenuShader *shader = nullptr;
  MANUAL_REQUIRE(host.menu_shader(shader, error) && shader);
  const auto object = shader->binding().object;
  MANUAL_REQUIRE(object != 0);
  for (size_t index = 0; index < source.flavors().size(); ++index) {
    MANUAL_REQUIRE(host.set_menu_flavors(source.flavors()[index], error));
    const FieldUiMenuShader *same = nullptr;
    MANUAL_REQUIRE(host.menu_shader(same, error));
    MANUAL_REQUIRE(same == shader && same->binding().object == object);
    for (size_t cell = 0; cell < source.palette()[index].size(); ++cell) {
      const auto raw = source.palette()[index][cell];
      const FieldColor expected{float((raw >> 16) & 255) / 255.0f,
                                float((raw >> 8) & 255) / 255.0f,
                                float(raw & 255) / 255.0f, 1.0f};
      MANUAL_REQUIRE(same->new_colors()[cell] == expected);
    }
    FieldGlobalExternalState after;
    MANUAL_REQUIRE(host.state(after, error));
    MANUAL_REQUIRE(
        after.ready == before.ready && after.inside == before.inside &&
        after.parent == before.parent && runtime->ready_cursor() == 0);
  }
  // Actual FLAVORS.find returns -1: the source Array selects its final row.
  MANUAL_REQUIRE(host.set_menu_flavors("not-a-source-flavor", error));
  const auto &last = source.palette().back();
  for (size_t cell = 0; cell < last.size(); ++cell) {
    const auto raw = last[cell];
    MANUAL_REQUIRE(shader->new_colors()[cell][0] ==
                   float((raw >> 16) & 255) / 255.0f);
  }
  PodunkUiHost unconstructed;
  const FieldUiMenuShader *unchanged = shader;
  MANUAL_REQUIRE(!unconstructed.menu_shader(unchanged, error));
  MANUAL_REQUIRE(unchanged == shader);
  MANUAL_REQUIRE(
      !unconstructed.set_menu_flavors(source.flavors().front(), error));
  FieldUiManagerRuntime uninitialized;
  MANUAL_REQUIRE(
      !uninitialized.set_menu_flavors(source.flavors().front(), error));
  MANUAL_REQUIRE(!uninitialized.checked_menu_shader(unchanged, error));
  MANUAL_REQUIRE(unchanged == shader);
}
