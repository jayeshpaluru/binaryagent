# Agent Instructions For binagent

You are generating input for `binagent`.
Treat this file as the only contract.
Do not rely on source code behavior.

## Primary Goal

Return binary that decodes to one complete standalone HTML document with inline CSS and JS.

## Output Contract (Strict, Machine-Checkable)

Return exactly one line of output.
That line must contain only 8-bit binary tokens separated by single spaces.

Required format:
- Token rule: each token length is exactly 8.
- Character rule: each token contains only `0` or `1`.
- Separator rule: tokens are separated by exactly one ASCII space (`0x20`).
- Line rule: output must be a single line (no newline within content).
- Boundary rule: no leading or trailing whitespace.

Hard rejects:
- Any non-binary character.
- Any token not exactly 8 chars.
- Tabs, multiple spaces, or newline-separated tokens.
- Any text before or after the binary stream.
- Markdown, code fences, comments, or explanations.

## Decoded HTML Contract (Required Features)

The decoded bytes must form a valid, complete HTML document and include all items below.

Required structure:
- `<!doctype html>`
- `<html>`, `<head>`, and `<body>`
- At least one `<style>` block
- At least one `<script>` block

Required layout and typography:
- Use `display: grid` or `display: flex` for intentional section layout.
- Include at least 2 meaningful sections/cards/panels.
- Define `font-family` and at least one typography treatment (`font-size`, `line-height`, or `letter-spacing`).

Required visual depth (at least 2 distinct techniques):
- gradient (`linear-gradient` or `radial-gradient`)
- `box-shadow`
- `backdrop-filter`
- `clip-path`

Required motion cues (at least 1):
- `animation` with `@keyframes`
- `transition`
- `transform`

Required interaction cues:
- At least one interactive control (`button` or `input`).
- At least one hover/focus style OR one JS event handler (`addEventListener`, `onclick`, etc.).

## Variability Contract (Across Runs)

Two runs should be visibly different.
Do not reuse the same template.

Minimum differences between two runs:
- Different color palette.
- Different layout composition (not just spacing tweaks).
- Different heading/label copy (at least 30% of visible text changed).

## Canonical Prompt (Use When Needed)

"Output ONLY strict binary bytes for a complete standalone HTML file with inline CSS and JS.
Rules:
- output must be a single line of tokens separated by single spaces
- each token must be exactly 8 bits, using only 0 or 1
- no leading/trailing whitespace, no markdown, no code fences, no explanations, no extra text
- decoded HTML must include doctype/html/head/body, inline style and script, intentional layout, typography treatment
- include at least two visual depth techniques, at least one motion cue, and at least one interaction via hover/focus or JS event
- produce a visually different design direction from prior runs (palette, layout, and copy)
Return only the binary stream."

## Quick Format Examples

Valid shape example:
- `00111100 01101000 01110100 01101101 01101100`

Invalid examples:
- `101 01010101` (token not 8 bits)
- `00111100\n01101000` (newline in stream)
- `Here is your output: 00111100 ...` (extra text)

## Commands To Materialize HTML

File input:

```bash
./binagent materialize --binary-in agent_output.binary.txt --html-out run.html
```

Stdin input:

```bash
./binagent materialize --html-out run.html < agent_output.binary.txt
```

Optional explicit binary copy + html:

```bash
./binagent materialize --binary-in agent_output.binary.txt --binary-out run.binary.txt --html-out run.html
```

Open generated HTML (macOS):

```bash
open run.html
```
