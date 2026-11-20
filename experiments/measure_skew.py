#!/usr/bin/env python3
"""逐站点定点注入,量每个数据集的脆弱性"偏斜":少数站点是否占了多数 SDC。
top-20% 站点占 SDC 份额 + 基尼系数。偏斜强→细粒度有收益;接近均匀→没收益。"""
import sys, csv, os, random, statistics
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

random.seed(1)
CAT = sys.argv[1] if len(sys.argv) > 1 else "fp64"
TRIALS = 24
MAX_SITES = 24
SSA = H.build_infrastructure()

# 编译一次(native + 该类别 FI),拿 site map + fi 二进制
smap = []
H.compile_and_evaluate_policy(SSA, "skew", "native", fi_trials=1, run_fi=True,
                              fi_target_type=CAT, dump_golden=True,
                              pass_flags=["-atft-isa-avx512"], fi_site_map_out=smap)
fi_bin = H.BUILD_DIR / f"bin_skew_fi_{CAT}_x86_64"

# 按 function 分组站点(function 名 = benchmark)
bysite = {}
for s in smap:
    bysite.setdefault(s["function"], []).append(s["site_id"])

def run_site(sid):
    env = dict(os.environ); env["ATFT_FI_STATIC_SITE"] = str(sid)
    out = H.run_cmd([str(fi_bin), "--fi-one", "skew", str(H.GOLDEN_DIR), str(TRIALS)],
                    allow_nonzero=True, timeout_s=120, env=env)
    sdc = {}
    for line in out.strip().splitlines():
        if line.startswith("FI5_CSV,"):
            p = line.split(","); sdc[p[2]] = float(p[8])  # benchmark -> sdc_pct
    return sdc

def gini(xs):
    xs = sorted(xs); n = len(xs); s = sum(xs)
    if n == 0 or s == 0: return 0.0
    return (2*sum((i+1)*x for i,x in enumerate(xs))/(n*s)) - (n+1)/n

print(f"\n=== 脆弱性偏斜 [类别={CAT}] 逐站点定点注入 ===")
print(f"{'benchmark':24}{'#site':>6}{'均值SDC':>9}{'top20%份额':>11}{'Gini':>7}")
rows = []
for fn, sids in sorted(bysite.items()):
    samp = random.sample(sids, min(MAX_SITES, len(sids)))
    persite = []
    for sid in samp:
        sdc = run_site(sid)
        # 该站点属于某 benchmark;取有注入效果的那个(通常唯一非零或对应 fn)
        v = max(sdc.values()) if sdc else 0.0
        persite.append(v)
    if not persite or sum(persite) == 0:
        continue
    persite.sort(reverse=True)
    k = max(1, len(persite)//5)
    top20 = sum(persite[:k]) / sum(persite) * 100
    rows.append((fn, len(samp), statistics.mean(persite), top20, gini(persite)))
    print(f"{fn[:24]:24}{len(samp):>6}{statistics.mean(persite):>9.1f}{top20:>10.1f}%{gini(persite):>7.2f}")

if rows:
    print(f"\n均匀分布基准: top20%份额≈20%, Gini≈0 ;  偏斜: top20%≫20%, Gini→1")
    print(f"全体中位 top20%份额 = {statistics.median([r[3] for r in rows]):.1f}%  "
          f"中位 Gini = {statistics.median([r[4] for r in rows]):.2f}")
