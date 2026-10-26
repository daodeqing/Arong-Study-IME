# 阿龍学习输入法

<!-- impeccable:product-schema 1 -->

## Platform

Windows desktop, native Win32 / Direct2D / DirectWrite.

## Users

用户在 Windows 文本编辑器、浏览器等输入框中连续输入中文与英文缩写，并通过整句英文翻译学习四六级词汇。此使用场景来自本次对话；其他用户群尚未确认。

## Product Purpose

使中文输入、英文译文提交和重点词学习在同一输入过程中完成。设置界面让用户调整输入方式、外观、API 连接及已有辅助功能。

## Operating Context

TSF DLL 与常驻后端通过版本化命名管道通信。设置窗口为独立进程；候选窗、悬浮工具栏和托盘菜单由后端的 Direct2D presenter 展示。项目运行独立于开发工具。

## Capabilities and Constraints

- 保留现有输入行为、翻译模型、请求时序与超时设置，以及已经实现的 Enter、长译文、缩写与四六级词卡功能。
- 翻译使用现有 API 连接；不得恢复先前移除的 HY-MT / llama.cpp 本地翻译方案。
- 界面保留现有功能与用户配置，使用原生控件库，不引入 WebView 或网页前端。
- 本次请求明确要求更新界面及全部产品图标，并检查界面和输入引擎使用过程中的内存。
- 内存结论必须区分工作集、私有提交内存和可复现基准；编译通过不代表输入框实测通过。
- 本地新增文件仅放在本项目目录，临时内容移入回收站；不自动替换本机安装版本。

## Brand Commitments

保留产品名“阿龍学习输入法”和已有事实性功能说明。中文优先，英文术语在必要时保留。

## Evidence on Hand

`server/src/settings/native/`、`server/src/window/`、`ui/` 为现有界面实现。`docs/images/` 包含旧界面截图，旧截图中的品牌与当前代码已不同，不能当作当前构建的截图。`dist/github-build/c34876206b4f/` 保存上一轮测试安装包。

## Product Principles

- 输入反馈清晰，输入不中断。
- 译文与学习说明分开呈现，提交只包含用户选择的文本。
- 对长时间运行的缓存设可解释的资源边界。
- 深浅主题、高 DPI 和原生窗口操作保持可用。

## Open Decisions

用户所说“内核”是否还包括输入引擎以外的组件已询问，尚未收到补充。本轮先检查已经明确请求的输入引擎及界面，不扩大到其他系统组件。
