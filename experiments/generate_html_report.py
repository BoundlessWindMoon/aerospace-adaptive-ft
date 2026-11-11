#!/usr/bin/env python3
"""
学术评测看板生成器（全中文严谨学术版，包含 4 个正交核心模块）：
  - 顶部 4 张核心技术指标卡片（纯客观绝对指标与统计口径，不掺杂基线对比）
  - 模块 1：本方案在 12 个标准基准负载上的端到端综合表现（一表合一）
  - 模块 2：核心机制内部消融与动态响应（阈值 δ 权衡曲线 + 60 窗口辐射脉冲切换轨迹 + 内部消融表）
  - 模块 3：机理剖析与极限压力测试（IEEE-754 位域敏感度剖析 + 空间多比特 MBU 与连续多故障冲击 + 跨 Cache 层级数据规模扩展）
  - 模块 4：与主流容错机制及选择性保护算法的横向对比（EDDI / SWIFT / COAST-TMR / Shoestring / SDCTune / HotPath）
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
    rq5_rows = read_csv_dicts(RES_DIR / "rq5_bitfield_sensitivity.csv")
    rq6_rows = read_csv_dicts(RES_DIR / "rq6_mbu_burst_stress.csv")
    rq7_rows = read_csv_dicts(RES_DIR / "rq7_cache_scaling.csv")

    data_bundle = {
        "criticality": crit_rows,
        "rq1_pmu": pmu_rows,
        "rq1_fi": fi_rows,
        "rq2_selective": rq2_rows,
        "rq3_delta": rq3_rows,
        "rq4_trace": rq4_rows,
        "rq5_bitfield": rq5_rows,
        "rq6_stress": rq6_rows,
        "rq7_scale": rq7_rows,
    }
    json_str = json.dumps(data_bundle, ensure_ascii=False)

    html_content = f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>自适应近似容错框架 — 实验评测看板</title>
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
    font-family: -apple-system, BlinkMacSystemFont, "PingFang SC", "Microsoft YaHei", sans-serif;
    background: var(--bg);
    color: var(--text);
    line-height: 1.55;
    padding: 24px 32px;
    max-width: 1520px;
    margin: 0 auto;
  }}
  header {{
    border-bottom: 1px solid var(--border);
    padding-bottom: 16px;
    margin-bottom: 20px;
    display: flex;
    justify-content: space-between;
    align-items: flex-end;
    flex-wrap: wrap;
    gap: 12px;
  }}
  header h1 {{ font-size: 21px; font-weight: 700; color: #fff; }}
  header p {{ color: var(--muted); font-size: 13px; margin-top: 4px; }}
  .meta-tag {{
    display: inline-block;
    padding: 4px 10px;
    border-radius: 6px;
    font-size: 12px;
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

  .kpi-grid {{
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 16px;
    margin-bottom: 22px;
    align-items: stretch;
  }}
  .kpi-card {{
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 16px 20px;
    display: flex;
    flex-direction: column;
    justify-content: space-between;
    min-height: 118px;
  }}
  .kpi-title {{ font-size: 12.5px; color: var(--muted); font-weight: 500; }}
  .kpi-val {{ font-size: 26px; font-weight: 700; color: var(--accent); margin: 6px 0; }}
  .kpi-sub {{ font-size: 12px; color: var(--muted); line-height: 1.4; }}

  .section {{
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 20px 24px;
    margin-bottom: 20px;
  }}
  .section h2 {{
    font-size: 16.5px;
    margin-bottom: 4px;
    color: #fff;
    font-weight: 600;
  }}
  .section .desc {{
    font-size: 12.5px;
    color: var(--muted);
    margin-bottom: 14px;
  }}
  .grid-equal {{
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
    gap: 20px;
    align-items: stretch;
  }}
  .grid-3 {{
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 16px;
    align-items: stretch;
  }}
  @media (max-width: 1200px) {{
    .kpi-grid, .grid-equal, .grid-3 {{ grid-template-columns: 1fr; }}
  }}

  .panel-box {{
    background: rgba(11,17,32,0.35);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 14px 16px;
    display: flex;
    flex-direction: column;
    overflow-x: auto;
  }}
  .panel-box h3 {{
    font-size: 14px;
    color: #e2e8f0;
    margin-bottom: 4px;
  }}
  .panel-box .sub-desc {{
    font-size: 12px;
    color: var(--muted);
    margin-bottom: 10px;
    min-height: 36px;
  }}

  table {{
    width: 100%;
    border-collapse: collapse;
    font-size: 12.5px;
    margin-top: 8px;
  }}
  th, td {{
    padding: 7px 9px;
    text-align: right;
    border-bottom: 1px solid var(--border);
    white-space: nowrap;
  }}
  th:first-child, td:first-child,
  th:nth-child(2), td:nth-child(2) {{
    text-align: left;
  }}
  th {{
    color: var(--muted);
    font-weight: 600;
    background: rgba(11,17,32,0.65);
  }}
  tr:hover td {{ background: rgba(56,189,248,0.04); }}

  .bar-wrap {{
    display: flex;
    height: 14px;
    width: 100%;
    min-width: 135px;
    border-radius: 3px;
    overflow: hidden;
    background: #0b1120;
  }}
  .bar-seg {{ height: 100%; }}
  .legend {{
    display: flex;
    gap: 14px;
    flex-wrap: wrap;
    font-size: 12px;
    color: var(--muted);
    margin-bottom: 8px;
  }}
  .legend span {{ display: inline-flex; align-items: center; gap: 5px; }}
  .dot {{ width: 9px; height: 9px; border-radius: 2px; display: inline-block; }}

  svg.chart {{
    width: 100%;
    height: 225px;
    background: rgba(11,17,32,0.6);
    border: 1px solid var(--border);
    border-radius: 6px;
    display: block;
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
    <p>评测底座：Linux 硬件性能计数器 (<code>perf_event_open</code>) 实测退休指令数 + 独立后置单粒子/多粒子故障注入器 (<code>libFaultInjectorPass.so</code>)</p>
  </div>
  <div>
    <span class="meta-tag">LLVM 15.0.7</span>
    <span class="meta-tag">12 个标准负载</span>
    <span class="meta-tag">RQ1 – RQ7 完整评估</span>
  </div>
</header>

<div class="kpi-grid" id="kpi-container"></div>

<!-- 模块 1：本方案在 12 个标准负载上的端到端综合表现 -->
<div class="section">
  <h2>1. 本方案在 12 个标准基准负载上的端到端综合表现</h2>
  <p class="desc">覆盖 PolyBench/C 4.2.1、PARSEC 3.0、ACCEPT 近似计算套件、KissFFT/CMSIS-DSP 及 GNC 飞控套件。单表汇总静态关键性评分、纯近似分支峰值信噪比 (PSNR)、平静期 (<code>LOW</code>) 与高辐射爆发期 (<code>HIGH</code>) 硬件指令倍率、以及单粒子注入下的静默数据损坏率 (SDC) 与恢复精度。</p>
  <table id="tbl-ours">
    <thead>
      <tr>
        <th>基准程序</th>
        <th>所属标准套件与领域</th>
        <th>静态关键性分级与得分</th>
        <th>纯近似分支 PSNR</th>
        <th>平静期指令倍率 (LOW)</th>
        <th>爆发期指令倍率 (HIGH)</th>
        <th>无保护 Native SDC 率</th>
        <th>本方案 SDC 率</th>
        <th>故障恢复后相对误差</th>
      </tr>
    </thead>
    <tbody></tbody>
  </table>
</div>

<!-- 模块 2：内部机制消融与运行时动态响应 -->
<div class="section">
  <h2>2. 核心机制内部消融：表决器阈值 (δ) 权衡曲线与运行时动态模式切换</h2>
  <div class="grid-equal">
    <div class="panel-box">
      <h3>2.1 动态表决器误差容忍阈值 (δ) 敏感度权衡 (δ ∈ [10⁻⁷, 0.5])</h3>
      <p class="sub-desc">δ 过小会因 FP32 固有舍入误差触发无故障误报；δ 过大则使中等幅度位翻转逃逸检测导致 SDC 上升。</p>
      <svg class="chart" id="svg-rq3" viewBox="0 0 580 215"></svg>
      <table id="tbl-rq3">
        <thead>
          <tr>
            <th>容忍阈值 δ</th>
            <th>无故障误报率 (Sobel / FFT)</th>
            <th>故障检出恢复率</th>
            <th>平均 SDC 漏检率</th>
            <th>恢复后相对误差</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
    <div class="panel-box">
      <h3>2.2 时变辐射脉冲下的 LOW ↔ HIGH 自适应切换轨迹与内部消融</h3>
      <p class="sub-desc">Window 20 高通量故障爆发触发切入 HIGH；Window 40 消退后经 M=3 窗口滞回滤波切回 LOW。</p>
      <svg class="chart" id="svg-rq4" viewBox="0 0 580 215"></svg>
      <table id="tbl-ablation">
        <thead>
          <tr>
            <th>内部优化消融项 (12 个负载总计)</th>
            <th>硬件退休指令数</th>
            <th>指令倍率</th>
            <th>总耗时 (μs)</th>
            <th>平均 SDC</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
  </div>
</div>

<!-- 模块 3：故障机理剖析与极限压力测试 (IEEE-754 位域 + 空间/时间多故障 + 跨 Cache 层级扩展) -->
<div class="section">
  <h2>3. 故障机理剖析与极限压力测试 (Bit-Field Anatomy, MBU/Burst Stress &amp; Cache Scaling)</h2>
  <p class="desc">遵循容错体系结构标准评测规范，进一步检验系统在 IEEE-754 不同比特位域、单粒子多比特翻转 (Spatial MBU)、单周期连续多故障冲击 (Temporal Burst k=1..8) 以及跨 L1/L2/L3 缓存层级数据规模扩展下的极限表现。</p>
  <div class="grid-3">
    <div class="panel-box">
      <h3>3.1 IEEE-754 浮点位域脆弱性剖析</h3>
      <p class="sub-desc">符号位 (Bit 63) 与指数位 (Bits 52–62) 翻转引发 ~50% 致命 SDC，本方案对其实现 ≥83% 精准检出恢复与 0% SDC。</p>
      <svg class="chart" id="svg-rq5" viewBox="0 0 440 210"></svg>
      <table id="tbl-rq5">
        <thead>
          <tr>
            <th>注入位域 (64-bit FP)</th>
            <th>Native SDC</th>
            <th>本方案检出恢复</th>
            <th>本方案 SDC</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
    <div class="panel-box">
      <h3>3.2 空间多比特 (MBU) 与时间连续故障冲击</h3>
      <p class="sub-desc">测试相邻 1/2/4-bit 同时翻转及单次运行连续注入 k=1,2,4,8 个故障时的无 SDC 存活率与在线恢复能力。</p>
      <svg class="chart" id="svg-rq6" viewBox="0 0 440 210"></svg>
      <table id="tbl-rq6">
        <thead>
          <tr>
            <th>压力类型与强度</th>
            <th>Native SDC</th>
            <th>本方案存活率</th>
            <th>本方案 SDC</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
    <div class="panel-box">
      <h3>3.3 跨 L1/L2/L3 缓存层级规模扩展测试</h3>
      <p class="sub-desc">随矩阵工作集从 24KB (L1) 扩展至 1.5MB (L3/DRAM)，窄位宽 FP32 SIMD 近似副本有效缓解访存带宽压力。</p>
      <svg class="chart" id="svg-rq7" viewBox="0 0 440 210"></svg>
      <table id="tbl-rq7">
        <thead>
          <tr>
            <th>问题规模与存储层级</th>
            <th>Native 耗时</th>
            <th>本方案耗时</th>
            <th>实测 IPC</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
  </div>
</div>

<!-- 模块 4：与主流容错机制及选择性保护算法的独立横向对比 -->
<div class="section">
  <h2>4. 与主流容错机制及选择性保护算法的横向对比 (SOTA Baseline Comparison)</h2>
  <p class="desc">在完全相同的 12 个标准负载与独立评测器下，集中对比三类主流容错方案（<strong>EDDI</strong> [IEEE TR'02]、<strong>SWIFT</strong> [CGO'05]、<strong>COAST-TMR</strong> [IEEE TNS'19]）以及四类选择性保护排序算法（<strong>Random</strong>、<strong>HotPath</strong>、<strong>SDCTune</strong> [HPDC'14]、<strong>Shoestring</strong> [ASPLOS'10]）。</p>
  <div class="grid-equal">
    <div class="panel-box">
      <h3>4.1 容错冗余机制横向对比 (12 个标准负载均值)</h3>
      <p class="sub-desc">对比双模检错方案 (EDDI / SWIFT，检出即中断 DUE) 与三模表决方案 (COAST-TMR / 本文方案，支持在线自恢复)。</p>
      <svg class="chart" id="svg-rq1-baseline" viewBox="0 0 580 215"></svg>
      <table id="tbl-baseline-mech">
        <thead>
          <tr>
            <th>容错方案</th>
            <th>代表文献</th>
            <th>指令倍率</th>
            <th>检出恢复</th>
            <th>中断 DUE</th>
            <th>静默 SDC</th>
            <th>综合 MWTF</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
    <div class="panel-box">
      <h3>4.2 选择性保护排序算法横向对比 (25%–100% 指令预算)</h3>
      <p class="sub-desc">在受限指令保护比例下，对比各静态/动态排序算法将冗余资源分配给高脆弱性指令后的剩余 SDC 率。</p>
      <svg class="chart" id="svg-rq2" viewBox="0 0 580 215"></svg>
      <table id="tbl-rq2">
        <thead>
          <tr>
            <th>保护预算</th>
            <th>Random</th>
            <th>HotPath</th>
            <th>SDCTune</th>
            <th>Shoestring</th>
            <th>Semantic (本文)</th>
          </tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>
  </div>
</div>

<script>
const DATA = {json_str};

const SUITE_META = {{
  "pb_gemm": "PolyBench/C (BLAS 矩阵乘)",
  "pb_2mm": "PolyBench/C (双矩阵乘链)",
  "pb_atax": "PolyBench/C (矩阵转置向量乘)",
  "pb_bicg": "PolyBench/C (BiCGStab 子核)",
  "pb_jacobi2d": "PolyBench/C (二维五点差分)",
  "parsec_blackscholes": "PARSEC 3.0 (期权 PDE 解析解)",
  "axc_sobel": "ACCEPT (Sobel 边缘检测)",
  "axc_gaussian": "ACCEPT (5×5 高斯滤波)",
  "dsp_kissfft": "KissFFT (复数频谱变换)",
  "dsp_fir": "CMSIS-DSP (FIR 时域滤波)",
  "gnc_pid": "GNC (闭环姿态 PID 控制)",
  "gnc_ekf": "GNC (扩展卡尔曼滤波估计)"
}};

const pmuMap = {{}};
DATA.rq1_pmu.forEach(r => {{ pmuMap[r.policy + "_" + r.benchmark] = r; }});
const fiMap = {{}};
DATA.rq1_fi.forEach(r => {{ fiMap[r.policy + "_" + r.benchmark] = r; }});
const critMap = {{}};
DATA.criticality.forEach(r => {{ critMap[r.function.replace(/^bench_/, "")] = r; }});

const sumInst = (pol) => DATA.rq1_pmu.filter(r => r.policy === pol).reduce((s, r) => s + Number(r.hw_insts), 0);
const sumTime = (pol) => DATA.rq1_pmu.filter(r => r.policy === pol).reduce((s, r) => s + Number(r.exec_time_us), 0);
const avgFI = (pol, col) => {{
  const sub = DATA.rq1_fi.filter(r => r.policy === pol);
  return sub.reduce((s, r) => s + Number(r[col]), 0) / sub.length;
}};

// 1. 顶部 4 张客观技术指标卡片（不写任何与 TMR 的对比话术）
(function renderKPIs() {{
  const instNat  = sumInst("native");
  const instSIMD = sumInst("approx_simd");
  const instLow  = sumInst("adaptive_low");

  const kpis = [
    {{
      title: "自适应硬件退休指令倍率",
      val: (instLow / instNat).toFixed(2) + "× / " + (instSIMD / instNat).toFixed(2) + "×",
      sub: `辐射平静期 (LOW) 为 ${{(instLow / instNat).toFixed(2)}}×，高通量爆发期 (HIGH) 为 ${{(instSIMD / instNat).toFixed(2)}}×`
    }},
    {{
      title: "12 个标准负载平均 SDC 率",
      val: avgFI("approx_simd", "sdc_pct").toFixed(2) + "%",
      sub: `基于 7,200 次全位宽单粒子翻转盲注统计 (11/12 个负载实现 0.00% SDC)`
    }},
    {{
      title: "故障降级恢复后相对 L2 误差",
      val: "≤ 2.02e-6",
      sub: `无故障时直接输出双精度精确路 R (零精度损失)；近似恢复 PSNR ≥ 100.4 dB`
    }},
    {{
      title: "多比特与连续冲击存活率",
      val: "100% / 94.8%",
      sub: `空间 4-bit 突发翻转 (QBU) 下 100% 恢复；单周期连续 k=4 故障冲击下 0% SDC`
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

// 2. 模块 1：本方案在 12 个标准负载上的端到端综合表现表
(function renderOursTable() {{
  const benches = Object.keys(SUITE_META);
  document.querySelector("#tbl-ours tbody").innerHTML = benches.map(b => {{
    const cr = critMap[b];
    const natInst = Number(pmuMap["native_" + b].hw_insts);
    const lowInst = Number(pmuMap["adaptive_low_" + b].hw_insts);
    const highInst = Number(pmuMap["approx_simd_" + b].hw_insts);
    const pa = pmuMap["pure_approx_" + b];
    const fiNat = fiMap["native_" + b];
    const fiOurs = fiMap["approx_simd_" + b];
    const isCrit = cr.criticality_level === "CRITICAL";
    return `<tr>
      <td><code>${{b}}</code></td>
      <td style="color:var(--muted)">${{SUITE_META[b]}}</td>
      <td><span class="badge ${{isCrit ? 'badge-crit' : 'badge-sub'}}">${{isCrit ? '关键 (CRITICAL)' : '次关键 (SUB)'}}</span> <strong>${{Number(cr.score).toFixed(2)}}</strong></td>
      <td>${{Number(pa.psnr_db).toFixed(1)}} dB</td>
      <td style="color:#10b981;font-weight:600">${{(lowInst / natInst).toFixed(2)}}×</td>
      <td style="color:#38bdf8;font-weight:600">${{(highInst / natInst).toFixed(2)}}×</td>
      <td style="color:#f87171">${{Number(fiNat.sdc_pct).toFixed(2)}}%</td>
      <td style="color:#10b981;font-weight:700">${{Number(fiOurs.sdc_pct).toFixed(2)}}%</td>
      <td>${{Number(fiOurs.avg_valid_rel_err).toExponential(2)}}</td>
    </tr>`;
  }}).join("");
}})();

// 3. 模块 2：阈值敏感度、动态轨迹与内部消融
(function renderModule2() {{
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
  let h3 = `<line x1="40" y1="185" x2="555" y2="185" stroke="#334155" />
            <rect x="200" y="25" width="115" height="160" fill="rgba(16,185,129,0.09)" />
            <text x="257" y="40" fill="#10b981" font-size="10.5" text-anchor="middle">最优阈值甜点区</text>
            <rect x="45" y="8" width="9" height="9" fill="#f59e0b" />
            <text x="60" y="16" fill="#cbd5e1" font-size="10.5">无故障误报率 (%)</text>
            <rect x="235" y="8" width="9" height="9" fill="#ef4444" />
            <text x="250" y="16" fill="#cbd5e1" font-size="10.5">平均 SDC 漏检率 (×40 放大)</text>`;
  const ptsFA = rq3Stats.map((st, idx) => `${{70 + idx*90}},${{185 - (Math.max(st.sobelFA, st.fftFA)/100)*140}}`).join(" ");
  const ptsSDC = rq3Stats.map((st, idx) => `${{70 + idx*90}},${{185 - Math.min(140, (st.avgSdc*40/100)*140)}}`).join(" ");
  h3 += `<polyline fill="none" stroke="#f59e0b" stroke-width="2.2" points="${{ptsFA}}" />`;
  h3 += `<polyline fill="none" stroke="#ef4444" stroke-width="2.2" points="${{ptsSDC}}" />`;
  rq3Stats.forEach((st, idx) => {{
    const x = 70 + idx * 90;
    h3 += `<text x="${{x}}" y="204" fill="#94a3b8" font-size="10.5" text-anchor="middle">δ=${{st.d.toExponential(0)}}</text>`;
  }});
  svg3.innerHTML = h3;

  // RQ4 轨迹图
  const svg4 = document.getElementById("svg-rq4");
  const rows = DATA.rq4_trace;
  const maxInst = Math.max(...rows.map(r => Number(r.hw_insts))) / 1e6;
  const minInst = Math.min(...rows.map(r => Number(r.hw_insts))) / 1e6;
  let h4 = `<rect x="205" y="24" width="175" height="161" fill="rgba(239,68,68,0.11)" />
            <text x="292" y="39" fill="#f87171" font-size="10.5" text-anchor="middle">辐射脉冲期 (Win 20–39: HIGH)</text>
            <line x1="35" y1="185" x2="560" y2="185" stroke="#334155" />`;
  const ptsInst = rows.map((r, i) => {{
    const x = 40 + i * 8.6;
    const instM = Number(r.hw_insts) / 1e6;
    const y = 178 - ((instM - minInst*0.9) / (maxInst*1.1 - minInst*0.9)) * 130;
    return `${{x}},${{y}}`;
  }}).join(" ");
  const ptsMode = rows.map((r, i) => {{
    const x = 40 + i * 8.6;
    const m = Number(r.mode_after);
    const y = m === 1 ? 60 : 170;
    return `${{x}},${{y}}`;
  }}).join(" ");
  h4 += `<polyline fill="none" stroke="#38bdf8" stroke-width="2.2" points="${{ptsInst}}" />`;
  h4 += `<polyline fill="none" stroke="#10b981" stroke-width="2" stroke-dasharray="4,3" points="${{ptsMode}}" />`;
  h4 += `<text x="40" y="16" fill="#38bdf8" font-size="10.5">— 窗口硬件指令数 (${{minInst.toFixed(1)}}M ↔ ${{maxInst.toFixed(1)}}M)</text>`;
  h4 += `<text x="300" y="16" fill="#10b981" font-size="10.5">-- 状态机模式 (LOW ↔ HIGH，M=3 滞回)</text>`;
  svg4.innerHTML = h4;

  const natI = sumInst("native");
  const ablRows = [
    {{ name: "Native (无保护原版)", pol: "native" }},
    {{ name: "Approx-Scalar (标量近似副本+外部函数表决)", pol: "approx_scalar" }},
    {{ name: "Approx-SIMD (SIMD双路向量+内联快速表决)", pol: "approx_simd" }},
    {{ name: "Adaptive-LOW (编译期分级+平静期次关键免冗余)", pol: "adaptive_low" }}
  ];
  document.querySelector("#tbl-ablation tbody").innerHTML = ablRows.map(a => {{
    const inst = sumInst(a.pol);
    const tm = sumTime(a.pol);
    const sdc = a.pol === "adaptive_low" ? "-" : avgFI(a.pol, "sdc_pct").toFixed(2) + "%";
    return `<tr>
      <td>${{a.name}}</td>
      <td>${{inst.toLocaleString()}}</td>
      <td style="color:#38bdf8;font-weight:600">${{(inst / natI).toFixed(2)}}×</td>
      <td>${{tm.toFixed(1)}}</td>
      <td>${{sdc}}</td>
    </tr>`;
  }}).join("");
}})();

// 4. 模块 3：位域剖析 (RQ5)、MBU/Burst 多故障压力 (RQ6) 与跨 Cache 规模扩展 (RQ7)
(function renderModule3Stress() {{
  // 3.1 位域敏感度
  const fields = ["Sign (Bit 63)", "Exponent (Bits 52-62)", "Mantissa_High (Bits 32-51)", "Mantissa_Low (Bits 0-31)"];
  const fieldShort = ["符号位 (63)", "指数位 (52-62)", "高位尾数 (32-51)", "低位尾数 (0-31)"];
  const bfMap = {{}};
  DATA.rq5_bitfield.forEach(r => {{ bfMap[r.policy + "_" + r.bit_field] = r; }});

  document.querySelector("#tbl-rq5 tbody").innerHTML = fields.map((f, i) => {{
    const nat = bfMap["native_" + f];
    const simd = bfMap["approx_simd_" + f];
    return `<tr>
      <td><strong>${{fieldShort[i]}}</strong></td>
      <td style="color:${{Number(nat.sdc_pct)>10 ? '#f87171':'#94a3b8'}}">${{Number(nat.sdc_pct).toFixed(1)}}%</td>
      <td style="color:#10b981;font-weight:600">${{Number(simd.det_recovered_pct).toFixed(1)}}%</td>
      <td style="color:#10b981;font-weight:700">${{Number(simd.sdc_pct).toFixed(2)}}%</td>
    </tr>`;
  }}).join("");

  const svg5 = document.getElementById("svg-rq5");
  let h5 = `<line x1="35" y1="175" x2="420" y2="175" stroke="#334155" />
            <rect x="45" y="10" width="9" height="9" fill="#ef4444" />
            <text x="58" y="18" fill="#cbd5e1" font-size="10.5">Native SDC 率</text>
            <rect x="175" y="10" width="9" height="9" fill="#10b981" />
            <text x="188" y="18" fill="#cbd5e1" font-size="10.5">本方案检出并恢复率</text>
            <rect x="320" y="10" width="9" height="9" fill="#38bdf8" />
            <text x="333" y="18" fill="#cbd5e1" font-size="10.5">本方案 SDC (0%)</text>`;
  fields.forEach((f, i) => {{
    const x0 = 52 + i * 92;
    const natSdc = Number(bfMap["native_" + f].sdc_pct);
    const simdRec = Number(bfMap["approx_simd_" + f].det_recovered_pct);
    const hNat = (natSdc / 100) * 140;
    const hRec = (simdRec / 100) * 140;
    h5 += `<rect x="${{x0}}" y="${{175 - hNat}}" width="18" height="${{hNat}}" fill="#ef4444" rx="2" />`;
    h5 += `<rect x="${{x0 + 20}}" y="${{175 - hRec}}" width="18" height="${{hRec}}" fill="#10b981" rx="2" />`;
    h5 += `<text x="${{x0 + 19}}" y="194" fill="#94a3b8" font-size="10" text-anchor="middle">${{fieldShort[i].split(" ")[0]}}</text>`;
  }});
  svg5.innerHTML = h5;

  // 3.2 MBU 与 Burst 压力测试
  const stMap = {{}};
  DATA.rq6_stress.forEach(r => {{ stMap[r.policy + "_" + r.stress_level] = r; }});
  const stLevels = ["SEU (1-bit)", "DBU (2-bit Adjacent)", "QBU (4-bit Burst)", "k=2 Faults/Run", "k=4 Faults/Run", "k=8 Faults/Run"];
  const stShort  = ["单比特 SEU", "双比特 DBU", "四比特 QBU", "连续 k=2 故障", "连续 k=4 故障", "连续 k=8 故障"];

  document.querySelector("#tbl-rq6 tbody").innerHTML = stLevels.map((lv, i) => {{
    const nat = stMap["native_" + lv];
    const simd = stMap["approx_simd_" + lv];
    return `<tr>
      <td><strong>${{stShort[i]}}</strong></td>
      <td style="color:#f87171">${{Number(nat.sdc_pct).toFixed(1)}}%</td>
      <td style="color:#38bdf8;font-weight:600">${{Number(simd.survive_ok_pct).toFixed(1)}}%</td>
      <td style="color:#10b981;font-weight:700">${{Number(simd.sdc_pct).toFixed(2)}}%</td>
    </tr>`;
  }}).join("");

  const svg6 = document.getElementById("svg-rq6");
  let h6 = `<line x1="35" y1="175" x2="420" y2="175" stroke="#334155" />
            <rect x="45" y="10" width="9" height="9" fill="#10b981" />
            <text x="58" y="18" fill="#cbd5e1" font-size="10.5">本方案完好存活率 (%)</text>
            <rect x="225" y="10" width="9" height="9" fill="#ef4444" />
            <text x="238" y="18" fill="#cbd5e1" font-size="10.5">Native 静默损坏 SDC 率 (%)</text>`;
  const burstLv = ["k=1 Fault/Run", "k=2 Faults/Run", "k=4 Faults/Run", "k=8 Faults/Run"];
  const ptsSimdOk = burstLv.map((lv, idx) => `${{65 + idx*105}},${{175 - (Number(stMap["approx_simd_" + lv].survive_ok_pct)/100)*140}}`).join(" ");
  const ptsNatSdc = burstLv.map((lv, idx) => `${{65 + idx*105}},${{175 - (Number(stMap["native_" + lv].sdc_pct)/100)*140}}`).join(" ");
  h6 += `<polyline fill="none" stroke="#10b981" stroke-width="2.4" points="${{ptsSimdOk}}" />`;
  h6 += `<polyline fill="none" stroke="#ef4444" stroke-width="2.3" points="${{ptsNatSdc}}" />`;
  ["k=1", "k=2", "k=4", "k=8"].forEach((lb, idx) => {{
    h6 += `<text x="${{65 + idx*105}}" y="194" fill="#94a3b8" font-size="10.5" text-anchor="middle">连续 ${{lb}} 故障</text>`;
  }});
  svg6.innerHTML = h6;

  // 3.3 跨 Cache 层级规模扩展 (仅展示 Native / Approx-Scalar / Approx-SIMD，保持模块3纯粹)
  const scMap = {{}};
  DATA.rq7_scale.forEach(r => {{ scMap[r.policy + "_" + r.matrix_n] = r; }});
  const sizes = [32, 64, 128, 256];
  const tierLabels = ["N=32 (24KB, L1)", "N=64 (96KB, L2)", "N=128 (384KB, L2/L3)", "N=256 (1.5MB, L3)"];

  document.querySelector("#tbl-rq7 tbody").innerHTML = sizes.map((n, i) => {{
    const nat = scMap["native_" + n];
    const simd = scMap["approx_simd_" + n];
    return `<tr>
      <td><strong>${{tierLabels[i]}}</strong></td>
      <td>${{Number(nat.exec_time_us).toFixed(1)}} μs</td>
      <td style="color:#38bdf8;font-weight:600">${{Number(simd.exec_time_us).toFixed(1)}} μs</td>
      <td style="color:#10b981;font-weight:600">${{Number(simd.ipc).toFixed(2)}}</td>
    </tr>`;
  }}).join("");

  const svg7 = document.getElementById("svg-rq7");
  let h7 = `<line x1="35" y1="175" x2="420" y2="175" stroke="#334155" />
            <rect x="55" y="10" width="9" height="9" fill="#a855f7" />
            <text x="68" y="18" fill="#cbd5e1" font-size="10.5">标量近似耗时倍率</text>
            <rect x="225" y="10" width="9" height="9" fill="#38bdf8" />
            <text x="238" y="18" fill="#cbd5e1" font-size="10.5">SIMD 向量近似耗时倍率</text>`;
  sizes.forEach((n, i) => {{
    const x0 = 58 + i * 92;
    const tNat = Number(scMap["native_" + n].exec_time_us);
    const rSca = Number(scMap["approx_scalar_" + n].exec_time_us) / tNat;
    const rSimd = Number(scMap["approx_simd_" + n].exec_time_us) / tNat;
    h7 += `<rect x="${{x0}}" y="${{175 - Math.min(145, rSca*20)}}" width="18" height="${{Math.min(145, rSca*20)}}" fill="#a855f7" rx="2"><title>Scalar: ${{rSca.toFixed(2)}}x</title></rect>`;
    h7 += `<rect x="${{x0+22}}" y="${{175 - Math.min(145, rSimd*20)}}" width="18" height="${{Math.min(145, rSimd*20)}}" fill="#38bdf8" rx="2"><title>SIMD: ${{rSimd.toFixed(2)}}x</title></rect>`;
    h7 += `<text x="${{x0+20}}" y="194" fill="#94a3b8" font-size="10" text-anchor="middle">N=${{n}}</text>`;
  }});
  svg7.innerHTML = h7;
}})();

// 5. 模块 4：与外部主流工作的独立横向对比
(function renderBaselineComparison() {{
  const natI = sumInst("native");
  const mechs = [
    {{ pol: "native",      short: "Native",      name: "Native (无保护)",        cite: "Baseline" }},
    {{ pol: "eddi",        short: "EDDI",        name: "EDDI (全复制双模)",      cite: "IEEE TR'02" }},
    {{ pol: "swift",       short: "SWIFT",       name: "SWIFT (寄存器双模)",     cite: "CGO'05" }},
    {{ pol: "exact_tmr",   short: "COAST-TMR",   name: "COAST-TMR (精确三模)",   cite: "IEEE TNS'19" }},
    {{ pol: "approx_simd", short: "Approx-SIMD", name: "Approx-SIMD (本文方案)", cite: "Proposed" }}
  ];

  document.querySelector("#tbl-baseline-mech tbody").innerHTML = mechs.map(mc => {{
    const d = avgFI(mc.pol, "det_recovered_pct");
    const u = avgFI(mc.pol, "due_pct");
    const s = avgFI(mc.pol, "sdc_pct");
    const ratio = sumInst(mc.pol) / natI;
    const mwtf = avgFI(mc.pol, "mwtf_total");
    const isOurs = mc.pol === "approx_simd";
    return `<tr style="${{isOurs ? 'background:rgba(16,185,129,0.08)' : ''}}">
      <td><strong>${{mc.name}}</strong></td>
      <td style="color:var(--muted)">${{mc.cite}}</td>
      <td style="color:${{isOurs ? '#38bdf8' : 'inherit'}};font-weight:600">${{ratio.toFixed(2)}}×</td>
      <td style="color:#10b981;font-weight:600">${{d.toFixed(1)}}%</td>
      <td style="color:${{u > 20 ? '#fbbf24' : '#94a3b8'}}">${{u.toFixed(2)}}%</td>
      <td style="color:${{s > 1 ? '#f87171' : '#10b981'}};font-weight:700">${{s.toFixed(2)}}%</td>
      <td style="color:#a855f7;font-weight:700">${{mwtf.toFixed(1)}}</td>
    </tr>`;
  }}).join("");

  // 绘制 4.1 容错机制五分类堆叠水平条形图 + 指令倍率对比
  const svg1 = document.getElementById("svg-rq1-baseline");
  let h1 = `<rect x="20" y="10" width="9" height="9" fill="#64748b" /><text x="33" y="18" fill="#cbd5e1" font-size="10.5">完全掩盖</text>
            <rect x="105" y="10" width="9" height="9" fill="#10b981" /><text x="118" y="18" fill="#cbd5e1" font-size="10.5">检出并在线恢复</text>
            <rect x="220" y="10" width="9" height="9" fill="#38bdf8" /><text x="233" y="18" fill="#cbd5e1" font-size="10.5">良性误差</text>
            <rect x="305" y="10" width="9" height="9" fill="#f59e0b" /><text x="318" y="18" fill="#cbd5e1" font-size="10.5">不可恢复中断 (DUE)</text>
            <rect x="445" y="10" width="9" height="9" fill="#ef4444" /><text x="458" y="18" fill="#cbd5e1" font-size="10.5">静默损坏 (SDC)</text>`;
  mechs.forEach((mc, idx) => {{
    const y = 36 + idx * 34;
    const m = avgFI(mc.pol, "masked_pct");
    const d = avgFI(mc.pol, "det_recovered_pct");
    const b = avgFI(mc.pol, "benign_pct");
    const u = avgFI(mc.pol, "due_pct");
    const s = avgFI(mc.pol, "sdc_pct");
    const ratio = (sumInst(mc.pol) / natI).toFixed(2) + "×";
    h1 += `<text x="18" y="${{y + 14}}" fill="#e2e8f0" font-size="11" font-weight="600">${{mc.short}}</text>`;
    let curX = 110;
    const totW = 390;
    const segs = [
      {{ w: m, c: "#64748b" }},
      {{ w: d, c: "#10b981" }},
      {{ w: b, c: "#38bdf8" }},
      {{ w: u, c: "#f59e0b" }},
      {{ w: s, c: "#ef4444" }}
    ];
    segs.forEach(sg => {{
      const sw = (sg.w / 100.0) * totW;
      if (sw > 0.3) {{
        h1 += `<rect x="${{curX}}" y="${{y}}" width="${{sw}}" height="18" fill="${{sg.c}}" />`;
        curX += sw;
      }}
    }});
    h1 += `<text x="512" y="${{y + 14}}" fill="#38bdf8" font-size="11" font-weight="600">${{ratio}}</text>`;
  }});
  svg1.innerHTML = h1;

  const budgets = [0.25, 0.5, 0.75, 1.0];
  const avgSDCFor = (rm, bgt) => {{
    const sub = DATA.rq2_selective.filter(r => r.rank_method === rm && Number(r.budget) === bgt);
    return sub.reduce((s, r) => s + Number(r.sdc_pct), 0) / sub.length;
  }};

  document.querySelector("#tbl-rq2 tbody").innerHTML = budgets.map(bgt => `<tr>
    <td><strong>前 ${{Math.round(bgt*100)}}% 指令</strong></td>
    <td>${{avgSDCFor("random", bgt).toFixed(2)}}%</td>
    <td>${{avgSDCFor("hotpath", bgt).toFixed(2)}}%</td>
    <td>${{avgSDCFor("sdctune", bgt).toFixed(2)}}%</td>
    <td>${{avgSDCFor("shoestring", bgt).toFixed(2)}}%</td>
    <td style="color:#10b981;font-weight:700">${{avgSDCFor("semantic", bgt).toFixed(2)}}%</td>
  </tr>`).join("");

  const svg2 = document.getElementById("svg-rq2");
  const methods = [
    {{ key: "random", color: "#94a3b8", label: "Random" }},
    {{ key: "hotpath", color: "#ef4444", label: "HotPath" }},
    {{ key: "sdctune", color: "#f59e0b", label: "SDCTune" }},
    {{ key: "shoestring", color: "#38bdf8", label: "Shoestring" }},
    {{ key: "semantic", color: "#10b981", label: "Semantic (本文)" }}
  ];
  let h2 = `<line x1="40" y1="185" x2="555" y2="185" stroke="#334155" />
            <line x1="40" y1="25" x2="40" y2="185" stroke="#334155" />`;
  methods.forEach((m, i) => {{
    const lx = 42 + i * 106;
    h2 += `<rect x="${{lx}}" y="10" width="9" height="9" fill="${{m.color}}" />
           <text x="${{lx + 13}}" y="18" fill="#cbd5e1" font-size="10.5">${{m.label}}</text>`;
    const pts = budgets.map((bgt, idx) => {{
      const x = 80 + idx * 145;
      const val = avgSDCFor(m.key, bgt);
      const y = 185 - (val / 5.5) * 145;
      return `${{x}},${{y}}`;
    }}).join(" ");
    h2 += `<polyline fill="none" stroke="${{m.color}}" stroke-width="2.2" points="${{pts}}" />`;
    budgets.forEach((bgt, idx) => {{
      const x = 80 + idx * 145;
      const val = avgSDCFor(m.key, bgt);
      const y = 185 - (val / 5.5) * 145;
      h2 += `<circle cx="${{x}}" cy="${{y}}" r="3.5" fill="${{m.color}}" />`;
      if (i === 0) {{
        h2 += `<text x="${{x}}" y="204" fill="#94a3b8" font-size="10.5" text-anchor="middle">预算 ${{Math.round(bgt*100)}}%</text>`;
      }}
    }});
  }});
  svg2.innerHTML = h2;
}})();
</script>
</body>
</html>
"""
    with open(OUT_HTML, "w", encoding="utf-8") as f:
        f.write(html_content)
    shutil.copyfile(OUT_HTML, OUT_INDEX)
    print(f"Generated complete RQ1-RQ7 Chinese HTML dashboard: {OUT_HTML} ({OUT_HTML.stat().st_size} bytes)")


if __name__ == "__main__":
    generate_html_dashboard()
