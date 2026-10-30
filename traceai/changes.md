# 匿名化修改记录

原始工程及原始签核记录未修改。以下改动发生在当前审核副本，详细摘要见 [changes.json](changes.json)。

- 删除不属于作品技术内容的独立说明。
- 技术报告与相关说明精简与作品技术无关的叙述；保留技术局限、创建时间及本机路径等历史信息。
- 配套 BIT、XSA、ELF、MAP、DCP 和原始签核报告保持原样。
- 源码归档为匿名化副本，替换五份历史说明和一份辅助打包脚本中的相关措辞，未改动 RTL、PS 算法、约束或模型。

源码归档中的改动文件：

- `baseline_v3_9/docs/TraceGPU_v3.9_版本规划.md`
- `docs/TraceGPU_v4.6_UART_CNN保留与总线CNN移除_修改说明.md`
- `docs/TraceGPU_v4.6_阶段进展与修复记录.md`
- `docs/TraceGPU_v4.7_PL实时异常触发与证据分析_详细规划.md`
- `docs/TraceGPU_v4.9_字体修复与重建说明.md`
- `scripts/package_v49_release.py`

文档及辅助材料修改：

- `fpga/prepare_allowlist.py`：核对匿名化说明摘要并仅在隔离工作副本补充预检清单。

- `README.md`
- `docs/interface.md`
- `fpga/rebuild.md`
- `host/app/README.md`
- `host/deploy/README.md`
- `licenses/README.md`
- `licenses/license_review.md`
- `report/ai_collaboration.md`
- `report/completion_checklist.md`
- `report/figures/report_figure_source.py`
- `report/technical_report.docx`
- `docs/architecture.md`
- `skill/README.md`

源归档的摘要与原签核不同；历史签核只适用于原始文件，匿名化源码副本尚未完成全量重新构建。已保留原始签核记录，不将其改写成新构建结果。

协议和工具接口中的 `user` 字段、第三方作者与版权信息保留。

技术报告围绕同步采集、PL 智能检测、硬件触发与时序证据重新组织论述，保留离线测试范围及工程约束。
