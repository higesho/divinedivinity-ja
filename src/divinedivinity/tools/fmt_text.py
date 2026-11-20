# localizations/<lang>/text.cmp : groups of (id, text1, text2)
import struct
def read(path):
    d=open(path,'rb').read();o=0
    ng=struct.unpack_from('<I',d,o)[0];o+=4;groups=[]
    for g in range(ng):
        gid,cnt=struct.unpack_from('<Ii',d,o);o+=8
        wide=cnt<0;cnt=abs(cnt);ents=[]
        for i in range(cnt):
            eid,=struct.unpack_from('<I',d,o);o+=4;texts=[]
            for k in range(2):
                n,=struct.unpack_from('<I',d,o);o+=4
                if n:
                    if wide: t=d[o:o+2*n].decode('utf-16le');o+=2*n+2
                    else: t=d[o:o+n].decode('latin1');o+=n+1
                else: t=None
                texts.append(t)
            ents.append((eid,texts[0],texts[1]))
        groups.append((gid,wide,ents))
    assert o==len(d),(o,len(d))
    return groups
def write(path,groups):
    out=[struct.pack('<I',len(groups))]
    for gid,wide,ents in groups:
        out.append(struct.pack('<Ii',gid,-len(ents) if wide else len(ents)))
        for eid,t1,t2 in ents:
            out.append(struct.pack('<I',eid))
            for t in (t1,t2):
                if t is None: out.append(struct.pack('<I',0))
                elif wide: b=t.encode('utf-16le');out.append(struct.pack('<I',len(b)//2)+b+b'\0\0')
                else: b=t.encode('latin1');out.append(struct.pack('<I',len(b))+b+b'\0')
    open(path,'wb').write(b''.join(out))
if __name__=='__main__':
    import sys
    for gid,wide,ents in read(sys.argv[1]):
        print('GROUP',gid,wide,len(ents))
        for e in ents[:int(sys.argv[2]) if len(sys.argv)>2 else 8]: print('  ',e)
