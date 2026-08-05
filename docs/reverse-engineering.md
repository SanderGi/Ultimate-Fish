# Chess Ultimate 5.731 recovery record

This document records enough provenance to reproduce the rule-recovery work
without checking proprietary app packages or recovered code into Git.

## Safety and authenticity assessment

The supplied Android package is an XAPK containing a base APK and three split
APKs. Every split passed ZIP integrity checks and Android signature verification
for v1, v2, and v3. Every split has the same signer and a valid Google Play
Source Stamp.

- Package: `com.JesseLugassy.ChessUltimate`
- Version: `5.731` (`versionCode` 198)
- Minimum Android SDK: 23
- Target Android SDK: 35
- XAPK SHA-256: `c1689c22d3da839274aa3ea3acbd36050c20b25a24b4ee06406f4cb53c238738`
- Signer certificate SHA-256: `2fd68ebcccf9ecc5b1a479105ee6c7497008be42b7fc0ea5d11742e46fdef9f8`
- Source Stamp certificate SHA-256: `3257d599a49d2c961a471ca9843f59d341a405884583fc087df4237b733bbd6d`

Split hashes:

| Split | SHA-256 |
| --- | --- |
| `com.JesseLugassy.ChessUltimate.apk` | `6602a9a1e2c34529b30dcfc11c3c6c363f1801e129121aaf3f9a16b29a272ad2` |
| `AddressablesAssetPack.apk` | `476cd0b7a71c7c46ac3e90d7ee66f8528039374f15e24c7d2e3f4a10cc537c6a` |
| `UnityDataAssetPack.apk` | `03f70dd99f2ffcb9bdc12abb724892b839237a4c184235ff7011cd95ea6d68e4` |
| `config.armeabi_v7a.apk` | `62a87654abe19cf823b99d60b4448dce7d69db203e03fe00c7da85d2700da36a` |

Requested permissions are Internet, notifications, vibration, network/Wi-Fi
state, Bluetooth/Nearby-related access, coarse location, foreground data-sync,
and Google Play billing. The manifest does not request SMS, contacts, files,
camera, or microphone access. It is not marked debuggable and does not opt into
cleartext traffic. Exported components are consistent with the Unity launcher,
Google services, and Play Asset Delivery.

This is strong evidence that the package is an authentic Google Play-derived
build. Static analysis cannot prove that any large networked application is
absolutely safe. A launch was attempted only inside dedicated Android emulators
with Wi-Fi, cellular data, and the default network disabled. Installation was
rejected before launch with `INSTALL_FAILED_NO_MATCHING_ABIS`: the supplied app
contains only 32-bit ARM native code, while the available Apple Silicon images
accept ARM64 apps. The Chess Ultimate process therefore never ran and never had
network access. All rule recovery continued from static files in a temporary
directory.

The outer archive contains an `ApkPure.com` comment despite its APKCombo
filename. That repackaging detail is why the signed inner APKs and Source Stamp,
not the outer filename, are the trust boundary.

## Does it work for rule recovery?

Yes. The Android build is materially better than the supplied iOS bundle for
this purpose. The iOS launcher and `UnityFramework` both have FairPlay
`cryptid=1`, so their native method bodies remain encrypted. The Android ARMv7
IL2CPP binary is unencrypted and pairs successfully with its metadata.

- Unity version: `6000.2.6f2`
- App build GUID: `ff0e39459b9b4c5e81c88a8a527edad1`
- `libil2cpp.so` SHA-256: `80dde7bc82513521fc72c85e248a64056f7c062bd5eca6ebb8ab8c6dc05d158e`
- `global-metadata.dat` SHA-256: `f67b5d27853b374599028250a451d04adc0c47cf2164600883b4efba793a4e33`

Il2CppDumper 6.7.46 recovered IL2CPP v31 registrations, 275,831 method
symbols, type definitions, field offsets, signatures, strings, and dummy
assemblies. Ghidra 12.1.2 then decompiled the ARM rule methods. The ARM image
required a `+0x10000` correction between dump RVAs and Ghidra's relocated image
base; applying that correction fixed the initially misleading bodies. A focused
pass recovered 368 simulation and drafting functions at addresses verified
directly against the ELF with LLVM objdump.

Cpp2IL was also tested. Its current ARMv7 backend maps the metadata but emits an
empty intermediate instruction list, so its apparent method-success count must
not be treated as recovered logic. It remains useful here only for clean type
and member declarations.

No recovered proprietary source, app asset, APK, XAPK, or IPA should be
committed. Only independently written rule specifications, conformance cases,
and extraction automation belong in this repository.

## Tool provenance

| Tool | Version/source | Verification |
| --- | --- | --- |
| Il2CppDumper | 6.7.46 official release | ZIP SHA-256 `db9bbbc538e33abfb057c7757ae5d6c1f16a05fdc0d13af8a5a67ea31faaba0c` |
| Cpp2IL | source commit `1dfeedd` | Built locally with .NET 10; used for declarations only |
| ILSpy CLI | 10.1.1.8388 | Used to render dummy assemblies as readable declarations |
| Ghidra | 12.1.2 official NSA release | ZIP SHA-256 `b62e81a0390618466c019c60d8c2f796ced2509c4c1aea4a37644a77272cf99d` |
| UnityPy | 1.25.3 | Used to read current TextAssets and serialized prefab fields |
| TypeTreeGenerator | 0.0.10 | Generated Unity type trees from Il2CppDumper dummy assemblies |

All tools and outputs were kept under `/private/tmp`.

Loading Il2CppDumper's `DummyDll` directory into TypeTreeGenerator allowed
UnityPy to deserialize every MonoBehaviour in the current asset bundles. This
independently confirmed the prefab type IDs, movement direction/range fields,
and cooldown values used to cross-check the native simulation constructors.
