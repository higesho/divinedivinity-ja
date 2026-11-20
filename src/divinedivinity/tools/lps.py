# find length-prefixed strings: u32 L (incl. NUL) + L bytes ending in NUL
import struct,re
def lpstrings(d,minlen=2):
    out=[];i=0;n=len(d)
    while i<n-4:
        L=struct.unpack_from('<I',d,i)[0]
        if 2<=L<8000 and i+4+L<=n and d[i+3+L]==0:
            t=d[i+4:i+3+L]
            if all(c>=0x20 or c in (9,10,13) for c in t) and re.search(rb'[A-Za-z]{%d}'%minlen,t):
                out.append((i,t));i+=4+L;continue
        i+=1
    return out
if __name__=='__main__':
    import sys
    d=open(sys.argv[1],'rb').read();s=lpstrings(d)
    print(len(s),sum(len(t) for _,t in s))
    for x in s[:int(sys.argv[2]) if len(sys.argv)>2 else 20]: print('  ',x)
