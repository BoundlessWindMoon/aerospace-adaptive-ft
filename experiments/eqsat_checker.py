#!/usr/bin/env python3
"""等价饱和合成校验式：原型（独立运行，不依赖 LLVM 与 egg）

要验的只有一件事：在"实数语义下等价"的小规则集上，e-graph 能不能**自己**找出
代价更低的等价校验式 —— 也就是把人工推导的 ABFT checksum 搜出来。

验证用例取矩阵乘的行校验目标式：
    sum_j sum_k A[i,k] * B[k,j]          （朴素：每行 N^2 次乘法）
期望搜到：
    sum_k A[i,k] * (sum_j B[k,j])        （每行 N 次乘法）

需要的规则只有两条：求和交换、与下标无关的因子外提。两条都在实数语义下
严格成立；浮点下重结合会改变舍入，这部分偏差由表决阈值框架管（见 METHOD.md 的 D3）。

用法：
    python3 experiments/eqsat_checker.py
"""

import itertools
from collections import defaultdict

N = 64  # 每个下标的迭代空间大小，只影响代价模型的权重


# ---------------------------------------------------------------- 项与 e-node
# enode = (op, payload, children...)
#   ('var', name_with_indices)      叶子，payload 带自由下标
#   ('mul', None, a, b)
#   ('add', None, a, b)
#   ('sum', idx, body)
def var(name, idxs):
    return ("var", (name, frozenset(idxs)))


def mul(a, b):
    return ("mul", None, a, b)


def add(a, b):
    return ("add", None, a, b)


def summ(idx, body):
    return ("sum", idx, body)


class EGraph:
    def __init__(self):
        self.parent = {}          # eclass id -> parent (union-find)
        self.classes = defaultdict(set)   # eclass id -> set of enodes
        self.memo = {}            # canonical enode -> eclass id
        self.next_id = 0

    def find(self, c):
        while self.parent[c] != c:
            self.parent[c] = self.parent[self.parent[c]]
            c = self.parent[c]
        return c

    def canon(self, enode):
        op, payload = enode[0], enode[1]
        kids = tuple(self.find(k) for k in enode[2:])
        return (op, payload) + kids

    def add_enode(self, enode):
        enode = self.canon(enode)
        if enode in self.memo:
            return self.find(self.memo[enode])
        cid = self.next_id
        self.next_id += 1
        self.parent[cid] = cid
        self.classes[cid].add(enode)
        self.memo[enode] = cid
        return cid

    def union(self, a, b):
        a, b = self.find(a), self.find(b)
        if a == b:
            return False
        self.parent[b] = a
        self.classes[a] |= self.classes.pop(b)
        return True

    def rebuild(self):
        """重新规范化所有 e-node，合并因 union 变得相同的节点。"""
        changed = True
        while changed:
            changed = False
            self.memo.clear()
            for cid in list(self.classes):
                # union 会在循环体内合并等价类并改动 self.classes，
                # 所以必须对快照迭代，并在每轮重新确认 cid 还是代表元
                if cid not in self.classes or self.find(cid) != cid:
                    continue
                newset = set()
                for en in list(self.classes[cid]):
                    cen = self.canon(en)
                    other = self.memo.get(cen)
                    if other is not None and self.find(other) != self.find(cid):
                        changed |= self.union(cid, other)
                    self.memo[cen] = self.find(cid)
                    newset.add(cen)
                root = self.find(cid)
                if root in self.classes:
                    self.classes[root] |= newset
                else:
                    self.classes[root] = newset


# ---------------------------------------------------------------- 自由下标分析
def free_indices(eg):
    """每个 e-class 的自由下标集合，按不动点求。

    因子外提规则需要它：只有当求和下标不在因子的自由下标里，才能把因子提出来。
    """
    free = defaultdict(frozenset)
    changed = True
    while changed:
        changed = False
        for cid in list(eg.classes):
            if eg.find(cid) != cid:
                continue
            acc = free[cid]
            for en in eg.classes[cid]:
                if en[0] == "var":
                    f = en[1][1]
                elif en[0] == "sum":
                    f = free[eg.find(en[2])] - {en[1]}
                else:
                    f = free[eg.find(en[2])] | free[eg.find(en[3])]
                acc = acc | f
            if acc != free[cid]:
                free[cid] = acc
                changed = True
    return free


