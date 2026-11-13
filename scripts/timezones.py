"""Build offline UTC-offset intervals from the pinned Python/IANA tzdata wheel.

No runtime time_t, network or host timezone database is used on the 6502.
TZif's explicit transitions are retained; its POSIX tail is expanded by
Python's ZoneInfo through 2200. Aliases share identical interval tables.
"""
import datetime as dt
import argparse
import io
import struct
import zipfile
from pathlib import Path
from zoneinfo import ZoneInfo
from zoneinfo._common import load_data

ROOT=(Path(__file__).resolve().parents[1] / 'src')
WHEEL=ROOT/'data/tzdata-2026.6-py2.py3-none-any.whl'
EPOCH=dt.datetime(1970,1,1,tzinfo=dt.timezone.utc)
START=int((dt.datetime(1900,1,1,tzinfo=dt.timezone.utc)-EPOCH).total_seconds())
END=int((dt.datetime(2201,1,1,tzinfo=dt.timezone.utc)-EPOCH).total_seconds())
BASE=int((dt.datetime(1999,12,31,tzinfo=dt.timezone.utc)-EPOCH).total_seconds())

def zone_names():
    with zipfile.ZipFile(WHEEL) as archive:
        return sorted(archive.read('tzdata/zones').decode().splitlines())

def intervals(data):
    zone=ZoneInfo.from_file(io.BytesIO(data))
    def offset(t):
        return int((EPOCH+dt.timedelta(seconds=t)).astimezone(zone).utcoffset().total_seconds())
    explicit=load_data(io.BytesIO(data))[1]
    rows=[(START,offset(START))]
    for t in explicit:
        if START<t<END and offset(t)!=rows[-1][1]:rows.append((t,offset(t)))
    # All historical transitions come from TZif. Beyond its last explicit
    # entry, recurring POSIX-tail changes are separated by more than a day.
    t=max(START,explicit[-1] if explicit else START)
    while t<END:
        nxt=min(t+86400,END)
        if offset(nxt)!=offset(t):
            lo,hi=t,nxt
            while hi-lo>1:
                mid=(lo+hi)//2
                if offset(mid)==offset(t):lo=mid
                else:hi=mid
            if hi<END:rows.append((hi,offset(hi)))
        t=nxt
    assert all(abs(off)<86400 for _,off in rows)
    return b''.join(struct.pack('<iii',(t-BASE)//86400,(t-BASE)%86400,off) for t,off in rows)

def build(output):
    output.mkdir(parents=True,exist_ok=True)
    names=zone_names();index=bytearray();payload=bytearray();cache={}
    with zipfile.ZipFile(WHEEL) as archive:
        for name in names:
            data=archive.read('tzdata/zoneinfo/'+name)
            if data not in cache:
                table=intervals(data)
                cache[data]=(24+48*len(names)+len(payload),len(table)//12)
                payload.extend(table)
            pos,count=cache[data]
            assert len(name)<40 and count<65536
            index.extend(struct.pack('<40sIH2x',name.encode(),pos,count))
        license=archive.read('tzdata-2026.6.dist-info/licenses/LICENSE').decode()
        license+='\n'+archive.read('tzdata-2026.6.dist-info/licenses/licenses/LICENSE_APACHE').decode()
    blob=struct.pack('<8sHH4x8s',b'SIDTZ001',len(names),48,b'2026f')+index+payload
    (output/'sidera.tz').write_bytes(blob)
    (output/'sidera-timezones-license.txt').write_text(
        'SIDERA timezone data: IANA 2026f, Python tzdata 2026.6.\n'
        'https://www.iana.org/time-zones/releases/2026f\n'
        'https://pypi.org/project/tzdata/2026.6/\n'
        'Source wheel retained in data/. UTC offset transitions expanded for 1900-2200.\n'
        'Future dates follow this release, not a prediction of future legislation.\n'
        'Historical coverage and accuracy vary by region, especially before 1970.\n\n'+license,
        encoding='utf8')
    print(len(names),'zones;',len(blob),'bytes')

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--output-dir',type=Path,default=ROOT.parent/'build/generated/rom')
    build(p.parse_args().output_dir)
