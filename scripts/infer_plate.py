#!/usr/bin/env python3
"""Standalone ONNX recognizer + five-color companion; input is a plate crop, not a full-frame detector.

The fine-tuned recognizer and the 5-class color net are one pair. This script does not
route between the original v2.4 recognizer and the fine-tuned one.

From the repo root:
  python scripts/infer_plate.py crop.jpg --layer single
  python scripts/infer_plate.py crop.jpg --layer double --points '[[x0,y0],[x1,y1],[x2,y2],[x3,y3]]'

--points uses percentages of the input image size (0-100).
Weights default to models/onnx/ (plate_rec_finetune.onnx, plate_color_finetune.onnx, charset.json).
If those files sit next to this script, that directory is used instead. Override with --models.
"""
import argparse,json,math,re
from pathlib import Path
import cv2,numpy as np,onnxruntime as ort

def default_model_dir():
 here=Path(__file__).resolve().parent
 if (here/'plate_rec_finetune.onnx').is_file() and (here/'charset.json').is_file():
  return here
 return here.parent/'models'/'onnx'
PROVINCES='京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼'
COLORS=['蓝','黄','绿','白','黑']
def resolve_color(text,color,layer):
 if '警' in text:return '白'
 if any(c in text for c in '使领港澳'):return '黑'
 if any(c in text for c in '学挂'):return '黄'
 if re.fullmatch('['+PROVINCES+'][A-Z](?:[DF][A-HJ-NP-Z0-9]{5}|[A-HJ-NP-Z0-9]{5}[DF])',text):return '绿'
 if color in ['白','黑']:return '黄' if layer=='double' else '蓝'
 return color
class PlateModel:
 def __init__(self,directory=None):
  d=Path(directory) if directory else default_model_dir();opts=ort.SessionOptions();opts.intra_op_num_threads=4
  self.rec=ort.InferenceSession(str(d/'plate_rec_finetune.onnx'),sess_options=opts,providers=['CPUExecutionProvider']);self.color=ort.InferenceSession(str(d/'plate_color_finetune.onnx'),sess_options=opts,providers=['CPUExecutionProvider']);self.tokens=json.load(open(d/'charset.json'))
 def __call__(self,image,layer='single',points=None):
  im=image.copy();h,w=im.shape[:2]
  if points is not None:
   p=np.array(points,np.float32)*np.array([w/100,h/100],np.float32);ww=max(8,round(max(np.linalg.norm(p[0]-p[1]),np.linalg.norm(p[2]-p[3]))));hh=max(4,round(max(np.linalg.norm(p[0]-p[3]),np.linalg.norm(p[1]-p[2]))));dst=np.array([[0,0],[ww-1,0],[ww-1,hh-1],[0,hh-1]],np.float32);im=cv2.warpPerspective(im,cv2.getPerspectiveTransform(p,dst),(ww,hh),borderMode=cv2.BORDER_REPLICATE)
  if layer=='double':
   h,w=im.shape[:2];down=im[h//3:];im=np.concatenate([cv2.resize(im[:max(1,h*5//12)],(down.shape[1],down.shape[0])),down],1)
  rw=min(160,max(1,math.ceil(48*im.shape[1]/im.shape[0])));rec=np.full((48,160,3),128,np.uint8);rec[:,:rw]=cv2.resize(im,(rw,48));rx=((rec.astype('float32')-127.5)/127.5).transpose(2,0,1)[None].copy();cx=(cv2.resize(im,(96,96)).astype('float32')/255).transpose(2,0,1)[None].copy()
  probs=self.rec.run(None,{self.rec.get_inputs()[0].name:rx})[0][0];text='';last=-1;conf=[]
  for row in probs:
   i=int(row.argmax())
   if i and i!=last and i<len(self.tokens):text+=self.tokens[i];conf.append(float(row[i]))
   last=i
  text=text.replace("'",'');cp=self.color.run(None,{self.color.get_inputs()[0].name:cx})[0][0];raw=COLORS[int(cp.argmax())]
  return dict(text=text,rec_confidence=float(np.mean(conf)) if conf else 0.,color_network=raw,color_network_confidence=float(cp.max()),color=resolve_color(text,raw,layer),layer=layer)
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('image');a.add_argument('--layer',choices=['single','double'],default='single');a.add_argument('--points');a.add_argument('--models');p=a.parse_args();im=cv2.imread(p.image)
 if im is None:raise ValueError('Cannot read image')
 print(json.dumps(PlateModel(p.models)(im,p.layer,json.loads(p.points) if p.points else None),ensure_ascii=False))
