from pathlib import Path
import sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools import run_emulator_semantics as test
test.CONFIG=test.CONFIG.replace("graphics_api=1","graphics_api=2").replace("log_filter=*:Warning","log_filter=*:Warning Debug.Emulated:Debug")
test.main()
