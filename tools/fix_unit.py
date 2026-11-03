p = r"E:\Port\AoM-Native-Port/include/ecs/gameplay.h"
s = open(p).read()
old = "const auto unit = units_.find(entity);\n            if (unit != units_.end() && unit->second.unit_type_name == \"Hoplite\" &&"
new = "const auto u2 = units_.find(entity);\n            if (u2 != units_.end() && u2->second.unit_type_name == \"Hoplite\" &&"
assert old in s, "old not found"
s = s.replace(old, new)
# also update hoplite_mesh references that use unit? they use u2 only in condition; body doesn't use unit. ok.
open(p, "w").write(s)
print("done")
