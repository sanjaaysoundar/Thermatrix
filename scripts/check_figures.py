import os, csv
rows = []
with open('results/figure_manifest.csv', newline='') as f:
    reader = csv.DictReader(f)
    rows = list(reader)

for r in rows:
    path = r['figure']
    status = "EXISTS" if os.path.exists(path) else "MISSING"
    base = os.path.basename(path)
    src = r['data_source'][:60]
    print(f"{status}: {base} | {src}")
