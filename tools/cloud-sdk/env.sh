# Source with Bash. All tools and writable application state remain in this workspace.
TOOLCHAIN_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export DEVKITPRO="$TOOLCHAIN_ROOT/sdk-image/opt/devkitpro"
export DEVKITARM="$DEVKITPRO/devkitARM"
export CTRULIB="$DEVKITPRO/libctru"
export PATH="$TOOLCHAIN_ROOT/bin:$TOOLCHAIN_ROOT/makerom-0.19.0:$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"
export XDG_DATA_HOME="$TOOLCHAIN_ROOT/data"
export XDG_CACHE_HOME="$TOOLCHAIN_ROOT/cache"
export XDG_CONFIG_HOME="$TOOLCHAIN_ROOT/home"

export PATH="$TOOLCHAIN_ROOT/python-tools/cmake/data/bin:$PATH"
