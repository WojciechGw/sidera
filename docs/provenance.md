# Provenance

The extraction is based on the current working tree of the project in the mtRAM
repository, approved by the owner, including uncommitted and untracked files.
Source HEAD: `b4654cd`. The commit alone does not reproduce that working tree.

[source-manifest.json](source-manifest.json) records original paths, destinations
and SHA-256 hashes of files before adaptation. Source paths are historical
information; builds and tests use only this repository. The application, all its
tests and generators, data, manuals, reference images, source BMP and a local
copy of `rpc_common`, `rpmt_bare.c`, `rpmt.h`, `rpmt_os_file.c` and `rpmt_os_dir.c`
were transferred. Network libraries and the mtRAM system are not needed to link
SIDERA.

On 13 November 2026, the RMT compatibility layer and unused shared code were
removed. The keyboard decoder from `rpc_common` was retained as
`src/platform/key.c` and `key.h`. RP6502 support is in `src/platform/`, and the
former `src/bare/` directory was renamed to `src/overlays/`. The manifest remains
an unchanged record of the original import; its destination fields are not an
index of the current tree. The scope and validation of the refactoring are
described in the [report](rmt-cleanup.md).

Tools in `tools/` come from RP6502-SDK (`picocomputer/rp6502`). They had already
been downloaded into the destination repository before migration; no custom
changes were made to them. Their license is retained in
[LICENSES/RP6502-SDK.txt](../LICENSES/RP6502-SDK.txt).
The emulator is pinned to version 0.43. Official cc65 installers are linked from
the [SDK documentation](https://picocomputer.github.io/sdk.html).

The author confirmed ownership of the application and transferred shared code.
The initial extraction did not grant a new license. On 13 November 2026,
Wojciech Gwioździk (WojciechGw) selected **GPL-3.0-only** for the project's
original code and documentation. The full text is in [LICENSE](../LICENSE).
Third-party data, artwork and tools retain their existing terms and attribution;
the scope and exceptions are in [LICENSE-NOTICE.txt](../LICENSE-NOTICE.txt).

The CITIES locator uses public-domain Natural Earth 1:110m land, version 4.1.0,
downloaded on 13 November 2026. Normalized rings, the source URL and archive
SHA-256 are retained in `src/data/world-land.json`; the shipped notice is
[sidera-map-license.txt](../src/rom/sidera-map-license.txt). SIDERA supplies the
separate geographic grouping rules and rasterizer; rebuilding needs no network.

Significant adaptations include local paths, standard SDK targets and packing,
generation into `build/`, portable test adapters and using `int32_t` for the
four-byte offset field in the time-zone file. The last change removes the
assumption that `long` is four bytes; it does not change the data format on cc65.
