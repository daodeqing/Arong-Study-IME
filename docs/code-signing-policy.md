# Code signing policy

Arong Study IME 0.8.0 is built on GitHub-hosted Windows runners by
[`package-release.yml`](../.github/workflows/package-release.yml). This repository has no bundled
code-signing credential. The current package is unsigned; the release includes an installer
SHA-256 sidecar. Unsigned builds may trigger SmartScreen and cannot use uiAccess in elevated apps.

The upstream project's signing approval and certificate do not transfer to this repository.
The optional SignPath and certificate scripts remain available for a separately configured future
release. No signing service is required by the 0.8.0 packaging workflow.

Source: <https://github.com/daodeqing/Arong-Study-IME>, GPL-3.0. The repository owner is
[@daodeqing](https://github.com/daodeqing), as listed in [CODEOWNERS](../.github/CODEOWNERS).

The installer includes the Server and helper executables (`ArongStudyIme*.exe`) and x64/Win32
TSF DLLs (`ArongStudyImeTsf.dll`). Third-party licenses and attribution remain in
[THIRD_PARTY_NOTICES.txt](../THIRD_PARTY_NOTICES.txt).

For build inputs and release verification, see [product-release.md](product-release.md).
For network features and local credential storage, see [PRIVACY.md](../PRIVACY.md).
