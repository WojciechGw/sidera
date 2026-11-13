# Application sources

The code is in `src/`, and the linker script is `src/overlays/overlay.cfg`.
The top-level `CMakeLists.txt` is the build entry point.

Variants without `SIDERA_OVERLAYS` are retained for host and sim65 tests.
The distributed ROM always uses five overlays.

[User manual](../src/rom/sidera-manual-en.txt),
[architecture](architecture.md), [building](../README.md).
