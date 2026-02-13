# Agent Instructions For binagent

You are generating input for `binagent`.
Only use this file as the contract; do not rely on reading source code.

## Output contract (strict)

Return ONLY strict binary text for a complete standalone HTML file.

Rules:
- each token must be exactly 8 bits
- tokens may contain only `0` and `1`
- tokens are separated by spaces
- no markdown
- no code fences
- no explanations
- no extra text before or after the binary stream

If you return non-binary text, the program will reject it.

## Creative UI contract (required)

The decoded HTML must include all of the following:
- HTML document structure (`<!doctype html>` and `<html>...`)
- CSS styling (`<style>` block or `style=` attributes)
- intentional layout composition (grid/flex and meaningful sections)
- typography design (`font-family` plus size/spacing treatment)
- at least two visual depth treatments (e.g. gradient, box-shadow, backdrop-filter, clip-path)
- motion cues (`animation`/`@keyframes`/`transition`/`transform`)
- interaction cues (button/input/hover or JS event handlers)

## Variability expectation

Outputs should be probabilistic and diverse.
Two runs should look different in structure and visual direction.
Avoid repeating the same template.

## Recommended prompt shape

Use this exactly when needed:

"Output ONLY strict binary bytes for a complete standalone HTML file with inline CSS and JS.
Rules:
- each token must be exactly 8 bits, using only 0 or 1
- tokens must be separated by spaces
- no markdown, no code fences, no explanations, no extra text
- decoded HTML must include layout, typography, at least two visual depth treatments, motion, and interaction
Two different runs should produce visibly different UI structure and styling direction.
Return only the binary stream."

## Commands To Materialize HTML

After the agent returns binary, run one of these commands:

File input (no extra binary copy):

```bash
./binagent materialize --binary-in agent_output.binary.txt --html-out run.html
```

Stdin input (saves binary + html):

```bash
./binagent materialize --html-out run.html < agent_output.binary.txt
```

Optional explicit binary copy + html:

```bash
./binagent materialize --binary-in agent_output.binary.txt --binary-out run.binary.txt --html-out run.html
```

Open the generated HTML (macOS):

```bash
open run.html
```
