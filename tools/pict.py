import struct
from PIL import Image
def unpackbits(d,p,n):
    out=bytearray(); end=p+n
    while p<end:
        c=d[p]; p+=1
        if c<128: out+=d[p:p+c+1]; p+=c+1
        elif c>128: out+=bytes([d[p]])*(257-c); p+=1
    return bytes(out)
def unpack16(d,p,n):
    # packbits on 16-bit units
    out=bytearray(); end=p+n
    while p<end:
        c=d[p]; p+=1
        if c<128: out+=d[p:p+(c+1)*2]; p+=(c+1)*2
        elif c>128: out+=d[p:p+2]*(257-c); p+=2
    return bytes(out)
def decode(d):
    p=10
    while p<len(d):
        if d[p:p+4]==b'\x00\x11\x02\xff': p+=4; continue
        op=struct.unpack('>H',d[p:p+2])[0]; p+=2
        if op==0x0C00: p+=24
        elif op==0x0001: p+=struct.unpack('>H',d[p:p+2])[0]
        elif op==0x001E: pass
        elif op in (0x0090,0x0098,0x009A):
            if op==0x009A: p+=4
            rb=struct.unpack('>H',d[p:p+2])[0]; p+=2
            ispix=rb&0x8000; rb&=0x3FFF
            t,l,b,r=struct.unpack('>hhhh',d[p:p+8]); p+=8
            w,h=r-l,b-t
            pix=8; pl=None; packtype=0
            if ispix:
                ver,packtype,packsize,hres,vres=struct.unpack('>HHIII',d[p:p+16]); p+=16
                ptype,psize,cc,csz,pb,pt,pr=struct.unpack('>HHHHIII',d[p:p+20]); p+=20
                pix=psize
                if op!=0x009A:
                    seed,fl,cs=struct.unpack('>IHH',d[p:p+8]); p+=8
                    pl=[(0,0,0)]*256
                    for i in range(cs+1):
                        v,rr,gg,bb=struct.unpack('>HHHH',d[p:p+8]); p+=8
                        pl[v if fl&0x8000 else i]=(rr>>8,gg>>8,bb>>8)
            p+=16+2
            packed = (op!=0x0090) and (rb>=8 or op==0x009A) and packtype!=1
            if op==0x0090: packed=False
            im=Image.new('RGB',(w,h)); px=im.load()
            for y in range(h):
                if packed:
                    if rb>250: n=struct.unpack('>H',d[p:p+2])[0]; p+=2
                    else: n=d[p]; p+=1
                    row=(unpack16 if pix==16 else unpackbits)(d,p,n); p+=n
                else:
                    row=d[p:p+rb]; p+=rb
                for x in range(w):
                    if pix==16:
                        v=(row[2*x]<<8)|row[2*x+1]
                        px[x,y]=(((v>>10)&31)*255//31,((v>>5)&31)*255//31,(v&31)*255//31)
                    elif pix==32 or pix==24:
                        if packtype==4 and pix==32:
                            # planar rgb after pack: componnent planes (cmpCount=3): R row, G row, B row
                            px[x,y]=(row[x],row[w+x],row[2*w+x])
                        else: px[x,y]=(row[4*x+1],row[4*x+2],row[4*x+3])
                    else:
                        if pix==8: idx=row[x]
                        elif pix==4: idx=(row[x//2]>>(4*(1-x%2)))&15
                        elif pix==2: idx=(row[x//4]>>(2*(3-x%4)))&3
                        else: idx=(row[x//8]>>(7-x%8))&1
                        px[x,y]=pl[idx]
            return im
        elif op==0x00FF: break
        else: raise Exception('unknown op %04x at %d'%(op,p))
