#include "encore/audio_server.hpp"
#include <cmath>
#include <limits>
namespace encore::upstream {
bool AudioServerRuntime::initialize(const AudioServerData &d, std::string &e) {
  if (data_ || !d.valid() || d.buses().empty()) {
    e = "AudioServer checked layout initialization rejected";
    return false;
  }
  data_ = &d;
  buses_ = d.buses();
  e.clear();
  return true;
}
int AudioServerRuntime::index(std::string_view n) const {
  for (size_t i = 0; i < buses_.size(); ++i)
    if (buses_[i].name == n)
      return static_cast<int>(i);
  return -1;
}
bool AudioServerRuntime::set_volume(uint32_t i, float v, std::string &e) {
  if (!data_ || i >= buses_.size() || !std::isfinite(v)) {
    e = "AudioServer actual bus volume/index rejected";
    return false;
  }
  buses_[i].volume_db = v;
  e.clear();
  return true;
}
bool AudioServerRuntime::gain(std::string_view name, float &out, bool &muted,
                              std::string &e) const {
  if (!data_) {
    e = "AudioServer bus body absent";
    return false;
  }
  // Native thread_find_bus_index falls back to the master bus, unlike the
  // public get_bus_index operation. Forward/self sends likewise use master.
  int found = index(name);
  size_t at = found < 0 ? 0 : static_cast<size_t>(found);
  std::vector<bool> soloed(buses_.size(), false);
  bool solo = false;
  for (size_t i = 0; i < buses_.size(); ++i) {
    if (!buses_[i].solo) {
      soloed[i] = false;
      continue;
    }
    solo = true;
    size_t p = i;
    soloed[p] = true;
    size_t remaining = buses_.size() + 1;
    while (p) {
      if (!--remaining) {
        e = "AudioServer native solo chain is cyclic";
        return false;
      }
      int n = index(buses_[p].send);
      size_t next = n < 0 ? 0 : static_cast<size_t>(n);
      if (p >= next)
        next = 0;
      p = next;
      soloed[p] = true;
    }
  }
  double gain = 0;
  bool silence = false;
  for (size_t remaining = buses_.size() + 1; remaining; --remaining) {
    const auto &b = buses_[at];
    if (!b.bypass)
      for (const auto &fx : b.effects)
        if (fx.enabled) {
          e = "AudioServer effect processor unavailable for actual bus: " +
              b.name + " / " + fx.native_class;
          return false;
        }
    silence = silence || (solo ? !soloed[at] : b.muted);
    gain += b.volume_db;
    if (!std::isfinite(gain) || gain > std::numeric_limits<float>::max() ||
        gain < -std::numeric_limits<float>::max()) {
      e = "AudioServer bus route gain overflow";
      return false;
    }
    if (!at) {
      out = static_cast<float>(gain);
      muted = silence;
      e.clear();
      return true;
    }
    int next = index(b.send);
    at = next < 0 || static_cast<size_t>(next) >= at
             ? 0
             : static_cast<size_t>(next);
  }
  e = "AudioServer actual bus route did not terminate";
  return false;
}
} // namespace encore::upstream
