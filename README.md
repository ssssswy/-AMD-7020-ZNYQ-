# TraceAI 智绘逻辑分析仪

基于 Zynq-7020 的逻辑采集、协议分析及异常检测项目。详细说明和工程文件见 [traceai/README.md](traceai/README.md)。

## 最新版本

[v5.1 源码与构建入口](traceai/v5.1/README.md) · [连续 UART 帧修复说明](traceai/v5.1/docs/UART连续帧修复说明.md)

v5.1 修复连续 UART 帧起始沿漏判、字节错位和帧错误。9,216 组主机测试及 Vitis 2025.2 新平台/BSP/应用构建通过，板上验收待进行。

## 开源许可

本仓库中的**原创且有权授权的代码与文档**采用 [MIT License](LICENSE) 授权（Copyright © 2026 ssssswy）。

第三方代码、字体、AMD IP/BSP 及其他厂商组件或生成文件**不因本仓库采用 MIT 而自动获得 MIT 授权**；它们仍受各自的许可证、版权及再分发条件约束。许可来源、未完成核对事项见 [traceai/licenses/license_review.md](traceai/licenses/license_review.md)。发布或复用这些内容前，请先核实相应授权。
