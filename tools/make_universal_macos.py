"""Combine two built Mach-O executables into an Apple universal container."""
import pathlib,struct,sys
root=pathlib.Path(__file__).resolve().parents[1]
inputs=[root/'bin/atracdenc-aal-aarch64-macos',root/'bin/atracdenc-aal-x86_64-macos']
output=root/'bin/atracdenc-aal-macos-universal'
images=[]
for path in inputs:
    data=path.read_bytes();assert struct.unpack_from('<I',data)[0]==0xfeedfacf
    cpu,subtype=struct.unpack_from('<II',data,4)
    images.append((cpu,subtype,data))
alignment=14;cursor=1<<alignment;records=[]
for cpu,subtype,data in images:
    records.append((cpu,subtype,cursor,len(data),alignment));cursor=(cursor+len(data)+(1<<alignment)-1)&~((1<<alignment)-1)
combined=bytearray(cursor)
struct.pack_into('>II',combined,0,0xcafebabe,len(records))
for i,(record,image) in enumerate(zip(records,images)):
    struct.pack_into('>5I',combined,8+i*20,*record)
    combined[record[2]:record[2]+record[3]]=image[2]
assert all(combined[rec[2]:rec[2]+rec[3]]==img[2] for rec,img in zip(records,images))
output.write_bytes(combined)
print(f'{output}: {len(combined)} bytes; slices arm64 + x86_64')
