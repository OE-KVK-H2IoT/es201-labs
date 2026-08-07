#!/usr/bin/env python3
"""pico-sdk-mcp — a tiny MCP server exposing the Pico SDK symbol index as tools.

For the capable models (they tool-call reliably): instead of us pre-injecting signatures,
the model calls `sdk_lookup` / `sdk_search` itself when it needs the ground truth — so it
can't invent APIs. Reads the same index `lab-ai sdk` builds (.ai/sdk-index.json).

Dependency-free: implements the MCP stdio transport (newline-delimited JSON-RPC 2.0) by
hand. Wire it into Continue as an MCP server — see .continue/README.md.
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
INDEX = HERE / "sdk-index.json"
META = HERE / "sdk-meta.json"


def load(p):
    try:
        return json.loads(p.read_text())
    except OSError:
        return {}


IDX = load(INDEX)
EXAMPLES = (load(META) or {}).get("examples", "")

TOOLS = [
    {"name": "sdk_lookup",
     "description": "Return the exact Pico SDK signature, #include, and doc summary for a function name.",
     "inputSchema": {"type": "object", "required": ["name"],
                     "properties": {"name": {"type": "string",
                                             "description": "exact function name, e.g. gpio_put"}}}},
    {"name": "sdk_search",
     "description": "Search Pico SDK symbols by substring; returns matching signatures + summaries.",
     "inputSchema": {"type": "object", "required": ["query"],
                     "properties": {"query": {"type": "string",
                                              "description": "substring to match in symbol names"}}}},
    {"name": "sdk_examples",
     "description": "Find real usages of a Pico SDK function in the official pico-examples.",
     "inputSchema": {"type": "object", "required": ["name"],
                     "properties": {"name": {"type": "string",
                                             "description": "exact function name, e.g. pwm_set_wrap"}}}},
]


def include_of(hdr):
    """src/.../include/hardware/gpio.h -> hardware/gpio.h (what you actually #include)."""
    return hdr.split("/include/", 1)[-1] if "/include/" in hdr else hdr


def sdk_lookup(name):
    e = IDX.get(name)
    if not e:
        near = sorted(k for k in IDX if name in k)[:8]
        hint = f" Did you mean: {', '.join(near)}?" if near else ""
        return f"'{name}' is not in the Pico SDK index — it likely does not exist.{hint}"
    out = [f"{e['sig']};", f'#include "{include_of(e["hdr"])}"']
    if e.get("brief"):
        out.append(f"// {e['brief']}")
    return "\n".join(out)


def sdk_search(query):
    hits = sorted(k for k in IDX if query.lower() in k.lower())[:25]
    if not hits:
        return f"No Pico SDK symbols match '{query}'."
    return "\n".join(f"{IDX[h]['sig']};"
                     + (f"   // {IDX[h]['brief']}" if IDX[h].get("brief") else "")
                     for h in hits)


def sdk_examples(name):
    if not EXAMPLES or not Path(EXAMPLES).exists():
        return "pico-examples not indexed (run `lab-ai sdk` with PICO_EXAMPLES_PATH set)."
    pat = re.compile(r"\b" + re.escape(name) + r"\s*\(")
    hits = []
    for c in Path(EXAMPLES).rglob("*.c"):
        try:
            for i, line in enumerate(c.read_text(errors="ignore").splitlines(), 1):
                if pat.search(line):
                    rel = c.relative_to(EXAMPLES).as_posix()
                    hits.append(f"{rel}:{i}: {line.strip()}")
                    if len(hits) >= 10:
                        return "\n".join(hits)
        except OSError:
            continue
    return "\n".join(hits) if hits else f"No pico-examples call {name}()."


def call_tool(name, args):
    if name == "sdk_lookup":
        return sdk_lookup(args.get("name", ""))
    if name == "sdk_search":
        return sdk_search(args.get("query", ""))
    if name == "sdk_examples":
        return sdk_examples(args.get("name", ""))
    raise ValueError(f"unknown tool: {name}")


def handle(req):
    """Return a JSON-RPC response dict, or None for notifications."""
    method, rid = req.get("method"), req.get("id")
    if method == "initialize":
        result = {"protocolVersion": req.get("params", {}).get("protocolVersion", "2024-11-05"),
                  "capabilities": {"tools": {}},
                  "serverInfo": {"name": "pico-sdk", "version": "0.1.0"}}
    elif method == "tools/list":
        result = {"tools": TOOLS}
    elif method == "tools/call":
        p = req.get("params", {})
        try:
            text = call_tool(p.get("name"), p.get("arguments", {}))
            result = {"content": [{"type": "text", "text": text}]}
        except Exception as e:  # report tool errors in-band, per MCP
            result = {"content": [{"type": "text", "text": str(e)}], "isError": True}
    elif method is not None and rid is None:
        return None                                     # a notification: no reply
    else:
        return {"jsonrpc": "2.0", "id": rid,
                "error": {"code": -32601, "message": f"method not found: {method}"}}
    return {"jsonrpc": "2.0", "id": rid, "result": result}


def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            resp = handle(json.loads(line))
        except json.JSONDecodeError:
            continue
        if resp is not None:
            sys.stdout.write(json.dumps(resp) + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    main()
