lines = open(r"E:\Port\AoM-Native-Port/src/engine/engine.cpp").read().splitlines()
depth = 0
start = 1238
for i in range(start-1, 1484):
    for ch in lines[i]:
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
print("depth after 1484:", depth)
