import torch
from torch import nn
from torch.nn import functional as F


class Backbone(nn.Module):
 def __init__(self):
  super().__init__();layers=[];cin=3
  for cout,pool in [(32,(2,2)),(64,(2,2)),(128,(2,1)),(192,(2,1))]:
   layers += [nn.Conv2d(cin,cout,3,padding=1),nn.BatchNorm2d(cout),nn.ReLU(),nn.Conv2d(cout,cout,3,padding=1),nn.BatchNorm2d(cout),nn.ReLU(),nn.MaxPool2d(pool)];cin=cout
  self.net=nn.Sequential(*layers)
 def forward(self,x):return self.net(x)

class ColorV2(nn.Module):
 def __init__(self):super().__init__();self.backbone=Backbone();self.fc=nn.Linear(192,5)
 def forward(self,x):
  # Same externally supplied BGR96x96 /255 tensor as v1, not the old full-crop preprocessing.
  x=F.interpolate(x[:,[2,1,0]],size=(48,192),mode='bilinear',align_corners=False);x=(x-.5)*2
  return self.fc(self.backbone(x).mean((2,3)))
