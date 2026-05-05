# Game Design Documents

This directory contains structured planning documents for each game module. These documents serve as the **authoritative design source** for implementation and are maintained as living documents throughout development.

## Method

Each module (prospecting, excavation, farming, energy, etc.) gets its own subdirectory with a standard structure:

```
docs/design/<module-name>/
    README.md                    # Table of contents, cross-references, entry point
    <module>-master-design.md    # Organized master design (pipeline, mechanics, gaps)
    <specific-topic>.md          # Deep-dive documents for complex subsystems
```

### How It Works

1. **README.md is the entry point.** When working on a module, read its README first. It lists all planning documents, their status, and cross-references to code and other docs.

2. **Master design is the source of truth.** Contains the full organized design with pipeline stages, mechanics, control systems, and a gaps inventory marked with `[?]`. Gaps are filled incrementally as design decisions are made.

3. **Topic documents go deep.** Complex subsystems (e.g., confidence metrics, depth sampling) get their own file for focused design work.

4. **Status tracking.** Each document header shows:
   - `Status: STUB` — outline only, needs design work
   - `Status: DRAFT` — initial content written, needs review
   - `Status: REVIEWED` — reviewed and approved for implementation
   - `Status: IMPLEMENTED` — design has been coded, document is reference

5. **Cross-references.** Each README links to:
   - Related source files (`src/Unit/unit.cpp`, etc.)
   - Related roadmap entries (`ROADMAP_IMMINENT.md`)
   - Related design docs in other modules
   - Existing documentation (`Prospecting_Extraction_Mechanics.md`, etc.)

### Auto-Context Loading

CLAUDE.md contains instructions to read a module's README.md when working on that module's code. This ensures design context is loaded automatically at the start of relevant work sessions.

## Module Directories

| Module | Directory | Status |
|--------|-----------|--------|
| Prospecting | `prospecting/` | Active design |
| Research | `research/` | Stub — interface requirements from prospecting AI |
| AI Automation | `ai-automation/` | Stub — cross-cutting pattern for all unit AI trees |
| Excavation | — | Future |
| Beneficiation | — | Future |
| Operations | — | Future |
| Directives | — | Future |
| Farming | — | Future |
| Energy | — | Future |
| Manufacturing | — | Future |

## Relationship to Other Docs

| Document | Location | Role |
|----------|----------|------|
| `CLAUDE.md` | Project root | Session guidance, architecture overview, auto-context rules |
| `ROADMAP_IMMINENT.md` | Project root | Current sprint tasks and progress |
| `ROADMAP_OVERALL.md` | Project root | Phase-level roadmap |
| `Prospecting_Extraction_Mechanics.md` | Project root | Legacy v2 extraction design (comprehensive, pre-redesign) |
| `CONVENTIONS.md` | Project root | Coding style rules |
| `docs/` snapshots | `docs/` | Historical state snapshots and test reports |
