"""Verify that the distributable WEB ZIP contains the current ROM and player."""
from pathlib import Path
import sys
import zipfile


def main():
    build = Path(sys.argv[1])
    with zipfile.ZipFile(build / 'web/sidera.zip') as package:
        expected = {'index.html', 'rp6502.js', 'rp6502.wasm', 'sidera.rp6502'}
        assert set(package.namelist()) == expected, 'Incomplete WEB package'
        assert package.testzip() is None, 'Corrupt WEB package'
        for name in expected:
            assert package.read(name) == (build / 'web/sidera' / name).read_bytes(), name
        assert package.read('sidera.rp6502') == (build / 'sidera/sidera.rp6502').read_bytes(), 'Stale WEB ROM'
        for name in ('rp6502.js', 'rp6502.wasm'):
            assert package.read(name) == (build / 'CMakeFiles/sidera.web/emulator' / name).read_bytes(), 'Wrong player: ' + name
        assert package.read('rp6502.wasm').startswith(b'\0asm\x01\0\0\0'), 'Invalid WebAssembly'
    print('PASS: WEB ZIP, served files, current ROM and configured SDK player agree')


if __name__ == '__main__':
    main()