# ---------------------------------------------------------------- 规则
def saturate(eg, root, max_iters=12):
    """应用规则直到不再产生新等价（或到迭代上限）。

    规则（实数语义下严格成立）：
      R1 乘法交换      a*b = b*a
      R2 求和交换      sum_a sum_b x = sum_b sum_a x
      R3 因子外提      sum_a (x*y) = x * (sum_a y)      当 a 不在 free(x)
      R4 求和线性性    sum_a (x+y) = sum_a x + sum_a y
    """
    for it in range(max_iters):
        free = free_indices(eg)
        matches = []
        for cid in list(eg.classes):
            if eg.find(cid) != cid:
                continue
            for en in list(eg.classes[cid]):
                if en[0] == "mul":
                    matches.append(("R1", cid, en))
                elif en[0] == "sum":
                    body = eg.find(en[2])
                    for ben in list(eg.classes[body]):
                        if ben[0] == "sum":
                            matches.append(("R2", cid, en, ben))
                        elif ben[0] == "mul":
                            matches.append(("R3", cid, en, ben, free))
                        elif ben[0] == "add":
                            matches.append(("R4", cid, en, ben))
        changed = False
        for m in matches:
            kind = m[0]
            if kind == "R1":
                _, cid, en = m
                changed |= eg.union(cid, eg.add_enode(("mul", None, en[3], en[2])))
            elif kind == "R2":
                _, cid, en, ben = m
                inner = eg.add_enode(("sum", en[1], ben[2]))
                swapped = eg.add_enode(("sum", ben[1], inner))
                changed |= eg.union(cid, swapped)
            elif kind == "R3":
                _, cid, en, ben, free = m
                a = en[1]
                for x, y in ((ben[2], ben[3]), (ben[3], ben[2])):
                    if a not in free[eg.find(x)]:
                        inner = eg.add_enode(("sum", a, y))
                        hoisted = eg.add_enode(("mul", None, x, inner))
                        changed |= eg.union(cid, hoisted)
            elif kind == "R4":
                _, cid, en, ben = m
                l = eg.add_enode(("sum", en[1], ben[2]))
                r = eg.add_enode(("sum", en[1], ben[3]))
                changed |= eg.union(cid, eg.add_enode(("add", None, l, r)))
        eg.rebuild()
        if not changed:
            return it + 1
    return max_iters


# ---------------------------------------------------------------- 代价与提取
# ---------------------------------------------------------------- 多项式代价
# 代价不是标量而是关于迭代空间 N 的多项式：系数表 [c0, c1, c2, ...] 表示 sum ci*N^i。
# 这样"渐近阶"就成了可逐节点组合的局部量，标准的自底向上提取能直接用它
# （对照 Suciu et al. 2025 指出的"非局部代价函数无法用于 top-k 提取"这一限制）。
def p_zero():
    return [0]


def p_const(c):
    return [c]


def p_add(a, b):
    n = max(len(a), len(b))
    return [(a[i] if i < len(a) else 0) + (b[i] if i < len(b) else 0) for i in range(n)]


def p_shift(a):
    """乘 N：系数整体升一次。"""
    return [0] + list(a)


def p_deg(a):
    d = 0
    for i, c in enumerate(a):
        if c:
            d = i
    return d


def p_key(a):
    """比较键：先比次数，再从高位到低位比系数。"""
    d = p_deg(a)
    return (d,) + tuple(reversed([a[i] if i < len(a) else 0 for i in range(d + 1)]))


def p_eval(a, n):
    return sum(c * (n ** i) for i, c in enumerate(a))


def p_str(a):
    d = p_deg(a)
    if d == 0:
        return str(a[0] if a else 0)
    parts = []
    for i in range(d, -1, -1):
        c = a[i] if i < len(a) else 0
        if not c:
            continue
        parts.append(f"{c}N^{i}" if i > 1 else (f"{c}N" if i == 1 else str(c)))
    return "+".join(parts) if parts else "0"


def kernel_cost(nest_depth, op_mix):
    """内核自身的代价多项式：体内每个算子在 N^depth 个迭代点上各算一次。"""
    mults = op_mix.count("*")
    adds = op_mix.count("+") + 1          # +1 是累加本身
    m = [0] * nest_depth + [max(mults, 0)]
    a = [0] * nest_depth + [max(adds, 1)]
    return m, a


