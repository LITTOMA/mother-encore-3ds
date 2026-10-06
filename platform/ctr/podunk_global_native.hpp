#pragma once
#include "podunk_global_host.hpp"
namespace encore::ctr {
// Actual native Node/Node2D ownership. Timer's timing/autostart stays with the
// existing FieldNativeTimers owner in PodunkGlobalHost, never a second clock.
class PodunkGlobalNative final : public PodunkGlobalNativeOwner {
public:
  bool initialize(const upstream::FieldGlobalConstructorData &,
                  upstream::FieldGlobalRegistry &, PodunkNativeRoot &,
                  std::string &);
  bool construct(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
                 const upstream::FieldNodeDescriptor &, std::string &) override;
  bool phase(upstream::FieldNodeTreeRuntime &, upstream::FieldObjectId,
             const upstream::FieldNodeBinding &, upstream::FieldTreePhase,
             std::string &) override;
  bool adopt_continuation_ready(upstream::FieldNodeTreeRuntime &,
      upstream::FieldObjectId,const upstream::FieldNodeBinding &,
      const upstream::HouseGlobalBridgeRuntime &,const upstream::FieldGlobalDataRuntime &,
      const upstream::FieldGlobalConstructorRuntime &,std::string &) override;
  bool input_registration(upstream::FieldObjectId, uint32_t, bool,
                          std::string &) override;
  bool release(upstream::FieldObjectId, const upstream::FieldNodeBinding &,
               std::string &) override;
  enum class Signal : uint32_t {
    TreeEntered = 1,
    TreeExiting,
    TreeExited,
    Ready,
    ChildEntered,
    ChildExiting
  };
  bool connect_signal(upstream::FieldObjectId, Signal, upstream::FieldObjectId,
                      std::string, bool one_shot, std::string &);

private:
  struct Connection {
    Signal signal{};
    upstream::FieldObjectId target = 0;
    std::string method;
    bool one_shot = false;
    uint64_t serial = 0;
  };
  struct Native {
    uint32_t source = 0;
    upstream::FieldIdentity identity{};
    std::string native;
    bool entered = false, post_entered = false, ready = false, continuation_ready = false;
    upstream::FieldTransform last_world{};
    std::vector<Connection> connections;
  };
  const upstream::FieldGlobalConstructorData *data_ = nullptr;
  upstream::FieldGlobalRegistry *registry_ = nullptr;
  PodunkNativeRoot *root_ = nullptr;
  std::map<upstream::FieldObjectId, Native> objects_;
  uint64_t connection_serial_ = 0;
  bool emit(upstream::FieldObjectId, Signal, upstream::FieldObjectId,
            std::string &);
};
} // namespace encore::ctr
