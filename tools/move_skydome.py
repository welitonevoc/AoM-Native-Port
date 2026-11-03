import re
p = r"E:\Port\AoM-Native-Port/src/engine/engine.cpp"
src = open(p).read().splitlines(keepends=True)
# find start
start = None
for i,l in enumerate(src):
    if l.strip() == "void Engine::generate_skydome() {":
        start = i
        break
assert start is not None
# find end: matching closing brace
depth = 0
end = None
for i in range(start, len(src)):
    for ch in src[i]:
        if ch == '{': depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                end = i
                break
    if end is not None:
        break
assert end is not None
func = src[start:end+1]
# remove from current
remaining = src[:start] + src[end+1:]
# remove trailing blank lines at removal point? keep
# insert before "void Engine::next_texture()"
marker = "void Engine::next_texture() {}"
mi = None
for i,l in enumerate(remaining):
    if l.strip() == marker:
        mi = i
        break
assert mi is not None
# add blank line
insert = func + ["\n"]
newsrc = remaining[:mi] + insert + remaining[mi:]
open(p, "w").write("".join(newsrc))
print("moved; func lines:", len(func))