def extract(eg, root):
    """最小代价提取。代价是 (乘法多项式, 加法多项式)，按总代价多项式比较。"""
    best = {}       # cid -> (mults_poly, adds_poly, enode)
    for _ in range(60):
        changed = False
        for cid in list(eg.classes):
            if eg.find(cid) != cid:
                continue
            for en in eg.classes[cid]:
                if en[0] == "var":
                    cand = (p_zero(), p_zero(), en)
                else:
                    kids = [eg.find(k) for k in en[2:]]
                    if any(k not in best for k in kids):
                        continue
                    if en[0] == "sum":
                        m, a, _ = best[kids[0]]
                        # sum: 体内代价 x N，再加 N 次累加
                        cand = (p_shift(m), p_add(p_shift(a), [0, 1]), en)
                    elif en[0] == "mul":
                        m0, a0, _ = best[kids[0]]
                        m1, a1, _ = best[kids[1]]
                        cand = (p_add(p_add(m0, m1), p_const(1)), p_add(a0, a1), en)
                    else:   # add / min：一条算术指令
                        m0, a0, _ = best[kids[0]]
                        m1, a1, _ = best[kids[1]]
                        cand = (p_add(m0, m1), p_add(p_add(a0, a1), p_const(1)), en)
                key = lambda t: p_key(p_add(t[0], t[1]))
                if cid not in best or key(cand) < key(best[cid]):
                    best[cid] = cand
                    changed = True
        if not changed:
            break
    return best


def render(eg, best, cid):
    _m, _a, en = best[eg.find(cid)]
    if en[0] == "var":
        name, idxs = en[1]
        return name
    kids = [render(eg, best, k) for k in en[2:]]
    if en[0] == "mul":
        return f"({kids[0]} * {kids[1]})"
    if en[0] == "add":
        return f"({kids[0]} + {kids[1]})"
    if en[0] == "min":
        return f"min({kids[0]}, {kids[1]})"
    return f"sum_{en[1]}({kids[0]})"


def checksum_capability(kind, acc_idx, check_idx, red_idx):
    """返回候选校验式的结构性检测能力说明。

    这里刻意不把“单错可检测”伪装成端到端故障覆盖率。对当前生成的
    行/列和类校验，在精确算术下，任意一个非零输出元素错误都会改变
    校验和；但单个秩一校验不能定位多个错误，具有相反符号的错误也可能
    抵消。纠错还需要原数据快照或其它独立信息。
    """
    output_width = f"N_{check_idx}"
    if kind == "REDUCTION":
        correction = "snapshot_required"
        scope = "reduction_output_slice"
    elif kind == "LINEAR_MAP":
        correction = "detection_only"
        scope = "linear_map_output_slice"
    else:
        correction = "unknown"
        scope = "unknown"
    return {
        "checksum_rank": 1,
        "checksum_scope": scope,
        "checksum_output_width": output_width,
        "detected_error_subspace_dimension": "1",
        "undetected_error_subspace_dimension": f"{output_width}-1",
        "error_localization_rank": 1,
        "single_error_detection": "all_nonzero_single_output_errors",
        "single_error_coverage": 1.0,
        "multi_error_cancellation": "possible",
        "multi_error_localization": "not_supported",
        "correction_capability": correction,
        "blind_spot": "opposite_signed_errors_can_cancel",
        "check_direction": check_idx,
        "output_indices": ",".join(acc_idx),
        "reduction_indices": ",".join(red_idx),
    }


