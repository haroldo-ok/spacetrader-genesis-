import struct
def read_fork(path):
    d=open(path,'rb').read()
    doff,moff,dlen,mlen=struct.unpack('>IIII',d[:16])
    m=d[moff:moff+mlen]
    tl_off,nl_off=struct.unpack('>HH',m[24:28])
    n=struct.unpack('>H',m[28:30])[0]+1
    out={}
    p=28+2
    for i in range(n):
        t,cnt,ro=struct.unpack('>4sHH',m[p+i*8:p+i*8+8])
        cnt+=1
        for j in range(cnt):
            q=28+ro+j*12
            rid,noff,attr_off=struct.unpack('>hHI',m[q:q+8])
            off=attr_off&0xFFFFFF
            ln=struct.unpack('>I',d[doff+off:doff+off+4])[0]
            out.setdefault(t.decode('latin1'),{})[rid]=d[doff+off+4:doff+off+4+ln]
    return out
if __name__=='__main__':
    import sys
    r=read_fork(sys.argv[1])
    for t,v in r.items():
        print(t,len(v),sorted(v)[:12],sum(len(x) for x in v.values()))
