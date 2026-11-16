# 面向空天计算的自适应容错框架

硕士学位论文《面向空天计算的自适应容错框架研究》的原型与评测工程。基于 LLVM 15
的两个 out-of-tree Pass（容错策略层 + 故障注入裁判层）加纯 C 运行时。

**一句话主张：容错开销应按程序结构付费，而不是对所有指令一视同仁。** 框架分层，
每层有各自的付费方式，层间由辐射强度驱动切换：

- 代数校验层（环类规整内核）：数学结构换冗余，N=256 时 1.04×，SDC → 0。
- 控制平面层（驱动分支的比较）：条件三模，单独 2.35×，该类 SDC → 0。
- 粗粒度冗余层（无结构的通用代码）：进程级复制，单核 2–3×、核 ≥ 3 墙钟 ≈1×，SDC/DUE → 0。
- 层间切换（随辐射强度）：泊松 SPRT + 失效预算最优控制。

## 权威文档与阅读顺序

- **[`tools/reports/framework_overview.md`](tools/reports/framework_overview.md) ← 权威总览**：
  最新、已跨会话对账的"全貌 + 四层 + 诚实账本"。数值口径一律以此为准，并含
  **「限制与口径（诚实账本）」**一节，可直接作论文"局限"章素材。
- [`docs/REPRO.md`](docs/REPRO.md)：每张图表 → 脚本 → 输入 → 输出的复现映射。
- 历史文档（反映早期 track，**数值以 framework_overview 为准**）：
  - `docs/METHOD.md`：指令级逐类别 track（早期，8.25× 口径已被粗粒度层取代）。
  - `tools/reports/full_report.md`、`method_improvement.md`：中期报告。
  - `docs/HANDOFF.md`：工程交接手册（坑清单、代码地图仍有效）。
  - `docs/DESIGN.md`、`docs/EXPERIMENT_SUMMARY.md`、`docs/LITERATURE_SURVEY.md`。

## 四层的诚实状态

- 代数校验层（环类）：**硬结论**，真实注错实测。
- 控制平面层：**硬结论**，真实注错实测。
- 粗粒度冗余层：**硬结论**，开销多核标度实测（`measure_coarse_scaling.py`）。
- 层间切换 / 最优控制：**框架正确、增量小**（双模 λ 下协态收益 < 1%，价值在消参 + 可证最优）。
- 半环类不等式检查：**受限**，只对已沉淀结果有效（97%），计算中故障仅 37%，非贡献点。

细节与数字见权威总览的「限制与口径」。

## 规模

93+ commit；`passes/AdaptiveFTPass.cpp` 3.8K 行、`runtime/` 1K 行、`experiments/` 8K+ 行、
`docs/` 10K+ 行；注入器覆盖 6 类故障点 × 4 种模式，17 个负载跨 5 套件。

## 构建

依赖：LLVM 15（`llvm-config` 可定位）、CMake ≥ 3.16、C/C++17 编译器。

```bash
cmake -S . -B build -DLLVM_DIR=$(llvm-config-15 --cmakedir)
cmake --build build -j
```

产物：`build/libAdaptiveFTPass.so`、`build/libFaultInjectorPass.so`、
`build/libatft_rt.a`、`build/libharness_rt.a`。

## 运行实验

脚本在 `experiments/`，从仓库根目录执行。本轮收尾新增的可复现测量：

```bash
python3 experiments/measure_coarse_scaling.py   # 粗粒度多核开销标度（warmup+中位+taskset）
python3 experiments/resource_pricing.py          # 资源价格调度（能量目标）
gcc -O3 -march=native experiments/semiring_check_kernel.c -o build/skk && ./build/skk  # 半环不等式检查（真实内核）
python3 experiments/adaptive_capstone.py         # λ(t) 驱动控制器 + 五组评估
```

Python 依赖：`numpy`；轨道剖面再生成另需 `sgp4 ppigrf pyyaml`。

## 目录结构

```
passes/        容错策略层 LLVM Pass (AdaptiveFTPass.cpp)
harness/       故障注入裁判层 Pass + PMU 运行时
runtime/       容错策略运行时 (atft_runtime.c)
experiments/   实验与分析脚本（active）；archive/ 为已归档探索脚本
results/       实测结果 CSV/JSON；opt/ 为优化后口径
tools/reports/ 对外报告与配图（framework_overview.* 为权威）
docs/          方法、复现清单、设计、文献综述、交接手册
third_party/   上游参考实现（vendored，不入库）
```

