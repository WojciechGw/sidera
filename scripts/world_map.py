"""Generate the offline 300x150 CITIES locator from public-domain land rings."""
import argparse,json,struct,math
from pathlib import Path
from world_regions import region

ROOT=Path(__file__).resolve().parents[1]
W,H=300,150
def project(lon,lat):return round((lon+180)*(W-1)/360),round((90-lat)*(H-1)/180)

def build():
    data=json.loads((ROOT/'src/data/world-land.json').read_text())
    land=bytearray(W*H)
    # ESRI outer rings are clockwise; counterclockwise rings are water holes.
    def area(ring):return sum(x*yy-xx*y for (x,y),(xx,yy) in zip(ring,ring[1:]))
    for ring in sorted(data['rings'],key=lambda p:area(p)>0):
        value=int(area(ring)<0)
        points=[project(*p) for p in ring]
        for y in range(H):
            cross=sorted(x+(xx-x)*(y+.5-yy)/(yyy-yy) for (x,yy),(xx,yyy) in zip(points,points[1:])
                         if (yy>y+.5)!=(yyy>y+.5))
            for left,right in zip(cross[::2],cross[1::2]):
                for x in range(max(0,math.ceil(left-.5)),min(W,math.ceil(right-.5))):land[y*W+x]=value
        for (x,y),(xx,yy) in zip(points,points[1:]):
            steps=max(abs(xx-x),abs(yy-y),1)
            for i in range(steps+1):land[round(y+(yy-y)*i/steps)*W+round(x+(xx-x)*i/steps)]=value
    regions=bytes(region(x*360/(W-1)-180,90-y*180/(H-1)) for y in range(H) for x in range(W))
    coast=bytearray(W*H)
    for y in range(H):
        for x in range(W):
            if land[y*W+x] and any(not(0<=xx<W and 0<=yy<H) or not land[yy*W+xx]
                   for xx,yy in ((x-1,y),(x+1,y),(x,y-1),(x,y+1))):coast[y*W+x]=1
    out=bytearray(struct.pack('<8sHHI',b'SIDMAP1\0',W,H,16+8*39*H))
    for chosen in range(8):
        for y in range(H):
            row=bytearray(39) # Six leading bits align the map at screen x=334.
            for x in range(W):
                if coast[y*W+x] or (chosen and regions[y*W+x]==chosen and land[y*W+x]):
                    row[(x+6)//8]|=128>>((x+6)%8)
            out.extend(row)
    out.extend(regions)
    return out

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'build/generated/rom/sidera.map');a=p.parse_args()
    a.output.parent.mkdir(parents=True,exist_ok=True);data=build();a.output.write_bytes(data)
    print(f'World locator: {W}x{H}, seven geographic regions, {len(data)} bytes')
