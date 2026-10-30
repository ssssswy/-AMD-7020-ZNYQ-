"""Read-only post-evaluation analysis. No fitting or FPGA writes."""
from common import *
from evaluate import stats,paired
from rules import strong_rules
from collections import Counter
import csv,math,sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

LABELS={'traditional_rules':'传统规则检测','current_cnn':'目前 CNN（工程 INT8）','rules_or_cnn':'规则 + CNN（OR，任一报错）','rules_and_cnn':'规则 + CNN（AND，双重确认）','existing_edge_only':'现有窄边沿规则（辅助对照）'}
GROUPS={'overall':'全部主测试','known':'已知异常家族、新波形','parameter_ood':'未见参数组合','novel_shape':'新异常注入形态','uart':'UART','spi':'SPI','i2c':'I²C','unobservable_control':'合法无校验数据翻转对照'}
def pct(x):return '—' if x is None else f'{100*x:.2f}%'
def table(summary,methods):
 out=['| 检测方案 | 检出率 | 误报率 | TP / 异常数 | FP / 正常数 |','|---|---:|---:|---:|---:|']
 for k in methods:
  v=summary[k];out.append(f"| {LABELS[k]} | {pct(v['recall'])} | {pct(v['fpr'])} | {v['tp']}/{v['anomalies']} | {v['fp']}/{v['normal']} |")
 return '\n'.join(out)
