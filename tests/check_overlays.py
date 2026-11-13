"""Check the shipped image, every overlay, bounds, identity and memory budget."""
import importlib.util
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from pack_overlays import BASE, LIMIT, NAMES, labels

tool, folder, image = sys.argv[1:]
work = Path(folder)
spec = importlib.util.spec_from_file_location('rp6502_tool', tool)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
rom = module.ROM()
rom.add_rom_file(image)
assets = dict(rom.assets)
assert len(assets) == len(rom.assets)
symbols = labels(work / 'sidera.lbl')
binary = (work / 'sidera.bin').read_bytes()
expected = dict(enumerate(binary, 0x200))
expected.update({0xFFFC: 0, 0xFFFD: 2})
assert rom.data == expected, 'Overlays must be assets, not initial CPU memory'
end = max(0x200 + len(binary), symbols['__BSS_RUN__'] + symbols['__BSS_SIZE__'])
assert end <= BASE and BASE % 2048 == 0
assert symbols['__STACKSIZE__'] == 2048
assert symbols['__RAM_START__'] + symbols['__RAM_SIZE__'] == 0xF700
build = binary[symbols['_ovl_build_id'] - 0x200:symbols['_ovl_build_id'] - 0x200 + 8]
app = (Path(__file__).resolve().parents[1] / 'src')
resources = {('manual' if p.name == 'sidera-manual-en.txt' else p.name): p.read_bytes()
             for p in (app / 'rom').iterdir()}
resources['help'] = (app / 'help.txt').read_bytes()
assert set(assets) == set(resources) | {f'sidera-{n}.ovl' for n in NAMES}
for name, content in resources.items():
    assert assets[name] == content, name
for number, name in enumerate(NAMES, 1):
    raw = assets[f'sidera-{name}.ovl']
    magic, ident, version, size, bss, zeros, stamp, crc = struct.unpack('<4sBBHHH8sI', raw[:24])
    assert (magic, ident, version, stamp) == (b'SOV1', number, 1, build)
    assert len(raw) == 24 + size and 0 < size <= bss <= LIMIT and bss + zeros <= LIMIT
    assert zlib.crc32(raw[24:]) == crc
    assert raw[24:] == (work / f'sidera.linked.bin.{name}').read_bytes()
for name in ('_rpmt_os_lock', '_rpmt_os_unlock', '_rpmt_yield'):
    assert rom.data[symbols[name]] == 0x60
for name in ('_ovl_select', '_ovl_loaded', '_ovl_failed', '_ovl_build_id', '_note',
             '_observer', '_moment', '_config_save', '_assets_close', '_tz_close'):
    assert symbols[name] < BASE, name
assert BASE <= symbols['_solar_draw'] < BASE + LIMIT
assert BASE <= symbols['_solar_orbit'] < BASE + LIMIT
assert BASE <= symbols['_solar_place_labels'] < BASE + LIMIT
assert BASE <= symbols['_solar_names'] < BASE + LIMIT
assert BASE <= symbols['_solar_labels'] < BASE + LIMIT - 63
assert symbols['_solar_visible'] < BASE and symbols['_solar_date'] < BASE
assert BASE <= symbols['_cities_key'] < BASE + LIMIT
assert BASE <= symbols['_show_cover'] < BASE + LIMIT
assert BASE <= symbols['_assets_base'] < BASE + LIMIT
free = BASE - end + (0xF700 - 0xE000)
print(f'PASS: SIDERA, five verified ROM overlays at $C800-$DEFF, scratch $DF00-$DFFF; '
      f'core ends ${end:04X}; {free} bytes unallocated below the C stack '
      f'({BASE - end} below the overlay, 5888 above it).')
