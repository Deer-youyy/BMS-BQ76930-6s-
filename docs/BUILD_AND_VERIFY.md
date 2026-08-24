# BUILD_AND_VERIFY — 项目构建与代码验证规范

> 项目级规范文档。
> 适用于以后所有 AI / 工程师对本工程修改代码（尤其是每次 Migration 完成后）的构建与验证流程。

## 1. 固定开发环境

本项目最终开发环境如下，**不可替换为其它工具链作为最终验收依据**：

| 项 | 值 |
| --- | --- |
| MCU | STM32F103 系列（以正式 `.uvprojx` 为准） |
| 代码生成 | STM32CubeMX |
| 外设库 | STM32 HAL |
| IDE | Keil MDK |
| 编译器 | ARM Compiler（以正式工程配置为准） |
| 工程文件 | `Projects/MDK-ARM/*.uvprojx`（正式构建配置唯一依据） |

> 本项目 **不是 GCC/CMake 工程**。

## 2. 验证流程（强制顺序）

每次代码修改后，必须按以下顺序执行验证：

1. 静态检查
2. Keil 工程检查（以正式 `.uvprojx` 为准）
3. 实际编译（优先用 Keil MDK / ARM Compiler）
4. 编译失败处理（如需要）
5. 输出最终验收报告

## 3. 静态检查

- [ ] 检查头文件依赖
- [ ] 检查声明 / 定义是否一致
- [ ] 检查 API 调用参数
- [ ] 检查类型匹配
- [ ] 检查重复定义
- [ ] 检查未定义符号
- [ ] 检查 include 路径
- [ ] 检查 Keil 工程文件是否注册新增源码

> GCC/CMake 只能作为辅助语法检查手段，
> **不得**作为最终“编译通过”的依据。

## 4. Keil 工程检查（以正式 `.uvprojx` 为准）

- [ ] 确认新增 `.c/.h` 已加入正确的 Target / Group
- [ ] 确认 Include Paths 正确
- [ ] 确认 Preprocessor Definitions 正确
- [ ] 确认芯片型号、ARM Compiler 配置与现有工程一致

## 5. 实际编译

- 优先使用 Keil MDK / ARM Compiler 对当前正式 `.uvprojx` 执行 Build / Rebuild。
- GCC/CMake 只能作为辅助语法检查，不得作为最终编译通过依据。
- **如果当前执行环境没有 Keil 命令行工具、或无法实际调用 Keil 编译器，不得声称“编译通过”。**

## 6. 编译失败处理

- 必须记录真实 compiler error / warning。
- 根据错误定位到具体文件与行号。
- 只修复与本次 Migration 直接相关的问题。
- **不允许**为了消除编译错误而修改无关业务逻辑。
- 修复后重新执行 Keil Build / Rebuild。

## 7. 最终验收报告模板

每次验证完成后，必须按以下模板输出报告：

```text
Build Environment:
- MCU:       STM32F103 系列（正式工程为准）
- IDE:       Keil MDK
- Toolchain: ARM Compiler（正式工程为准）
- Project:   Projects/MDK-ARM/*.uvprojx
- Target:    <正式工程 Target 名>

Verification:
- Static Check:       PASS/FAIL
- Keil Build:         PASS/FAIL/NOT_AVAILABLE
- Keil Rebuild:       PASS/FAIL/NOT_AVAILABLE
- New Files Registered: PASS/FAIL
- Include Path:        PASS/FAIL
- Link Symbols:        PASS/FAIL
```

### 无法调用 Keil 时

- 必须明确写 `Keil Build: NOT_AVAILABLE`。
- **不得使用 GCC 编译结果冒充 Keil Build PASS。**
- 可以完成静态检查，并检查 `.uvprojx`、源码依赖和潜在编译问题；
  但最终必须明确标记：

> `Keil Build 未执行，需要在本地 Keil MDK 环境进行最终验证。`

## 8. 适用范围

- `.uvprojx` 是正式工程构建配置的**唯一依据**。
- 每次 Migration 完成后，都必须按“静态检查 → Keil 工程检查 → Keil 实际编译 → 结果报告”执行验证。
- 本规范随 Migration 执行情况持续适用，不局限于单个迭代。