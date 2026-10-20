阿龍学习输入法 0.8.2：API 接口测试版。

本版沿用 0.8.1 的输入与翻译实现，同步版本资源并补充密钥/API 配置和实际按键说明。用于测试用户自行配置的 OpenAI 兼容接口，不包含 HY-MT 翻译模型或 llama.cpp 翻译运行库，也不会自动下载这些组件。

使用方式：设置 → AI 与云联想，填写 Base URL、访问密钥和模型，先检查连接，再开启整句翻译。输入拼音后按空格/数字选中文并积累整句；Ctrl+Enter 确认英文，译文未就绪时等待后再次确认；Enter 提交中文，Shift 切换中文输出/英文翻译。

本版尚未加入单次 Enter 等待后自动上屏及 SentenceAutoCommit 交互。真实宿主行为和你所选接口的延迟、质量仍需测试；不承诺 1 秒内翻译完成。默认 127.0.0.1:8317 地址需要本机已有相应接口服务。

配置与限制说明：https://github.com/daodeqing/Arong-Study-IME/blob/v0.8.2/docs/api-translation-testing.md

Windows x64/x86 完整安装包由 GitHub Actions 构建并运行自动测试。此构建未签名，uiAccess 不保证生效；SHA-256 随安装包发布。此次发布不自动安装或替换本机输入法。
