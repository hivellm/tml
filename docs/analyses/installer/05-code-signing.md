# Code Signing & Trust

## Why Signing Is Mandatory

| Platform | Without Signing | With Signing |
|----------|----------------|--------------|
| **Windows** | SmartScreen blocks download, "Unknown publisher" warning, some AV flags as malicious | Clean install, "Verified publisher: HiveLLM" |
| **macOS** | Gatekeeper blocks execution, requires manual override in System Preferences | Runs without warnings, passes notarization |
| **Linux** | Package managers warn "unsigned package", some repos reject | GPG-signed .deb/.rpm accepted by all repos |

**For a compiler that generates executables, unsigned distribution is especially problematic** — users will assume the tool itself is malware if their OS flags it.

## Windows: Authenticode Signing

### Certificate Options

| Provider | Type | Cost/Year | Trust Level | Notes |
|----------|------|-----------|-------------|-------|
| **DigiCert** | EV Code Signing | ~$500 | Highest (instant SmartScreen trust) | Industry standard, HSM-based |
| **Sectigo** | EV Code Signing | ~$400 | Highest | Budget option |
| **SSL.com** | EV Code Signing | ~$350 | Highest | Cheapest EV |
| **DigiCert** | Standard OV | ~$200 | Medium (builds SmartScreen reputation over time) | No HSM required |
| **SignPath** | Free for OSS | $0 | Medium (OV) | Limited to open-source projects |
| **Azure Trusted Signing** | Cloud-based | ~$10/month | High | New, Microsoft-operated, no HSM needed |

**Recommendation: Azure Trusted Signing** — cheapest, Microsoft-backed, CI/CD native, no hardware tokens.

**Alternative: SignPath Foundation** — free for open-source, but requires approval process.

### EV vs. OV Certificates

| Feature | OV (Organization Validated) | EV (Extended Validation) |
|---------|---------------------------|--------------------------|
| SmartScreen trust | Builds over time (500+ downloads) | Immediate trust from first download |
| Hardware token | Not required | Required (HSM or cloud HSM) |
| Cost | ~$200/year | ~$350-500/year |
| CI/CD | Easy (PFX file) | Requires HSM integration |

**For a new project, EV is strongly recommended** — OV certificates start with zero SmartScreen reputation and users will see warnings for weeks/months until enough downloads accumulate.

### Signing Process

```powershell
# Sign with Azure Trusted Signing (in CI)
az login --service-principal ...
AzureSignTool.exe sign \
    -kvu https://your-vault.vault.azure.net \
    -kvc your-cert-name \
    -fd sha256 \
    -td sha256 \
    -tr http://timestamp.digicert.com \
    build/release/bin/tml.exe \
    build/release/bin/tml_mcp.exe \
    build/release/bin/tml_daemon.exe \
    build/release/bin/plugins/*.dll

# Sign the MSI
AzureSignTool.exe sign \
    -kvu https://your-vault.vault.azure.net \
    -kvc your-cert-name \
    -fd sha256 \
    -td sha256 \
    -tr http://timestamp.digicert.com \
    TML-0.2.9-x64.msi
```

**Critical: Timestamp the signature** (`-tr` flag). Without timestamping, signatures expire when the certificate expires. With timestamping, signatures remain valid indefinitely.

### What to Sign

| File | Sign? | Why |
|------|-------|-----|
| `tml.exe` | YES | Main executable, SmartScreen checks this |
| `tml_mcp.exe` | YES | Separate process, may trigger AV |
| `tml_daemon.exe` | YES | Background process, AV sensitive |
| `tml_compiler.dll` | YES | Large DLL, AV may scan |
| `tml_codegen_x86.dll` | YES | Large DLL, AV may scan |
| `tml_mcp.dll` | Recommended | Small but loaded dynamically |
| `tml_test.dll` | Recommended | Small but loaded dynamically |
| `tml_tools.dll` | Recommended | Small but loaded dynamically |
| `libcrypto-3-x64.dll` | NO | Third-party, already signed by OpenSSL |
| `sqlite3.dll` | NO | Third-party |
| `TML-0.2.9-x64.msi` | YES | Installer itself must be signed |

## macOS: Developer ID + Notarization

### Certificate: Apple Developer ID

- **Cost:** $99/year (Apple Developer Program)
- **Type:** "Developer ID Application" for binaries, "Developer ID Installer" for .pkg
- Requires Apple Developer account enrollment

