# Arong Study IME for Windows

[中文 README](README.md) · [Releases](https://github.com/daodeqing/Arong-Study-IME/releases) · [Privacy](PRIVACY.md)

<!-- badges:start -->
[![CI](https://img.shields.io/github/actions/workflow/status/daodeqing/Arong-Study-IME/ci.yml?branch=main&label=CI)](https://github.com/daodeqing/Arong-Study-IME/actions/workflows/ci.yml)
[![CodeQL](https://img.shields.io/github/actions/workflow/status/daodeqing/Arong-Study-IME/codeql.yml?branch=main&label=CodeQL)](https://github.com/daodeqing/Arong-Study-IME/actions/workflows/codeql.yml)
[![Release](https://img.shields.io/github/v/release/daodeqing/Arong-Study-IME?label=release)](https://github.com/daodeqing/Arong-Study-IME/releases)
[![Downloads](https://img.shields.io/github/downloads/daodeqing/Arong-Study-IME/total?label=downloads)](https://github.com/daodeqing/Arong-Study-IME/releases)
[![License](https://img.shields.io/github/license/daodeqing/Arong-Study-IME)](LICENSE)
[![Stars](https://img.shields.io/github/stars/daodeqing/Arong-Study-IME?style=flat)](https://github.com/daodeqing/Arong-Study-IME/stargazers)
<!-- badges:end -->

Arong Study IME is a free and open-source Chinese and Japanese input method for Windows 10 and 11. English output is the default. The candidate window stays Chinese; when a local OpenAI-compatible model is configured, selecting a Chinese candidate commits English. Press `Shift` to switch between English and Chinese output; `Ctrl+Space` remains the Windows input-method switch.

This repository is the whole Windows product: the TSF text service DLL, the resident server, the conversion engine, a self-written native GUI framework, the HTML surfaces and the installer. The engine was imported from [MSIME-Engine](https://github.com/metasequoiaime/MSIME-Engine) and specialized for Windows; it now lives in `engine/` as a first-party component, so a plain clone builds the product. See [`engine/UPSTREAM.md`](engine/UPSTREAM.md).

**This is a public beta.** Expect rough edges and read the release notes before upgrading.

## Install

Download the installer from [Releases](https://github.com/daodeqing/Arong-Study-IME/releases), run it, then switch with `Win + Space`.

Unsigned builds may trigger SmartScreen and cannot use uiAccess. Verify the installer with the SHA256 published with the release.

You need the **Microsoft Visual C++ 2015–2022 Redistributable (x64)**. The server and settings app are 64-bit; the x86 redistributable is a separate package and does not substitute for it. If you cannot switch to the IME, the server keeps exiting, or the settings window closes immediately, install that first.

## What it does

- Chinese input: full pinyin, double pinyin (Xiaohe / Ziranma / Shoudao / Microsoft / Sogou / Ziguang / Zhineng ABC / Guobiao / Pinyin Jiajia), Wubi 86
- Japanese input: romaji, with hiragana, katakana and dictionary candidates; also available as a temporary mode from Chinese
- Helpcode (形码) filtering on pinyin schemes: six tables, single- and double-code
- Local OpenAI-compatible output translation; remote AI providers remain manually configurable
- English-Chinese glosses in the vertical candidate window
- Mixed Chinese-English input, emoji and kaomoji, a dedicated English candidate mode
- Local input statistics (off by default): character counts and active time stay on this machine; view and clean them under Settings → Statistics
- Voice input with streaming recognition and optional cleanup
- Handwriting, on-screen keyboard, floating toolbar, clipboard history
- User dictionary management, candidate frequency tuning, skins with light and dark variants

## Privacy

An input method sees every keystroke, so the boundaries are stated explicitly in [PRIVACY.md](PRIVACY.md): which features reach the network, what each one sends, what the defaults are, and how to turn each off.

Cloud candidates can send the current spelling to Google's input-tools service. Output translation uses only the configured loopback model endpoint, keeps translations in a bounded memory cache and does not log request bodies. API credentials are protected with Windows DPAPI in the local configuration file.

There is no telemetry, analytics or crash reporting of any kind.

## Building and contributing

[CONTRIBUTING.md](CONTRIBUTING.md) has the environment, the build order and what to run before opening a pull request. Contributions do not have to be code — dictionary entries, documentation, icons and compatibility testing all count, and the [recruiting page](https://github.com/metasequoiaime/.github/blob/main/RECRUITING.md) lists them by area.

Note that most issues, documentation and code comments are in Chinese. English pull requests and issues are welcome.

## Code signing policy

The 0.8.0 package is unsigned. It is built and tested on GitHub-hosted Windows runners and ships with a SHA-256 sidecar; upstream signing approvals do not transfer to this repository.

What is signed, who the committers, reviewers and approvers are, the build and release process and the current status are in the [code signing policy](docs/code-signing-policy.md). The privacy policy is [PRIVACY.md](PRIVACY.md).

## Licence

GPL-3.0. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).
