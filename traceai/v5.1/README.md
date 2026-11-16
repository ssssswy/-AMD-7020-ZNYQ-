# TraceAI v5.1 源码

连续 UART 帧边界修复版，包含 v5.0 的 32 MiB 八路波形历史、异常历史和 us/ms/s 时间单位功能。完整停止位后可直接发送下一帧，PS 解码器能正确接收连续帧。

- [修复的问题、原因和验证结果](docs/UART连续帧修复说明.md)
- [完整维护软件](software_v5_1)：源码、头文件、字库和测试源码
- [构建与回归脚本](scripts)
- [板测清单](docs/测试清单.md)
- [原有硬件、IP、模型与工程恢复入口](../README.md)

## 使用与构建

本目录是本机 v5.1 维护源码的逐字节浏览副本，硬件沿用 HW49011。完整 Vitis 构建依赖原工程的 RTL/IP、模型、冻结软件、兼容 XSA/BIT 以及 `logs/ai_soc_49011/verified_result.json` 等签核输入。单独下载本目录不包含全部硬件依赖和历史签核记录。

在已有完整工程的新副本中，将 `software_v5_1` 放在根目录，并把本目录的 `scripts` 和 `hardware_inputs` 文件合并到对应目录。保留 `software_v4_0` 和 HW49011 的完整配套资料。使用 Vivado/Vitis 2025.2、xc7z020clg400-2，并按安装位置检查脚本中的工具路径。

```powershell
python scripts/rebase_v51_fonts.py
python scripts/test_v51_regression.py --attempt github001
python scripts/run_v51_software.py --attempt 51002 --hardware-attempt 49011 --execute
```

构建编号必须未使用。主机回归使用 `D:/cygwin/bin/gcc.exe`。移动源码后先绑定字库路径，再执行构建；Vitis 应打开实际生成的 `vitis_ws_ai_<编号>` 目录。

本版本的本机交付为软件构建 51001，已通过全新平台、BSP、应用构建。脚本中的新 attempt 标识输出目录；启动串口中的 SW51001 标识是本发布源码的版本身份。仓库中的原有硬件与历史材料保留，旧版浏览源码按原路径存放。

许可证沿用[仓库说明](../../README.md)；第三方字库、PainterEngine 和 AMD 组件的许可约定不变。
