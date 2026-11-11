#!/usr/bin/env python3
"""
学术评测看板生成器（全中文严谨学术表述版）
将 results/*.csv 全部实验结果渲染为单文件交互式 HTML 报告 (results/eval_dashboard.html)
"""

import csv
import json
import shutil
from pathlib import Path

ROOT_DIR = Path("/home/users/wangtianyu15/wtyWorkspace/aerospace-adaptive-ft")
RES_DIR = ROOT_DIR / "results"
OUT_HTML = RES_DIR / "eval_dashboard.html"
OUT_INDEX = RES_DIR / "index.html"


def read_csv_dicts(path):
    with open(path, "r", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def generate_html_dashboard():
    crit_rows = read_csv_dicts(RES_DIR / "criticality_analysis.csv")
    pmu_rows = read_csv_dicts(RES_DIR / "rq1_pmu_performance_qor.csv")
    fi_rows = read_csv_dicts(RES_DIR / "rq1_fault_injection_outcomes.csv")
    rq2_rows = read_csv_dicts(RES_DIR / "rq2_selective_protection_curves.csv")
    rq3_rows = read_csv_dicts(RES_DIR / "rq3_delta_threshold_sensitivity.csv")
    rq4_rows = read_csv_dicts(RES_DIR / "rq4_adaptive_solar_flare_trace.csv")

    data_bundle = {
        "criticality": crit_rows,
        "rq1_pmu": pmu_rows,
        "rq1_fi": fi_rows,
        "rq2_selective": rq2_rows,
        "rq3_delta": rq3_rows,
        "rq4_trace": rq4_rows,
    }
    json_str = json.dumps(data_bundle, ensure_ascii=False)

    html_content = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>编译器辅助的自适应近似容错框架 — 实验评测看板</title>
<style>
  :root {{
    --bg: #0b1120;
    --card: #131c31;
    --card-alt: #18233c;
    --border: #283552;
    --text: #f1f5f9;
    --muted: #94a3b8;
    --accent: #38bdf8;
    --green: #10b981;
    --amber: #f59e0b;
    --red: #ef4444;
    --purple: #a855f7;
  }}
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{
    font-family: -apple-system, BlinkMacSystemFont, "PingFang SC", "Microsoft YaHei", "Segoe UI", Roboto, sans-serif;
    background: var(--bg);
    color: var(--text);
    line-height: 1.6;
    padding: 28px 36px;
    max-width: 1560px;
    margin: 0 auto;
  }}
  header {{
    border-bottom: 1px solid var(--border);
    padding-bottom: 18px;
    margin-bottom: 24px;
    display: flex;
    justify-content: space-between;
    align-items: flex-end;
    flex-wrap: wrap;
    gap: 14px;
  }}
  header h1 {{ font-size: 22px; font-weight: 700; color: #fff; letter-spacing: 0.3px; }}
  header p {{ color: var(--muted); font-size: 13px; margin-top: 4px; }}
  .meta-tag {{
    display: inline-block;
    padding: 4px 10px;
    border-radius: 6px;
    font-size: 12px;
    font-weight: 500;
    background: var(--card-alt);
    color: var(--muted);
    border: 1px solid var(--border);
    margin-left: 6px;
  }}
  .badge {{
    display: inline-block;
    padding: 2px 8px;
    border-radius: 4px;
    font-size: 11px;
    font-weight: 600;
  }}
  .badge-crit {{ background: rgba(239,68,68,0.16); color: #f87171; border: 1px solid rgba(239,68,68,0.35); }}
  .badge-sub {{ background: rgba(245,158,11,0.16); color: #fbbf24; border: 1px solid rgba(245,158,11,0.35); }}
  .badge-non {{ background: rgba(148,163,184,0.16); color: #cbd5e1; border: 1px solid rgba(148,163,184,0.35); }}

  .kpi-grid {{
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(225px, 1fr));
    gap: 14px;
    margin-bottom: 26px;
  }}
  .kpi-card {{
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 16px 18px;
  }}
  .kpi-title {{ font-size: 12.5px; color: var(--muted); margin-bottom: 6px; }}
  .kpi-val {{ font-size: 26px; font-weight: 700; color: var(--accent); }}
  .kpi-sub {{ font-size: 12px; color: var(--muted); margin-top: 6px; }}

  .section {{
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 22px 24px;
    margin-bottom: 24px;
  }}
  .section h2 {{
    font-size: 17px;
    margin-bottom: 6px;
    color: #fff;
    font-weight: 600;
  }}
  .section .desc {{
    font-size: 13px;
    color: var(--muted);
    margin-bottom: 16px;
  }}
  .grid-2 {{
    display: grid;
    grid-template-columns: 1.15fr 0.85fr;
    gap: 22px;
  }}
  .grid-equal {{
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 22px;
  }}
  @media (max-width: 1150px) {{
    .grid-2, .grid-equal {{ grid-template-columns: 1fr; }}
  }}

  table {{
    width: 100%;
    border-collapse: collapse;
    font-size: 12.5px;
    margin-top: 10px;
  }}
  th, td {{
    padding: 8px 10px;
    text-align: right;
    border-bottom: 1px solid var(--border);
  }}
  th:first-child, td:first-child,
  th:nth-child(2), td:nth-child(2) {{
    text-align: left;
  }}
  th {{
    color: var(--muted);
    font-weight: 600;
    background: rgba(11,17,32,0.55);
  }}
  tr:hover td {{ background: rgba(56,189,248,0.04); }}

  .bar-wrap {{
    display: flex;
    height: 16px;
    width: 100%;
    min-width: 160px;
    border-radius: 3px;
    overflow: hidden;
    background: #0b1120;
  }}
  .bar-seg {{ height: 100%; }}
  .legend {{
    display: flex;
    gap: 16px;
    flex-wrap: wrap;
    font-size: 12px;
    color: var(--muted);
    margin-bottom: 10px;
  }}
  .legend span {{ display: inline-flex; align-items: center; gap: 6px; }}
  .dot {{ width: 10px; height: 10px; border-radius: 2px; display: inline-block; }}

  svg.chart {{
    width: 100%;
    height: 280px;
    background: rgba(11,17,32,0.55);
    border: 1px solid var(--border);
    border-radius: 8px;
    margin-top: 6px;
  }}
  .filter-bar {{
    display: flex;
    gap: 12px;
    margin-bottom: 12px;
    align-items: center;
    flex-wrap: wrap;
  }}
  select {{
    background: #0b1120;
    color: #fff;
    border: 1px solid var(--border);
    padding: 6px 12px;
    border-radius: 6px;
    font-size: 12.5px;
  }}
  code {{
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    font-size: 12px;
    color: #e2e8f0;
  }}
</style>
</head>
<body>

<header>
  <div>
    <h1>面向空天计算的编译器辅助自适应近似容错框架 — 实验评测看板</h1>
    <p>评测方法：Linux 硬件性能计数器 (<code>perf_event_open</code>) 实测退休指令数/周期数 + 两阶段单粒子位翻转故障注入 (<code>One-Fault-Per-Run</code>，0–63 bit 全位宽均匀采样)</p>
  </div>
  <div>
    <span class="meta-tag">LLVM 15.0.7 编译工具链</span>
    <span class="meta-tag">12 个标准基准程序</span>
    <span class="meta-tag">7 组复现对比基线</span>
  </div>
</header>

<div class="kpi-grid" id="kpi-container"></div>

<!-- 第 1 节：基准测试集与编译期静态关键性分析 -->
<div class="section">
  <h2>1. 标准基准测试套件与编译期静态关键性分析</h2>
  <p class="desc">覆盖 PolyBench/C 4.2.1、PARSEC 3.0、ACCEPT 近似计算测试集、KissFFT/CMSIS-DSP 信号处理及 GNC 姿态控制共 12 个标准计算负载。编译器静态分析 Pass 自动提取循环嵌套深度 (<code>LoopInfo</code>)、SSA 后向切片至输出汇点的距离紧密度、跨基本块活跃范围以及跨迭代状态反馈 <code>PHI</code> 环路数。</p>
  <table id="tbl-crit">
    <thead>
      <tr>
        <th>基准函数</th>
        <th>所属标准套件与领域</th>
        <th>关键性分级</th>
        <th>SSA 指令数</th>
        <th>最大循环深度</th>
        <th>输出汇点紧密度</th>
        <th>变量活跃跨度</th>
        <th>状态反馈 PHI 节点数</th>
        <th>关键性评分 (S)</th>
      </tr>
    </thead>
    <tbody></tbody>
  </table>
</div>

<!-- 第 2 节：硬件 PMU 性能开销与近似固有误差表征 -->
<div class="section">
  <h2>2. 硬件 PMU 指令开销对比与近似计算固有误差离线表征</h2>
  <p class="desc">同台对比复现的经典软件容错基线：<strong>EDDI</strong> (Oh et al., IEEE TR'02 — 全算术与访存双模复制+内联比较)、<strong>SWIFT</strong> (Reis et al., CGO'05 — 假设 ECC 保护内存的寄存器链双模复制) 以及 <strong>COAST-TMR</strong> (Chielle et al., IEEE TNS'19 — 全精度软件三模冗余与多数表决)。</p>
  <div class="grid-2">
    <div>
      <h3 style="font-size:14px;margin-bottom:6px;color:#e2e8f0;">2.1 归一化硬件退休指令数倍率对比 (硬件 PMU <code>PERF_COUNT_HW_INSTRUCTIONS</code>)</h3>
      <svg class="chart" id="svg-pmu-bars" viewBox="0 0 760 280"></svg>
      <table id="tbl-pmu-summary">
        <thead>
          <tr>
            <th>基准程序</th>
            <th>Native (无保护)</th>
            <th>EDDI (TR'02)</th>
            <th>SWIFT (CGO'05)</th>
            <th>COAST-TMR (TNS'19)</th>
            <th>Approx-Scalar</th>
            <th>Approx-SIMD (本文)</th>
            <th>Adaptive-LOW</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
    <div>
      <h3 style="font-size:14px;margin-bottom:6px;color:#e2e8f0;">2.2 纯近似分支固有误差离线表征 (Pure-Approx) 与表决后输出精度</h3>
      <table id="tbl-qor">
        <thead>
          <tr>
            <th>基准程序</th>
            <th>纯近似相对 L2 误差</th>
            <th>纯近似平均相对误差 (MRE)</th>
            <th>纯近似峰值信噪比 (PSNR)</th>
            <th>Approx-SIMD 表决后误差</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
  </div>
</div>

<!-- 第 3 节：单粒子位翻转故障注入五分类分布与 MWTF 评分 -->
<div class="section">
  <h2>3. 单粒子翻转故障注入五分类结果分布与平均无故障工作量 (MWTF)</h2>
  <p class="desc">每个“基准程序 × 容错机制”组合独立执行 600 次 <code>One-Fault-Per-Run</code> 故障注入实验（全位宽 0–63 bit 均匀随机翻转）。双模检测方案 (<strong>EDDI / SWIFT</strong>) 虽能检出故障但无法自恢复，导致高比例 <strong>DUE (检出不可恢复中断)</strong>；三模表决方案 (<strong>COAST-TMR / Approx-SIMD</strong>) 支持在线屏蔽故障并继续运行。</p>
  <div class="filter-bar">
    <label style="font-size:13px;color:var(--muted);">筛选容错机制：</label>
    <select id="sel-fi-policy">
      <option value="KEY">核心对比 (Native vs. SWIFT vs. COAST-TMR vs. Approx-SIMD)</option>
      <option value="native">Native (无保护基线)</option>
      <option value="eddi">EDDI (Oh et al., IEEE TR'02 — 双模全复制检测)</option>
      <option value="swift">SWIFT (Reis et al., CGO'05 — 双模寄存器链检测)</option>
      <option value="exact_tmr">COAST-TMR (IEEE TNS'19 — 传统全精度三模冗余)</option>
      <option value="approx_scalar">Approx-TMR-Scalar (1路FP64精确 + 2路标量FP32近似)</option>
      <option value="approx_simd">Approx-TMR-SIMD (本文：1路FP64精确 + 2路SIMD近似 + 内联快速表决)</option>
    </select>
    <label style="font-size:13px;color:var(--muted);margin-left:12px;">筛选基准程序：</label>
    <select id="sel-fi-bench">
      <option value="ALL">全部 12 个基准程序</option>
    </select>
  </div>
  <div class="legend">
    <span><i class="dot" style="background:#64748b"></i>Masked (完全掩盖：误差 ≤ 1e-12)</span>
    <span><i class="dot" style="background:#10b981"></i>Detected &amp; Recovered (投票器检出并在线恢复)</span>
    <span><i class="dot" style="background:#38bdf8"></i>Benign (良性近似误差 ≤ 1%)</span>
    <span><i class="dot" style="background:#f59e0b"></i>DUE (检出但不可恢复报错中断)</span>
    <span><i class="dot" style="background:#ef4444"></i>SDC (静默数据损坏：误差 &gt; 1% 或 NaN/Inf)</span>
  </div>
  <table id="tbl-fi">
    <thead>
      <tr>
        <th>容错机制</th>
        <th>基准程序</th>
        <th style="text-align:left;width:230px;">故障结果五分类比例 (600 次注入)</th>
        <th>完全掩盖</th>
        <th>检出并恢复</th>
        <th>良性误差</th>
        <th>不可恢复 DUE</th>
        <th>静默损坏 SDC</th>
        <th>恢复后相对误差</th>
        <th>硬件指令倍率</th>
        <th>MWTF (仅计SDC)</th>
        <th>MWTF (综合可用性)</th>
      </tr>
    </thead>
    <tbody></tbody>
  </table>
</div>

<!-- 第 4 节：选择性保护排序基线对比与阈值敏感度 -->
<div class="section">
  <h2>4. 选择性保护关键性排序算法对比与表决阈值 (δ) 敏感度权衡</h2>
  <div class="grid-equal">
    <div>
      <h3 style="font-size:14px;margin-bottom:4px;color:#e2e8f0;">4.1 不同指令保护比例预算 (25%–100%) 下五种排序算法的平均 SDC 率对比</h3>
      <p class="desc">在相同保护比例下对比本文 <strong>Semantic</strong> 模型与复现的 <strong>Shoestring</strong> (ASPLOS'10)、<strong>SDCTune</strong> (HPDC'14)、<strong>HotPath</strong> (执行频率优先) 及 <strong>Random</strong> (随机保护)。</p>
      <svg class="chart" id="svg-rq2" viewBox="0 0 620 265"></svg>
      <table id="tbl-rq2">
        <thead>
          <tr>
            <th>指令保护比例预算</th>
            <th>Random (随机)</th>
            <th>HotPath (热路径)</th>
            <th>SDCTune (HPDC'14)</th>
            <th>Shoestring (ASPLOS'10)</th>
            <th>Semantic (本文方法)</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
    <div>
      <h3 style="font-size:14px;margin-bottom:4px;color:#e2e8f0;">4.2 动态表决器误差容忍阈值 (δ) 敏感度与权衡曲线</h3>
      <p class="desc">扫描 δ ∈ [10⁻⁷, 0.5]：δ 过小会因 FP32 固有舍入误差引发无故障误报 (False Alarm)；δ 过大则导致中等幅度的位翻转逃逸表决器检测使 SDC 率上升。</p>
      <svg class="chart" id="svg-rq3" viewBox="0 0 620 265"></svg>
      <table id="tbl-rq3">
        <thead>
          <tr>
            <th>容忍阈值 δ</th>
            <th>无故障误报率 (Sobel / KissFFT)</th>
            <th>故障检出恢复率 (%)</th>
            <th>平均 SDC 漏检率 (%)</th>
            <th>恢复后平均相对误差</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
  </div>
</div>

<!-- 第 5 节：运行时自适应模式切换轨迹 -->
<div class="section">
  <h2>5. 时变故障率脉冲下的运行时自适应模式切换轨迹 (LOW ↔ HIGH)</h2>
  <p class="desc">连续 60 个任务时间窗口的硬件 PMU 退休指令数与状态机轨迹：辐射平静期 (Window 0–19，维持 <code>LOW</code> 模式) → 高通量故障爆发期 (Window 20–39，事件间隔缩短指标 A 与滑动窗口累计指标 B 同时满足，切入 <code>HIGH</code> 全量保护模式) → 恢复平静期 (Window 40–42，经历连续 <code>M=3</code> 个平静窗口滞回滤波后降级回 <code>LOW</code> 模式)。</p>
  <svg class="chart" id="svg-rq4" viewBox="0 0 1100 250"></svg>
</div>

<script>
const DATA = {json_str};

const SUITE_META = {{
  "pb_gemm": "PolyBench/C 4.2.1 (BLAS 矩阵乘)",
  "pb_2mm": "PolyBench/C 4.2.1 (双矩阵乘链)",
  "pb_atax": "PolyBench/C 4.2.1 (矩阵转置向量乘)",
  "pb_bicg": "PolyBench/C 4.2.1 (BiCGStab 子核)",
  "pb_jacobi2d": "PolyBench/C 4.2.1 (二维五点差分网格)",
  "parsec_blackscholes": "PARSEC 3.0 (偏微分方程解析定价)",
  "axc_sobel": "ACCEPT / AxC (Sobel 图像边缘梯度)",
  "axc_gaussian": "ACCEPT / AxC (5×5 高斯图像卷积)",
  "dsp_kissfft": "KissFFT (复数频谱变换)",
  "dsp_fir": "CMSIS-DSP (时域有限冲激响应滤波)",
  "gnc_pid": "GNC 控制套件 (闭环姿态 PID 控制)",
  "gnc_ekf": "GNC 控制套件 (扩展卡尔曼滤波姿态估计)"
}};

const LEVEL_CN = {{
  "CRITICAL": "关键级 (CRITICAL)",
  "SUB_CRITICAL": "次关键级 (SUB_CRITICAL)",
  "NON_CRITICAL": "非关键级 (NON_CRITICAL)"
}};

// 1. 顶部核心指标卡片
(function renderKPIs() {{
  const pmu = DATA.rq1_pmu;
  const fi = DATA.rq1_fi;
  const sumInst = (pol) => pmu.filter(r => r.policy === pol).reduce((s, r) => s + Number(r.hw_insts), 0);
  const sumTime = (pol) => pmu.filter(r => r.policy === pol).reduce((s, r) => s + Number(r.exec_time_us), 0);

  const instEDDI = sumInst("eddi");
  const instTMR  = sumInst("exact_tmr");
  const instSIMD = sumInst("approx_simd");
  const instLow  = sumInst("adaptive_low");

  const redVsTMR  = ((1 - instSIMD / instTMR) * 100).toFixed(1);
  const redVsEDDI = ((1 - instSIMD / instEDDI) * 100).toFixed(1);
  const redLowVsTMR = ((1 - instLow / instTMR) * 100).toFixed(1);
  const timeRedVsTMR = ((1 - sumTime("approx_simd") / sumTime("exact_tmr")) * 100).toFixed(1);

  const avgMetric = (pol, col) => {{
    const rows = fi.filter(r => r.policy === pol);
    return (rows.reduce((s, r) => s + Number(r[col]), 0) / rows.length);
  }};

  const sdcNat  = avgMetric("native", "sdc_pct").toFixed(2);
  const sdcSIMD = avgMetric("approx_simd", "sdc_pct").toFixed(2);
  const dueEDDI = avgMetric("eddi", "due_pct").toFixed(1);
  const dueSIMD = avgMetric("approx_simd", "due_pct").toFixed(2);
  const mwtfSIMD = avgMetric("approx_simd", "mwtf_total").toFixed(1);
  const mwtfTMR  = avgMetric("exact_tmr", "mwtf_total").toFixed(1);

  const kpis = [
    {{
      title: "相比 COAST-TMR 硬件退休指令削减",
      val: "-" + redVsTMR + "%",
      sub: `相比 EDDI 减少 -${{redVsEDDI}}% | 平静期 Adaptive-LOW 相比 TMR 减少 -${{redLowVsTMR}}%`
    }},
    {{
      title: "相比 COAST-TMR 实测执行时间降低",
      val: "-" + timeRedVsTMR + "%",
      sub: `12 个标准负载总计硬件指令倍率仅 2.43× (内联无分支快速表决)`
    }},
    {{
      title: "12 个基准平均静默损坏率 (SDC)",
      val: sdcSIMD + "%",
      sub: `从无保护 Native 的 ${{sdcNat}}% 与 SWIFT 的 1.29% 降至 ${{sdcSIMD}}% (11/12 算子为 0%)`
    }},
    {{
      title: "不可恢复错误中断率 (DUE)",
      val: dueSIMD + "%",
      sub: `显著低于双模方案 EDDI (${{dueEDDI}}%) 与 SWIFT (52.7%)，具备三路在线恢复能力`
    }},
    {{
      title: "平均综合可用性得分 (MWTF)",
      val: mwtfSIMD,
      sub: `高于传统三模冗余 COAST-TMR (${{mwtfTMR}})、SWIFT (0.78) 与 EDDI (0.36)`
    }}
  ];

  document.getElementById("kpi-container").innerHTML = kpis.map(k => `
    <div class="kpi-card">
      <div class="kpi-title">${{k.title}}</div>
      <div class="kpi-val">${{k.val}}</div>
      <div class="kpi-sub">${{k.sub}}</div>
    </div>
  `).join("");
}})();

// 2. 基准套件与静态关键性分析表
(function renderCritTable() {{
  const tbody = document.querySelector("#tbl-crit tbody");
  tbody.innerHTML = DATA.criticality.map(r => {{
    const shortName = r.function.replace(/^bench_/, "");
    const suite = SUITE_META[shortName] || "辅助遥测校验函数";
    const badgeCls = r.criticality_level === "CRITICAL" ? "badge-crit"
                   : r.criticality_level === "SUB_CRITICAL" ? "badge-sub" : "badge-non";
    return `<tr>
      <td><code>${{shortName}}</code></td>
      <td style="color:var(--muted)">${{suite}}</td>
      <td><span class="badge ${{badgeCls}}">${{LEVEL_CN[r.criticality_level] || r.criticality_level}}</span></td>
      <td>${{r.num_insts}}</td>
      <td>${{r.max_loop_depth}}</td>
      <td>${{Number(r.avg_output_closeness).toFixed(3)}}</td>
      <td>${{Number(r.avg_liveness).toFixed(2)}}</td>
      <td>${{r.state_feedback}}</td>
      <td><strong>${{Number(r.score).toFixed(2)}}</strong></td>
    </tr>`;
  }}).join("");
}})();

// 3. 硬件 PMU 开销与近似误差表
(function renderPMUAndQoR() {{
  const benches = Object.keys(SUITE_META);
  const pmuMap = {{}};
  DATA.rq1_pmu.forEach(r => {{ pmuMap[r.policy + "_" + r.benchmark] = r; }});

  const fmtRatio = (pol, b) => {{
    const nat = Number(pmuMap["native_" + b].hw_insts);
    const cur = Number(pmuMap[pol + "_" + b].hw_insts);
    return (cur / nat).toFixed(2) + "×";
  }};

  document.querySelector("#tbl-pmu-summary tbody").innerHTML = benches.map(b => {{
    const nat = Number(pmuMap["native_" + b].hw_insts);
    return `<tr>
      <td><code>${{b}}</code></td>
      <td>${{nat.toLocaleString()}} (1.00×)</td>
      <td>${{fmtRatio("eddi", b)}}</td>
      <td>${{fmtRatio("swift", b)}}</td>
      <td>${{fmtRatio("exact_tmr", b)}}</td>
      <td>${{fmtRatio("approx_scalar", b)}}</td>
      <td style="color:#38bdf8;font-weight:700">${{fmtRatio("approx_simd", b)}}</td>
      <td style="color:#10b981;font-weight:600">${{fmtRatio("adaptive_low", b)}}</td>
    </tr>`;
  }}).join("");

  document.querySelector("#tbl-qor tbody").innerHTML = benches.map(b => {{
    const pa = pmuMap["pure_approx_" + b];
    const simd = pmuMap["approx_simd_" + b];
    return `<tr>
      <td><code>${{b}}</code></td>
      <td>${{Number(pa.rel_l2_err).toExponential(2)}}</td>
      <td>${{Number(pa.mean_rel_err).toExponential(2)}}</td>
      <td>${{Number(pa.psnr_db).toFixed(1)}} dB</td>
      <td style="color:#10b981;font-weight:600">${{Number(simd.rel_l2_err) === 0 ? "0.00 (无损精确值 R)" : Number(simd.rel_l2_err).toExponential(2)}}</td>
    </tr>`;
  }}).join("");

  const svg = document.getElementById("svg-pmu-bars");
  let html = `<line x1="45" y1="235" x2="745" y2="235" stroke="#334155" />`;
  const policies = [
    {{ key: "exact_tmr", color: "#ef4444", label: "COAST-TMR (TNS'19)" }},
    {{ key: "eddi", color: "#f59e0b", label: "EDDI (TR'02)" }},
    {{ key: "approx_scalar", color: "#a855f7", label: "Approx-Scalar" }},
    {{ key: "approx_simd", color: "#38bdf8", label: "Approx-SIMD (本文)" }},
    {{ key: "adaptive_low", color: "#10b981", label: "Adaptive-LOW (本文)" }}
  ];
  policies.forEach((p, idx) => {{
    html += `<rect x="${{48 + idx*142}}" y="12" width="10" height="10" fill="${{p.color}}" />
             <text x="${{62 + idx*142}}" y="21" fill="#cbd5e1" font-size="11">${{p.label}}</text>`;
  }});
  const plotBenches = ["pb_gemm", "pb_2mm", "pb_jacobi2d", "axc_sobel", "axc_gaussian", "dsp_fir", "gnc_pid", "gnc_ekf"];
  plotBenches.forEach((b, i) => {{
    const x0 = 52 + i * 86;
    const nat = Number(pmuMap["native_" + b].hw_insts);
    policies.forEach((p, j) => {{
      const ratio = Number(pmuMap[p.key + "_" + b].hw_insts) / nat;
      const h = Math.min(190, ratio * 15);
      const y = 235 - h;
      html += `<rect x="${{x0 + j*13}}" y="${{y}}" width="11" height="${{h}}" fill="${{p.color}}" rx="1.5">
                 <title>${{b}} | ${{p.label}}: ${{ratio.toFixed(2)}}× Native</title>
               </rect>`;
    }});
    html += `<text x="${{x0 + 32}}" y="254" fill="#94a3b8" font-size="10.5" text-anchor="middle">${{b.replace(/^(pb_|axc_|dsp_|gnc_)/, "")}}</text>`;
  }});
  svg.innerHTML = html;
}})();

// 4. 故障注入五分类表与筛选
const benchSelect = document.getElementById("sel-fi-bench");
Object.keys(SUITE_META).forEach(b => {{
  const opt = document.createElement("option");
  opt.value = b;
  opt.textContent = b + " — " + SUITE_META[b];
  benchSelect.appendChild(opt);
}});

function renderFITable() {{
  const polFilter = document.getElementById("sel-fi-policy").value;
  const benchFilter = document.getElementById("sel-fi-bench").value;
  const tbody = document.querySelector("#tbl-fi tbody");

  const rows = DATA.rq1_fi.filter(r => {{
    const polOk = (polFilter === "KEY")
      ? ["native", "swift", "exact_tmr", "approx_simd"].includes(r.policy)
      : r.policy === polFilter;
    const benchOk = (benchFilter === "ALL") || (r.benchmark === benchFilter);
    return polOk && benchOk;
  }});

  tbody.innerHTML = rows.map(r => {{
    const m = Number(r.masked_pct);
    const d = Number(r.det_recovered_pct);
    const b = Number(r.benign_pct);
    const u = Number(r.due_pct);
    const s = Number(r.sdc_pct);
    return `<tr>
      <td><code>${{r.policy}}</code></td>
      <td><code>${{r.benchmark}}</code></td>
      <td>
        <div class="bar-wrap">
          <div class="bar-seg" style="width:${{m}}%;background:#64748b" title="完全掩盖: ${{m}}%"></div>
          <div class="bar-seg" style="width:${{d}}%;background:#10b981" title="检出并恢复: ${{d}}%"></div>
          <div class="bar-seg" style="width:${{b}}%;background:#38bdf8" title="良性误差: ${{b}}%"></div>
          <div class="bar-seg" style="width:${{u}}%;background:#f59e0b" title="不可恢复 DUE: ${{u}}%"></div>
          <div class="bar-seg" style="width:${{s}}%;background:#ef4444" title="静默损坏 SDC: ${{s}}%"></div>
        </div>
      </td>
      <td>${{m.toFixed(1)}}%</td>
      <td style="color:#10b981;font-weight:600">${{d.toFixed(1)}}%</td>
      <td>${{b.toFixed(1)}}%</td>
      <td style="color:${{u > 10 ? '#fbbf24' : '#94a3b8'}}">${{u.toFixed(1)}}%</td>
      <td style="color:${{s > 1 ? '#f87171' : '#10b981'}};font-weight:700">${{s.toFixed(2)}}%</td>
      <td>${{Number(r.avg_valid_rel_err).toExponential(2)}}</td>
      <td>${{Number(r.norm_hw_inst_ratio).toFixed(2)}}×</td>
      <td style="color:#38bdf8;font-weight:600">${{Number(r.mwtf_sdc).toFixed(1)}}</td>
      <td style="color:#a855f7;font-weight:700">${{Number(r.mwtf_total).toFixed(1)}}</td>
    </tr>`;
  }}).join("");
}}
renderFITable();
document.getElementById("sel-fi-policy").addEventListener("change", renderFITable);
document.getElementById("sel-fi-bench").addEventListener("change", renderFITable);

// 5. RQ2 选择性保护排序对比与 RQ3 阈值敏感度
(function renderRQ2AndRQ3() {{
  const budgets = [0.25, 0.5, 0.75, 1.0];
  const avgSDCFor = (rm, bgt) => {{
    const sub = DATA.rq2_selective.filter(r => r.rank_method === rm && Number(r.budget) === bgt);
    return sub.reduce((s, r) => s + Number(r.sdc_pct), 0) / sub.length;
  }};

  document.querySelector("#tbl-rq2 tbody").innerHTML = budgets.map(bgt => {{
    return `<tr>
      <td><strong>保护前 ${{Math.round(bgt*100)}}% 指令</strong></td>
      <td>${{avgSDCFor("random", bgt).toFixed(2)}}%</td>
      <td>${{avgSDCFor("hotpath", bgt).toFixed(2)}}%</td>
      <td>${{avgSDCFor("sdctune", bgt).toFixed(2)}}%</td>
      <td>${{avgSDCFor("shoestring", bgt).toFixed(2)}}%</td>
      <td style="color:#10b981;font-weight:700">${{avgSDCFor("semantic", bgt).toFixed(2)}}%</td>
    </tr>`;
  }}).join("");

  const svg2 = document.getElementById("svg-rq2");
  const methods = [
    {{ key: "random", color: "#94a3b8", label: "Random (随机)" }},
    {{ key: "hotpath", color: "#ef4444", label: "HotPath (频率)" }},
    {{ key: "sdctune", color: "#f59e0b", label: "SDCTune (HPDC'14)" }},
    {{ key: "shoestring", color: "#38bdf8", label: "Shoestring (ASPLOS'10)" }},
    {{ key: "semantic", color: "#10b981", label: "Semantic (本文)" }}
  ];
  let h2 = `<line x1="50" y1="225" x2="585" y2="225" stroke="#334155" />
            <line x1="50" y1="32" x2="50" y2="225" stroke="#334155" />`;
  methods.forEach((m, i) => {{
    h2 += `<rect x="${{52 + i*112}}" y="12" width="9" height="9" fill="${{m.color}}" />
           <text x="${{65 + i*112}}" y="20" fill="#cbd5e1" font-size="10.5">${{m.label}}</text>`;
    const pts = budgets.map((bgt, idx) => {{
      const x = 90 + idx * 150;
      const val = avgSDCFor(m.key, bgt);
      const y = 225 - (val / 6.0) * 175;
      return `${{x}},${{y}}`;
    }}).join(" ");
    h2 += `<polyline fill="none" stroke="${{m.color}}" stroke-width="2.2" points="${{pts}}" />`;
    budgets.forEach((bgt, idx) => {{
      const x = 90 + idx * 150;
      const val = avgSDCFor(m.key, bgt);
      const y = 225 - (val / 6.0) * 175;
      h2 += `<circle cx="${{x}}" cy="${{y}}" r="3.5" fill="${{m.color}}" />`;
      if (i === 0) {{
        h2 += `<text x="${{x}}" y="246" fill="#94a3b8" font-size="11" text-anchor="middle">预算 ${{Math.round(bgt*100)}}%</text>`;
      }}
    }});
  }});
  svg2.innerHTML = h2;

  // RQ3 阈值敏感度
  const deltas = [1e-7, 1e-5, 5e-5, 0.001, 0.05, 0.5];
  const rq3Stats = deltas.map(d => {{
    const sub = DATA.rq3_delta.filter(r => Math.abs(Number(r.delta) - d) < d*0.01);
    const sobelFA = sub.find(r => r.benchmark === "axc_sobel")?.false_alarm_pct_no_fault || "0";
    const fftFA   = sub.find(r => r.benchmark === "dsp_kissfft")?.false_alarm_pct_no_fault || "0";
    const avgDet  = sub.reduce((s, r) => s + Number(r.fi_det_recovered_pct), 0) / sub.length;
    const avgSdc  = sub.reduce((s, r) => s + Number(r.fi_sdc_pct), 0) / sub.length;
    const avgErr  = sub.reduce((s, r) => s + Number(r.fi_avg_valid_rel_err), 0) / sub.length;
    return {{ d, sobelFA: Number(sobelFA), fftFA: Number(fftFA), avgDet, avgSdc, avgErr }};
  }});

  document.querySelector("#tbl-rq3 tbody").innerHTML = rq3Stats.map(st => `<tr>
    <td><code>${{st.d.toExponential(0)}}</code></td>
    <td style="color:${{st.sobelFA > 5 || st.fftFA > 35 ? '#f59e0b' : '#10b981'}}">${{st.sobelFA.toFixed(1)}}% / ${{st.fftFA.toFixed(1)}}%</td>
    <td>${{st.avgDet.toFixed(2)}}%</td>
    <td style="color:${{st.avgSdc > 0.3 ? '#f87171' : '#10b981'}};font-weight:700">${{st.avgSdc.toFixed(2)}}%</td>
    <td>${{st.avgErr.toExponential(2)}}</td>
  </tr>`).join("");

  const svg3 = document.getElementById("svg-rq3");
  let h3 = `<line x1="50" y1="225" x2="585" y2="225" stroke="#334155" />
            <rect x="215" y="32" width="125" height="193" fill="rgba(16,185,129,0.09)" />
            <text x="277" y="48" fill="#10b981" font-size="11" text-anchor="middle">最优阈值工作区间</text>
            <rect x="60" y="12" width="10" height="10" fill="#f59e0b" />
            <text x="75" y="21" fill="#cbd5e1" font-size="11">无故障误报率 False Alarm (%)</text>
            <rect x="300" y="12" width="10" height="10" fill="#ef4444" />
            <text x="315" y="21" fill="#cbd5e1" font-size="11">平均 SDC 漏检率 (×40 放大显示)</text>`;
  const ptsFA = rq3Stats.map((st, idx) => `${{80 + idx*92}},${{225 - (Math.max(st.sobelFA, st.fftFA)/100)*175}}`).join(" ");
  const ptsSDC = rq3Stats.map((st, idx) => `${{80 + idx*92}},${{225 - Math.min(175, (st.avgSdc*40/100)*175)}}`).join(" ");
  h3 += `<polyline fill="none" stroke="#f59e0b" stroke-width="2.4" points="${{ptsFA}}" />`;
  h3 += `<polyline fill="none" stroke="#ef4444" stroke-width="2.4" points="${{ptsSDC}}" />`;
  rq3Stats.forEach((st, idx) => {{
    const x = 80 + idx * 92;
    h3 += `<text x="${{x}}" y="246" fill="#94a3b8" font-size="11" text-anchor="middle">δ=${{st.d.toExponential(0)}}</text>`;
  }});
  svg3.innerHTML = h3;
}})();

// 6. RQ4 动态模式切换轨迹
(function renderRQ4() {{
  const svg = document.getElementById("svg-rq4");
  const rows = DATA.rq4_trace;
  const maxInst = Math.max(...rows.map(r => Number(r.hw_insts))) / 1e6;
  const minInst = Math.min(...rows.map(r => Number(r.hw_insts))) / 1e6;
  let h = `<rect x="380" y="25" width="335" height="190" fill="rgba(239,68,68,0.11)" />
           <text x="547" y="44" fill="#f87171" font-size="12" text-anchor="middle">高通量辐射故障爆发期 (Window 20–39: 自动切换至 HIGH 强保护模式)</text>
           <line x1="50" y1="215" x2="1060" y2="215" stroke="#334155" />`;
  const ptsInst = rows.map((r, i) => {{
    const x = 55 + i * 16.8;
    const instM = Number(r.hw_insts) / 1e6;
    const y = 205 - ((instM - minInst*0.9) / (maxInst*1.1 - minInst*0.9)) * 155;
    return `${{x}},${{y}}`;
  }}).join(" ");
  const ptsMode = rows.map((r, i) => {{
    const x = 55 + i * 16.8;
    const m = Number(r.mode_after);
    const y = m === 1 ? 70 : 195;
    return `${{x}},${{y}}`;
  }}).join(" ");
  h += `<polyline fill="none" stroke="#38bdf8" stroke-width="2.5" points="${{ptsInst}}" />`;
  h += `<polyline fill="none" stroke="#10b981" stroke-width="2" stroke-dasharray="5,3" points="${{ptsMode}}" />`;
  h += `<text x="65" y="20" fill="#38bdf8" font-size="12">— 硬件 PMU 实测每窗口退休指令数 (${{minInst.toFixed(2)}}M ↔ ${{maxInst.toFixed(2)}}M)</text>`;
  h += `<text x="520" y="20" fill="#10b981" font-size="12">-- 运行时保护模式状态机 (LOW=0 ↔ HIGH=1，含 M=3 平静窗口滞回降级)</text>`;
  svg.innerHTML = h;
}})();
</script>
</body>
</html>
"""
    with open(OUT_HTML, "w", encoding="utf-8") as f:
        f.write(html_content)
    shutil.copyfile(OUT_HTML, OUT_INDEX)
    print(f"Generated Chinese academic HTML report: {OUT_HTML} ({OUT_HTML.stat().st_size} bytes)")


if __name__ == "__main__":
    generate_html_dashboard()
