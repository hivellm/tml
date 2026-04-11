# CI/CD Release Pipeline

## Overview

GitHub Actions workflow that builds, signs, packages, and publishes TML for all platforms on every git tag (`v*`).

```
git tag v0.2.9 → push tag → GitHub Actions:
  ├── build-windows (x64) → sign → MSI → sign MSI → upload
  ├── build-linux (x64, arm64) → .deb + .rpm + .tar.gz → sign → upload
  ├── build-macos (arm64, x64) → sign → notarize → .pkg → upload
  └── create-release → attach all artifacts → publish
```

## Workflow: `.github/workflows/release.yml`

```yaml
name: Release

on:
  push:
    tags: ['v*']

permissions:
  contents: write   # Create releases
  id-token: write   # Azure OIDC

env:
  TML_VERSION: ${{ github.ref_name }}  # e.g., "v0.2.9"

jobs:
  # ============================================================
  # Windows x64
  # ============================================================
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup build tools
        run: |
          choco install wixtoolset
          dotnet tool install --global AzureSignTool

      - name: Build release
        shell: cmd
        run: scripts\build.bat release --pack

      - name: Sign binaries
        env:
          AZURE_VAULT_URL: ${{ secrets.AZURE_SIGNING_VAULT_URL }}
          AZURE_CERT_NAME: ${{ secrets.AZURE_SIGNING_CERT_NAME }}
        run: |
          AzureSignTool sign -kvu $env:AZURE_VAULT_URL -kvc $env:AZURE_CERT_NAME `
            -fd sha256 -td sha256 -tr http://timestamp.digicert.com `
            build/release/bin/tml.exe `
            build/release/bin/tml_mcp.exe `
            build/release/bin/tml_daemon.exe `
            build/release/bin/plugins/*.dll

      - name: Stage installer files
        run: |
          mkdir staging
          xcopy /E build\release\bin staging\bin\
          xcopy /E lib\core staging\lib\core\
          xcopy /E lib\std staging\lib\std\
          xcopy /E lib\test staging\lib\test\
          copy LICENSE staging\
          copy CHANGELOG.md staging\

      - name: Build MSI
        shell: cmd
        run: |
          wix build -o TML-%TML_VERSION%-x64.msi ^
            -d ProductVersion=%TML_VERSION:~1% ^
            -d StagingDir=staging ^
            installer/wix/Product.wxs

      - name: Sign MSI
        run: |
          AzureSignTool sign -kvu $env:AZURE_VAULT_URL -kvc $env:AZURE_CERT_NAME `
            -fd sha256 -td sha256 -tr http://timestamp.digicert.com `
            TML-$env:TML_VERSION-x64.msi

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: windows-x64
          path: TML-*.msi

  # ============================================================
  # Linux x64 + arm64
  # ============================================================
  build-linux:
    strategy:
      matrix:
        include:
          - arch: x64
            runner: ubuntu-latest
            triple: x86_64-unknown-linux-gnu
          - arch: arm64
            runner: ubuntu-24.04-arm
            triple: aarch64-unknown-linux-gnu
    runs-on: ${{ matrix.runner }}
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake ninja-build \
            libssl-dev libsqlite3-dev zlib1g-dev libzstd-dev libbrotli-dev

      - name: Install nfpm
        run: |
          curl -sfL https://install.goreleaser.com/github.com/goreleaser/nfpm.sh | sh -s -- -b /usr/local/bin

      - name: Build release
        run: scripts/build.sh release

      - name: Strip binaries
        run: |
          strip build/release/bin/tml
          strip build/release/bin/tml-mcp
          strip build/release/bin/tml-daemon
          strip build/release/bin/plugins/*.so

      - name: Build packages
        run: |
          # Tarball
          mkdir -p staging/{bin/plugins,lib,share/doc/tml,share/man/man1}
          cp build/release/bin/tml build/release/bin/tml-mcp build/release/bin/tml-daemon staging/bin/
          cp build/release/bin/plugins/*.so staging/bin/plugins/
          cp -r lib/core lib/std lib/test staging/lib/
          cp -r docs/examples staging/share/doc/tml/
          cp LICENSE staging/
          cp installer/linux/install.sh staging/
          tar czf tml-${TML_VERSION}-linux-${{ matrix.arch }}.tar.gz -C staging .

          # .deb and .rpm
          nfpm package --packager deb --target tml-${TML_VERSION}-${{ matrix.arch }}.deb
          nfpm package --packager rpm --target tml-${TML_VERSION}-${{ matrix.arch }}.rpm

      - name: Sign packages
        env:
          GPG_KEY: ${{ secrets.GPG_SIGNING_KEY }}
        run: |
          echo "$GPG_KEY" | base64 -d | gpg --import
          dpkg-sig --sign builder tml-${TML_VERSION}-${{ matrix.arch }}.deb
          rpm --addsign tml-${TML_VERSION}-${{ matrix.arch }}.rpm
          sha256sum tml-${TML_VERSION}-linux-${{ matrix.arch }}.tar.gz > SHA256SUMS
          gpg --detach-sign SHA256SUMS

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: linux-${{ matrix.arch }}
          path: |
            tml-*.tar.gz
            tml-*.deb
            tml-*.rpm
            SHA256SUMS*

  # ============================================================
  # macOS (arm64 + x86_64 universal)
  # ============================================================
  build-macos:
    runs-on: macos-14  # Apple Silicon
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install dependencies
        run: brew install cmake ninja openssl@3 sqlite zstd brotli

      - name: Build arm64
        run: |
          CMAKE_OSX_ARCHITECTURES=arm64 scripts/build.sh release
          mkdir -p build-arm64
          cp -r build/release build-arm64/

      - name: Build x86_64
        run: |
          CMAKE_OSX_ARCHITECTURES=x86_64 scripts/build.sh release --clean
          mkdir -p build-x86
          cp -r build/release build-x86/

      - name: Create universal binaries
        run: |
          mkdir -p staging/bin staging/lib/tml/plugins
          lipo -create build-arm64/release/bin/tml build-x86/release/bin/tml -output staging/bin/tml
          lipo -create build-arm64/release/bin/tml-mcp build-x86/release/bin/tml-mcp -output staging/bin/tml-mcp
          lipo -create build-arm64/release/bin/tml-daemon build-x86/release/bin/tml-daemon -output staging/bin/tml-daemon
          # Plugins: ship both architectures (codegen is target-specific)
          cp build-arm64/release/bin/plugins/*.dylib staging/lib/tml/plugins/
          cp -r lib/core lib/std lib/test staging/lib/tml/

      - name: Sign binaries
        env:
          APPLE_CERT: ${{ secrets.APPLE_DEVELOPER_ID_CERT }}
          APPLE_CERT_PASSWORD: ${{ secrets.APPLE_CERT_PASSWORD }}
        run: |
          # Import certificate
          echo "$APPLE_CERT" | base64 -d > cert.p12
          security create-keychain -p "" build.keychain
          security import cert.p12 -k build.keychain -P "$APPLE_CERT_PASSWORD" -T /usr/bin/codesign
          security set-key-partition-list -S apple-tool:,apple: -k "" build.keychain

          # Sign everything
          find staging -type f \( -name "tml*" -o -name "*.dylib" \) -exec \
            codesign --force --sign "Developer ID Application: HiveLLM LLC" \
              --options runtime \
              --entitlements installer/macos/tml.entitlements \
              --timestamp {} \;

      - name: Build .pkg
        run: |
          pkgbuild --root staging --identifier com.hivellm.tml \
            --version ${TML_VERSION#v} --install-location /usr/local \
            --scripts installer/macos/scripts tml-component.pkg
          productbuild --distribution installer/macos/Distribution.xml \
            --package-path . --resources installer/macos/resources \
            --sign "Developer ID Installer: HiveLLM LLC" \
            TML-${TML_VERSION}-macos.pkg

      - name: Notarize
        env:
          APPLE_ID: ${{ secrets.APPLE_ID }}
          APPLE_TEAM_ID: ${{ secrets.APPLE_TEAM_ID }}
          APPLE_NOTARY_PASSWORD: ${{ secrets.APPLE_NOTARY_PASSWORD }}
        run: |
          xcrun notarytool submit TML-${TML_VERSION}-macos.pkg \
            --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
            --password "$APPLE_NOTARY_PASSWORD" --wait
          xcrun stapler staple TML-${TML_VERSION}-macos.pkg

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: macos-universal
          path: TML-*.pkg

  # ============================================================
  # Create GitHub Release
  # ============================================================
  create-release:
    needs: [build-windows, build-linux, build-macos]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/download-artifact@v4

      - name: Create release
        uses: softprops/action-gh-release@v2
        with:
          name: TML ${{ github.ref_name }}
          draft: true  # Review before publishing
          generate_release_notes: true
          files: |
            windows-x64/*.msi
            linux-x64/*.tar.gz
            linux-x64/*.deb
            linux-x64/*.rpm
            linux-arm64/*.tar.gz
            linux-arm64/*.deb
            linux-arm64/*.rpm
            macos-universal/*.pkg
```

## Secrets Required

| Secret | Platform | Source |
|--------|----------|--------|
| `AZURE_SIGNING_VAULT_URL` | Windows | Azure Key Vault URL |
| `AZURE_SIGNING_CERT_NAME` | Windows | Certificate name in vault |
| `AZURE_CLIENT_ID` | Windows | Azure service principal |
| `AZURE_TENANT_ID` | Windows | Azure tenant |
| `APPLE_DEVELOPER_ID_CERT` | macOS | Base64-encoded .p12 file |
| `APPLE_CERT_PASSWORD` | macOS | Certificate password |
| `APPLE_ID` | macOS | Apple Developer email |
| `APPLE_TEAM_ID` | macOS | Apple Team ID |
| `APPLE_NOTARY_PASSWORD` | macOS | App-specific password |
| `GPG_SIGNING_KEY` | Linux | Base64-encoded GPG private key |

## Release Checklist

1. Update `VERSION` file
2. Update `CHANGELOG.md`
3. `git tag v0.2.9 && git push --tags`
4. CI builds all platforms (~30 min)
5. Review draft release on GitHub
6. Publish release
7. Update Homebrew formula (if applicable)
8. Update Winget manifest (if applicable)
