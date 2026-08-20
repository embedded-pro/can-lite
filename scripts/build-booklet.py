#!/usr/bin/env python3
"""Build the can-lite design booklet as a PDF and/or a multi-page static site.

Chapter order is derived from documents/booklet/README.md: every
`[Title](target.md)` link found in that index is a chapter, in the order the
index lists them. Targets are resolved relative to documents/booklet/, so the
booklet can pull in documents that live elsewhere in the tree (the protocol
specification and the architecture document are included that way).

Diagrams are authored as ```mermaid fences inside the chapters. They render on
GitHub as-is; for the booklet each fence is compiled to an SVG with
mermaid-cli (mmdc) and replaced by an image reference. When mmdc is not
available the fence is left in place as a code block so the build still
succeeds.

Outputs (under build/booklet/):
    CanLiteDesign.pdf   single-file PDF, rendered by Pandoc + XeLaTeX
    site/               static site: index.html + one page per chapter
    book.md             the assembled single-document Markdown (PDF source)

Usage:
    python scripts/build-booklet.py [--format pdf|html|all] [--assemble-only]
                                    [--skip-diagrams]

Exit codes:
    0  success
    1  missing sources or a Pandoc render failed
    2  a required tool (pandoc) is not installed
"""

import argparse
import hashlib
import html
import pathlib
import re
import shutil
import subprocess
import sys
from datetime import date

ROOT = pathlib.Path(__file__).resolve().parent.parent
BOOKLET_DIR = ROOT / "documents" / "booklet"
INDEX = BOOKLET_DIR / "README.md"
ASSETS = BOOKLET_DIR / "assets"
REQUIREMENTS_DIR = ROOT / "documents" / "requirements"

BUILD_DIR = ROOT / "build" / "booklet"
SITE_DIR = BUILD_DIR / "site"
DIAGRAMS_DIR = BUILD_DIR / "diagrams"
GENERATED_DIR = BUILD_DIR / "generated"

OUTPUT_STEM = "CanLiteDesign"
BOOK_TITLE = "can-lite"
BOOK_SUBTITLE = "The Design Booklet"
REPO_BLOB = "https://github.com/embedded-pro/can-lite/blob/main"

LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)]+\.md)(#[^)]*)?\)")
H1_RE = re.compile(r"^#\s+(.+?)\s*$", re.MULTILINE)
HEADING_RE = re.compile(r"^(#{1,6})\s")
IMAGE_RE = re.compile(r"(!\[[^\]]*\]\()([^)]+)(\))")
MERMAID_RE = re.compile(r"```mermaid\n(.*?)```", re.DOTALL)
PART_RE = re.compile(r"^##\s+(.+?)\s*$")

REQUIREMENTS_SLUG = "requirements"

DIAGRAM_FAILURES = []


class Chapter:
    def __init__(self, number, title, source, part, page):
        self.number = number
        self.title = title
        self.source = source
        self.part = part
        self.page = page


def read_index():
    """Parse the booklet index into an ordered list of Chapter objects."""
    if not INDEX.is_file():
        print(f"ERROR: booklet index not found: {INDEX}", file=sys.stderr)
        return []

    part = ""
    seen = set()
    chapters = []

    for line in INDEX.read_text(encoding="utf-8").splitlines():
        part_match = PART_RE.match(line)
        if part_match:
            part = part_match.group(1)
            continue

        for _, target, _anchor in LINK_RE.findall(line):
            target = target.strip()
            if target.startswith(("http://", "https://")):
                continue

            source = (BOOKLET_DIR / target).resolve()
            if source in seen:
                continue
            if not source.is_file():
                print(f"WARNING: index links a missing file: {target}", file=sys.stderr)
                continue

            seen.add(source)
            number = len(chapters) + 1
            chapters.append(Chapter(number, title_of(source), source, part, page_name(source)))

    return chapters


def page_name(source):
    return f"{source.stem}.html"


def title_of(path):
    text = strip_front_matter(path.read_text(encoding="utf-8"))
    match = H1_RE.search(text)
    return match.group(1).strip() if match else path.stem.replace("-", " ").title()


def strip_front_matter(text):
    if not text.startswith("---"):
        return text
    end = text.find("\n---", 3)
    return text[end + 4:].lstrip("\n") if end != -1 else text


def has_tool(name):
    return shutil.which(name) is not None


def render_mermaid(code, skip_diagrams):
    """Compile one mermaid diagram to SVG; return its path, or None."""
    if skip_diagrams or not has_tool("mmdc"):
        return None

    DIAGRAMS_DIR.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha1(code.encode("utf-8")).hexdigest()[:12]
    svg_out = DIAGRAMS_DIR / f"diagram-{digest}.svg"
    if svg_out.exists():
        return svg_out

    mmd_in = DIAGRAMS_DIR / f"diagram-{digest}.mmd"
    mmd_in.write_text(code, encoding="utf-8")

    command = [
        "mmdc",
        "--input", str(mmd_in),
        "--output", str(svg_out),
        "--outputFormat", "svg",
        "--backgroundColor", "white",
        "--configFile", str(ASSETS / "mermaid.json"),
        "--puppeteerConfigFile", str(ASSETS / "puppeteer.json"),
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    mmd_in.unlink(missing_ok=True)

    if result.returncode != 0 or not svg_out.exists():
        first_line = next((line for line in result.stderr.splitlines() if "rror" in line), "")
        print(f"ERROR: mmdc failed for diagram {digest}: {first_line}", file=sys.stderr)
        if digest not in DIAGRAM_FAILURES:
            DIAGRAM_FAILURES.append(digest)
        return None

    print(f"  diagram-{digest}.svg")
    return svg_out


def replace_diagrams(text, skip_diagrams):
    def replace(match):
        code = match.group(1)
        svg = render_mermaid(code, skip_diagrams)
        if svg is None:
            return match.group(0)
        return f"![]({svg})"

    return MERMAID_RE.sub(replace, text)


def absolute_images(text, source):
    def replace(match):
        target = match.group(2).strip()
        if target.startswith(("http://", "https://", "/")):
            return match.group(0)
        return f"{match.group(1)}{(source.parent / target).resolve()}{match.group(3)}"

    return IMAGE_RE.sub(replace, text)


def rewrite_links(text, source, chapters, output):
    """Retarget cross-document Markdown links for the chosen output format."""
    by_source = {chapter.source: chapter for chapter in chapters}

    def replace(match):
        label, target, anchor = match.group(1), match.group(2).strip(), match.group(3) or ""
        if target.startswith(("http://", "https://")):
            return match.group(0)

        resolved = (source.parent / target).resolve()
        chapter = by_source.get(resolved)

        if chapter is not None:
            if output == "html":
                return f"[{label}]({chapter.page}{anchor})"
            return f"{label} (Chapter {chapter.number})"

        try:
            relative = resolved.relative_to(ROOT)
        except ValueError:
            return label
        return f"[{label}]({REPO_BLOB}/{relative}{anchor})"

    return LINK_RE.sub(replace, text)


def prepare(source, chapters, output, skip_diagrams):
    text = strip_front_matter(source.read_text(encoding="utf-8"))
    text = replace_diagrams(text, skip_diagrams)
    text = absolute_images(text, source)
    text = rewrite_links(text, source, chapters, output)
    return text.strip() + "\n"


def requirements_chapter():
    """Render documents/requirements/*.yaml into a generated appendix chapter."""
    try:
        import yaml
    except ImportError:
        print("WARNING: pyyaml not installed — requirements appendix skipped.", file=sys.stderr)
        return None

    files = sorted(REQUIREMENTS_DIR.glob("**/*.yaml"))
    if not files:
        return None

    lines = [
        "# Appendix A — Requirements Catalogue",
        "",
        "Generated from `documents/requirements/*.yaml` at build time. These are the",
        "normative requirements the design in this booklet implements; each section",
        "corresponds to one requirement file.",
        "",
    ]

    for path in files:
        entries = yaml.safe_load(path.read_text(encoding="utf-8"))
        if not entries:
            continue

        section = path.stem.replace("-", " ").replace("_", " ").title()
        lines += [f"## {section}", "", "| ID | Title | Shall |", "|----|-------|-------|"]
        for entry in entries:
            shall = " ".join(str(entry.get("shall", "")).split()).replace("|", r"\|")
            lines.append(f"| `{entry['id']}` | {entry.get('title', '')} | {shall} |")
        lines.append("")

    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    generated = GENERATED_DIR / f"{REQUIREMENTS_SLUG}.md"
    generated.write_text("\n".join(lines), encoding="utf-8")
    return generated


def git_version():
    try:
        result = subprocess.run(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=ROOT, capture_output=True, text=True, check=True,
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "dev"


def assemble(chapters, book_md, skip_diagrams):
    parts = []
    current_part = None

    for chapter in chapters:
        if chapter.part and chapter.part != current_part:
            current_part = chapter.part
            parts.append(f"\\part{{{latex_escape(chapter.part)}}}\n")
        parts.append(prepare(chapter.source, chapters, "pdf", skip_diagrams))

    book_md.parent.mkdir(parents=True, exist_ok=True)
    book_md.write_text("\n\n".join(parts), encoding="utf-8")
    print(f"Assembled {len(chapters)} chapters → {book_md}")


def latex_escape(text):
    for character, replacement in (("\\", r"\textbackslash{}"), ("&", r"\&"), ("%", r"\%"),
                                   ("$", r"\$"), ("#", r"\#"), ("_", r"\_"),
                                   ("{", r"\{"), ("}", r"\}")):
        text = text.replace(character, replacement)
    return text


def render_pdf(book_md):
    output = BUILD_DIR / f"{OUTPUT_STEM}.pdf"
    command = [
        "pandoc", str(book_md),
        "--metadata-file", str(ASSETS / "metadata.yaml"),
        "-M", f"date={date.today():%B %d, %Y}",
        "-M", f"version={git_version()}",
        "--from", "markdown+raw_tex",
        "--top-level-division=chapter",
        "--toc", "--toc-depth=2",
        "--pdf-engine=xelatex",
        "-H", str(ASSETS / "preamble.tex"),
        "--include-after-body", str(ASSETS / "back-cover.tex"),
        "-o", str(output),
    ]
    return run(command, output)


def sidebar(chapters, current):
    items = []
    part = None
    for chapter in chapters:
        if chapter.part and chapter.part != part:
            part = chapter.part
            items.append(f'<li class="part-label">{html.escape(part)}</li>')
        current_attr = ' aria-current="page"' if chapter is current else ""
        items.append(
            f'<li><a href="{chapter.page}"{current_attr}>'
            f"{chapter.number}. {html.escape(chapter.title)}</a></li>"
        )

    return (
        '<div class="layout">\n'
        '<nav class="sidebar">\n'
        f'<a class="brand" href="index.html">{html.escape(BOOK_TITLE)}</a>\n'
        f'<span class="brand-sub">{html.escape(BOOK_SUBTITLE)}</span>\n'
        "<ol>\n" + "\n".join(items) + "\n</ol>\n"
        f'<a class="download" href="{OUTPUT_STEM}.pdf">Download PDF</a>\n'
        "</nav>\n<main>\n"
    )


def pager(chapters, current):
    next_page = '<span class="spacer"></span>'
    index = chapters.index(current)

    if index > 0:
        earlier = chapters[index - 1]
        previous_page = f'<a href="{earlier.page}">← {html.escape(earlier.title)}</a>'
    else:
        previous_page = '<a href="index.html">← Contents</a>'

    if index + 1 < len(chapters):
        later = chapters[index + 1]
        next_page = f'<a href="{later.page}">{html.escape(later.title)} →</a>'

    return f'<div class="pager">{previous_page}{next_page}</div>\n</main>\n</div>\n'


def render_site(chapters, skip_diagrams):
    SITE_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ASSETS / "book.css", SITE_DIR / "book.css")

    if DIAGRAMS_DIR.exists():
        destination = SITE_DIR / "diagrams"
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(DIAGRAMS_DIR, destination)

    status = 0
    fragments = BUILD_DIR / "fragments"
    fragments.mkdir(parents=True, exist_ok=True)

    for chapter in chapters:
        markdown = prepare(chapter.source, chapters, "html", skip_diagrams)
        markdown = re.sub(rf"!\[\]\({re.escape(str(DIAGRAMS_DIR))}/", "![](diagrams/", markdown)
        markdown = re.sub(r"\A#\s+.+?\n", "", markdown, count=1)

        chapter_md = fragments / f"{chapter.source.stem}.md"
        chapter_md.write_text(markdown, encoding="utf-8")

        heading = f'<h1 class="title">{html.escape(chapter.title)}</h1>'
        before = fragments / f"{chapter.source.stem}-before.html"
        after = fragments / f"{chapter.source.stem}-after.html"
        before.write_text(sidebar(chapters, chapter) + heading + "\n", encoding="utf-8")
        after.write_text(pager(chapters, chapter), encoding="utf-8")

        command = [
            "pandoc", str(chapter_md),
            "--standalone",
            "--from", "markdown",
            "--toc", "--toc-depth=3",
            "--css", "book.css",
            "-M", f"pagetitle={chapter.title} — {BOOK_TITLE} {BOOK_SUBTITLE}",
            "-M", "document-css=false",
            "--include-before-body", str(before),
            "--include-after-body", str(after),
            "-o", str(SITE_DIR / chapter.page),
        ]
        status |= run(command, SITE_DIR / chapter.page, quiet=True)

    status |= render_landing(chapters)
    if status == 0:
        print(f"Wrote site with {len(chapters)} chapters → {SITE_DIR}")
    return status


def render_landing(chapters):
    """Write the landing page directly as HTML.

    Pandoc parses the contents of a <div> as Markdown, which would wrap the
    card links in a paragraph and collapse the grid, so this one page is
    emitted without going through Pandoc.
    """
    sections = []
    part = None
    for chapter in chapters:
        if chapter.part and chapter.part != part:
            if part is not None:
                sections.append("</div>")
            part = chapter.part
            sections.append(f"<h2>{html.escape(part)}</h2>")
            sections.append('<div class="landing-grid">')
        sections.append(
            f'<a href="{chapter.page}"><span class="num">Chapter {chapter.number}</span>'
            f"{html.escape(chapter.title)}</a>"
        )
    if part is not None:
        sections.append("</div>")

    title = f"{BOOK_TITLE} — {BOOK_SUBTITLE}"
    page = [
        "<!DOCTYPE html>",
        '<html lang="en">',
        "<head>",
        '<meta charset="utf-8">',
        '<meta name="viewport" content="width=device-width, initial-scale=1.0">',
        f"<title>{html.escape(title)}</title>",
        '<link rel="stylesheet" href="book.css">',
        "</head>",
        "<body>",
        sidebar(chapters, None),
        f'<h1 class="title">{html.escape(title)}</h1>',
        "<p>A layer-by-layer design reference for the can-lite protocol stack: class",
        "models, message flows, corner cases and the wire specification. The same",
        f'content is available as a single <a href="{OUTPUT_STEM}.pdf">PDF</a>.</p>',
        *sections,
        f'<p class="build-meta">Revision <code>{html.escape(git_version())}</code> · '
        f"built {date.today():%B %d, %Y}</p>",
        "</main>",
        "</div>",
        "</body>",
        "</html>",
        "",
    ]

    output = SITE_DIR / "index.html"
    output.write_text("\n".join(page), encoding="utf-8")
    return 0


def run(command, output, quiet=False):
    result = subprocess.run(command, cwd=ROOT)
    if result.returncode != 0:
        print(f"ERROR: pandoc failed for {output.name}", file=sys.stderr)
        return 1
    if not quiet:
        print(f"Wrote {output}")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Build the can-lite design booklet.")
    parser.add_argument("--format", choices=["pdf", "html", "all"], default="all")
    parser.add_argument("--assemble-only", action="store_true",
                        help="Write build/booklet/book.md only (no Pandoc).")
    parser.add_argument("--skip-diagrams", action="store_true",
                        help="Leave mermaid fences as code blocks instead of rendering them.")
    args = parser.parse_args()

    chapters = read_index()
    if not chapters:
        return 1

    appendix = requirements_chapter()
    if appendix is not None:
        chapters.append(Chapter(len(chapters) + 1, title_of(appendix), appendix,
                                "Appendices", page_name(appendix)))

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    book_md = BUILD_DIR / "book.md"

    if not args.skip_diagrams and not has_tool("mmdc"):
        print("WARNING: mmdc not found — diagrams are kept as code blocks.", file=sys.stderr)

    assemble(chapters, book_md, args.skip_diagrams)
    if args.assemble_only:
        return 0

    if not has_tool("pandoc"):
        print("ERROR: pandoc not found on PATH.", file=sys.stderr)
        return 2

    status = 0
    if args.format in ("pdf", "all"):
        status |= render_pdf(book_md)
    if args.format in ("html", "all"):
        status |= render_site(chapters, args.skip_diagrams)
        pdf = BUILD_DIR / f"{OUTPUT_STEM}.pdf"
        if pdf.is_file():
            shutil.copyfile(pdf, SITE_DIR / pdf.name)

    if DIAGRAM_FAILURES:
        print(f"ERROR: {len(DIAGRAM_FAILURES)} diagram(s) failed to render: "
              f"{', '.join(DIAGRAM_FAILURES)}", file=sys.stderr)
        status |= 1

    return 1 if status else 0


if __name__ == "__main__":
    raise SystemExit(main())
