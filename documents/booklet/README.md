# can-lite — The Design Booklet

The design view of **can-lite**, a zero-heap CAN 2.0B client-server protocol
library for bare-metal embedded systems. Parts I to III walk the stack one layer
at a time — composition, pipelines, state machines, corner cases, budgets — and
Part IV reproduces the project's normative documents, so design and
specification are one document.

The booklet does not restate what those documents own. Wire format, message
catalogues and encoding belong to the specifications; architecture decisions and
the category authoring guide belong to the design documents; requirements belong
to the requirements files. Chapters link to them rather than copying them, and
no chapter contains source code.

## Part I — Orientation

| # | Chapter                                        | What it covers                                                      |
|---|------------------------------------------------|---------------------------------------------------------------------|
| 1 | [About This Booklet](01-about-this-booklet.md) | What the booklet adds, what the other documents own, how to read it |

## Part II — The Layers in Depth

| # | Chapter                                                               | What it covers                                                                                               |
|---|-----------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| 2 | [The Layer Map](02-layer-map.md)                                      | Composition and ownership, where the layering bends, the build graph, a frame's journey                      |
| 3 | [HAL and Bus Drivers](03-hal-and-drivers.md)                          | What the library requires of a bus, the host adapters, the virtual bus, driver assumptions                   |
| 4 | [The Core Layer: Frame Flow](04-core-frame-flow.md)                   | Routing by identifier, the outbound queue, payload validity, number representation                           |
| 5 | [Category Dispatch and Sequence Policy](05-category-dispatch.md)      | Dispatch outcomes, the segmented path, and the contract the two halves of a category must keep               |
| 6 | [The Server: Receive Pipeline](06-server-pipeline.md)                 | Every gate a received frame passes, why some rejections are silent, sequence validation, liveness, heartbeat |
| 7 | [The Client: Per-Server State](07-client-state.md)                    | The two state tables, sequence supply, resynchronisation, outstanding commands, server tracking, discovery   |
| 8 | [Segmentation: The ISO-TP State Machines](08-isotp-state-machines.md) | Channels, the sender and receiver machines, aborts, and the deliberate omissions                             |
| 9 | [The Built-in Categories: Design Rationale](09-builtin-categories.md) | Why the system category is a category, and why the firmware upgrade category keeps no state                  |

## Part III — Behaviour Under Stress

| #  | Chapter                                                     | What it covers                                                                             |
|----|-------------------------------------------------------------|--------------------------------------------------------------------------------------------|
| 10 | [Corner Cases and Failure Modes](10-corner-cases.md)        | The catalogue: condition, mechanism, observable outcome, per layer                         |
| 11 | [Timing, Memory and Bus Budget](11-timing-and-resources.md) | Timer inventory, how the parameters constrain each other, static footprint, bus arithmetic |
| 12 | [Verification Strategy](12-verification.md)                 | How the three levels relate, and where verification stops                                  |

## Part IV — Reference Documents

| #  | Chapter                                                                 | What it covers                      |
|----|-------------------------------------------------------------------------|-------------------------------------|
| 13 | [Architecture and Design Decisions](../design/architecture.md)          | The living architecture record      |
| 14 | [Extending can-lite with Categories](../design/extending-categories.md) | The category authoring guide        |
| 15 | [Protocol Specification](../spec/can-protocol.md)                       | Normative wire format               |
| 16 | [Firmware Upgrade Specification](../spec/firmware-upgrade.md)           | Normative firmware upgrade category |

## Appendices

| #  | Chapter                    | What it covers                   |
|----|----------------------------|----------------------------------|
| 17 | [Glossary](20-glossary.md) | The vocabulary this booklet adds |

Appendix A, the requirements catalogue, is generated at build time from
`documents/requirements/*.yaml` and appended after the glossary.

## Building the booklet

Install the dependencies used by CI (PyYAML, mermaid-cli, Pandoc, and a XeLaTeX toolchain), then build both outputs with `python scripts/build-booklet.py --format all`.

Outputs land in `build/booklet/`:

| Path                | Content                                                 |
|---------------------|---------------------------------------------------------|
| `CanLiteDesign.pdf` | The complete booklet as a single PDF                    |
| `site/index.html`   | The static site: landing page plus one page per chapter |
| `book.md`           | The assembled Markdown the PDF is rendered from         |

`--format pdf` and `--format html` build one output; `--skip-diagrams` leaves
the diagram sources as code blocks when mermaid-cli is unavailable;
`--assemble-only` stops after writing `book.md`.

CI builds both outputs on every pull request that touches the documents,
publishes the site to GitHub Pages on `main`, and attaches the PDF to every
published release.

## Adding or changing a chapter

1. Check first that the material does not belong to an existing document. Wire
   format goes to `documents/spec/`, architecture decisions to
   `documents/design/architecture.md`, authoring guidance to
   `documents/design/extending-categories.md`, requirements to
   `documents/requirements/`. The booklet adds the layer narrative, the
   diagrams, the corner cases and the budgets — and links to the rest.
2. Create the file with a single top-level heading and its own section
   numbering (`## 1.`, `## 2.`); chapters are numbered by the renderer.
3. Link it from the table above, under the right part — the index drives both
   the PDF and the site.
4. Author diagrams as ```` ```mermaid ```` fences so they render on GitHub as
   well as in the booklet. A fence that fails to parse fails the build; avoid
   `;` inside sequence-diagram notes and `{}` inside class-diagram members.
5. Keep source code out. Name the components and describe the behaviour.
