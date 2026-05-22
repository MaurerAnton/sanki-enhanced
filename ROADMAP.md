# Sanki-Enhanced Roadmap

## v0.2.0 — Terminal SRS (current)

- [x] Qt5 GUI for e-readers (Kobo/InkBox)
- [x] TUI interface (FTXUI) — 6 screens
- [x] Study modes: Random, Boxes, SM-2
- [x] `.apkg` deck import
- [x] GitHub CI: auto-build + signed Arch packages
- [x] Card browser with search
- [x] Card editor (add/delete/edit)
- [x] Pomodoro timer
- [x] SubSession from top reviewed cards
- [x] Due forecast for SM-2 sessions
- [x] Rust sync server (`sanki-sync/`)

---

## v0.3 — Stability & ARM

- [ ] Real-world `.apkg` stress testing
- [ ] Error handling: corrupted decks, missing files, invalid paths
- [ ] ARM (aarch64) CI builds
- [ ] Crash recovery (auto-save session state)
- [ ] `--help` / `--version` CLI flags
- [ ] Statistics screen (text-based charts: unicode bars, ascii graphs)
- [ ] SM-2 leech management UI (un-leech, ignore)

---

## v0.4 — Sync & Rich UX

- [ ] Anki sync from TUI (integrate `sanki-sync`)
  - Sync button in main menu
  - Progress bar during sync
  - Scheduled background sync
- [ ] Card tag support
  - Tag display in browser
  - Tag filter in session creation
- [ ] Import deck from URL (download `.apkg` with one command)
- [ ] Export session back to `.apkg`
- [ ] Dark theme for TUI
- [ ] Configurable keybindings (`~/.config/sanki/keybindings`)

---

## v0.5 — Media & Collaboration

- [ ] Image display in TUI (show file path / ASCII preview)
- [ ] Built-in `anki-attach` in TUI (open books via koreader)
- [ ] Shared deck support (import from AnkiWeb community)
- [ ] Multi-profile: separate settings per user
- [ ] Auto-detect deck language (for TTS/pronunciation)

---

## v0.6 — Advanced Scheduling

- [ ] FSRS (Free Spaced Repetition Scheduler) — SM-2 replacement
- [ ] A/B testing of review modes
- [ ] Adaptive intervals based on answer speed
- [ ] Forgetting curve graphs per session
- [ ] Desktop notifications ("time to review")

---

## v1.0 — Stable Release

- [ ] Feature parity GUI ↔ TUI
- [ ] Stable plugin API
- [ ] Optional web interface (based on sync server)
- [ ] Full Anki profile migration (settings, stats, media)
- [ ] Documentation: man pages, wiki, video tutorials
- [ ] AUR package (`sanki-enhanced`)
- [ ] Debian/Ubuntu `.deb` package

---

## Backlog (ideas)

- Mobile companion (Termux on Android)
- Language Reactor / Netflix subtitle → flashcard pipeline
- Speech recognition for pronunciation checking
- Collaborative decks (real-time sync via CRDT)
- AI-powered card generation from text
- Neovim / VS Code plugin
