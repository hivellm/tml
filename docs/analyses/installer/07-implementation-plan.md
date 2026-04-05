# Implementation Plan — Phased Approach

## Phase 0: Release Build (Prerequisites)

**Goal:** Ensure `scripts/build.bat release` and `scripts/build.sh release` produce clean, stripped, production-ready binaries.

- [ ] 0.1 Verify release build works on Windows (`scripts/build.bat release --pack`)
- [ ] 0.2 Verify release build works on Linux (`scripts/build.sh release`)
- [ ] 0.3 Strip debug symbols from release binaries (Windows: `/DEBUG:NONE`, Linux: `strip`)
- [ ] 0.4 Measure release binary sizes (target: MSI < 60 MB)
- [ ] 0.5 Verify plugin packing produces valid manifest.json with SHA256 hashes
- [ ] 0.6 Test that packed plugins load correctly at runtime
- [ ] 0.7 Create `scripts/stage-release.bat` / `stage-release.sh` — copies all distributable files to `staging/` directory

## Phase 1: Windows MSI Installer

**Goal:** `TML-0.2.9-x64.msi` that installs TML, sets PATH, and allows `tml --version` from any terminal.

- [ ] 1.1 Create `installer/wix/` directory structure
- [ ] 1.2 Write `Product.wxs` — product definition, upgrade code, media template
- [ ] 1.3 Write `Directories.wxs` — install directory layout
- [ ] 1.4 Write `harvest-files.bat` — auto-generate `Components.wxs` from staging directory (using `wix heat`)
- [ ] 1.5 Write `Features.wxs` — Core (required), StdLib (required), Docs (optional)
- [ ] 1.6 Implement PATH environment variable addition (`Environment` table)
- [ ] 1.7 Add `TML_HOME` environment variable pointing to lib directory
- [ ] 1.8 Create installer assets: icon (.ico), banner (493x58 .bmp), dialog (493x312 .bmp)
- [ ] 1.9 Add license dialog (Apache 2.0 in RTF format)
- [ ] 1.10 Write `build-msi.bat` — end-to-end: stage → harvest → build MSI
- [ ] 1.11 Test: fresh install → `tml --version` in new terminal → works
- [ ] 1.12 Test: upgrade install (0.2.8 → 0.2.9) → preserves PATH, replaces files
- [ ] 1.13 Test: uninstall → removes files, removes PATH entry
- [ ] 1.14 Test: silent install (`msiexec /i ... /quiet`)
- [ ] 1.15 Test: custom install directory

## Phase 2: Linux Packages

**Goal:** `.tar.gz` with install script, `.deb`, `.rpm` — all working.

- [ ] 2.1 Create `installer/linux/` directory
- [ ] 2.2 Write `install.sh` — universal install script (PREFIX-based)
- [ ] 2.3 Write `uninstall.sh`
- [ ] 2.4 Write `nfpm.yaml` — package config for .deb and .rpm
- [ ] 2.5 Write `postinstall.sh` — set TML_HOME, print usage instructions
- [ ] 2.6 Write `preremove.sh` — clean up generated caches
- [ ] 2.7 Write `build-linux-packages.sh` — builds tarball + .deb + .rpm
- [ ] 2.8 Decide: bundle .so dependencies or rely on system packages
- [ ] 2.9 If bundling: set RPATH on binaries (`$ORIGIN/../lib/tml/deps`)
- [ ] 2.10 Create `tml.1` man page
- [ ] 2.11 Test on Ubuntu 22.04: `sudo dpkg -i tml.deb` → `tml --version`
- [ ] 2.12 Test on Fedora: `sudo rpm -i tml.rpm` → `tml --version`
- [ ] 2.13 Test tarball: `./install.sh ~/.local` → `tml --version`

## Phase 3: macOS Package

**Goal:** Signed, notarized `.pkg` that installs cleanly on macOS.

