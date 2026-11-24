import os

guids = [
    "A1F58958", "2032C7EF", "7F02A37E", "43937DC1", "FBB03414", "2347D60B", "03910848"
]

root = r"C:\Program Files (x86)\Windows Kits\10\Include"
found = set()
for base, dirs, files in os.walk(root):
    for f in files:
        if f.endswith(".h"):
            path = os.path.join(base, f)
            try:
                with open(path, "r", encoding="latin-1") as fp:
                    content = fp.read()
                    for g in guids:
                        if g.lower() in content.lower():
                            found.add((g, f))
            except Exception:
                pass

for g, f in found:
    print(f"{g} found in {f}")
print("Done search.")
