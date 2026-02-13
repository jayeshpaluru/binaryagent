# binagent (C)

`binagent` takes binary text from an AI agent and materializes two files:
- the raw binary text exactly as the agent produced it
- the decoded HTML generated from that binary

The tool does not design UI or inject templates. The agent controls the HTML structure.

## Why this exists

Many AI tools return text. This project enforces a strict contract where the model returns only binary bytes, and the C program converts that into a viewable HTML file.

Because AI outputs are probabilistic, two runs can and should produce different HTML structures, layouts, and behavior, even with similar prompts.

## Binary format contract

The agent output must be plain text where:
- each token is exactly 8 bits
- only `0` and `1` are allowed
- tokens are separated by whitespace
- no markdown, no prose, no code fences

If the agent emits HTML directly (instead of binary tokens), `binagent` rejects it.

## Program quality contract (enforced)

After decoding, `binagent` enforces these rules before writing the HTML file:
- output must look like HTML (`<html>` or `<!doctype html>`)
- output must include CSS styling (`<style>` block or `style=` attributes)
- output must be rich enough to resemble a creative UI (not a tiny/plain page)
- output must show intentional layout structure (`grid`/`flex`/`main`/`section`)
- output must include typography intent (`font-family` plus `letter-spacing` or `clamp()` or similar)
- output must include at least two visual depth cues (for example: `gradient`, `box-shadow`, `backdrop-filter`, `clip-path`)
- output must include motion cues (`animation`, `@keyframes`, `transition`, or `transform`)
- output must include interaction cues (for example: controls, hover states, or JS event handlers)

This keeps the flow strict: `binary -> program -> html` and prevents low-effort/plain outputs.

Example token sequence:

```text
00111100 01101000 01110100 01101101 01101100 00111110
```

## Build

```bash
make
```

## Usage

```bash
./binagent materialize [--binary-in FILE|-] [--html-out FILE] [--binary-out FILE]
```

Defaults:
- `--html-out output.html`
- if `--binary-in` is omitted, input is read from `stdin`
- if input is `stdin` and `--binary-out` is omitted, binary is saved to `output.binary.txt`
- if input is a file and `--binary-out` is omitted, no extra binary copy is written

## Typical workflow

1. Ask your AI agent for strict binary output only.
2. Save the response as a file (or pipe it directly).
3. Run `materialize`.
4. Open the generated HTML.

This is intentionally one-way for agent output: `binary -> program -> html`.
Do not feed generated HTML back to the agent as input for this tool.

### File input

```bash
./binagent materialize \
  --binary-in agent_output.binary.txt \
  --html-out run.html
```

### Stdin input

```bash
./binagent materialize \
  --html-out run.html < agent_output.binary.txt
```

### Save an explicit binary copy (optional)

```bash
./binagent materialize \
  --binary-in agent_output.binary.txt \
  --binary-out run.binary.txt \
  --html-out run.html
```

## Recommended agent prompt

```text
Output ONLY strict binary bytes for a complete standalone HTML file with inline CSS and JS.
Design intent: produce a bold, creative, high-contrast UI concept that feels art-directed, not boilerplate.
Rules:
- each token must be exactly 8 bits, using only 0 or 1
- tokens must be separated by spaces
- no markdown, no code fences, no explanations, no extra text
- decoded HTML must include:
  - intentional layout composition (grid or flex)
  - expressive typography (custom font-family + sizing/letter-spacing treatment)
  - at least two visual depth treatments (e.g. gradient + box-shadow/backdrop-filter/clip-path)
  - motion (animation/keyframes/transition/transform)
  - interactivity (button/input/hover or JS event handlers)
Two different runs should produce visibly different UI structure and styling direction.
Return only the binary stream.
```

## Smoke test

```bash
./scripts/smoke.sh
```

## Repository layout

- `src/main.c`: binary parser + materialization command
- `Makefile`: build target
- `scripts/smoke.sh`: quick end-to-end verification
