# Windows MSI Installer

## Technology: WiX Toolset v4+

**Why WiX:**
- Industry standard for MSI creation (used by Rust, Go, Node.js, Git)
- Free, open-source (MS-RL license)
- XML-based declarative format (`.wxs` files)
- Full MSI feature set: PATH modification, upgrade/repair, uninstall
- CI/CD friendly (command-line `wix build`)
- v4+ uses modern .NET tooling (`dotnet tool install wix`)

**Alternatives considered:**
- NSIS — popular but script-based, no native MSI (creates .exe)
- Inno Setup — similar to NSIS, no MSI
- Advanced Installer — commercial, unnecessary for our needs
- MSIX — modern but limited (no PATH modification, requires Store or sideloading)

## Installer Layout

```
TML-0.2.9-x64.msi
│
├── [INSTALLDIR] = C:\Program Files\TML\
│   ├── bin\
│   │   ├── tml.exe
│   │   ├── tml_mcp.exe
│   │   ├── tml_daemon.exe
│   │   ├── plugins\
│   │   │   ├── tml_compiler.dll
│   │   │   ├── tml_codegen_x86.dll
│   │   │   ├── tml_mcp.dll
│   │   │   ├── tml_test.dll
│   │   │   ├── tml_tools.dll
│   │   │   └── manifest.json
│   │   ├── libcrypto-3-x64.dll
│   │   ├── libssl-3-x64.dll
│   │   ├── sqlite3.dll
│   │   ├── zlib1.dll
│   │   ├── zstd.dll
│   │   ├── brotlicommon.dll
│   │   ├── brotlidec.dll
│   │   └── brotlienc.dll
│   ├── lib\
│   │   ├── core\
│   │   ├── std\
│   │   └── test\
│   ├── docs\
│   │   └── examples\
│   ├── LICENSE
│   └── CHANGELOG.md
│
├── Registry:
│   └── HKLM\SOFTWARE\TML\InstallDir = [INSTALLDIR]
│
├── Environment:
│   └── PATH += [INSTALLDIR]\bin   (System-wide)
│
└── Start Menu:
    └── TML\
        ├── TML Documentation
        └── Uninstall TML
```

## PATH Configuration

### Approach: System PATH via MSI Environment table

```xml
<Component Id="PathEnv" Guid="...">
    <Environment Id="TmlPath"
                 Name="PATH"
                 Value="[INSTALLDIR]bin"
                 Permanent="no"
                 Part="last"
                 Action="set"
                 System="yes" />
</Component>
```