def from_sums(path):
    """读 `-atft-dump-sums` 的产物，对每条归约自动构造校验目标式并搜索。

    构造规则（不针对具体内核）：累加数组的自由下标 = 结果下标，其余为归约下标；
    取一个结果下标 c 作校验方向，目标式 = sum_c sum_{归约下标} prod(terms)。
    这就是"把结果沿 c 方向求和"的不变量，ABFT 的行/列校验和是它的特例。
    """
    import csv as _csv
    rows = [r for r in _csv.DictReader(open(path))
            if r["kind"] in ("REDUCTION", "LINEAR_MAP")]
    out = []
    for r in rows:
        if not r["function"].startswith("bench_"):
            continue
        acc_base, acc_idx = r["accumulator"].split("@", 1)[0].split(":")
        acc_idx = [x for x in acc_idx.split(";") if x]
        terms = []
        for t in r["terms"].split("|"):
            if not t:
                continue
            # 项的格式是 base:i0;i1;@偏移。偏移此处不参与代价搜索（它影响的是
            # 生成校验器时的边界项），必须先切掉，否则会被当成一个"下标"。
            body = t.split("@", 1)[0]
            base, idx = body.split(":")
            terms.append((base, [x for x in idx.split(";") if x]))
        if not terms or not acc_idx:
            continue
        all_idx = sorted({i for _, ix in terms for i in ix} | set(acc_idx))
        red_idx = [i for i in all_idx if i not in acc_idx]
        check_idx = acc_idx[-1]

        eg = EGraph()
        if r["kind"] == "REDUCTION":
            if not red_idx:
                continue
            # 归约：目标式 = sum_c sum_red prod(terms)
            node = None
            for base, ix in terms:
                v = eg.add_enode(var("%s[%s]" % (base, ",".join(ix)), ix))
                node = v if node is None else eg.add_enode(mul(node, v))
            for i in red_idx:
                node = eg.add_enode(summ(i, node))
            root = eg.add_enode(summ(check_idx, node))
        else:
            # 线性映射（stencil 类）：没有归约下标，不变量是"把输出沿所有下标求和"。
            # 带下标的项相加、循环不变的标量因子相乘 —— 后者正是因子外提的作用对象。
            scal = [b for b, ix in terms if not ix]
            arrs = [(b, ix) for b, ix in terms if ix]
            if not arrs:
                continue
            node = None
            for base, ix in arrs:
                v = eg.add_enode(var("%s[%s]" % (base, ",".join(ix)), ix))
                node = v if node is None else eg.add_enode(add(node, v))
            for b in scal:
                node = eg.add_enode(mul(eg.add_enode(var(b, set())), node))
            for i in sorted(set(acc_idx)):
                node = eg.add_enode(summ(i, node))
            root = node
            # 映射形态没有归约下标，red_idx 保持为空（之前这里写成 acc_idx，
            # 会把输出下标误当成归约下标，阶段循环数就算不对了）

        after_pre = extract(eg, root)
        m_pre, a_pre, _ = after_pre[eg.find(root)]
        saturate(eg, root)
        after = extract(eg, root)
        m_chk, a_chk, _ = after[eg.find(root)]
        km, ka = kernel_cost(int(r["nest_depth"]), r["op_mix"])

        chk_tot = p_add(m_chk, a_chk)
        ker_tot = p_add(km, ka)

        # 阶段循环修正：循环巢里既不是输出下标、也不是校验式归约下标的那些层，
        # 是**顺序阶段**（jacobi2d 的时间步就是这种）。中间结果会被下一阶段消费，
        # 所以校验必须逐阶段做一次，校验代价要乘上这些层的迭代次数 —— 次数升一阶。
        # 这条规则是 2mm 实测教训（检出 36.5% 但 SDC 不降）的一般化。
        n_stage = max(0, int(r["nest_depth"]) - len(set(acc_idx)) - len(red_idx))
        chk_deg_eff = p_deg(chk_tot) + n_stage
        admit = chk_deg_eff < p_deg(ker_tot)
        capability = checksum_capability(r["kind"], acc_idx, check_idx, red_idx)
        out.append({"kind": r["kind"], "function": r["function"],
                    "acc": "%s[%s]" % (acc_base, ",".join(acc_idx)),
                    "check_dir": check_idx, "red": ",".join(red_idx),
                    "kernel_cost": p_str(ker_tot), "kernel_deg": p_deg(ker_tot),
                    "naive_chk": p_str(p_add(m_pre, a_pre)),
                    "chk_cost": p_str(chk_tot), "chk_deg": chk_deg_eff,
                    "n_stage": n_stage,
                    "chk_mults": p_str(m_chk),
                    "expr": render(eg, after, root),
                    "admit": admit,
                    "layer": "代数校验层" if admit else "回落指令冗余",
                    **capability})
    return out


def case_gemm(eg):
    """矩阵乘的行校验：sum_j sum_k A[i,k]*B[k,j]"""
    A = eg.add_enode(var("A[i,k]", {"i", "k"}))
    B = eg.add_enode(var("B[k,j]", {"k", "j"}))
    return eg.add_enode(summ("j", eg.add_enode(summ("k", eg.add_enode(mul(A, B))))))


def case_atax(eg):
    """atax/bicg 形态：sum_i sum_j A[i,j]*x[j]（矩阵向量积的全和）"""
    A = eg.add_enode(var("A[i,j]", {"i", "j"}))
    x = eg.add_enode(var("x[j]", {"j"}))
    return eg.add_enode(summ("i", eg.add_enode(summ("j", eg.add_enode(mul(A, x))))))


