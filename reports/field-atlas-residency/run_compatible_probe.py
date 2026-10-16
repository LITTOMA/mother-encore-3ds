from pathlib import Path
import hashlib,subprocess,sys,types
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root))
# The user's menu build uses probe v7; main still emits v6. Reuse the already
# reviewed dual-version reader without changing game bytes or the main branch.
ref='7a0f1156dead098724ff7cb5eba2ff6cae639ecf'
modules=[]
for name in ('semantic_probe','run_emulator_semantics'):
    source=subprocess.check_output(['git','show',ref+':tools/'+name+'.py'],cwd=root)
    target=root/'build/field-atlas'/('reader-'+name+'.py');target.write_bytes(source)
    module=types.ModuleType('tools.'+name);module.__file__=str(target)
    sys.modules[module.__name__]=module;exec(compile(source,module.__file__,'exec'),module.__dict__);module.ROOT=root;modules.append(module)
    print('Test reader:',ref,name,hashlib.sha256(source).hexdigest(),flush=True)
modules[-1].CONFIG=modules[-1].CONFIG.replace('graphics_api=1','graphics_api=2').replace('log_filter=*:Warning','log_filter=*:Warning Debug.Emulated:Debug')
modules[-1].main()
