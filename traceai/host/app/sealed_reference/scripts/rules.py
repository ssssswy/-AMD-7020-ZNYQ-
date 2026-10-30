"""Detectors accept ONLY observed lines and declared receiver configuration.
No transmitter frame anchors, clean traces, injection spans or labels.
"""
import math
import numpy as np
def runs(v):
 e=np.r_[0,np.flatnonzero(v[1:]!=v[:-1])+1,len(v)];return list(zip(e[:-1],e[1:]))
def edge_rules(raw,c):
 proto=c['protocol'];n=1 if proto=='uart' else 4 if proto=='spi' else 2
 gl=(100000000//c['rate_hz']//8)/c['period_ticks'] if proto=='uart' else c['glitch']
 ph=0 if proto=='uart' else c['clock'];edge=[None]*n;data=[None,None];sample=None;out=[]
 def add(ch,kind,a,b,limit):
  if a is not None and limit and b>=a and b-a<limit and 512<=a<768:out.append(dict(channel=ch,kind=kind,begin=int(a),end=int(b),observed=float(b-a),limit=float(limit)))
 for t in range(1,768):
  before=raw[:n,t-1];now=raw[:n,t];changed=before!=now
  active=proto!='spi' or before[1]==0 and now[1]==0
  if proto=='spi' and changed[1]:edge=[None]*n;data=[None,None];sample=None
  for j in range(n):
   if not changed[j]:continue
   relevant=active and not(proto=='i2c' and j==1 and (before[0] or now[0]))
   if relevant and edge[j] is not None and t>edge[j]:
    width=t-edge[j]
    if gl and width<gl:add(j,'glitch',edge[j],t,gl)
    elif j==0 and ph and width<ph:add(j,'clock_phase',edge[j],t,ph)
   edge[j]=t if relevant else None
  if proto=='i2c' and now[0]:edge[1]=None
  if proto=='spi' and active:
   sampling=changed[0] and ((before[0]==c['cpol'])==(c['cpha']==0))
   if sampling:
    for j in range(2):add(j+2,'setup',t if changed[j+2] else data[j],t,c['setup'])
    sample=t
   for j in range(2):
    if changed[j+2]:add(j+2,'hold',sample,t,c['hold']);data[j]=t
 return out
def uart_rules(raw,c):
 rx=raw[0];u=c['u'];out=[]
 for a,b in runs(rx):
  if a>0 and b<768 and 512<=a<768 and b-a<.45*u:out.append(dict(kind='runt_less_than_min_bit',channel=0,begin=int(a),end=int(b),observed=b-a,limit=.45*u))
 # Conventional observed start qualification and nominal receiver sampling.
 # Prefix is processed for synchronization; only target evidence is scored.
 active=None;candidate=None;previous=1;high=0;sampled=0;parity_sum=0
 bits=c['data_bits'];parity=c['parity']!='N';total=1+bits+parity+c['stop_bits'];checked=set()
 for t,value in enumerate(rx):
  if active is not None and t>=active+total*u:active=None
  if active is not None and previous and not value and high>=.5*u and t>=active+(total-.4)*u:active=None
  if active is None:
   if candidate is None and previous and not value and high>=max(1,.5*u):candidate=t-.5
   if candidate is not None:
    if value:candidate=None
    elif t>=candidate+.5*u:active=candidate;candidate=None;sampled=parity_sum=0;checked=set()
  if active is not None:
   while sampled<bits and t>=active+(1.5+sampled)*u:parity_sum^=int(value);sampled+=1
   failed=False
   if parity and 'parity' not in checked and t>=active+(1.5+bits)*u:
    checked.add('parity')
    if int(value)!=(parity_sum^(c['parity']=='O')):
     failed=True
     if t>=512:out.append(dict(kind='parity',channel=0,begin=t,end=t,observed=int(value),expected=parity_sum^(c['parity']=='O')))
   for j in range(c['stop_bits']):
    if j not in checked and t>=active+(1.5+bits+parity+j)*u:
     checked.add(j)
     if not value:
      failed=True
      if t>=512:out.append(dict(kind='stop_low',channel=0,begin=t,end=t,observed=0,expected=1))
   if previous!=value:
    nearest=max(0,round((t-active)/u));distance=abs(t-active-nearest*u)
    # Same declared transmitter tolerance, but with an OBSERVED anchor.
    tolerance=(1/.985-1+.01/.985)*nearest*u+1
    if distance>tolerance:
     failed=True
     if t>=512:out.append(dict(kind='edge_grid',channel=0,begin=t,end=t,observed=float(distance),limit=float(tolerance)))
   # Re-arm after an invalid candidate, INCLUDING errors in the warm-up prefix.
   # The first pass incorrectly held a known bad phase until frame end.
   if failed:active=None;candidate=None
  high=high+1 if value else 0;previous=int(value)
 return out
def strong_rules(raw,c):
 if c['protocol']=='uart':return uart_rules(raw,c)
 out=edge_rules(raw,c);spi=c['protocol']=='spi';known=False;count=0;on=False;last=None
 for t in range(1,768):
  if spi:
   if raw[1,t-1] and not raw[1,t]:known=True;count=0
   if raw[0,t]!=raw[0,t-1] and raw[0,t]==(c['cpol'] if c['cpha'] else 1-c['cpol']) and not raw[1,t]:count+=1
   if not raw[1,t-1] and raw[1,t]:
    if known and count%c['word_bits'] and t>=512:out.append(dict(kind='incomplete_configured_word',channel=1,begin=t,end=t,observed=count,limit=c['word_bits']))
    known=False
  else:
   state_change=raw[1,t]!=raw[1,t-1] and raw[0,t] and raw[0,t-1]
   if state_change:
    if on and known and count%9 not in (0,1) and t>=512:out.append(dict(kind='incomplete_byte',channel=1,begin=t,end=t,observed=count,limit=9))
    on=not raw[1,t];known=on;count=0;last=t if on else None
   if not raw[0,t-1] and raw[0,t] and on:
    count+=1
    # A SDA change while the following SCL-high phase is a START/STOP, not
    # ordinary setup/hold. Do not reinterpret that legal state change.
    end=t+1
    while end<768 and raw[0,end]:end+=1
    if known and not np.any(raw[1,t+1:end]!=raw[1,t]):
     changes=np.flatnonzero(raw[1,1:]!=raw[1,:-1])+1
     prior=changes[changes<=t];after=changes[changes>=t]
     if len(prior) and c['setup'] and t-prior[-1]<c['setup'] and t>=512:out.append(dict(kind='setup',channel=1,begin=t,end=t,observed=int(t-prior[-1]),limit=c['setup']))
     if len(after) and c['hold'] and after[0]-t<c['hold'] and t>=512:out.append(dict(kind='hold',channel=1,begin=t,end=t,observed=int(after[0]-t),limit=c['hold']))
   if on and raw[0,t]!=raw[0,t-1]:last=t
   if not on:last=None
   if last is not None and c['timeout'] and t-last==c['timeout'] and t>=512:out.append(dict(kind='configured_timeout',channel=0,begin=t,end=t,observed=c['timeout'],limit=c['timeout']))
 return out
