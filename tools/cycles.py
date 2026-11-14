"""Find unsafe include cycles among sdk/map headers.

An edge exists when a header #includes another map header, or when it names
pm::api::Ref<pm::map::X> for a Type entry X declared in another header (a wrapper needs that
entry where it sits). An include written after the file's `}  // namespace pm::map` line is
"late": sdk/internals/mapping.md lets two headers include each other late, because each
header's own entries are then defined before either header's wrappers are seen, which is
where the Ref edges live. A cycle is reported only when some pair along it lacks such a late
include (a top-of-file include, or none at all): that is the case the layout forbids. Cycles
made only of late includes are the layout, so they are printed as allowed. Entries that reach
across the cycle themselves (a `.base` or `within` pointer) must declare the partner's entry
extern above their own entry; the script does not check that, the compiler does.

Run: python tools/cycles.py [map-dir]. Exit 0 when nothing is unsafe."""
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "sdk", "map")
NOT_CYCLE = ("entry.h", "game_api.h", "all.h")

decl = defaultdict(set)  # ident -> headers defining that EntryDesc
text = {}
for fn in sorted(os.listdir(MAP)):
    if not fn.endswith(".h") or fn == "all.h":
        continue
    t = open(os.path.join(MAP, fn), encoding="utf-8").read()
    text[fn] = t
    for m in re.finditer(r"inline constexpr pm::sdk::EntryDesc ([A-Za-z_][A-Za-z0-9_]*)\{", t):
        decl[m.group(1)].add(fn)

inc_re = re.compile(r'#include "([A-Za-z0-9_]+\.h)"')
ref_re = re.compile(r"pm::api::Ref<pm::map::([A-Za-z_][A-Za-z0-9_]*)>")
edges = defaultdict(set)
late = defaultdict(set)  # header -> partner it includes after its pm::map section
for fn, t in text.items():
    ns_end = t.find("}  // namespace pm::map")
    for m in inc_re.finditer(t):
        partner = m.group(1)
        if partner in text and partner not in NOT_CYCLE:
            edges[fn].add(partner)
            if ns_end >= 0 and m.start() > ns_end:
                late[fn].add(partner)
    for m in ref_re.finditer(t):
        for src in decl.get(m.group(1), ()):
            if src != fn:
                edges[fn].add(src)

seen, stack, cycles = set(), [], {}  # canonical key -> cycle in the order the walk found it


def dfs(u):
    stack.append(u)
    onstack = set(stack)
    for v in sorted(edges[u]):
        if v in onstack:
            cyc = tuple(stack[stack.index(v):])
            cycles.setdefault(tuple(sorted(cyc)), cyc)
        elif v not in seen:
            dfs(v)
    stack.pop()
    seen.add(u)


for start in sorted(text):
    if start not in seen:
        dfs(start)

bad = 0
for key in sorted(cycles):
    cyc = cycles[key]
    n = len(cyc)
    allowed = all(cyc[(i + 1) % n] in late[cyc[i]] for i in range(n))
    line = " -> ".join(list(cyc) + [cyc[0]])
    if allowed:
        print("allowed (after-the-entries includes):", line)
    else:
        print("cycle:", line)
        bad += 1
if not cycles:
    print("no cycles")
sys.exit(1 if bad else 0)
