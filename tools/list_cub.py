import struct, sys
sys.path.insert(0, r"E:\Port\AoM-Native-Port\tools")
# reuse module
import importlib.util
spec = importlib.util.spec_from_file_location("pb", r"E:\Port\AoM-Native-Port\tools\parse_bar.py")
# parse_bar.py runs main on import? No, guarded by __name__. Good.
pb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pb)
r = pb.parse_bar(r"E:\Port\Age of Mythology\textures\textures.bar")
for e in r[5]:
    if e[0].endswith(".cub"):
        print(e[0], e[2])
