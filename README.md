# 阿龍学习输入法

一款面向 Windows 10 / Windows 11 的自由开源中文输入法，基于 Windows TSF (Text Services Framework) 原生架构开发。

默认作为普通中文拼音输入法使用。可在「设置 → AI 与云联想」开启 **拼音整句翻译成英文**，并配置 OpenAI 兼容接口和模型。默认 Base URL 是 `http://127.0.0.1:8317/v1`，匿名服务的访问密钥可留空；本地地址也可能是远程模型的转发入口。

开启整句翻译后：

- 用拼音输入，按空格或数字选择中文；已选中文保留在输入法组合区，继续逐字选词组成一句话。
- 后台只翻译最新的已选中文，候选窗显示完整英文预览。按 **Ctrl+Enter** 确认英文；未完成时显示翻译状态并保留中文，完成后再次确认，或用空格选择已就绪的英文。
- **Enter** 提交中文。单按 **Shift** 切换本次中文/英文输出，拼音输入保持开启；关闭整句功能时 Shift 恢复普通中英文输入切换。`Ctrl+Space` 保留 Windows 的输入法切换。
- 模型失败、未配置或译文过长时保留中文，可修改、重试或按 Enter 提交中文；不会把原始拼音当作英文译文提交。较长句子请分句输入。
- 译文只保留在内存中，不写入词库、学习日志或请求日志。Google 云候选出厂关闭，用户可自行开启。

模型翻译耗时受模型和服务负载影响，后台预览不会阻塞选字。功能不会自动改写应用中已经正式提交的文字；句子应在组合区选完后再确认。

当前发布：**0.8.2 API 接口测试版**。本版沿用 0.8.1 的翻译实现，不包含 HY-MT 翻译模型或 llama.cpp；配置步骤、按键和未完成项见 [API 接口测试说明](docs/api-translation-testing.md)。

## 项目概述

本项目专注于提供纯净、高效、现代化的 Windows 输入体验，采用端侧优先与模块化解耦架构：
- **TSF 核心服务**：纯原生 C++ 实现，严格遵循 Windows TSF 规范，轻量嵌入各应用程序上下文。
- **独立常驻服务**：算法调度、词库管理、候选词计算与快捷功能均在独立后台进程运行，保障宿主应用稳定性。
- **Direct2D 原生界面**：自研 UI 渲染组件（msimeui），支持深色/浅色模式、高分屏 DPI 自适应及多款定制皮肤。
- **全功能输入引擎**：支持全拼、多方案双拼（小鹤/自然码/微软/搜狗等）、86 五笔、日文罗马字输入，并内置辅助码筛选、云端候选、语音转写与剪贴板历史。

## 目录结构

| 目录 | 职责与说明 |
|---|---|
| windows/ | TSF 文本服务 DLL，负责按键截取、编辑会话与 IPC 通信 |
| server/ | 输入法常驻服务进程，负责候选词状态、词库调度与 Direct2D 候选窗展示 |
| engine/ | 跨平台输入引擎，提供分词、拼音解析、辅助码匹配与契约协议 |
| ui/ | 原生 Direct2D / DirectWrite 界面渲染库 (msimeui) |
| installer/ | 安装包打包与发布脚本 (Inno Setup) |
| vendor/ | 第三方子模块依赖 (opencc、cpp-pinyin) |
| scripts/ | 构建、测试与词库同步辅助脚本 |

## 构建与运行要求

源码编译需要准备以下开发环境：
1. **编译器**：Visual Studio 2022（需安装 C++ 桌面开发组件及 MSVC v143+ 工具集）。
2. **构建系统**：CMake 3.25 或更高版本。
3. **第三方依赖**：通过 vcpkg 安装依赖库，并配置全局环境变量 VCPKG_ROOT 与 BOOST_ROOT。
4. **子模块与数据**：
   - 执行 git submodule update --init --recursive 初始化 vendor/。
   - 运行 python scripts/product_lock.py fetch-dictionaries --staging-root . 获取标准词库。

由于 TSF DLL 使用静态 CRT，而 Server 端使用动态 CRT，两个模块需通过独立 CMake 命令分别构建：
```powershell
# 构建 TSF 动态链接库
cmake -S windows -B windows/build64 -A x64
cmake --build windows/build64 --config Release

# 构建 Server 服务端
cmake -S server -B server/build -A x64
cmake --build server/build --config Release
```

## GitHub Actions 自动化打包与发布

本项目配置了独立的 GitHub Actions 云端构建工作流（.github/workflows/package-release.yml），支持在 GitHub-hosted windows-2025 环境中全自动编译组件并打包生成 Inno Setup 安装包：
- **手动触发**：在 GitHub 仓库页面进入 **Actions** -> **Build and Package Release** -> **Run workflow**，可按需选择打包精简版（light）或包含词库模型的完整版（full）。
- **Release 发布**：推送版本标签（例如 `v0.8.2`），工作流将自动编译并在 GitHub Release 中发布安装包文件。
- **构件下载**：每次成功构建的安装包均会自动上传至 Actions Artifacts 供直接下载测试。

## 开源协议

本项目采用 **GNU General Public License v3.0 (GPL-3.0)** 许可证。任何修改或派生作品在分发时均须遵循 GPL-3.0 开源并保留原作者版权声明。



参考源码：https://github.com/metasequoiaime/MSIME-Windows
