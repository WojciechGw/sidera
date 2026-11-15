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
from zoneinfo._common import load_data

root=(Path(__file__).resolve().parents[1] / 'src')
blob=(root/'rom/sidera.tz').read_bytes()
magic,count,size,version=struct.unpack_from('<8sHH4x8s',blob)
assert (magic,size,version.rstrip(b'\0'))==(b'SIDTZ001',48,b'2026f')
assert blob[12:16]==b'DST1'
base=dt.datetime(1999,12,31,tzinfo=dt.timezone.utc)
start=dt.datetime(1900,1,1,tzinfo=dt.timezone.utc)
end=dt.datetime(2201,1,1,tzinfo=dt.timezone.utc)
cases=[];expected=[];rng=random.Random(6502)
with zipfile.ZipFile(root/'data/tzdata-2026.6-py2.py3-none-any.whl') as wheel:
    for i in range(count):
        name,pos,n=struct.unpack_from('<40sIH',blob,24+i*48)
        name=name.split(b'\0')[0].decode();assert pos+n*13<=len(blob)
        data=wheel.read('tzdata/zoneinfo/'+name)
        zone=ZoneInfo.from_file(io.BytesIO(data))
        annual={year:{dt.datetime(year,month,15,tzinfo=zone).dst().total_seconds() for month in range(1,13)}
                for year in range(1899,2202)}
        explicit=load_data(io.BytesIO(data))[1]
        for t in explicit:
            for delta in (-1,0,1):
                local=(dt.datetime(1970,1,1,tzinfo=dt.timezone.utc)+dt.timedelta(seconds=t+delta)).astimezone(zone)
                if local.year in annual:annual[local.year].add(local.dst().total_seconds())
        dates={start,end-dt.timedelta(seconds=1)}
        for t in explicit:
            at=dt.datetime(1970,1,1,tzinfo=dt.timezone.utc)+dt.timedelta(seconds=t)
            for delta in (-1,0,1):
                when=at+dt.timedelta(seconds=delta)
                if start<=when<end:dates.add(when)
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
            flags=annual[local.year];dst=local.dst().total_seconds()
            negative=min(flags)<0
            kind=2 if dst>0 or (negative and dst==0) else int(max(flags)>0 or (negative and 0 in flags))
            expected.append(f'{int(local.utcoffset().total_seconds())} {local:%Y-%m-%d %H:%M:%S} {kind} {name}')
# Keep the many small, real file reads on the native temporary filesystem.
with tempfile.TemporaryDirectory(prefix='sidera-timezones-') as directory:
    shutil.copyfile(root/'rom/sidera.tz',Path(directory)/'sidera.tz')
    result=subprocess.run([sys.argv[1],directory],input=''.join(cases),text=True,capture_output=True,check=True)
actual=result.stdout.splitlines()
assert len(actual)==len(expected),(len(actual),len(expected),result.stderr)
for i,(a,e) in enumerate(zip(actual,expected)):
    assert a==e,(cases[i].strip(),a,e)
# Legacy external catalogues still convert, but cannot claim a DST status.
with tempfile.TemporaryDirectory(prefix='sidera-timezones-legacy-') as directory:
    legacy=bytearray(blob);legacy[12:16]=bytes(4)
    (Path(directory)/'sidera.tz').write_bytes(legacy)
    result=subprocess.run([sys.argv[1],directory],input=cases[0]+cases[-1],text=True,capture_output=True,check=True)
    unknown=[]
    for value in (expected[0],expected[-1]):
        fields=value.split();fields[-2]='3';unknown.append(' '.join(fields))
    assert result.stdout.splitlines()==unknown
# A valid offset table with missing/corrupt seasonal metadata must not invent
# a season, or break ordinary time conversion.
_,pos,n=struct.unpack_from('<40sIH',blob,24)
for corrupt in (False,True):
    with tempfile.TemporaryDirectory(prefix='sidera-timezones-damaged-') as directory:
        damaged=bytearray(blob[:pos+n*12])
        if corrupt:damaged.extend(bytes([255])*n)
        (Path(directory)/'sidera.tz').write_bytes(damaged)
        result=subprocess.run([sys.argv[1],directory],input=cases[0],text=True,capture_output=True,check=True)
        fields=expected[0].split();fields[-2]='3'
        assert result.stdout.strip()==' '.join(fields)
print(f'PASS: {len(cases)} C timezone conversions and DST previews, {count} zones; every transition +/-1s, 1900/2200 edges and rewind')
