# Optional isolated Linux SDK restoration

This directory contains only verified official-image manifests and a small downloader, not an SDK binary distribution.
Use Python3.12+ and first create `downloads/`. Run `python3 fetch-official-sdk.py` from any directory; it places SDK payloads next to this script.
The script retrieves a public read-only registry token, checks every layer SHA256, and extracts SDK/license/package metadata only with tar data filtering. It does not run image entrypoints or install system settings.

Official image: docker.io/devkitpro/devkitarm. Manifest digest and package list are in reference/.
Official vendor source: https://github.com/devkitPro/docker

Additional standalone official ZIPs and their recorded digests:
- Godot3.6.2: https://github.com/godotengine/godot/releases/download/3.6.2-stable/Godot_v3.6.2-stable_linux_headless.64.zip
- makerom0.19.0: https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-ubuntu_x86_64.zip
- CTRTool1.3.0: https://github.com/3DSGuy/Project_CTR/releases/download/ctrtool-v1.3.0/ctrtool-v1.3.0-ubuntu_x86_64.zip
- bannertool1.2.2: https://github.com/Epicpkmn11/bannertool/releases/download/v1.2.2/bannertool.zip

Extract tools in this directory's versioned folders or bin/, verify archives against reference/downloads.sha256 and bannertool-manifest.json, then adjust PATH as needed. env.sh reflects the original isolated layout and is a template, not an automatic complete installer.
CMake was3.31.10, GCC host14.2; these are ordinary host dependencies. Exact bit-reproducibility is not claimed.

Optional emulator: official Azahar2126.1.2 AppImage from https://github.com/azahar-emu/azahar/releases/tag/2126.1.2
SHA2561ea15020334ee2e8fd16fbb3911fa7eafa8111e311c54dd4387b61b2ea742df6.
Use a graphical session. This project's emulator checks do not require or obtain firmware, keys or system dumps.
