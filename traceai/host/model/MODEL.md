# UART 异常检测模型

本作品保留冻结的 UART INT8 网络。模型在电脑端训练和导出，在线推理核心及判定逻辑位于 FPGA PL；PS 配置采集和读取结果。SPI、I²C 使用边沿规则，当前作品没有将两者的 CNN 部署为检测模型。

## 运行模型与身份

`uart_rom/` 是维护工程用于 UART 的权重、层表与判定查找表。其 [manifest.json](uart_rom/manifest.json) 记录了文件摘要和模型组合身份。组合的 17,108 个 INT8 权重和 179 个参数字对应四个模型，分别为全局分支、校验位分支、帧起点分支和停止位分支。PL 的 UART 网络在观测窗口内输出候选分数与类别；实际告警还受帧上下文、硬件忙状态和事件逻辑影响。

| 模型 ID | 作用 | 输入形状 | 输出数 | 对应冻结参数 |
|---|---|---|---:|---|
| 0 | 全局分支 | 24×256 | 3 | [global.npz](historical_reference/global.npz) |
| 1 | 校验位局部分支 | 20×64 | 5 | [parity.npz](historical_reference/parity.npz) |
| 2 | 帧起点分支 | 20×64 | 5 | [anchor.npz](historical_reference/anchor.npz) |
| 3 | 停止位局部分支 | 6×64 | 2 | [stop.npz](historical_reference/stop.npz) |

四个 NPZ 是冻结的参考参数。`historical_reference/manifest.json`、`weights.mem` 和 `params.mem` 保留原始六模型组合的历史身份，其中的 SPI/I²C 元数据是历史记录，不能用于宣称它们仍在当前 PL 中运行。维护源 `src/ai/v46/tg_ai_decision.sv` 的 `model_id > 3` 会触发 fault；当前部署身份应以 `uart_rom/manifest.json` 及最终硬件源和签核为准。

## 判定阈值和数值解释

校验位有效分数为对应输出差按 Q11 换算后，加上起点 margin 的非正部分；仅当有效分数严格大于 **1920** 时触发该局部异常。停止位分支采用同一尺度的起点负向修正，严格大于 **9856** 时触发。判定所用的 Q11 量化数值分别相当于 0.9375 和 4.8125；它们是模型分数阈值，不是检测概率。

全局判定使用 [global_decision.mem](uart_rom/global_decision.mem) 和 `tg_ai_decision.sv` 中的定点查表、饱和和边界处理；历史浮点参数的阈值为 0.914604137609747，约等于 log-odds 2.37119。不要直接把单个 Q11 阈值用于全局分支。起点分支提供候选可信度修正，候选策略和类别映射应结合 RTL、状态机与上层事件定义理解。

原始参考清单和保留文件的 SHA256 见 [provenance.json](provenance.json)。训练、量化和模型选择的完整环境不在本目录中；本目录支持核对冻结参数及 PL 初始化文件，不应称为完整的从零重训包。

## 评估边界

[电脑离线评估](../app/README.md) 使用封存合成数据、缓存特征和电脑参考程序。它不是实板实时推理、Ryzen AI PC 边缘应用或上位机与 FPGA 通信程序；其离线检出和误报比例不能直接作为板上实时覆盖率、端到端延迟或闭环成功率。
