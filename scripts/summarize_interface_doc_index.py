import json
import os
from typing import Any, Dict, List


ENDPOINTS_OF_INTEREST = [
    "/USM/clientAdminLog.do",
    "/USM/clientAlertlog.do",
    "/USM/clientULog.do",
    "/USM/clientFirewallLog.do",
    "/USM/hostDefenceWarning.do",
    "/USM/IllegalConnectLog.do",
    "/USM/illegalConnectLog.do",
    "/USM/virusProtectLog.do",
    "/USM/docGuardLog.do",
    "/USM/sysGuardLog.do",
    "/USM/upLoadNotWhiteFile.do",
    "/USM/loopholeProtectLog.do",
    "/USM/resourceMessage.do",
    "/USM/resourceMessageLog.do",
]


def _md_escape(s: str) -> str:
    return s.replace("|", "\\|").replace("\n", "<br>")


def main() -> None:
    workspace = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    src = os.path.join(workspace, "artifacts", "interface_doc_index.json")
    with open(src, "r", encoding="utf-8") as f:
        data: Dict[str, Any] = json.load(f)

    endpoint_tables: Dict[str, List[Dict[str, Any]]] = data.get("endpointTables", {})

    lines: List[str] = []
    lines.append(f"source: {data.get('source')}\n")
    lines.append(f"paragraphCount: {data.get('paragraphCount')}\n")
    lines.append(f"tableCount: {data.get('tableCount')}\n")

    all_eps = set(endpoint_tables.keys())
    lines.append("## Endpoints (from tables)\n")
    for ep in sorted(all_eps):
        lines.append(f"- {ep} (tables={len(endpoint_tables.get(ep, []))})")

    lines.append("\n## Endpoints of interest\n")
    for ep in ENDPOINTS_OF_INTEREST:
        tables = endpoint_tables.get(ep)
        if not tables:
            continue
        lines.append(f"\n### {ep}\n")
        for t in tables[:5]:
            ti = t.get("tableIndex")
            rows = t.get("rows") or []
            lines.append(f"TableIndex: {ti} rows={len(rows)}\n")
            # Render first ~25 rows for quick view
            preview = rows[:25]
            # Normalize column count
            max_cols = max((len(r) for r in preview), default=0)
            if max_cols == 0:
                continue
            # header
            hdr = [f"C{i+1}" for i in range(max_cols)]
            lines.append("|" + "|".join(hdr) + "|")
            lines.append("|" + "|".join(["---"] * max_cols) + "|")
            for r in preview:
                rr = list(r) + [""] * (max_cols - len(r))
                lines.append("|" + "|".join(_md_escape(x) for x in rr) + "|")
            lines.append("")

    out_path = os.path.join(workspace, "artifacts", "interface_doc_endpoints_summary.md")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    print("Wrote", os.path.relpath(out_path, workspace).replace("\\", "/"))


if __name__ == "__main__":
    main()
