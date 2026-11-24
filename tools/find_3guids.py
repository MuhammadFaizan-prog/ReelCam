import os

guids = [
    "43937DC1", "2032C7EF", "7F02A37E", "A1F58958"
]

root = r"C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um"
for f in os.listdir(root):
    if f.endswith(".h"):
        path = os.path.join(root, f)
        try:
            with open(path, "r", encoding="latin-1") as fp:
                content = fp.read()
                for g in guids:
                    if g.lower() in content.lower():
                        print(f"{g} found in {f}")
        except Exception:
            pass
