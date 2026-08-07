# Continue editor rules (short on purpose)

The full tutor behaviour — persona, verification gate, platform rules — is the **model's**
system prompt (`.ai/system-prompt.md`, set on the OWUI Lab Tutor model / injected by the proxy).
These Continue rules stay short so they don't stack with it and drown a small model.

- Use the active file first.
- Prefer minimal unified diffs; don't rewrite whole files unless asked.
- Do not invent Pico SDK APIs — if a symbol isn't in the active file or retrieved context, say so.
- Use GP16/GP17 for lab LEDs unless the active file says otherwise.
- `sleep_ms()` is always blocking.
