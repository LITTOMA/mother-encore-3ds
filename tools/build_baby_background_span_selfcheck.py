#!/usr/bin/env python3
"""Build an isolated ARM fallback-span candidate; does not launch an emulator."""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError("Selfcheck source shape changed: " + old)
    return text.replace(old,new)


def main():
    sdk = Path(os.environ["DEVKITPRO"])
    arm = Path(os.environ.get("DEVKITARM",str(sdk/"devkitARM")))
    production_digest = digest(ROOT/"include/encore/background_kernel.hpp")
    if production_digest not in {"47a3bcd332e3c22c9e0f23f0cad3030fe866615c76574ab8f45cf058e4e8902e", "e722c99c7a475444faf71a1d5165bb1c0f9443995dd9f6426a2a399d4b6054c5"}:
        raise SystemExit("Production header differs from reviewed revisions")
    expected = {"include/encore/background_kernel.hpp":production_digest,
                "tests/baby_background_combined_prototype.hpp":"47a3bcd332e3c22c9e0f23f0cad3030fe866615c76574ab8f45cf058e4e8902e",
                "tests/baby_background_scalar_reference.hpp":"883d150df13a8bfefda5009a078fdc07051e2ec195ab8b86a01424e03470bbef"}
    for path,sha in expected.items():
        if digest(ROOT/path) != sha:
            raise SystemExit("Frozen input changed: " + path)
    build = ROOT/"build/baby-background-arm-selfcheck/spans"
    build.mkdir(parents=True,exist_ok=True)
    source = (ROOT/"tests/baby_background_arm_selfcheck.cpp").read_text()
    source = replace_once(source,'#include "encore/background_kernel.hpp"','''#define BackgroundKernel CurrentProductionBackgroundKernel
#include "tests/baby_background_combined_prototype.hpp"
#undef BackgroundKernel
#include "tests/baby_background_span_prototype.hpp"''')
    source = replace_once(source,'#define SELF_CHECK_LABEL "Production " SELF_CHECK_KERNEL_TAG " / Scalar883d150d"','#define SELF_CHECK_LABEL "Fallback spans / production47a3bcd / scalar883d150d"')
    source = replace_once(source,'std::vector<encore::BackgroundKernel::Layer> layers;', 'std::vector<encore::BackgroundKernel::Layer> layers;\n    std::vector<encore::CurrentProductionBackgroundKernel::Layer> production_layers;')
    source = replace_once(source,'layers.push_back(layer<encore::BackgroundKernel>(b,source[n],palette[n]));','layers.push_back(layer<encore::BackgroundKernel>(b,source[n],palette[n]));\n        production_layers.push_back(layer<encore::CurrentProductionBackgroundKernel>(b,source[n],palette[n]));')
    source = replace_once(source,'encore::BackgroundKernel guarded;encore::ScalarBackgroundKernel scalar;','encore::BackgroundKernel guarded;encore::ScalarBackgroundKernel scalar;encore::CurrentProductionBackgroundKernel production;\n        if(!production.prepare(production_layers,width,height,error)){message("FAIL production prepare\\n");return false;}')
    source = replace_once(source,'for(float time:times){','if(!production.prepare_mapped_output(offsets.data(),offsets.size(),texture_count,error)){message("FAIL production layout\\n");return false;}\n        for(float time:times){')
    source = replace_once(source,'mapped_bytes_compared+=texture_count*sizeof(uint32_t);','''mapped_bytes_compared+=texture_count*sizeof(uint32_t);
            start=svcGetSystemTick();const bool production_ok=production.compose_mapped(time,clear,mapped.data()+1,texture_count);const double production_ms=elapsed(start);
            if(!production_ok){message("FAIL production mapped\\n");return false;}
            for(size_t i=0;i<texture_count;++i)if(mapped[i+1]!=mapped_expected[i]){message("FAIL production parity\\n");return false;}
            message("Production mapped %.1f ms\\n",production_ms);''')
    generated = build/"span-selfcheck.cpp"
    generated.write_text(source)
    staged=build/"romfs"
    paths=["data/doll-entry.encbattle","doll-preview/doll-background.bpx","doll-preview/doll-palette.bpx"]
    for path in paths:
        destination=staged/path
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(ROOT/"romfs"/path,destination)
    elf=build/"baby-background-span-selfcheck.elf"
    smdh=build/"baby-background-span-selfcheck.smdh"
    output=build/"baby-background-span-selfcheck.3dsx"
    commands=[[
        str(arm/"bin/arm-none-eabi-g++"),"-std=gnu++17","-O2","-g","-Wall","-Wextra","-Wpedantic","-Werror","-ffp-contract=off",
        "-march=armv6k","-mtune=mpcore","-mfloat-abi=hard","-mtp=soft","-D__3DS__","-fno-exceptions","-fno-rtti",
        "-ffunction-sections","-fdata-sections","-I"+str(ROOT),"-I"+str(ROOT/"include"),"-isystem",str(sdk/"libctru/include"),
        "-specs=3dsx.specs","-Wl,--gc-sections",str(generated),str(ROOT/"runtime/battle_data.cpp"),str(ROOT/"runtime/file_io.cpp"),
        "-L"+str(sdk/"libctru/lib"),"-lctru","-lm","-o",str(elf)],
        [str(sdk/"tools/bin/smdhtool"),"--create","Baby Span ARM Selfcheck","Exact fallback batching","Encore Native tests",str(ROOT/"assets/icon.png"),str(smdh)],
        [str(sdk/"tools/bin/3dsxtool"),str(elf),str(output),"--smdh="+str(smdh),"--romfs="+str(staged)]]
    with (build/"build.log").open("w") as log:
        for command in commands:
            log.write(shlex.join(command)+"\n");log.flush()
            subprocess.run(command,check=True,stdout=log,stderr=subprocess.STDOUT)
    report={"scope":"ARM compile/package only; execution pending and no ARM speed claim", "commands":commands,
            "sources":{p:digest(ROOT/p) for p in [*expected,"tests/baby_background_span_prototype.hpp","tests/baby_background_arm_selfcheck.cpp","tools/build_baby_background_span_selfcheck.py"]},
            "generated_source_sha256":digest(generated),"elf_sha256":digest(elf),"binary_sha256":digest(output),"binary":str(output),
            "romfs_sha256":{p:digest(staged/p) for p in paths},"expected_pixels":614400,"expected_mapped_bytes":4194304}
    (build/"build.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,indent=2))


if __name__ == "__main__":
    main()
