# Third-party licenses

This file records licenses for every third-party asset vendored under
`lib/coverage/src/template/`.

## Prism (syntax highlighter)

- Currently shipped as a **stub** at `src/template/prism.min.js`
  (Apache-2.0, TML project) that exposes the minimum API the reporter
  uses and falls back to HTML-escape. No syntax highlighting without
  the real Prism.
- To enable real highlighting, drop the real `prism.min.js` (≈14 KB
  min+gz, ≈60 KB raw) from <https://prismjs.com/> into this directory,
  overwriting the stub. When doing so, record here:
  - Source: <https://prismjs.com/download.html> with components `core`,
    `clike`, `cpp`, `c`, `rust`, `javascript`, `markup`
  - License: MIT © Lea Verou
  - SPDX: `MIT`

## TML Prism grammar

- `src/template/tml.prism.js` — hand-authored TML grammar definition
  for Prism. Authored by the TML project under Apache-2.0. No external
  license applies.

## Bundled fonts / icons

None at this time. If the reporter ever adopts a web font or icon
font, record the source + license here before it ships.