### Signing Process

```bash
# Sign all binaries and dylibs
codesign --force --deep --sign "Developer ID Application: HiveLLM LLC (TEAMID)" \
    --options runtime \
    --timestamp \
    staging/bin/tml \
    staging/bin/tml-mcp \
    staging/bin/tml-daemon \
    staging/lib/tml/plugins/*.dylib \
    staging/lib/tml/deps/*.dylib

# Sign the .pkg installer
productsign --sign "Developer ID Installer: HiveLLM LLC (TEAMID)" \
    TML-0.2.9-macos-unsigned.pkg \
    TML-0.2.9-macos.pkg
```

### Notarization

Apple requires all distributed software to be notarized (uploaded to Apple for automated security scanning):

```bash
# Submit for notarization
xcrun notarytool submit TML-0.2.9-macos.pkg \
    --apple-id "dev@hivellm.com" \
    --team-id "TEAMID" \
    --password "@keychain:AC_PASSWORD" \
    --wait

# Staple the notarization ticket
xcrun stapler staple TML-0.2.9-macos.pkg
```

**Timeline:** Notarization typically completes in 2-15 minutes. CI should wait for it.

### Hardened Runtime

macOS requires the "hardened runtime" entitlement for notarization. This restricts:
- Dynamic code generation (JIT) — needs `com.apple.security.cs.allow-jit` entitlement
- Loading unsigned dylibs — needs `com.apple.security.cs.disable-library-validation`
- DYLD environment variables — needs `com.apple.security.cs.allow-dyld-environment-variables`

**Entitlements file** (`tml.entitlements`):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <!-- Required for LLVM JIT -->
    <key>com.apple.security.cs.allow-jit</key>
    <true/>
    <!-- Required for loading plugin dylibs -->
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
</dict>
</plist>
```

## Linux: GPG Signing

### Package Signing

```bash
# Generate GPG key (one-time)
gpg --full-gen-key  # RSA 4096, no expiry, team@hivellm.com

# Export public key for repository
gpg --armor --export team@hivellm.com > tml-signing-key.asc

# Sign .deb
dpkg-sig --sign builder tml-0.2.9-amd64.deb

# Sign .rpm (via rpmsign or nfpm)
rpm --addsign tml-0.2.9-x86_64.rpm
```

### Repository Signing

APT and DNF repositories are signed with the same GPG key:

```bash
# APT repository (Release file)
gpg --detach-sign --armor Release

# Users add the key
curl -fsSL https://packages.hivellm.com/gpg | sudo gpg --dearmor -o /usr/share/keyrings/tml.gpg
```

### Tarball Checksums

For direct downloads, provide checksums:

```
# SHA256SUMS (signed with GPG)
a1b2c3d4...  tml-0.2.9-linux-x64.tar.gz
e5f6g7h8...  tml-0.2.9-linux-arm64.tar.gz

# SHA256SUMS.sig (detached signature)
gpg --detach-sign SHA256SUMS
```

Users verify:
```bash
gpg --verify SHA256SUMS.sig SHA256SUMS
sha256sum -c SHA256SUMS
```

## Cost Summary

| Item | Annual Cost | Notes |
|------|-------------|-------|
| Azure Trusted Signing | ~$120 | Windows EXE/MSI signing |
| Apple Developer Program | $99 | macOS signing + notarization |
| GPG key | $0 | Linux package signing |
| **Total** | **~$220/year** | |

**Alternative (budget):**

| Item | Cost | Notes |
|------|------|-------|
| SignPath Foundation | $0 | Windows (OSS only, approval required) |
| Apple Developer | $99 | Cannot avoid for macOS |
| GPG | $0 | Free |
| **Total** | **$99/year** | |

## CI/CD Integration

All signing happens in GitHub Actions. Secrets stored in GitHub Secrets or Azure Key Vault:

- `AZURE_SIGNING_VAULT_URL` — Azure Trusted Signing endpoint
- `AZURE_SIGNING_CERT_NAME` — Certificate name
- `APPLE_DEVELOPER_ID` — Apple signing identity
- `APPLE_NOTARY_PASSWORD` — App-specific password for notarytool
- `GPG_SIGNING_KEY` — Base64-encoded GPG private key

See [06-ci-cd-pipeline.md](06-ci-cd-pipeline.md) for full workflow.
