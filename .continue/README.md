# Continue tutor config (rules + slash-commands)

This folder is the **portable pedagogy layer** for the [Continue](https://continue.dev)
coding assistant — the tutor's rules and slash-commands, *not* the model/server config.
Continue auto-discovers `rules/` and `prompts/` when you open a workspace, so nothing here
is tied to a particular lab network.

## Two layers

| Layer | Where | What |
|---|---|---|
| **Models** (once per machine) | global `~/.continue/config.yaml` | Ollama models + `apiBase` — the deploy-managed assistant. See `docs/.../_lecturer/ai-assistant-setup.md` §2. |
| **Tutor rules + commands** (this repo) | `.continue/rules/`, `.continue/prompts/` | `00-tutor.md` (always on) + the active lab's generated `lab.md`; six `.prompt` slash-commands. |

## How to apply it

**Option A — open the folder (students, zero setup).** Open this `es201/` folder (or a lab
subfolder) in VS Code with the Continue extension installed. Continue auto-loads the
workspace `.continue/rules/*.md` and `.continue/prompts/*.prompt`. Type `/` in Continue chat
— you should see **hint, diff, explaindiff, whybroke, review, hobnext**.

**Option B — make it global (lab-PC image).** Symlink (or copy) the two folders into the
global dir so they apply in every workspace:

```bash
ln -s "$PWD/.continue/rules"   ~/.continue/rules-es201
ln -s "$PWD/.continue/prompts" ~/.continue/prompts-es201
# (or copy the files into ~/.continue/rules and ~/.continue/prompts)
```

**Per-lab rules** come from each lab's `lab.yaml` → `./lab-ai gen` renders
`<lab>/.continue/rules/lab.md`. Re-run `./lab-ai gen --all` after editing any `lab.yaml`.

**If the commands don't appear:** reload VS Code (`Ctrl/Cmd+Shift+P` → *Developer: Reload
Window*), confirm the Continue extension is active, and open the Continue config (gear icon)
to check for load errors. Continue's prompt/rules schema still shifts between versions —
verify against the [current docs](https://docs.continue.dev/customize/deep-dives/prompts)
if a key is ignored.

## SDK grounding (so the model doesn't invent APIs)

Build the Pico SDK symbol index once (needs `PICO_SDK_PATH`, or put the path in
`.ai/sdk.path`):

```bash
./lab-ai sdk        # -> .ai/sdk-index.json  (~1700 symbols: signature + #include + doc summary)
```

If `doxygen` is installed it's used automatically (signatures **and** doc summaries, ~3 s);
otherwise it falls back to header-parsing (signatures only). `pico-examples` is picked up
if it sits beside the SDK or via `PICO_EXAMPLES_PATH`.

Two ways it grounds the tutor, both off this one index:

- **`lab-ai bundle`** auto-injects the real signatures + summaries of the SDK functions in
  your code/question (for the copy-paste / external-model path), and **`lab-ai check
  <reply>`** lints an AI's proposed diff for calls that aren't in the SDK (catches invented
  APIs like `gpio_toggle`).
- **MCP tools (capable models only).** `.ai/pico-sdk-mcp.py` exposes `sdk_lookup(name)`
  (signature + `#include` + summary), `sdk_search(query)`, and `sdk_examples(name)` (real
  usages from pico-examples) so a tool-calling model (gpt-oss:120b, qwen3.6) pulls the facts
  itself. Register it in Continue (schema varies by version — verify against the
  [MCP docs](https://docs.continue.dev/customize/deep-dives/mcp)):

  ```yaml
  # in ~/.continue/config.yaml (or a .continue/mcpServers/ block)
  mcpServers:
    - name: pico-sdk
      command: python3
      args: ["<absolute path to>/src/es201/.ai/pico-sdk-mcp.py"]
  ```

  Then a model can call `sdk_lookup` mid-answer instead of guessing. Don't bother wiring
  this for the small 7B — it won't drive tool calls reliably; the `bundle` injection is the
  path for that tier.
