# Aerospace Adaptive Fault-Tolerant Framework

面向空天计算的自适应容错编译框架。基于 LLVM 15 的两个 out-of-tree Pass
（容错策略层 + 故障注入裁判层）加纯 C 运行时，在一组 PolyBench 内核上实测
不同防护策略的开销与覆盖率，并用一个 λ(t) 驱动的控制器按轨道辐射环境自适应
选档，在满足可靠性预算的前提下把冗余开销压到接近 1×。

核心结论：真实轨道 λ 剖面下自适应开销 1.03×，比传统软件 TMR 省 48–68%，
全可靠性区间 Pareto 占优；单一 λ/B 参数化控制器轨道无关。

## 目录结构

```
passes/        容错策略层 LLVM Pass (AdaptiveFTPass.cpp → libAdaptiveFTPass.so)
harness/       故障注入裁判层 Pass + PMU 运行时 (FaultInjectorPass.cpp, harness_rt.c)
runtime/       容错策略运行时 (atft_runtime.c → libatft_rt.a)
include/       运行时头文件
benchmarks/    基准 shim (obpmark_shim)
experiments/   实验与分析脚本（active）；archive/ 为已归档的探索脚本
results/       实测结果 CSV/JSON；opt/archive/ 为已归档散件（不入库）
docs/          方法、复现清单、设计文档、文献综述
third_party/   上游参考实现（COAST/LLTFI/PolyBench/KissFFT 等，vendored，不入库）
```

## 构建

依赖：LLVM 15（`llvm-config` 可定位）、CMake ≥ 3.16、C/C++17 编译器。

```bash
cmake -S . -B build -DLLVM_DIR=$(llvm-config-15 --cmakedir)
cmake --build build -j
```

产物：`build/libAdaptiveFTPass.so`、`build/libFaultInjectorPass.so`、
`build/libatft_rt.a`、`build/libharness_rt.a`。

## 运行实验

实验与分析脚本在 `experiments/`，从仓库根目录执行，例如：

```bash
python3 experiments/adaptive_capstone.py      # 自适应控制器 + 五组评估，产出 results/opt/capstone_*.csv
python3 experiments/lower_envelope.py         # 每核最便宜 SDC=0 策略的下包络
python3 experiments/build_property_table.py   # 每核属性表（向量化/覆盖率/开销）
```

Python 依赖：`numpy`（capstone 的在线实验）；轨道剖面再生成另需 `sgp4 ppigrf pyyaml`。

## 复现

每张图表 → 产它的脚本 → 输入 → 输出 CSV 的完整映射见
[`docs/REPRO.md`](docs/REPRO.md)。方法与口径见 [`docs/METHOD.md`](docs/METHOD.md)，
系统与设计背景见 [`docs/DESIGN.md`](docs/DESIGN.md)。
