# 安装阿龍学习输入法

> 本文描述当前内测版的安装流程。具体版本号、下载链接与该版本的已知问题一律以 [Releases](https://github.com/daodeqing/Arong-Study-IME/releases) 页面为准——这里不再写死版本号，避免文档落后于实际发布。

## 目录

- [环境要求](#环境要求)
- [下载](#下载)
- [安装与注意事项](#安装与注意事项)
- [在Windows中启用水杉](#在Windows中启用水杉)
- [常见问题](#常见问题)

## 环境要求

| 项目 | 要求 |
|---|---|
| 操作系统 | Windows 10 / Windows 11 |
| 处理器架构 | 同时提供 64 位与 32 位组件，x64 / x86 Windows 均可安装 |
| 磁盘空间 | 安装包约 61 MB，安装后占用约 250 MB［待确认：实测安装后目录大小］ |
| 网络 | 云联想、AI 联想、语音输入、候选词翻译等在线功能需要联网；离线状态下本地输入不受影响 |
| 其他 | 使用手写识别需先在 Windows 中安装「中文手写包」 |
| Visual C++ 运行库 | **需要 x64 版**。TSF DLL 采用静态 CRT 构建，被加载进宿主进程时不依赖 MSVCP140 / VCRUNTIME140；但 Server 与设置程序是动态 CRT 的 64 位程序，缺少运行库时会表现为服务反复退出、设置窗口一闪即关。详见下文[常见问题](#常见问题) |

## 下载

最省事的方式是用[官网下载页](https://msime.app/download/)：它读取的 `update.json` 始终指向当前推荐的版本，并附带该安装包的 SHA256 校验值。

也可以直接从 GitHub 取：

1. 打开 [Releases 页面](https://github.com/daodeqing/Arong-Study-IME/releases)。
2. 下载最上面那个版本的 `ArongStudyIme_Setup_v<版本号>.exe`。

Releases 页面计划分两个频道：

| 频道 | 页面上的样子 | 来源 | 谁该用 |
|---|---|---|---|
| 发布 | 正常发布，最新的一个带 **Latest** 徽章 | 人工挑选后发布 | **一般都用这个** |
| 自动构建 | 标记 **Pre-release**，版本带 `-beta` 且标题带「（自动构建）」 | 每次合并到 `main` 后由 CI 自动发布，未经人工挑选 | 只在需要某个尚未进入发布频道的修复、且能接受未经验证的改动时用 |

> **目前发布频道还是空的。** 迄今为止的每一个 release 都标着 **Pre-release**，所以页面上不会出现 **Latest** 徽章，`releases/latest` 这个地址也是 404——上表描述的是计划中的做法，不是页面当下的样子。在第一个人工挑选的发布出现之前，直接取列表最上面那一个即可。
>
> 无论哪个频道，当前都还是内测阶段，版本号仍在 `0.x`，功能与稳定性可能变动，重要环境建议先在虚拟机或非主力机试用。「发布」频道的含义是「有人挑过」，不是「已经稳定」。
> 参与内测与反馈：[Telegram 群](https://t.me/msimegroup)、QQ 群 829919142。

## 安装与注意事项

双击安装包，按安装向导提示完成安装。

- 安装默认路径是 `C:\Program Files\ArongStudyIme`；TSF DLL 安装在系统 `IME\ArongStudyIme` 目录
- 安装程序会把阿龍学习输入法加入输入法列表，无须手动添加
- 安装完成后自动启动 `ArongStudyImeServer`

## 在 Windows 中启用阿龍学习输入法

使用 `Win + 空格` 选择阿龍学习输入法。默认拼音选中文，单按 `Shift` 切换普通中文/直接英文输入。开启整句翻译后，在英文候选模式下再按 `Shift` 可使用“英文输入、中文输出”。`Ctrl + Space` 保留 Windows 的输入法切换。

需要翻译时，在设置 → AI 与云联想开启「拼音整句翻译成英文」，选择服务商、接口和模型并测试连接。默认本地 OpenAI 兼容 Base URL 为 `http://127.0.0.1:8317/v1`，访问密钥可为空；需要鉴权的服务应填写自己的密钥。本地端口可以转发远程模型，模型是否在本机运行取决于服务本身。

整句模式下，空格/数字只确认中文，已选中文在组合区持续累积；后台更新整句英文预览。按 `Ctrl+Enter` 确认已就绪英文，或在英文候选显示后按空格确认。翻译未完成时保留中文并提示，等待后再次确认；普通 `Enter` 提交中文，`Shift` 切换本次中英文输出并保持拼音输入。ASCII 符号会直接加入句子，不再被候选翻页键吞掉。`Backspace` 可撤回最近选词；较长句子请分句输入。模型离线或请求失败不会丢掉中文，也不会把拼音字母当作翻译提交。

Google 云候选出厂关闭。整句缓冲和译文只在内存中存在，不自动改写应用里已经提交的文字。

## 常见问题

### 安装后无法切换到阿龍学习输入法，或设置窗口一闪即关

绝大多数情况是缺少 **Visual C++ 2015–2022 Redistributable（x64）**。

TSF DLL 被加载进宿主程序进程，采用静态 CRT 构建，不依赖运行库；但 Server 和设置程序是独立的 64 位程序，采用动态 CRT，缺少运行库时进程会在启动阶段直接退出。典型表现有三种：切换不到阿龍学习输入法、Server 反复退出、设置窗口打开后立即消失。

到[微软官方下载页](https://learn.microsoft.com/zh-cn/cpp/windows/latest-supported-vc-redist?view=msvc-170)下载 `vc_redist.x64.exe` 安装，然后重启 Windows。

**注意 x86 与 x64 是两套独立的运行库。** 即使机器上已经装了较新的 x86 版本，也不能替代这里需要的 x64 版本。Windows 事件查看器里如果记录了 `ArongStudyImeServer.exe` 或 `ArongStudyImeSettings.exe` 因 `MSVCP140.dll` 或 `VCRUNTIME140.dll` 无法启动，就是这个原因。

### 候选窗始终不出现

候选窗由 Server 用 Direct2D 绘制，不依赖 WebView2 Runtime。先按上一节确认 x64 运行库已安装，再在任务管理器里确认 `ArongStudyImeServer.exe` 正在运行。在独占全屏的游戏里，候选窗会被有意隐藏，以免把游戏切出全屏。

### 手写识别没有候选结果

需要先在 Windows 中安装「中文手写包」：设置 → 时间和语言 → 语言和区域 → 中文（简体，中国）→ 语言选项 → 手写。

### 其他问题

在 [Issues](https://github.com/daodeqing/Arong-Study-IME/issues) 中反馈。为便于定位，请附上：输入法版本号、Windows 版本（`Win + R` 输入 `winver`）、出问题的宿主程序、以及最小复现步骤。

与按键、候选窗相关的问题，可以在数据目录下的 `config.toml`（默认 `%LOCALAPPDATA%\ArongStudyIme\config.toml`，安装时若选了别的位置就在那里）的 `[general]` 段设 `tsf_diagnostic_log = true`，复现一次后附上日志。日志中可能包含你输入的内容，贴出前请先自行检查。
