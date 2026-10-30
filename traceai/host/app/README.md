# 电脑端离线评估材料

[evaluate_v49_detection_pc.py](evaluate_v49_detection_pc.py) 是维护工程中实际使用的电脑离线评估脚本原样副本。它以已有合成样本与缓存特征评估冻结 UART 网络和规则参考，不采集开发板、不与 FPGA 通信，也不驱动物理执行器。

`sealed_reference/` 保留脚本引用的已封存数据、UART 缓存特征、实验配置和评估脚本副本。文件摘要见 [provenance.json](../model/provenance.json)。真值仅用于最终统计；它不是 PC 端实时输入。该批数据为合成数据，不能写成真实板测。

## 原脚本的路径和依赖

原脚本假设自己位于完整工程的 `scripts/`，并在同级历史实验目录寻找 `datasets/`、`features/`、`scripts/` 与 `experiment.json`。它会在工程 `output/` 下新建一个固定名称的结果目录，已存在时拒绝覆盖。将脚本直接放进本目录后，原始路径假设不成立，不能直接双击运行。

完整执行还依赖 NumPy、PyTorch、Cygwin GCC、经原路径构建的 `features/reference.exe`，以及工程内 `scripts/v44_rom_reference.py`、历史层表、硬件契约和 PS 的特征/边沿参考 C 源码。原脚本固定了 `D:/cygwin/bin/gcc.exe`，历史配置和清单也含原机绝对路径。此处未复制主机 EXE；后续应从可信源代码重新编译，并调整工作副本的路径后验证输出。

因此当前材料保存了可审计的输入、源码和来源，但尚未证明脱离原机后可从零完整重跑。重建与 PC 应用缺口见 [部署说明](../deploy/README.md)。
