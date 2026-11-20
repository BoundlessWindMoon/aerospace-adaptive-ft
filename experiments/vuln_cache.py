#!/usr/bin/env python3
"""脆弱性 profile 缓存(PGO 式):按函数指纹缓存 FI 实测的 site->SDC 排名。
第一次遇到某函数(指纹未命中)→ 定点注入测量 → 写缓存;往后命中直接读,不再测。
代码改 → 指纹变 → 自动失效重测。演示:连跑两次,第一次 MISS 测量,第二次全 HIT。"""
import sys, csv, hashlib, random, os
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_harness as H

random.seed(7)
CAT = sys.argv[1] if len(sys.argv) > 1 else "int_arith"
TRIALS, MAX_SITES = 20, 64
CACHE = H.RES_DIR / "vuln_cache"; CACHE.mkdir(parents=True, exist_ok=True)

def func_fingerprints(ll_path):
    """按函数体文本算指纹(代码不变→指纹不变→命中)。行级解析 .ll。"""
    fps, cur, body = {}, None, []
    for line in Path(ll_path).read_text().splitlines():
        if line.startswith("define") and "@" in line:
            cur = line.split("@", 1)[1].split("(", 1)[0].strip()
            body = [line]
        elif cur is not None:
            body.append(line)
            if line.strip() == "}":
                fps[cur] = hashlib.sha1("\n".join(body).encode()).hexdigest()[:16]
                cur = None
    return fps

SSA = H.build_infrastructure()
fps = func_fingerprints(SSA)

# 编译 + FI,拿 site map(site_key, function) 与 fi 二进制
smap = []
H.compile_and_evaluate_policy(SSA, "vc", "native", fi_trials=1, run_fi=True,
                              fi_target_type=CAT, dump_golden=True,
                              pass_flags=["-atft-isa-avx512"], fi_site_map_out=smap)
fi_bin = H.BUILD_DIR / f"bin_vc_fi_{CAT}_x86_64"

sites_by_fn = {}
for s in smap:
    if not s["site_key"]:
        continue  # 空 key = O3 后合成的控制/地址整数op,不在 ATFT 保护域,不进缓存
    sites_by_fn.setdefault(s["function"], []).append((s["site_id"], s["site_key"]))

def run_site(sid):
    env = dict(os.environ); env["ATFT_FI_STATIC_SITE"] = str(sid)
    out = H.run_cmd([str(fi_bin), "--fi-one", "vc", str(H.GOLDEN_DIR), str(TRIALS)],
                    allow_nonzero=True, timeout_s=120, env=env)
    v = 0.0
    for ln in out.strip().splitlines():
        if ln.startswith("FI5_CSV,"): v = max(v, float(ln.split(",")[8]))
    return v

hits = misses = 0
for fn, sites in sorted(sites_by_fn.items()):
    fp = fps.get(fn)
    if not fp:   # SSA 里找不到(可能是 .atft_low/clone),跳过
        continue
    cf = CACHE / f"{fp}_{CAT}.csv"
    if cf.exists():
        hits += 1
        rank = list(csv.DictReader(open(cf)))
        print(f"HIT   {fn[:24]:24} fp={fp} 缓存 {len(rank)} 站点,top SDC={rank[0]['sdc'] if rank else '-'}")
        continue
    misses += 1
    samp = random.sample(sites, min(MAX_SITES, len(sites)))
    rows = sorted(((sk, run_site(sid)) for sid, sk in samp), key=lambda x: -x[1])
    with open(cf, "w", newline="") as f:
        w = csv.writer(f); w.writerow(["site_key", "sdc"])
        for sk, v in rows: w.writerow([sk, round(v, 2)])
    print(f"MISS  {fn[:24]:24} fp={fp} 测量 {len(samp)} 站点 → 写缓存,top SDC={rows[0][1] if rows else 0:.1f}")

print(f"\n本次:HIT {hits}  MISS {misses}  (缓存目录 {CACHE})")
print("再跑一次应全部 HIT;改任一函数代码→其指纹变→该函数 MISS 重测。")

# 合并所有命中函数的缓存成一份 site_key,sdc 总表,供 pass 以 -atft-vuln-cache 读取
merged = CACHE / f"vuln_cache_{CAT}.csv"
seen = {}
for fn, sites in sorted(sites_by_fn.items()):
    fp = fps.get(fn)
    if not fp:
        continue
    cf = CACHE / f"{fp}_{CAT}.csv"
    if not cf.exists():
        continue
    for r in csv.DictReader(open(cf)):
        sk = r["site_key"]
        if not sk:
            continue
        v = float(r["sdc"])
        seen[sk] = max(seen.get(sk, 0.0), v)
with open(merged, "w", newline="") as f:
    w = csv.writer(f); w.writerow(["site_key", "sdc"])
    for sk, v in sorted(seen.items(), key=lambda x: -x[1]):
        w.writerow([sk, round(v, 2)])
print(f"合并总表 → {merged}  ({len(seen)} 个带 key 站点)")
