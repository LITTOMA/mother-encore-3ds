# Development contract

Read `README.md`, `docs/STATUS.md` and `docs/DEVELOPMENT_PLAN.md` first.
This is a partial native C++ 3DS port of Mother: Encore. Do not describe it as a complete game.

1. Keep the official upstream submodule read-only. Use separate work copies for imports, reference probes and candidate updates. Never patch generated packs or manifests by hand.
2. Never invent an upstream commit, a passed build, a hardware run or an implemented mechanism.
3. Unknown source, behavior, opcodes and versions must fail closed. Do not add blanket ignore rules to make CI pass.
4. Updating a reviewed SHA requires an actual semantic review, mapped implementation and relevant tests. Include dependent source in the review.
5. Keep 3DS headers and rendering out of `runtime/`. Desktop and 3DS must share the same game core.
6. Check source and permission before adding fonts, game assets or third-party binaries. Preserve their copyright and license notices; the root license does not relicense dependencies.
7. M0 battle, movement, flags and VM are isolated fixture rules. Do not claim they reproduce upstream gameplay or substitute them for original game mechanics.
8. Maintain pack format, capabilities, rule compatibility and save schema independently. Use stable IDs for saved identity.
9. Preserve actual test logs and distinguish host, cross-build, packaging, emulator and hardware verification. Historical results only cover their recorded source and artifacts.
10. Tests are manual and start only on explicit request. Do not run local test suites or full CI mode during routine development, pushes or PR updates. Every branch push automatically builds genuine 3DSX/CIA and uploads Actions artifacts; manual build mode does the same without tests. Retain comprehensive `make test`, GCC/Clang sanitizers and real `make 3dsx` / `make cia` verification for requested full runs, with actual logs and default leak checking. Resource compilation still performs its required source and format admission.
11. Add negative tests for every new parser, opcode and version path. Never silently discard unsupported content or partially commit a failed load.
12. Keep build outputs in `build/` or `dist/`. Never manufacture files with `.3dsx` or `.cia` extensions without invoking the real toolchain.
13. Avoid a generic Godot or GDScript reimplementation unless the audited game requires that scope.
14. Do not update trace expectations merely to remove a mismatch. Explain the source behavior change and verify the affected semantics.
15. Commit workflow and build fixes with the exact failure and verification level. CI configuration existing is not CI passing.
16. Strict program/data separation is mandatory: all real game content, bindings and rule tuning must live in independent binary resources. C++ contains execution, schemas and checked loading only. Do not retain game-content `constexpr` tables or hide them under different names.

Current priority: complete the full scoped content/data migration before further gameplay expansion. A pinned upstream and real cross-builds are established; physical-console validation remains explicitly unverified. Use `docs/STATUS.md` for the supported scope and `docs/DEVELOPMENT_PLAN.md` for work order.

Follow `docs/GIT_WORKFLOW.md`: use standard Git, focused task branches and draft PRs for unfinished work; push completed commits and verify the remote commit. Record sources, compatibility changes, actual checks and remaining limitations for review.
