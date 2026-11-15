"""Build offline UTC-offset intervals from the pinned Python/IANA tzdata wheel.

No runtime time_t, network or host timezone database is used on the 6502.
TZif's explicit transitions are retained; its POSIX tail is expanded by
Python's ZoneInfo through 2200. Aliases share identical interval tables.
"""
import datetime as dt
import argparse
import bisect
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
# UTC endpoints can fall in the adjacent local year. Include those years when
# classifying seasons, while keeping shipped conversion rows within 1900..2200.
SEASON_START=int((dt.datetime(1899,1,1,tzinfo=dt.timezone.utc)-EPOCH).total_seconds())
SEASON_END=int((dt.datetime(2202,1,1,tzinfo=dt.timezone.utc)-EPOCH).total_seconds())
BASE=int((dt.datetime(1999,12,31,tzinfo=dt.timezone.utc)-EPOCH).total_seconds())

def zone_names():
    with zipfile.ZipFile(WHEEL) as archive:
        return sorted(archive.read('tzdata/zones').decode().splitlines())

def intervals(data):
    zone=ZoneInfo.from_file(io.BytesIO(data))
    def state(t):
        local=(EPOCH+dt.timedelta(seconds=t)).astimezone(zone)
        return int(local.utcoffset().total_seconds()), int(local.dst().total_seconds())
    explicit=load_data(io.BytesIO(data))[1]
    rows=[(SEASON_START,*state(SEASON_START))]
    for t in explicit:
        if SEASON_START<t<SEASON_END and state(t)!=rows[-1][1:]:rows.append((t,*state(t)))
    # All historical transitions come from TZif. Beyond its last explicit
    # entry, recurring POSIX-tail changes are separated by more than a day.
    t=max(SEASON_START,explicit[-1] if explicit else SEASON_START)
    current=state(t)
    while t<SEASON_END:
        nxt=min(t+86400,SEASON_END)
        following=state(nxt)
        if following!=current:
            lo,hi=t,nxt
            while hi-lo>1:
                mid=(lo+hi)//2
                if state(mid)==current:lo=mid
                else:hi=mid
            if hi<SEASON_END:rows.append((hi,*state(hi)))
        t=nxt;current=following
    # Classify standard time using the selected local calendar year, not whether
    # this zone ever used DST. Keep transitions that change only the DST flag.
    starts=[r[0] for r in rows]
    years={}
    boundaries=[]
    for year in range(1899,2202):
        first=int((dt.datetime(year,1,1,tzinfo=zone)-EPOCH).total_seconds())
        last=int((dt.datetime(year+1,1,1,tzinfo=zone)-EPOCH).total_seconds())
        years[year]={state(first)[1]} | {dst for at,_,dst in rows if first<=at<last}
        if START<first<END:boundaries.append(first)
    classified=[]
    for at in sorted({START,*boundaries,*[t for t in starts if START<t<END]}):
        _,off,dst=rows[bisect.bisect_right(starts,at)-1]
        year=(EPOCH+dt.timedelta(seconds=at)).astimezone(zone).year
        # IANA can encode winter as negative DST (e.g. Ireland). Present its
        # lower-offset period as winter, and the zero-DST summer as daylight.
        negative=any(value<0 for value in years[year])
        daylight=dst>0 or (negative and dst==0)
        seasonal=any(value>0 for value in years[year]) or (negative and 0 in years[year])
        kind=2 if daylight else int(seasonal) # 0: no DST this year, 1: winter, 2: summer
        if not classified or (off,kind)!=classified[-1][1:]:classified.append((at,off,kind))
    assert all(abs(off)<86400 for _,off,_ in classified)
    # Preserve the SIDTZ001 12-byte lookup rows. DST1 appends one classification
    # byte per row, allowing older readers to keep converting UTC unchanged.
    table=b''.join(struct.pack('<iii',(t-BASE)//86400,(t-BASE)%86400,off) for t,off,_ in classified)
    return table+bytes(kind for _,_,kind in classified),len(classified)

def build(output):
    output.mkdir(parents=True,exist_ok=True)
    names=zone_names();index=bytearray();payload=bytearray();cache={}
    with zipfile.ZipFile(WHEEL) as archive:
        for name in names:
            data=archive.read('tzdata/zoneinfo/'+name)
            if data not in cache:
                table,count=intervals(data)
                cache[data]=(24+48*len(names)+len(payload),count)
                payload.extend(table)
            pos,count=cache[data]
            assert len(name)<40 and count<65536
            index.extend(struct.pack('<40sIH2x',name.encode(),pos,count))
        license=archive.read('tzdata-2026.6.dist-info/licenses/LICENSE').decode()
        license+='\n'+archive.read('tzdata-2026.6.dist-info/licenses/licenses/LICENSE_APACHE').decode()
    blob=struct.pack('<8sHH4s8s',b'SIDTZ001',len(names),48,b'DST1',b'2026f')+index+payload
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