- [ ] 3.1 Create `installer/macos/` directory
- [ ] 3.2 Write `Distribution.xml` for productbuild
- [ ] 3.3 Write entitlements file (JIT + library loading)
- [ ] 3.4 Write `build-macos-pkg.sh` — pkgbuild + productbuild
- [ ] 3.5 Build for arm64 (Apple Silicon)
- [ ] 3.6 Build for x86_64 (Intel)
- [ ] 3.7 Create universal binaries with `lipo`
- [ ] 3.8 Fix dylib install names with `install_name_tool`
- [ ] 3.9 Test unsigned .pkg on macOS (manual Gatekeeper override)
- [ ] 3.10 Write Homebrew formula (`tml.rb`)
- [ ] 3.11 Test Homebrew tap: `brew tap hivellm/tml && brew install tml`

## Phase 4: Code Signing

**Goal:** All distributed binaries are signed and trusted by OS.

- [ ] 4.1 Set up Azure Trusted Signing account (Windows)
- [ ] 4.2 Set up Apple Developer account (macOS)
- [ ] 4.3 Generate GPG key pair for Linux package signing
- [ ] 4.4 Write `scripts/sign-windows.bat` — signs all EXE/DLL + MSI
- [ ] 4.5 Write `scripts/sign-macos.sh` — codesign + notarize + staple
- [ ] 4.6 Write `scripts/sign-linux.sh` — GPG sign packages + checksums
- [ ] 4.7 Test Windows: no SmartScreen warning on fresh download
- [ ] 4.8 Test macOS: no Gatekeeper warning, notarization passes
- [ ] 4.9 Test Linux: `dpkg-sig --verify` succeeds

## Phase 5: CI/CD Pipeline

**Goal:** `git tag v0.2.9 && git push --tags` produces signed installers for all platforms.

- [ ] 5.1 Create `.github/workflows/release.yml`
- [ ] 5.2 Configure GitHub Secrets (Azure, Apple, GPG)
- [ ] 5.3 Windows job: build → sign → MSI → sign MSI → upload
- [ ] 5.4 Linux x64 job: build → strip → .deb/.rpm/.tar.gz → sign → upload
- [ ] 5.5 Linux arm64 job: same as x64 on ARM runner
- [ ] 5.6 macOS job: build → universal binary → sign → notarize → .pkg → upload
- [ ] 5.7 Release job: download all artifacts → create draft GitHub release
- [ ] 5.8 Test: push tag → all jobs green → draft release with all artifacts
- [ ] 5.9 Document release process in `docs/dev/releasing.md`

## Phase 6: Package Managers (Post-Launch)

**Goal:** `winget install tml`, `brew install tml`, `apt install tml`.

- [ ] 6.1 Submit Winget manifest to `microsoft/winget-pkgs`
- [ ] 6.2 Submit Homebrew formula to `homebrew/homebrew-core` (or maintain tap)
- [ ] 6.3 Set up APT repository (S3 + CloudFront)
- [ ] 6.4 Set up DNF/YUM repository
- [ ] 6.5 Create Chocolatey package
- [ ] 6.6 Automate package manager updates on release

## Priority & Dependencies

```
Phase 0 (release build) ──────────────────────────┐
  │                                                │
  ├── Phase 1 (Windows MSI) ─┐                     │
  ├── Phase 2 (Linux pkgs) ──┼── Phase 4 (signing) ┤
  └── Phase 3 (macOS pkg) ───┘                     │
                                                    │
                              Phase 5 (CI/CD) ──────┘
                                    │
                              Phase 6 (pkg managers)
```

**Phases 1-3 can be done in parallel.** Phase 4 requires accounts/certificates. Phase 5 integrates everything. Phase 6 is post-launch.

## Estimated Effort

| Phase | Effort | Blocking? |
|-------|--------|-----------|
| Phase 0 | 1-2 days | Yes — everything depends on clean release build |
| Phase 1 | 2-3 days | No — can parallelize with 2 & 3 |
| Phase 2 | 1-2 days | No |
| Phase 3 | 2-3 days | No — macOS-specific tooling is fiddly |
| Phase 4 | 1-2 days | Blocked by account approvals (Azure: hours, Apple: days) |
| Phase 5 | 2-3 days | Blocked by Phase 4 (needs signing secrets) |
| Phase 6 | 1-2 days | Blocked by Phase 5 (needs working CI) |
| **Total** | **~10-15 days** | |
