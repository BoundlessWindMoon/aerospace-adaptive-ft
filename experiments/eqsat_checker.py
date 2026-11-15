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
def extract(eg, root, weight_mul=1.0, weight_add=1.0):
    """最小代价提取。代价 = 乘法数 x weight_mul + 加法数 x weight_add，
    求和把体内代价按迭代空间放大 N 倍并再加 N 次累加。"""
    best = {}       # cid -> (cost, mults, adds, enode)
    for _ in range(40):
        changed = False
        for cid in list(eg.classes):
            if eg.find(cid) != cid:
                continue
            for en in eg.classes[cid]:
                if en[0] == "var":
                    cand = (0.0, 0, 0, en)
                else:
                    kids = [eg.find(k) for k in en[2:]]
                    if any(k not in best for k in kids):
                        continue
                    if en[0] == "sum":
                        c, m, a, _ = best[kids[0]]
                        cand = (N * c + N * weight_add, N * m, N * a + N, en)
                    elif en[0] == "mul":
                        c0, m0, a0, _ = best[kids[0]]
                        c1, m1, a1, _ = best[kids[1]]
                        cand = (c0 + c1 + weight_mul, m0 + m1 + 1, a0 + a1, en)
                    else:   # add / min：代价同级，都是一条算术指令
                        c0, m0, a0, _ = best[kids[0]]
                        c1, m1, a1, _ = best[kids[1]]
                        cand = (c0 + c1 + weight_add, m0 + m1, a0 + a1 + 1, en)
                if cid not in best or cand[0] < best[cid][0]:
                    best[cid] = cand
                    changed = True
        if not changed:
            break
    return best


def render(eg, best, cid):
    cost, m, a, en = best[eg.find(cid)]
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
            red_idx = list(acc_idx)

        before = extract(eg, root)
        m0 = before[eg.find(root)][1]
        saturate(eg, root)
        after = extract(eg, root)
        m1, a1 = after[eg.find(root)][1], after[eg.find(root)][2]
        out.append({"kind": r["kind"],
                    "function": r["function"], "acc": "%s[%s]" % (acc_base, ",".join(acc_idx)),
                    "check_dir": check_idx, "red": ",".join(red_idx),
                    "m0": m0, "m1": m1, "a1": a1, "expr": render(eg, after, root),
                    "layer": "代数校验层" if m1 < m0 else "回落指令冗余"})
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
    cases = [("gemm 行校验", case_gemm), ("atax/bicg 全和", case_atax),
             ("jacobi2d 常系数 stencil", case_stencil),
             ("floyd-warshall (min-plus)", case_minplus)]
    print(f"{'内核':>26}{'搜索前乘法':>12}{'搜索后乘法':>12}{'降幅':>8}{'等价类':>7}{'结论':>10}")
    details = []
    for name, build in cases:
        eg = EGraph()
        root = build(eg)
        before = extract(eg, root)
        m0 = before[eg.find(root)][1]
        saturate(eg, root)
        after = extract(eg, root)
        m1, a1 = after[eg.find(root)][1], after[eg.find(root)][2]
        ncls = sum(1 for c in eg.classes if eg.find(c) == c)
        verdict = "代数校验层" if m1 < m0 else "回落指令冗余"
        ratio = f"{m0 / m1:.0f}x" if m1 and m1 < m0 else "-"
        print(f"{name:>26}{m0:>12}{m1:>12}{ratio:>8}{ncls:>7}{verdict:>10}")
        details.append((name, render(eg, after, root), m1, a1))
    print()
    for name, expr, m, a in details:
        print(f"  {name}: {expr}")
        print(f"      乘法 {m}  加法 {a}")
    print(f"\n（N = {N}；乘法数是 ABFT 省下的那一项，加法仍是 O(N^2)，"
          f"与 Huang & Abraham 1984 的账本一致）")


def main_from_sums(path):
    rows = from_sums(path)
    print(f"从 IR 抽到的归约 -> 自动合成校验式（N = {N}）\n")
    print(f"{'形态':>11}{'函数':>18}{'输出':>14}{'下标':>9}"
          f"{'搜索前乘法':>11}{'搜索后乘法':>11}{'降幅':>7}{'判定':>12}")
    for r in rows:
        ratio = f"{r['m0'] / r['m1']:.0f}x" if r["m1"] and r["m1"] < r["m0"] else "-"
        print(f"{r['kind']:>11}{r['function']:>18}{r['acc']:>14}{r['red']:>9}"
              f"{r['m0']:>11}{r['m1']:>11}{ratio:>7}{r['layer']:>12}")
    print()
    for r in rows:
        print(f"  {r['function']}: {r['expr']}")
        print(f"      乘法 {r['m1']}  加法 {r['a1']}")
    return rows


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        main_from_sums(sys.argv[1])
    else:
        main()



