# Locally verified 2026-10-01: includes 3ds-dev, host GCC/CMake/Python.
# Avoid package updates here: they would invalidate this tested toolchain base.
FROM devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5
WORKDIR /work
CMD ["bash"]
