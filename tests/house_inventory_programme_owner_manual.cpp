// Manual negative binding case; unregistered and unrun. No mock successful
// Ready/Factory/Room/Inventory receipt is fabricated for a positive path.
#include "encore/world.hpp"
#include <cassert>
using namespace encore::upstream;
struct Unobserved final:OpeningHouseInventoryOwner {
  const OpeningWorld*receiver=nullptr;mutable unsigned observed=0;
  const OpeningWorld*world()const override{return receiver;}
  bool observe_inventory(OpeningHouseInventoryState&,std::string&e)const override{
    ++observed;e="No actual HouseRuntime/Drawer/session source owner";return false;
  }
};
int main(){OpeningWorld actual;Unobserved missing;missing.receiver=&actual;std::string e;
  assert(!actual.bind_house_inventory_owner(missing,e));
  assert(!actual.house_inventory_owner()&&missing.observed==0);
  assert(!actual.admit_house_inventory_command(0,0,e));
  assert(!actual.unbind_house_inventory_owner(missing,e));
}
