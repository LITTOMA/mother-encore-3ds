#include "encore/global_yaml_file.hpp"
#include "manual_require.hpp"
using namespace encore::upstream;
// Caller supplies the actual initialized source namespace/Registry/cache host;
// no mock ObjectIDs, fake Directory completion or source scene execution.
void global_yaml_file_reference_manual(GlobalYamlFileHost &host,
                                       FieldGlobalRegistry &registry,
                                       const GlobalPackedFile &source) {
  std::string error;
  std::shared_ptr<GlobalYamlFileReference> file;
  MANUAL_REQUIRE(host.make_file(0, file, error));
  auto outer = file->binding().object;
  MANUAL_REQUIRE(registry.native_reference(outer).get() == file.get());
  MANUAL_REQUIRE(!file->open("user://outside-scope.yaml", 1, error));
  MANUAL_REQUIRE(!file->open("res://" + source.source, 2, error));
  MANUAL_REQUIRE(!file->is_open());
  MANUAL_REQUIRE(file->open("res://" + source.source, 1, error));
  std::string text;
  MANUAL_REQUIRE(file->get_as_text(true, text, error));
  MANUAL_REQUIRE(file->position() == 0);
  MANUAL_REQUIRE(!file->invoke_node_method("_ready", error));
  MANUAL_REQUIRE(file->close(error));
  MANUAL_REQUIRE(!file->get_line(text, error));
  file.reset();
  MANUAL_REQUIRE(!registry.object_exists(outer));
  std::shared_ptr<GlobalYamlSmartReader> reader;
  MANUAL_REQUIRE(host.make_reader(reader, error));
  auto object = reader->binding().object;
  auto member = reader->member_file_object();
  MANUAL_REQUIRE(member && object < member);
  MANUAL_REQUIRE(registry.object_exists(object) &&
                 registry.object_exists(member));
  MANUAL_REQUIRE(reader->open("res://" + source.source, error));
  bool end = false;
  while (!end) {
    MANUAL_REQUIRE(reader->end_reached(end, error));
    if (!end)
      MANUAL_REQUIRE(reader->next_line(false, text, error));
  }
  MANUAL_REQUIRE(reader->close(error));
  reader.reset();
  MANUAL_REQUIRE(!registry.object_exists(member) &&
                 !registry.object_exists(object));
}
