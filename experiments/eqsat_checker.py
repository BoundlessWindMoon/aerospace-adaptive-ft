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
                if self.find(cid) != cid:
                    continue
                newset = set()
                for en in self.classes[cid]:
                    cen = self.canon(en)
                    if cen in self.memo and self.find(self.memo[cen]) != cid:
                        changed |= self.union(cid, self.memo[cen])
                    self.memo[cen] = cid
                    newset.add(cen)
                self.classes[cid] = newset


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


if __name__ == "__main__":
    main()



