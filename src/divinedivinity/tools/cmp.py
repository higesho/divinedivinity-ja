# Divine Divinity .cmp archive reader/writer
import struct,sys,os
KEY=bytes.fromhex('0c40550c2d41622d0306481e0548140530323334636346331809280f06223917')
def xor(b,start=0):
    return bytes(c^KEY[(start+i)&31] for i,c in enumerate(b))
def read(path):
    d=open(path,'rb').read()
    n=struct.unpack_from('<I',d,0)[0];o=4;ents=[]
    for _ in range(n):
        ln=struct.unpack_from('<I',d,o)[0];o+=4
        raw=d[o:o+ln+1];o+=ln+1
        name=bytes((~c&0xff)^KEY[i&31] for i,c in enumerate(raw[:ln])).decode('latin1')
        a,b,c=struct.unpack_from('<3I',d,o);o+=12
        ents.append((name,a,b,c,raw[ln]))
    return d,o,ents
if __name__=='__main__':
    d,o,ents=read(sys.argv[1])
    print('hdr end',o,'filesize',len(d))
    for e in ents: print(e)