def main():
 out=ROOT/'report';out.mkdir(exist_ok='--refresh' in sys.argv);conf=ROOT/'confirmation'
 metric=json.loads((conf/'results/metrics.json').read_text());first=json.loads((ROOT/'results_first_pass/metrics.json').read_text());
 truth=[json.loads(x) for x in (conf/'datasets/truth.jsonl').read_text(encoding='utf-8').splitlines()];z=np.load(conf/'results/predictions.npz');raw=np.load(conf/'datasets/raw.npz')['raw'];y=z['truth'];n=len(y)
 methods={k:z[k] for k in ('traditional_rules','current_cnn','rules_or_cnn','existing_edge_only')};methods['rules_and_cnn']=methods['traditional_rules']&methods['current_cnn']
 masks={name:np.array([r['split']==name for r in truth]) for name in CFG['splits']}
 masks.update({name:np.array([r['protocol']==name and r['split']!='unobservable_control' for r in truth]) for name in CFG['protocols']})
 masks.update(overall=np.array([r['split']!='unobservable_control' for r in truth]),unobservable_control=np.array([r['split']=='unobservable_control' for r in truth]),all_including_semantic_controls=np.ones(n,bool))
 summary={k:{name:stats(y[m],p[m]) for name,p in methods.items()} for k,m in masks.items()}
 overall=masks['overall'];r=methods['traditional_rules'];c=methods['current_cnn'];combined=methods['rules_or_cnn'];pos=overall&y;neg=overall&~y
 incremental={name:dict(recall=paired(combined[mask&y],r[mask&y]),fpr=paired(combined[mask&~y],r[mask&~y])) for name,mask in masks.items() if name!='unobservable_control'}
 gate=CFG['retention_gate'];og=incremental['overall'];ng=incremental['novel_shape'];checks=dict(recall_gain=og['recall']['gain_pp']>=gate['combined_recall_gain_pp'],total_fpr=summary['overall']['rules_or_cnn']['fpr']<=gate['max_false_positive_rate'],added_fpr=og['fpr']['gain_pp']<=gate['max_added_false_positive_pp'],novel_significant=ng['recall']['ci95_pp'][0]>0)
 # Fixed observation contract: point accuracy is NOT window-level recall.
 localized={}
 for proto in CFG['protocols']:
  hits=[i for i,t in enumerate(truth) if t['protocol']==proto and y[i] and c[i] and overall[i]];located=[i for i in hits if np.isfinite(z['cnn_position'][i])];within=[]
  for i in located:
   a,b=truth[i]['truth']['injection'];within.append(a-1<=z['cnn_position'][i]<=b+1)
  localized[proto]=dict(cnn_true_windows=len(hits),with_candidate_point=len(located),candidate_within_injection_plus_1=int(sum(within)),candidate_hit_fraction=sum(within)/len(within) if within else None,meaning='diagnostic overlap only; timing effects may extend outside injection; global windows have no point')
 extra=[i for i in np.flatnonzero(pos&c&~r)];examples=[]
 for i in extra:examples.append(dict(index=int(i),group=truth[i]['group'],protocol=truth[i]['protocol'],split=truth[i]['split'],family=truth[i]['family'],injection=truth[i]['truth']['injection'],candidate=float(z['cnn_position'][i]),score=int(z['cnn_score'][i]),cut=int(z['cnn_cut'][i]),source=int(z['cnn_source'][i])))
 # Isolate the declared receiver recovery correction on the SAME first set.
 ft=[json.loads(x) for x in (ROOT/'datasets/truth.jsonl').read_text(encoding='utf-8').splitlines()];fr=np.load(ROOT/'datasets/raw.npz')['raw'];fm=np.array([t['split']!='unobservable_control' for t in ft]);fy=np.array([t['label'] for t in ft],bool)
 fixed=np.array([bool(strong_rules(w,t['config'])) for w,t in zip(fr,ft)]);before=first['summary']['overall']['traditional_rules'];after=stats(fy[fm],fixed[fm]);save(out/'first_set_rule_recovery_sensitivity.json',dict(before=before,after=after,exploratory=True,not_used_to_change_CNN=True))
 # Distribution audit. Preserve actual integer-sampling parameters.
 configs={}
 for proto in CFG['protocols']:
  for split in CFG['splits']:
   rows=[t for t in truth if t['protocol']==proto and t['split']==split];us=np.array([t['config']['u'] for t in rows]);configs[proto+'/'+split]=dict(count=len(rows),rates=sorted({t['config']['rate_hz'] for t in rows}),samples_per_bit_min=float(us.min()),samples_per_bit_max=float(us.max()),inside_UART_reserved_21p2_21p8=int(((us>21.2)&(us<21.8)).sum()) if proto=='uart' and split=='parameter_ood' else None)
 changed=verify_unchanged();first_hashes={t['raw_sha256'] for t in ft};new_hashes={t['raw_sha256'] for t in truth};assert not first_hashes&new_hashes
 assert sha(ROOT/'datasets_failed_serialization_001/raw.npz')==sha(ROOT/'datasets/raw.npz')
 save(out/'analysis.json',dict(summary=summary,incremental_OR_vs_rules=incremental,predeclared_retention_gate=gate,gate_results=checks,default_gate_pass=all(checks.values()),localization_diagnostic=localized,CNN_only_true_windows=examples,distribution=configs,project_maintained_inputs_unchanged=changed,cross_set_overlap=0,failed_serialization_did_not_change_raw=True,AND_is_posthoc_exploratory_boolean_only=True))
 with (out/'消融结果汇总.csv').open('w',newline='',encoding='utf-8-sig') as f:
  w=csv.writer(f);w.writerow(['测试组','检测方案','异常数','正常数','TP','FN','FP','TN','检出率','误报率','检出率95%CI下','检出率95%CI上','误报率95%CI下','误报率95%CI上'])
  for name,values in summary.items():
   for key,v in values.items():w.writerow([GROUPS.get(name,name),LABELS[key],v['anomalies'],v['normal'],v['tp'],v['fn'],v['fp'],v['tn'],v['recall'],v['fpr'],*v['recall_ci95'],*v['fpr_ci95']])
 plt.rcParams.update({'font.family':'DejaVu Sans','font.size':11,'axes.spines.top':False,'axes.spines.right':False})
 colors=['#3575be','#e68c24','#d55455','#2a9b8f'];main=['traditional_rules','current_cnn','rules_or_cnn','rules_and_cnn'];names=['Rules','INT8 CNN','Rules OR CNN','Rules AND CNN*']
 fig,axes=plt.subplots(1,2,figsize=(13,4.6),constrained_layout=True)
 for ax,field,label in zip(axes,['recall','fpr'],['Detection rate (%)','False positive rate (%)']):
  vals=np.array([summary['overall'][k][field] for k in main])*100;ci=np.array([summary['overall'][k][field+'_ci95'] for k in main])*100
  ax.bar(names,vals,color=colors,width=.65);ax.errorbar(range(4),vals,yerr=np.stack((np.maximum(0,vals-ci[:,0]),np.maximum(0,ci[:,1]-vals))),fmt='none',ecolor='#202a36',capsize=4)
  for j,v in enumerate(vals):ax.text(j,max(v,ci[j,1])+(.5 if field=='recall' else .25),f'{v:.2f}',ha='center',fontsize=12)
  ax.set_ylim(0,108 if field=='recall' else 15);ax.set_ylabel(label);ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True)
 fig.suptitle('Frozen-model ablation | 2,700 abnormal + 2,700 normal windows\n95% Wilson intervals; *AND is an exploratory confirmation policy',fontsize=13);fig.savefig(out/'ablation_overall.png',dpi=150);plt.close(fig)
 fig,axes=plt.subplots(2,3,figsize=(14,7),constrained_layout=True)
 for col,group in enumerate(['known','parameter_ood','novel_shape']):
  for row,field in enumerate(['recall','fpr']):
   ax=axes[row,col];vals=[summary[group][k][field]*100 for k in main[:3]];ax.bar(['Rules','CNN','OR'],vals,color=colors[:3],width=.6)
   for j,v in enumerate(vals):ax.text(j,v+(.6 if row==0 else .3),f'{v:.2f}',ha='center')
   ax.set_ylim(0,109 if row==0 else 19);ax.grid(axis='y',alpha=.15);ax.set_axisbelow(True)
   if row==0:ax.set_title({'known':'Known families, new traces','parameter_ood':'Held-out parameter combinations','novel_shape':'Withheld injection mechanisms'}[group])
   if col==0:ax.set_ylabel('Detection rate (%)' if row==0 else 'False positive rate (%)')
 fig.suptitle('Confirmation set: 900 abnormal + 900 normal windows per split');fig.savefig(out/'ablation_splits.png',dpi=150);plt.close(fig)
 # Deterministic examples: both extra TPs, then first relevant failure/control.
 selected=extra.copy()
 for condition in [lambda i:truth[i]['protocol']=='spi' and neg[i] and c[i],lambda i:truth[i]['protocol']=='uart' and neg[i] and r[i] and not c[i],lambda i:truth[i]['split']=='novel_shape' and y[i] and r[i] and not c[i],lambda i:neg[i] and r[i] and c[i]]:
  selected.append(next(i for i in range(n) if condition(i)))
 selected=selected[:6];fig,axes=plt.subplots(3,2,figsize=(15,10),constrained_layout=True)
 for ax,i in zip(axes.flat,selected):
  t=truth[i];lines={'uart':1,'spi':4,'i2c':2}[t['protocol']];line_names={'uart':['RX'],'spi':['SCK','CS','MOSI','MISO'],'i2c':['SCL','SDA']}[t['protocol']]
  for j in range(lines):ax.step(np.arange(768),raw[i,j]+j*1.6,where='post',linewidth=1.2,label=line_names[j])
  if t['label']:
   a,b=t['truth']['injection'];ax.axvspan(a,b,color='#e85454',alpha=.18,label='Injected span')
  if c[i] and np.isfinite(z['cnn_position'][i]):ax.axvline(z['cnn_position'][i],color='#e08b21',linestyle='--',label='CNN candidate')
  ax.axvline(512,color='#8792a2',linestyle=':');ax.set_xlim(480,768);ax.set_ylim(-.3,1.5+(lines-1)*1.6);ax.set_yticks([]);ax.set_xlabel('Observed sample index (512..767 = target)')
  ax.set_title(f"#{i} {t['protocol'].upper()} | {'anomaly' if y[i] else 'LEGAL'} | rules={int(r[i])} CNN={int(c[i])}\n{t['family'] if y[i] else t['split']}",fontsize=11);ax.legend(loc='upper right',fontsize=8,ncol=min(3,lines+1));ax.grid(axis='x',alpha=.15)
 fig.suptitle('Deterministic case audit: CNN-only true positives and disagreement examples');fig.savefig(out/'case_waveforms.png',dpi=150);plt.close(fig)
 primary=main[:3];ov=summary['overall'];local=localized
 lines=['# TraceGPU 规则 / CNN 消融实验报告','', '日期：2026-10-27。仅电脑端实验；未修改、构建或下载 Vivado/Vitis 工程。','',
 '**结论：目前没有证据支持继续把 CNN 作为默认异常触发必经链路。建议保留模型及源码作为可选实验分支，主链路优先完善可解释的协议/时序规则。不能据此宣称 CNN 对所有未知故障无效，也不应立即删除现有工程中的模型。**','',
 '## 1. 核心结果（新确认集）','',table(ov,primary),'',
 f"规则 + CNN（OR）仅多检出 **2/2700** 段，增益 **{og['recall']['gain_pp']:.3f} 个百分点**，同时增加 **156/2700** 段正常误报，增加 **{og['fpr']['gain_pp']:.3f} 个百分点**。多出的两例分别是 UART 校验位翻转和停止位拉低，均是已知异常家族，不是新异常形态。",'',
 'OR 为两者任一报警；本身不会降低误报。整体数据按 UART / SPI / I²C 等量宏观采样，不能当成现场故障流行率或报警精确率。检出率=TP/(TP+FN)，误报率=FP/(FP+TN)，以目标窗口为单位。','',
 '## 2. 泛化与新异常形态','',
 '| 测试组 | 规则：检出 / 误报 | CNN：检出 / 误报 | OR：检出 / 误报 |','|---|---:|---:|---:|']
 for group in CFG['splits']:
  s=summary[group];lines.append('| '+GROUPS[group]+' | '+' | '.join(pct(s[k]['recall'])+' / '+pct(s[k]['fpr']) for k in primary)+' |')
 lines += ['', '每组 900 异常 + 900 正常。新形态中 CNN 检出 800/900，漏报 100 段；规则检出 900/900。CNN 在该组没有新增真阳性，却新增 33 段规则未报的正常误报。测试中新形态仍映射到原有毛刺/时序类别，不是增加全新分类标签后的开放集识别。','',
 '新形态由旧单故障注入流程未使用的结构化机制生成：UART 多次短脉冲串、连续拉低、相关边沿抖动、正负沿不对称偏移；SPI 多线脉冲串、时钟连抖、CS 反弹；I²C 多线脉冲串、时钟连抖、重复 START 反弹。它们可能与旧波形共享局部模式，不能声称所有局部图样从未见过。','',
 '参数集使用新速率组合和预留采样密度附近的参数。采样周期必须取整数 100 MHz tick，因此实际 samples/bit 会偏离预设空洞；完整实际范围在 analysis.json 的 distribution 中。该组按“新参数组合/密度偏移”解释，不把每条都说成严格落入训练排除区。','',
 '## 3. 分协议结果','', '| 协议 | 规则：检出 / 误报 | CNN：检出 / 误报 | OR：检出 / 误报 |','|---|---:|---:|---:|']
 for group in CFG['protocols']:
  s=summary[group];lines.append('| '+GROUPS[group]+' | '+' | '.join(pct(s[k]['recall'])+' / '+pct(s[k]['fpr']) for k in primary)+' |')
 lines += ['', '每协议 900 异常 + 900 正常。含合法暂停和新参数的正常 SPI 场景中，当前 CNN 误报较多；本次未逐项隔离暂停与参数变化的因果贡献。I²C CNN 接近规则，但没有独立增益。UART 规则仍受有限历史、半帧开始、错误候选锁相影响，146 段主测试规则误报全部来自 UART。规则侧也未达到可直接用于硬触发的低误报目标，不能只看 99.67% 检出率就认为已经完成。','',
 '## 4. 另一种组合：双重确认（探索性）','',table(ov,['rules_and_cnn']),'',
 'AND 在冻结输出上作逻辑交集，没有训练或调阈值；这是看到首轮结果后增加的探索性策略，不冒充预先登记的主比较。它能过滤规则误报，但相对规则损失 212 段真异常，相对 CNN 也丢失 2 段。可作为低频提示的研究方向，暂不建议作为安全关键的唯一触发门。','',
 '## 5. 对照是否公平','',
 '- 三组使用完全相同的原始电平、512 点历史 + 256 点目标、预期速率与接收配置。发射端真实起点、异常区间、干净对照、标签不输入任何检测器。标签仅在全部预测确定后读入。',
 '- 传统规则包含 UART 起始位资格、校验位、停止位、配置位网格偏移、最短脉宽；SPI CS 使能下毛刺、时钟半周期、setup/hold、配置字长完整性；I²C START/STOP 语义、毛刺、最短时钟、setup/hold、完整 9 位字节、显式配置超时。完整字/字节为本实验声明的事务契约，不能把任意 SPI 协议都假定为相同字长。',
 '- 允许合法 UART 数据/占空比变化、±1.5% 速率偏差与 ≤1% 抖动、帧间隙；允许 SPI 被选中时暂停和非选中 SCK 活动；允许 I²C ACK/NACK、合法重复 START、配置范围内时钟拉伸。',
 '- 另列项目现有窄边沿规则，避免将它冒充完整传统检测。该对照检出 '+pct(ov['existing_edge_only']['recall'])+'、误报 '+pct(ov['existing_edge_only']['fpr'])+'；CNN 相对它有覆盖增益，但这不能证明 CNN 优于补齐协议规则。',
 '- 150 段无校验 UART 整数据位改变仍是合法未知载荷，单列为正常语义对照；未人为标作故障。该组规则误报 23/150，CNN 2/150，OR 23/150。全部 5,550 段合并指标也保存在 analysis.json，主表保持三协议各等量。','',
 '## 6. 模型、量化与阈值冻结','',
 '读取当前 v4.6_full 工程，而非选择测试成绩最好的一份旧模型。权重/层表摘要 SHA256：`'+CFG['model_hash']+'`。网络权重共 22,660 字节；这是权重数，不是整个 IP 的资源用量。','',
 '| 模型 | 输入 | 判定 |','|---|---|---|',
 '| UART 全局 R2 | 24×256 | 原工程 tg_ai_global_decision 双精度边界，显示 Q8=607 仅作显示，未用四舍五入值代替决策 |',
 '| UART R8 校验位 + R9 起点 | 20×64 | 有效 Q11 分差严格 >1920 |',
 '| UART R11 C 停止位 + R9 起点 | 6×64 | 有效 Q11 分差严格 >9856 |',
 '| SPI R2 | 43×64 | 最高异常 Q9 residual 严格 >−773 |',
 '| I²C R2 | 38×64 | 最高异常 Q9 residual 严格 >−589 |','',
 'UART 局部有效分数保留 min(R9 margin,0)，停止位输入资格通道采用工程同样的 Q6 编码；未移除候选、未调整阈值。全部神经网络执行工程 ROM 定义的 INT8 权重、定点激活、INT32 累加、正向加半后移位、ReLU/饱和，不是重新训练的 FP32 网络。批量实现用 CPU float64 承载精确整数运算，逐层检查整数性、INT32 范围和 <2^53 精确上界；566 例逐值比对独立 NumPy 整数模型及已有 RTL 金值通过。特征调用未改动的工程 C 编码器。本次没有另跑全量 RTL 原始波形仿真、板测、FPS 或 PL 速度测量。','',
 '## 7. 位置与证据','',
 '| CNN 独有真阳性索引 | 家族 | 注入范围 | CNN 候选 | 分数 / 阈值 |','|---|---|---|---:|---:|']
 for e in examples:lines.append(f"| {e['index']} | {e['family']} | [{e['injection'][0]}, {e['injection'][1]}) | {e['candidate']:.2f} | {e['score']} / {e['cut']} |")
 lines += ['', '两例候选均位于注入区间。总体检出率只回答“窗口是否报警”，不等于异常分类正确或定位正确。全局 CNN 只提供窗口范围；局部/总线候选可能偏离注入区间。analysis.json 另列候选覆盖诊断，不将候选点画在波形上就当成已定位根因。','',
 f"![总体指标]({(out/'ablation_overall.png').as_posix()})",'', f"![泛化分组]({(out/'ablation_splits.png').as_posix()})",'', f"![波形抽查]({(out/'case_waveforms.png').as_posix()})",'',
 '## 8. 首轮、修复与独立性','',
 f"首轮种子 {CFG['seed']}：规则检出 {pct(first['summary']['overall']['traditional_rules']['recall'])} / 误报 {pct(first['summary']['overall']['traditional_rules']['fpr'])}；CNN {pct(first['summary']['overall']['current_cnn']['recall'])} / {pct(first['summary']['overall']['current_cnn']['fpr'])}；OR {pct(first['summary']['overall']['rules_or_cnn']['recall'])} / {pct(first['summary']['overall']['rules_or_cnn']['fpr'])}。原始输出完整保留在 results_first_pass/。",'',
 f"首轮规则接收机在历史区发现错误候选后仍等待该错误帧的完整结束，导致错误相位带入目标。仅修正电脑端规则实验脚本：历史与目标中的校验/停止/位网格失败都重新武装下一次可观测下降沿。177 项定向检查通过；同一首轮数据修复前后，规则检出由 {pct(before['recall'])} 变为 {pct(after['recall'])}，误报由 {pct(before['fpr'])} 变为 {pct(after['fpr'])}，因此恢复同步也有取舍。详细输出 first_set_rule_recovery_sensitivity.json。规则仍有相位歧义，因此报告保留其正常误报，不用真起点消除它。",'',
 '修复后先冻结规则和 CNN，再用新种子 202610270463 生成确认集。两个数据集各 5,550 段，原始波形 hash 交集为 0；确认集也剔除首轮及历史训练/测试 hash。历史去重不能排除分布/局部形状相似，已知 UART 故障仍复用旧注入与物理标签审计函数，新总线渲染与新形态生成实现独立。','',
 '生成阶段第一次因 NumPy 整数 JSON 序列化失败而保留失败目录；修复序列化后使用同一种子重新生成，raw.npz hash 完全相同，没有根据成绩重抽数据。两个集合均未用于重新训练或阈值选择。','',
 '确认集整体规则检出率 95% Wilson 区间 '+str([round(v*100,3) for v in ov['traditional_rules']['recall_ci95']])+'%，CNN '+str([round(v*100,3) for v in ov['current_cnn']['recall_ci95']])+'%。OR 相对规则召回增量的配对 bootstrap 95% 区间 '+str([round(v,4) for v in og['recall']['ci95_pp']])+' 个百分点，新增误报区间 '+str([round(v,4) for v in og['fpr']['ci95_pp']])+' 个百分点。每段窗口为统计单元，同父正常/异常对照配对生成；不同现场及异常发生率仍可能改变结果。','',
 '## 9. 是否保留 CNN、下一步','',
 '- 当前冻结模型保留为可选实验分支；这次不改 PL/PS、菜单默认值或阈值，也不删除 R5～R11 历史文件。',
 '- 正式异常触发优先用可解释规则。先解决 UART 同步可信度与状态连续性；SPI / I²C 现阶段未展示额外 CNN 收益，不建议继续增加它们的 CNN 计算开销作为默认检测路径。',
 '- 若保留 AI，先作为软提示/二次确认，并在 UI 明确“实验模型”。当前 AND 结果可作为低误报方向，但需要新的前瞻验证，不能掩盖 8.19% 左右的漏报。',
 '- 继续训练前先采集真实设备正常数据和故障数据：跨 STM32/传感器/线长/速率，按设备与会话隔离，记录硬件证据。重点寻找“规则失败而 CNN 稳定成功”的故障，而不是继续合成更明显的毛刺。',
 '- 评价目标改为相同误报预算下的新增检出、候选定位、时延/资源开销与每分钟误报。模型 residual 非概率，本次未通过扫阈值在测试集挑最好数字。',
 '- 实验前登记的默认保留门槛为召回增加 ≥5 个百分点、OR 总误报 ≤1.5%、新增误报 ≤0.5 个百分点、新形态召回增益区间下限 >0。四项均未通过。该门槛是本次工程成本判断，不是普适标准；若规则接近满分，5 点增益本就受天花板约束。结论由“2 个新增检出 / 156 个新增误报 / 新形态无独立增益”支撑。','',
 '这些结果只适用于本次合成的 UART / SPI / I²C 及声明的事务/时序契约。没有测试模拟电压质量、亚采样毛刺、未知载荷语义错误、所有通用逻辑信号或真正开放集异常。本次比较冻结工作点，没有独立验证集上的等误报预算 ROC/PR 对比，不能推出规则在所有工作点都优于 CNN。0/900 误报的 95% Wilson 上限约 0.43%，不能理解为现实中绝不误报。','',
 '## 10. 文件与复现','',
 '- `消融结果汇总.csv`：分组结果及 95% 区间。',
 '- `analysis.json`：OR 增益、AND 对照、候选诊断、真实采样参数、保留门槛。',
 '- `../confirmation/results/case_results.csv`：确认集每段标签与三组冻结预测。',
 '- `../confirmation/results/rule_evidence.jsonl`：规则报警证据。',
 '- `../confirmation/datasets/`：原始波形、仅观测配置、分离的审计标签。',
 '- `../confirmation/evidence/`：工程/模型 hash、566 项整数一致性、177 项规则检查、历史去重清单。',
 '- `../results_first_pass/`：首轮全部结果，未覆盖。',
 '- 使用训练环境 Python 运行 `generate.py → integer_model.py / test_rules.py → features.py → evaluate.py`，再由根目录 `report.py` 汇总。输出目录仅允许新建，不覆盖现有实验；复现请复制到新的实验目录并保持 experiment.json 与冻结模型相同。','',
 f"维护源文件与模型保护清单校验：{'PASS' if changed else 'FAIL'}。FPGA 工程保持原样；本实验不产生可下载 BIT/XSA/ELF。",'']
 (out/'TraceGPU_CNN消融实验报告_20261027.md').write_text('\n'.join(lines),encoding='utf-8')
 save(out/'evaluation_sources.json',dict(frozen_evaluation={str(p.relative_to(ROOT)):sha(p) for p in [conf/'results/metrics.json',conf/'results/evaluation_inputs.json',conf/'results/predictions.npz',conf/'datasets/manifest.json',ROOT/'results_first_pass/metrics.json']},report_script_sha256=sha(__file__),protected_file_count=len(json.loads((ROOT/'evidence/freeze.json').read_text())['project_inputs'])))
 save(out/'artifact_hashes.json',{p.name:sha(p) for p in out.iterdir() if p.is_file() and p.name!='artifact_hashes.json'})
 print('REPORT_DONE',json.dumps(dict(overall=summary['overall'],incremental=og,gate=checks,localization=localized),ensure_ascii=False),flush=True)
if __name__=='__main__':main()
