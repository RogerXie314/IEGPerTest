# -*- coding: utf-8 -*-
"""Static verification for the scale refactor branch (V6.9.1).

Checks source invariants and packaged artifact count. Run from anywhere:
    python tools/verify_scale_build.py
"""
import os
import re
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_source_encoding import check_file, tracked_sources

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "external", "IEG_Code", "code", "WLServerTest")
CPP = os.path.join(SRC, "WLServerTestDlg.cpp")
H = os.path.join(SRC, "WLServerTestDlg.h")
RC = os.path.join(SRC, "WLServerTest.rc")
ZIP = os.path.join(ROOT, "artifacts", "WLServerTest_V6.9.1.zip")


def read(path):
    with open(path, "rb") as f:
        return f.read().decode("gbk", "ignore")


def check(label, ok, detail=""):
    print(("PASS  " if ok else "FAIL  ") + label + (("  -- " + detail) if detail else ""))
    return ok


def main():
    cpp, h, rc = read(CPP), read(H), read(RC)
    results = []

    # Old fixed array must only appear in explanatory comments.
    active_sock_array = [ln for ln in cpp.splitlines()
                         if "g_sock[1000]" in ln and not ln.lstrip().startswith("//")]
    results.append(check("R1 no active g_sock[1000]", not active_sock_array,
                         str(active_sock_array[:2])))
    results.append(check("R1 dynamic socket table", "g_vecSockTable" in cpp
                         and "SetSocketForClient(" in cpp and "GetSocketForClient(" in cpp))
    results.append(check("R1 atomic socket count", "InterlockedIncrement(&g_nSocketCount)" in cpp))

    hb = re.search(r"#define HB_CLIENTCOUNT_PER_THREAD\s+(\d+)", h)
    results.append(check("R2a heartbeat clients per thread == 20",
                         bool(hb) and hb.group(1) == "20",
                         hb.group(1) if hb else "not found"))
    results.append(check("R2a ceil thread count",
                         "iNeedHB_ClientCount + HB_CLIENTCOUNT_PER_THREAD - 1" in cpp))
    results.append(check("R2a actual per-thread client count",
                         "pThreadArgs->iClientCount = iThisThread_ClientIndex" in cpp))

    results.append(check("R3 no TIME_CRITICAL log thread",
                         "THREAD_PRIORITY_TIME_CRITICAL" not in cpp))
    results.append(check("R3 heartbeat/log same priority",
                         cpp.count("THREAD_PRIORITY_ABOVE_NORMAL") >= 2))

    results.append(check("R4 async policy fetch",
                         "EnqueuePolicyFetch(" in cpp and "ThreadFunc_PolicyFetchWorker" in cpp))
    results.append(check("R5 registration pool (32 workers)",
                         "REG_POOL_SIZE = 32" in cpp and "ThreadFunc_Register_Pool" in cpp
                         and "AfxBeginThread((AFX_THREADPROC)ThreadFunc_Register_SameTime" not in cpp))
    results.append(check("R6 reconnect socket republish",
                         cpp.count("SetSocketForClient(pHeapArgs->iArrClientIndex[i], sock[i])") >= 3))
    results.append(check("R6 refresh socket in log thread",
                         "GetSocketForClient(pHeapArgs->iThisClient_VectorIndex)" in cpp))

    results.append(check("version caption V6.9.1", 'CAPTION "WLServerTest V6.9.1"' in rc))
    results.append(check("version numbers 6,9,1,0",
                         "FILEVERSION 6,9,1,0" in rc and "PRODUCTVERSION 6,9,1,0" in rc))

    # Encoding guard: GBK source must never contain UTF-8 replacement bytes.
    enc_bad = []
    for path in tracked_sources():
        problems = check_file(path)
        if problems:
            enc_bad.append((os.path.basename(path), problems[:3]))
    results.append(check("source encoding clean (no EF BF BD / UTF-8 BOM)",
                         not enc_bad, str(enc_bad[:3])))

    if os.path.isfile(ZIP):
        with zipfile.ZipFile(ZIP) as zf:
            names = zf.namelist()
        results.append(check("artifact zip has 32 files", len(names) == 32, str(len(names))))
    else:
        results.append(check("artifact zip exists", False, ZIP))

    failed = results.count(False)
    print("\n%d/%d checks passed" % (len(results) - failed, len(results)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
