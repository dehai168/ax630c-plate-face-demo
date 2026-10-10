import csv,os,zipfile,io
from PIL import Image,ImageDraw,ImageFont,ImageFile
ImageFile.LOAD_TRUNCATED_IMAGES=True
import glob
font=glob.glob('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc') or glob.glob('/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc') or glob.glob('/usr/share/fonts/opentype/noto/*.ttc')
F=ImageFont.truetype(font[0],16)
rows=list(csv.reader(open('results/results.csv',encoding='utf-8-sig')))[1:]
z=zipfile.ZipFile('results/vis_special_plates.zip','w',zipfile.ZIP_STORED)
for r in rows:
    try: im=Image.open('data/'+r[1]).convert('RGB')
    except Exception: im=Image.new('RGB',(176,120),(0,0,0))
    im=im.resize((264,180))
    can=Image.new('RGB',(264,180+46),(255,255,255)); can.paste(im,(0,0)); d=ImageDraw.Draw(can)
    ok=r[5]
    col=(0,140,0) if ok=='是' else (200,0,0) if ok=='否' else (0,0,0)
    d.text((3,181),f"识别: {r[4]} {r[8]} {r[9]}",font=F,fill=col)
    d.text((3,202),(f"真值: {r[3]}  " if r[3] else '')+f"格式:{r[10]}",font=F,fill=(60,60,60))
    b=io.BytesIO(); can.save(b,'JPEG',quality=78)
    sub='有真值/' if r[3] else ''
    z.writestr(f"{sub}{int(r[0]):05d}_{r[1].replace('.png','.jpg')}",b.getvalue())
z.close(); print(os.path.getsize('results/vis_special_plates.zip'))
