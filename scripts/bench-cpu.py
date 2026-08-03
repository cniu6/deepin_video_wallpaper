#!/usr/bin/env python3
"""Sample dde-shell desktop plugin + Xorg CPU for ~5s."""
import os, time, subprocess, sys

def find_pid():
    for l in subprocess.check_output(["ps","-eo","pid,cmd"], text=True).splitlines():
        if "dde-shell -p org.deepin.ds.desktop" in l:
            return int(l.split()[0])
    return None

def cpu(p):
    return sum(int(x) for x in open(f"/proc/{p}/stat").read().split()[13:15])

def thr(p):
    d = {}
    for t in os.listdir(f"/proc/{p}/task"):
        try:
            st = open(f"/proc/{p}/task/{t}/stat").read().split()
            d[t] = (int(st[13]) + int(st[14]), open(f"/proc/{p}/task/{t}/comm").read().strip())
        except Exception:
            pass
    return d

def main():
    label = sys.argv[1] if len(sys.argv) > 1 else "sample"
    pid = find_pid()
    if not pid:
        print(f"{label}: NO_DESKTOP_PID")
        return 1
    xpid = int(subprocess.check_output(["pgrep","-n","Xorg"]).decode())
    hz = os.sysconf("SC_CLK_TCK")
    time.sleep(1)
    a, xa, pa = thr(pid), cpu(xpid), cpu(pid)
    time.sleep(5)
    b, xb, pb = thr(pid), cpu(xpid), cpu(pid)
    dde = (pb - pa) * 100 / 5 / hz
    xorg = (xb - xa) * 100 / 5 / hz
    print(f"{label}: dde~{dde:.1f}% Xorg~{xorg:.1f}% pid={pid}")
    for dc, n in sorted(((b[t][0] - a.get(t, (0, ""))[0], b[t][1]) for t in b), reverse=True)[:8]:
        print(f"  {n} ~{dc * 100 / 5 / hz:.1f}%")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
