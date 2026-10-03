#include "encore/load_progress.hpp"
#include <cassert>
#include <thread>
int main(){unsigned main_events=0,worker_events=0;const auto observe=[](void* p,const encore::LoadProgress&){++*static_cast<unsigned*>(p);};encore::ScopedLoadProgress main_observer(observe,&main_events);std::thread worker([&]{encore::report_load_progress(encore::LoadPhase::Checksum,1,1);{encore::ScopedLoadProgress other(observe,&worker_events);encore::report_load_progress(encore::LoadPhase::Texture,1,1);}encore::report_load_progress(encore::LoadPhase::Scene,1,1);});worker.join();encore::report_load_progress(encore::LoadPhase::Scene,1,1);assert(main_events==1&&worker_events==1);}
