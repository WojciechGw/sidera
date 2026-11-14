"""Compare actual C interval lookup/local dates against the pinned ZoneInfo."""
import datetime as dt
import io
import random
import struct
import subprocess
import sys
import zipfile
import tempfile
import shutil
from pathlib import Path
from zoneinfo import ZoneInfo

root=(Path(__file__).resolve().parents[1] / 'src')
blob=(root/'rom/sidera.tz').read_bytes()
magic,count,size,version=struct.unpack_from('<8sHH4x8s',blob)
assert (magic,size,version.rstrip(b'\0'))==(b'SIDTZ001',48,b'2026f')
base=dt.datetime(1999,12,31,tzinfo=dt.timezone.utc)
start=dt.datetime(1900,1,1,tzinfo=dt.timezone.utc)
end=dt.datetime(2201,1,1,tzinfo=dt.timezone.utc)
cases=[];expected=[];rng=random.Random(6502)
with zipfile.ZipFile(root/'data/tzdata-2026.6-py2.py3-none-any.whl') as wheel:
    for i in range(count):
        name,pos,n=struct.unpack_from('<40sIH',blob,24+i*48)
        name=name.split(b'\0')[0].decode();assert pos+n*12<=len(blob)
        zone=ZoneInfo.from_file(io.BytesIO(wheel.read('tzdata/zoneinfo/'+name)))
        dates={start,end-dt.timedelta(seconds=1)}
        for year in (1900,1915,1945,1970,2000,2026,2038,2100,2200):
            dates.add(dt.datetime(year,rng.randrange(1,13),15,12,34,56,tzinfo=dt.timezone.utc))
        # Every compiled transition, both sides including its exact second.
        for j in range(n):
            day,sec,offset=struct.unpack_from('<iii',blob,pos+j*12)
            t=base+dt.timedelta(days=day,seconds=sec)
            for delta in (-1,0,1):
                when=t+dt.timedelta(seconds=delta)
                if start<=when<end:dates.add(when)
        # Reverse order too: cache must handle simulation rewind.
        ordered=sorted(dates);ordered+=list(reversed(ordered[-10:]))
        for utc in ordered:
            cases.append(f'{i} {utc.year} {utc.month} {utc.day} {utc.hour} {utc.minute} {utc.second}\n')
            local=utc.astimezone(zone)
            expected.append(f'{int(local.utcoffset().total_seconds())} {local:%Y-%m-%d %H:%M:%S}')
# Keep the many small, real file reads on the native temporary filesystem.
with tempfile.TemporaryDirectory(prefix='sidera-timezones-') as directory:
    shutil.copyfile(root/'rom/sidera.tz',Path(directory)/'sidera.tz')
    result=subprocess.run([sys.argv[1],directory],input=''.join(cases),text=True,capture_output=True,check=True)
actual=result.stdout.splitlines()
assert len(actual)==len(expected),(len(actual),len(expected),result.stderr)
for i,(a,e) in enumerate(zip(actual,expected)):
    assert a==e,(cases[i].strip(),a,e)
print(f'PASS: {len(cases)} C timezone conversions, {count} zones; every transition +/-1s, 1900/2200 edges and rewind')
