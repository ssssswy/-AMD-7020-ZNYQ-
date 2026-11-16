# TraceGPU v3.3：BUS 分组、质量与性能工作台

本目录为 Vivado/Vitis 2025.2 的独立 PS 软件。目标器件和显示配置沿用领航者 `xc7z020clg400-2`、800×480、25 MHz LCD。版本目录为 `D:/FPGA_gpu/TraceGPU_2025_2/v3.3`，硬件明确复用 `../hardware_reference/tracegpu_v3_2.bit` / `.xsa`，不是重新生成的 v3.3 硬件。禁止混用四输入旧 BIT。

新增最多两组 2～8 位 BUS 派生视图：bit0 是 LSB，任意不重复物理通道映射、位序反转、HEX/BIN、A/B 游标值；X 表示未知/未采集/缺口，~ 表示同一像素内有状态变化。只读停采源分片计算，生产步长16记录/步、硬上限64，完成后发布2×680列缓存。新视口/会话/配置使旧缓存不可显示。入口为逻辑模式→菜单→测量浏览→BUS分组，先暂停并等待只读快照。

新增可点击 Q 标志及数据质量详情，将采集原因与历史正常退休、结果缓存省略、未计量视口项分开；不同单位不相加，未知为--。32位源计数下降标记epoch不确定；保护触发快照使用自身证据，不冒用live-session计数。Q:OK仅表示给定证据范围内未见异常，不承诺所有线上字节完整或精确缺口区间定位。

状态保留实际启动与收尾失败原因；CH5～CH8游标使用八路VECTOR适配器，不越界访问四UART环。性能诊断保存完成/换帧边界的prepare/draw/flush/switch/frame及实际路径原因。BUS派生视图由CPU绘制；GPU百分比不是LUT/DSP利用率。停采缩略密度身份缓存避免每轮空闲循环重复扫描512桶，收益待板测，不预填FPS。

本版分开维护 8 路同步逻辑输入、4 个实时 UART 引擎、2 个软件解码实例和 4 个触发条件槽。CH1～CH4 保留硬件 UART。CH1～CH8 共用 16 字节 VECTOR 历史，停采或取得保护快照后可以选择任意通道 UART、任意合法 I²C/SPI 映射重解码。八路总览、低四路、高四路、单路视图共享时间轴、T0 和 A/B 游标；通道显示掩码与采集掩码分开。

capture control、VECTOR 和 trigger ABI 为 `0x00030200`；四个既有 UART 核的版本仍为 `0x00020900`，GPU/batch/display 接口版本也保持实际硬件版本。新软件首先检查控制版本，再读取八路能力，矛盾或旧 BIT 禁止采集。导出 TGC3 容器版本 3 使用 512 字节头，保存 8 路能力、实际采集掩码、4 路 UART 固定映射、2 个解码实例和触发证据；可靠串口传输继续使用 TGX3。

2025.2 SDT 平台通过基地址查找 PS I²C/GPIO 和 AXI DMA。时间接口由 `tracegpu_time.h` 适配 xiltimer，启动时先执行 1 µs sleep 初始化 Cortex-A9 全局计时器，再建立 GPU 超时和性能计数。汇编字库使用本版本真实字体资产路径。旧 BSP 主机回归保留 legacy 分支。

普通历史仍在 `0x06000000..0x06800000`：4 MiB 四 UART ring + 4 MiB 共享 VECTOR。保护历史在 `0x06800000..0x07000000`，保持 8 MiB；程序/堆栈被链接脚本限制在 1～8 MiB。GPU、ARGB 双画布、scanout、cache 与 DMA 的既有所有权和地址不变，最终应用的 ELF/MAP 必须另外核实所有 LOAD/NOLOAD 段。

执行 `../scripts/test_ps_complete_v3_3.ps1`、`test_pc_complete_v3_3.ps1` 运行本版回归；`run_vitis_2025_2_v3_3.py --execute` 创建独立原生工作区及ELF/MAP。旧*v3_2*脚本仅保留为硬件来源证据，不是本版软件入口。说明见根README与`../docs/TraceGPU_v3.3_构建与验证说明.md`。主机画面和数字均为合成夹具，不能代替真实八针、LCD/触摸、UART、串口导出、FPS和长稳板测。
