# 逻辑分析仪硬件与软件复建说明

审核基准采用 `build/design.bit`、`build/design.xsa`、`build/application.elf`、`build/application.map` 和同一硬件实现的 `build/routed.dcp`。`build/pair_check.json` 验证 XSA 内嵌 BIT 与独立 BIT 字节一致，并记录从该 XSA 提取的硬件描述与 `ps7_init.tcl`。这些身份检查不证明实板运行。

## 源码和范围

`src/source_snapshot.zip` 是源码归档的匿名化副本。原归档已检查 7800 个成员的完整性与关键 RTL/IP/字体/构建入口；尚未完成异机全量复建。归档包含历史备份、旧文档和历史候选，维护范围由 `README_v4_9.md`、当前软件 `software_v4_0`、`src/ai/v49_compare` 及 `ip_repo/trigger_v49_validated` 指定。不要把旧文件或旧报告当成当前签核。

匿名化副本修改了归档中的五份历史说明及一个辅助打包脚本中的措辞，未修改 RTL、PS 算法、约束或模型；因此原始 `source_manifest.json` 中这六个文件的摘要不再适用于该副本。原始签核文件作为历史记录保留，差异与新旧摘要见 [变更记录](../changes.json)。配套 BIT、XSA、ELF、MAP 和 DCP 未修改。原归档的预检结果不能自动视作匿名副本的构建验证。

原源码在独立解压目录首次预检未通过。原因是旧基线允许变更清单没有包含 17 个现用软件源码或测试文件，基线比对报 `Unapproved baseline modification`。这些文件经核对与配套软件构建输入摘要一致。对当前匿名化归档另行解压，核对 17 个软件文件及 4 个影响基线检查的说明文件后更新工作副本的允许清单，只读预检通过。首次失败见 [source_preflight.json](report/source_preflight.json)，当前副本结果见 [anonymous_preflight.json](report/anonymous_preflight.json)。这不等于已完成综合、实现或板测。

归档中有 57 个成员名含中文、大写或空格，在移植和公开分发时需核对文件命名；它暂用于本地审查和溯源，最终公开前应在独立副本中改造命名与引用，并重新构建和核验，保留原件证据。字库、自研代码和第三方/IP 分发许可还需核对。

## 工具和操作顺序

工具版本为 Vivado/Vitis 2025.2，目标器件为 `xc7z020clg400-2`。在新的工作目录解压源码，不覆盖原工程。完整路径尽量使用英文且避免过长，并在已安装的 Python 环境执行以下命令。

1. 审查解压出的原 README 与脚本，确认目标器件、板级约束、ROM、IP 和工具路径。
2. 运行 `python scripts/rebase_v47_fonts.py`，将两个字体 `.incbin` 绑定到解压目录。这个命令会修改工作副本中的汇编文件，需重新记录摘要。
3. 在资料目录执行 `python fpga/prepare_allowlist.py --work <解压后的工作副本>`。脚本核对配套软件输入摘要及匿名化说明的变更摘要，仅对隔离副本补充对应允许变更并保存记录；遇到其他差异会停止。随后在解压目录以未使用的编号执行只读预检：`python scripts/run_v49_cold.py --ai-variant v49_compare --native-variant trigger_v49_validated --attempt 99901`。预检通过之前不要执行后续构建。
4. 原维护硬件构建以 `--execute` 启动；构建脚本中 Vivado 路径固定为 `E:/AMD/2025.2/Vivado`。在其他安装目录需先检查和适配副本。完整综合实现需要重新通过时序、DRC、配对与信号检查，不能继承已有工件的结果。
5. 按同一新硬件编号运行 `python scripts/verify_v49_soc_artifacts.py --attempt 99901`。脚本依赖原始工程生成副本、来源摘要和实际签核结果；移机路径兼容性尚未验证。
6. 软件以 `python scripts/run_v49_software.py --attempt 99902 --hardware-attempt 99901 --execute` 创建平台、BSP 和应用，脚本固定 Vitis 路径 `E:/AMD/2025.2/Vitis`；新编号不覆盖旧结果。构建前必须先具备该编号硬件的独立签核文件。
7. 对新 BIT/XSA/ELF、寄存器 ABI、源成员和内存报告建立新的配对记录。复建可能产生不同布局和工件摘要，须使用当次报告。

以上为依据维护脚本梳理的流程，当前审核目录没有在干净异机跑通完整构建。最终硬件 WNS +0.018 ns、WHS +0.019 ns，但 LCD 外部输出延迟约束未完整覆盖；不能据此声称显示接口时序已闭合或板上通过。当前 Slice 占用 99.92%，新布局可能不能通过。

## 设备和数据边界

UART 冻结 CNN、规则、同步采集和触发运行在 PL，PS 配置硬件并完成界面/数据处理。SPI/I²C 使用规则检测。当前资料未显示在线 AI PC 推理及电脑与设备协同通信链。

本说明不执行串口、JTAG、Flash 或程序下载。工作区原始签核报告见 `report/`，外部显示时序、现场启动、长时间采集和最终实测均待后续验收。
