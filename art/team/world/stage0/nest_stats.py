import json,sys
rows=[json.loads(l) for l in open(sys.argv[1])]
keys=["nests","nests_wanted","hearts","hearts_wanted","nest_gap_min","heart_gap_min","heart_nest_gap_min","gap_ratio_min","nests_on_road","nests_road_flag","hearts_on_road","hearts_road_flag","road_nest_candidates","road_heart_candidates","relax_hearts","relax","nest_tries","tendrils","tendrils_per_heart_min","doors_knotted","hearts_off_road","hearts_off_road_knotted","hearts_road_knotted"]
for kind in ("neighborhood","town"):
    R=[r for r in rows if r["kind"]==kind]
    print(kind,len(R))
    for k in keys:
        v=[r[k] for r in R if k in r]
        if not v: continue
        vv=[x for x in v if x!=-1]
        print("  %-26s min %-6s mean %-6s max %-6s sum %s"%(k,min(vv),round(sum(vv)/len(vv),2),max(vv),round(sum(vv),1)))
    print("  short nests", sum(1 for r in R if r["nests"]<r["nests_wanted"]), "short hearts", sum(1 for r in R if r["hearts"]<r["hearts_wanted"]))
    print("  relax hist", {l:sum(1 for r in R if r.get("relax")==l) for l in (0,1,2)})
    print("  flag mismatch", sum(1 for r in R if r.get("nests_on_road")!=r.get("nests_road_flag") or r.get("hearts_on_road")!=r.get("hearts_road_flag")))

from collections import Counter
for kind in ("neighborhood","town"):
    R=[r for r in rows if r["kind"]==kind]
    for key in ("nests_where","hearts_where","hearts_unknotted_where"):
        c=Counter()
        for r in R: c.update(r.get(key,{}))
        print(kind,key,dict(c))
    print(kind,"areas with 0/1 road hearts",sum(1 for r in R if r["hearts_road_flag"]==0),sum(1 for r in R if r["hearts_road_flag"]==1))