def case_stencil(eg):
    """jacobi2d 形态：sum_i sum_j c*(u1+u2)，c 是与下标无关的常系数"""
    c = eg.add_enode(var("c", set()))
    u1 = eg.add_enode(var("u[i-1,j]", {"i", "j"}))
    u2 = eg.add_enode(var("u[i+1,j]", {"i", "j"}))
    body = eg.add_enode(mul(c, eg.add_enode(add(u1, u2))))
    return eg.add_enode(summ("i", eg.add_enode(summ("j", body))))


def case_minplus(eg):
    """floyd-warshall 形态：min_k(D[i,k] + D[k,j])。
    min-plus 半环上没有可用的线性性，规则集里也没有 min 的规则 ——
    期望搜索失败，这正是"该内核进不了代数校验层"的判据。"""
    D1 = eg.add_enode(var("D[i,k]", {"i", "k"}))
    D2 = eg.add_enode(var("D[k,j]", {"k", "j"}))
    return eg.add_enode(summ("k", eg.add_enode(("min", None, D1, D2))))


def main():
    """自带用例：四个人工编码的形态，用来回归规则集与代价模型本身。"""
    cases = [("gemm 行校验", case_gemm, 3, "**"),
             ("atax/bicg 全和", case_atax, 2, "*"),
             ("jacobi2d 常系数 stencil", case_stencil, 2, "*+"),
             ("floyd-warshall (min-plus)", case_minplus, 1, "+")]
    print(f"{'内核':>26}{'内核代价':>10}{'朴素校验':>12}{'搜到的校验':>12}"
          f"{'等价类':>7}{'判定':>12}")
    results = []
    for name, build, depth, opmix in cases:
        eg = EGraph()
        root = build(eg)
        pre = extract(eg, root)
        m0, a0, _ = pre[eg.find(root)]
        saturate(eg, root)
        post = extract(eg, root)
        m1, a1, _ = post[eg.find(root)]
        km, ka = kernel_cost(depth, opmix)
        chk, ker = p_add(m1, a1), p_add(km, ka)
        admit = p_deg(chk) < p_deg(ker)
        ncls = sum(1 for c in eg.classes if eg.find(c) == c)
        print(f"{name:>26}{p_str(ker):>10}{p_str(p_add(m0, a0)):>12}{p_str(chk):>12}"
              f"{ncls:>7}{('代数校验层' if admit else '回落指令冗余'):>12}")
        results.append((name, render(eg, post, root), admit))
    print()
    for name, expr, admit in results:
        print(f"  {name}: {expr}")
    return results


def main_from_sums(path, json_out=None):
    rows = from_sums(path)
    if json_out:
        import json
        with open(json_out, "w") as f:
            json.dump(rows, f, indent=2, ensure_ascii=False)
        print(f"候选报告已写入: {json_out}")
    print("准入判据：校验式的代价多项式次数 < 内核自身的次数")
    print("（代价是关于迭代空间 N 的多项式；搜索前后的比较不是判据 —— "
          "matvec 搜索后乘法也降一阶，但总代价与内核同阶，所以不准入）\n")
    print(f"{'形态':>11}{'函数':>18}{'内核代价':>10}{'朴素校验':>12}"
          f"{'搜到的校验':>12}{'阶段层':>7}{'次数':>7}{'判定':>12}")
    for r in rows:
        cmpstr = (f"{r['chk_deg']}<{r['kernel_deg']}" if r["admit"]
                  else f"{r['chk_deg']}>={r['kernel_deg']}")
        print(f"{r['kind']:>11}{r['function']:>18}{r['kernel_cost']:>10}{r['naive_chk']:>12}"
              f"{r['chk_cost']:>12}{r['n_stage']:>7}{cmpstr:>7}{r['layer']:>12}")
    print()
    for r in rows:
        if not r["admit"]:
            continue
        print(f"  {r['function']}: {r['expr']}")
        print(f"      校验式乘法 {r['chk_mults']}，总代价 {r['chk_cost']}")
    n_ad = sum(1 for r in rows if r["admit"])
    print(f"\n准入 {n_ad} / {len(rows)} 条；"
          f"涉及负载 {len({r['function'] for r in rows if r['admit']})} 个")
    return rows


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        out = None
        if "--json-out" in sys.argv:
            pos = sys.argv.index("--json-out")
            if pos + 1 >= len(sys.argv):
                raise SystemExit("--json-out 需要路径")
            out = sys.argv[pos + 1]
        main_from_sums(sys.argv[1], out)
    else:
        main()