**Key decisions:**
- `System="yes"` — adds to system PATH (requires admin), available to ALL users
- `Part="last"` — appends to end (doesn't override existing tools)
- `Permanent="no"` — removed on uninstall (clean removal)
- `Action="set"` — adds the value

**PATH takes effect:**
- New terminal windows: immediately
- Existing terminals: requires restart
- GUI apps: requires logoff/logon
- The installer should display: "Open a new terminal to use `tml`"

### Alternative: Per-User PATH (non-admin install)

```xml
<Environment Id="TmlPathUser"
             Name="PATH"
             Value="[INSTALLDIR]bin"
             Permanent="no"
             Part="last"
             Action="set"
             System="no" />
```

For non-admin install, use `%LOCALAPPDATA%\TML\` as install directory.

## WiX Project Structure

```
installer/
├── wix/
│   ├── Product.wxs              # Main product definition
│   ├── Directories.wxs          # Directory layout
│   ├── Components.wxs           # File components (auto-generated)
│   ├── Features.wxs             # Feature tree (Core, Docs, Examples)
│   ├── UI.wxs                   # Custom UI dialogs
│   ├── Variables.wxi            # Version, GUIDs, paths
│   └── License.rtf              # Apache 2.0 in RTF format
├── assets/
│   ├── tml-icon.ico             # Product icon (256x256)
│   ├── banner.bmp               # 493x58 top banner
│   ├── dialog.bmp               # 493x312 welcome dialog
│   └── tml-logo.svg             # Source logo
├── scripts/
│   ├── build-msi.bat            # Build MSI from staged files
│   ├── harvest-files.bat        # Auto-generate Components.wxs from build/
│   └── sign-msi.bat             # Sign the MSI with certificate
└── README.md
```

## Product.wxs (Skeleton)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">

    <?include Variables.wxi ?>

    <Package
        Name="TML"
        Manufacturer="HiveLLM"
        Version="$(var.ProductVersion)"
        UpgradeCode="$(var.UpgradeCode)"
        Scope="perMachine"
        Language="1033"
        Codepage="1252">

        <MajorUpgrade
            DowngradeErrorMessage="A newer version of TML is already installed."
            AllowSameVersionUpgrades="yes" />

        <MediaTemplate EmbedCab="yes" CompressionLevel="high" />

        <!-- Features -->
        <Feature Id="Core" Title="TML Compiler" Level="1" Absent="disallow">
            <ComponentGroupRef Id="BinFiles" />
            <ComponentGroupRef Id="PluginFiles" />
            <ComponentGroupRef Id="RuntimeDlls" />
            <ComponentRef Id="PathEnv" />
        </Feature>

        <Feature Id="StdLib" Title="Standard Library" Level="1">
            <ComponentGroupRef Id="CoreLib" />
            <ComponentGroupRef Id="StdLib" />
            <ComponentGroupRef Id="TestLib" />
        </Feature>

        <Feature Id="Docs" Title="Documentation" Level="1">
            <ComponentGroupRef Id="DocFiles" />
            <ComponentGroupRef Id="ExampleFiles" />
        </Feature>

        <!-- UI -->
        <UIRef Id="WixUI_InstallDir" />
        <Property Id="WIXUI_INSTALLDIR" Value="INSTALLDIR" />
        <WixVariable Id="WixUILicenseRtf" Value="License.rtf" />
        <WixVariable Id="WixUIBannerBmp" Value="assets\banner.bmp" />
        <WixVariable Id="WixUIDialogBmp" Value="assets\dialog.bmp" />

    </Package>
</Wix>
```

## Upgrade Strategy

### Major/Minor Upgrades

- **Same minor version** (0.2.8 → 0.2.9): In-place upgrade, preserves user settings
- **New minor version** (0.2.x → 0.3.0): Full upgrade, may migrate settings
- **New major version** (0.x → 1.0): Side-by-side possible (different install dir)

### UpgradeCode

```xml
<!-- This GUID must NEVER change — it identifies the product family -->
<Property Id="UpgradeCode" Value="{GENERATE-ONCE-AND-FREEZE}" />
```

The UpgradeCode stays constant across all versions. The ProductCode changes per build.

## Build Command

```bash
# Install WiX v4 (one-time)
dotnet tool install --global wix

# Build MSI
wix build -o TML-0.2.9-x64.msi \
    -d ProductVersion=0.2.9 \
    -d BuildDir=../build/release/bin \
    -d LibDir=../lib \
    Product.wxs Directories.wxs Components.wxs Features.wxs

# Or via build script
scripts/build-msi.bat release
```

## Size Optimization

| Strategy | Savings |
|----------|---------|
| Strip debug info from DLLs | ~30% on plugins |
| zstd-pack plugins (already supported) | ~75% on plugins |
| CAB compression in MSI (high) | ~20% additional |
| Exclude .pdb from main installer | -330 MB |
| **Projected MSI size** | **~50-60 MB** |

## Silent/Automated Install

```powershell
# Silent install (for CI/scripts)
msiexec /i TML-0.2.9-x64.msi /quiet /norestart

# Silent install to custom directory
msiexec /i TML-0.2.9-x64.msi /quiet INSTALLDIR="D:\TML"

# Uninstall
msiexec /x TML-0.2.9-x64.msi /quiet
```

## Winget Integration

After MSI is stable, publish to Windows Package Manager:

```yaml
# manifests/h/HiveLLM/TML/0.2.9/HiveLLM.TML.installer.yaml
PackageIdentifier: HiveLLM.TML
PackageVersion: 0.2.9
InstallerType: msi
Installers:
  - Architecture: x64
    InstallerUrl: https://github.com/hivellm/tml/releases/download/v0.2.9/TML-0.2.9-x64.msi
    InstallerSha256: <sha256>
    ProductCode: '{...}'
```

Then: `winget install HiveLLM.TML`

## Chocolatey Integration

```powershell
# chocolatey/tml.nuspec
choco install tml --version 0.2.9
```

Package points to the MSI download URL with checksum verification.
