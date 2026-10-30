"""Directed pre-evaluation checks, separate from all measured test seeds."""
from common import *
from rules import strong_rules,edge_rules

def uart(bits,parity,stops,value):
 c=dict(protocol='uart',rate_hz=115200,period_ticks=54,u=16.,data_bits=bits,parity=parity,stop_bits=stops)
 v=[0]+[(value>>j)&1 for j in range(bits)]
 if parity!='N':v.append((sum(v)%2)^(parity=='O'))
 v.extend([1]*stops);raw=np.ones((4,768),np.uint8)
 for j,x in enumerate(v):raw[0,520+j*16:520+(j+1)*16]=x
 return raw,c

def bus(spi,cpol=0,cpha=0,nack=0):
 c=dict(protocol='spi' if spi else 'i2c',rate_hz=100000,period_ticks=63,u=16.,cpol=cpol,cpha=cpha,word_bits=8,glitch=2,clock=4,setup=2,hold=1,timeout=0)
 r=np.ones((4,768),np.uint8)
 if spi:
  r[0]=cpol;r[1,518:665]=0;r[2:]=0
  for j in range(8):
   a=522+j*16;r[0,a+8:a+16]=1-cpol
   r[2,a+(10 if cpha else 2):]=j%2;r[3,a+(10 if cpha else 2):]=1-j%2
 else:
  r[1,520:]=0;r[0,530:]=0
  for j,x in enumerate([1,0,1,0,0,1,0,1,nack]):
   a=530+j*16;r[1,a+3:]=x;r[0,a+8:a+16]=1
  r[1,677:]=0;r[0,682:]=1;r[1,690:]=1
 return r,c

def main():
 checks=[]
 def check(name,r,c,expected):
  events=strong_rules(r,c);assert bool(events)==expected,(name,events)
  checks.append(dict(name=name,expected=expected,events=events))
 for bits in (7,8):
  for parity in ('N','E','O'):
   for stops in (1,2):
    for value in (0,1,0x55,0x7f,0xff):
     r,c=uart(bits,parity,stops,value);check(f'UART legal {bits}{parity}{stops} {value}',r,c,False)
     s=r.copy();a=520+(1+bits+(parity!='N'))*16;s[0,a:a+16]=0;check('UART stop low',s,c,True)
     if parity!='N':
      s=r.copy();a=520+(1+bits)*16;s[0,a:a+16]^=1;check('UART parity flipped',s,c,True)
 r,c=uart(8,'N',1,0x55);r[0,741:742]=0;check('UART runt',r,c,True)
 for pol in (0,1):
  for pha in (0,1):
   r,c=bus(True,pol,pha);check(f'SPI legal mode {pol}{pha}',r,c,False)
   s=r.copy();s[1,640:665]=1;check('SPI truncated word',s,c,True)
   s=r.copy();s[2,543]^=1;check('SPI closed runt',s,c,True)
 for nack in (0,1):
  r,c=bus(False,nack=nack);check(f'I2C legal ACK/NACK {nack}',r,c,False)
  s=r.copy();s[0,600]^=1;check('I2C SCL runt',s,c,True)
 save(ROOT/'evidence/rules_directed_selftest.json',dict(pass_all=True,count=len(checks),checks=checks,not_main_test_data=True))
 print('RULES_DIRECTED_PASS',len(checks))
if __name__=='__main__':main()
