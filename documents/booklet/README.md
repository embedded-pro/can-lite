# can-lite — The Design Booklet

This is the design reference for **can-lite**, a zero-heap CAN 2.0B client-server
protocol library for bare-metal embedded systems. It walks the stack from the
HAL up to the application categories, one layer per chapter, with class
diagrams, message-flow diagrams, the corner cases each layer has to survive, and
the normative wire specification at the back.

The booklet is written for three readers:

- **Integrators** wiring can-lite into a product — start at Chapters 1–3, then
  read the layer that carries your traffic.
- **Category authors** adding application-specific messages — Chapters 6, 12 and
  the specification in Part V.
- **Maintainers** changing the protocol core — the whole of Parts II and IV, and
  the requirements catalogue in the appendix.

Every diagram in this booklet is generated from a `mermaid` fence in the chapter
source, and every class listing is transcribed from the header it documents, so
the booklet moves when the code moves.

## Part I — Foundations

| # | Chapter | What it covers |
|---|---------|----------------|
| 1 | [Introduction and Scope](01-introduction.md) | What can-lite is, what it deliberately is not, and the constraints that shaped it |
| 2 | [The Layered Architecture](02-layered-architecture.md) | The six layers, their dependency rules, ownership and composition |
| 3 | [Embedded Foundations](03-embedded-foundations.md) | Bounded containers, `WithStorage`, observers, timers and the event-driven execution model |

## Part II — The Layers

| # | Chapter | What it covers |
|---|---------|----------------|
| 4 | [HAL and Bus Drivers](04-hal-and-drivers.md) | `hal::Can`, `CanBusAdapter`, the host adapters and the virtual bus |
| 5 | [The Core Layer](05-core-layer.md) | Identifier layout, `CanFrameTransport`, `CanPayload`, `CanFrameCodec` |
| 6 | [The Category Layer](06-category-layer.md) | `CanCategory`, message-type dispatch, sequence policy, acknowledgement |
| 7 | [The Protocol Layer: Server](07-protocol-server.md) | Receive pipeline, rate limiting, sequence validation, heartbeat, liveness |
| 8 | [The Protocol Layer: Client](08-protocol-client.md) | Per-server state, sequence source, ack tracking, resynchronisation, discovery |
| 9 | [The Transport Layer: ISO-TP](09-transport-isotp.md) | ISO 15765-2 segmentation, the sender and receiver state machines, channels |

## Part III — Categories

| # | Chapter | What it covers |
|---|---------|----------------|
| 10 | [The System Category](10-system-category.md) | Heartbeat, acknowledgement, status request and category discovery |
| 11 | [The Firmware Upgrade Category](11-firmware-upgrade-category.md) | Session model, block transfer, verification and activation |
| 12 | [Authoring a Category](12-authoring-a-category.md) | Building an application category end to end, with the demo category as the worked example |

## Part IV — Behaviour Under Stress

| # | Chapter | What it covers |
|---|---------|----------------|
| 13 | [Corner Cases and Failure Modes](13-corner-cases.md) | The catalogue of edge conditions, per layer, with the observable outcome of each |
| 14 | [Timing, Memory and Bus Budget](14-timing-and-resources.md) | Static footprint, timer inventory, timing parameters and bus-load arithmetic |
| 15 | [Verification Strategy](15-verification.md) | Unit tests, BDD integration tests, the virtual bus and requirement traceability |

## Part V — Reference Documents

| # | Chapter | What it covers |
|---|---------|----------------|
| 16 | [Architecture and Design Decisions](../design/architecture.md) | The living architecture record |
| 17 | [Extending can-lite with Categories](../design/extending-categories.md) | The category authoring guide |
| 18 | [Protocol Specification](../spec/can-protocol.md) | Normative wire format |
| 19 | [Firmware Upgrade Specification](../spec/firmware-upgrade.md) | Normative firmware upgrade category |

## Appendices

| # | Chapter | What it covers |
|---|---------|----------------|
| 20 | [Glossary](20-glossary.md) | Terms, abbreviations and the constants they map to |

Appendix A, the requirements catalogue, is generated at build time from
`documents/requirements/*.yaml` and appended after the glossary.

## Building the booklet

```bash
pip install pyyaml
npm install -g @mermaid-js/mermaid-cli          # diagram rendering
sudo apt-get install -y pandoc texlive-xetex \
    texlive-latex-extra texlive-fonts-recommended lmodern librsvg2-bin

python scripts/build-booklet.py --format all
```

Outputs land in `build/booklet/`:

| Path | Content |
|------|---------|
| `CanLiteDesign.pdf` | The complete booklet as a single PDF |
| `site/index.html` | The static site: landing page plus one page per chapter |
| `book.md` | The assembled Markdown the PDF is rendered from |

`--format pdf` and `--format html` build one output only; `--skip-diagrams`
leaves the mermaid fences as code blocks (useful when mermaid-cli is not
installed); `--assemble-only` stops after writing `book.md`.

CI builds both outputs on every pull request that touches the booklet, publishes
the site to GitHub Pages on `main`, and attaches the PDF to every published
release.

## Conventions used in this booklet

| Convention | Meaning |
|------------|---------|
| `services::Name` | A type in the library; the `services` namespace is implied and usually dropped in prose |
| REQ-CAN-nnn | A requirement from `documents/requirements/can-protocol.yaml`, listed in Appendix A |
| Chapter *n* | A cross-reference; in the PDF the chapter number is authoritative, on the site the same reference is a link |
| Code excerpts | Transcribed from the named header or source file, shortened where marked with `...` |
