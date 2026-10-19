# 阿龍学习输入法发布与追溯

本仓一个提交固定 TSF、Server、Engine、GUI 与安装器源码。`vendor/opencc` 与
`vendor/cpp-pinyin` 使用固定 gitlink，构建前运行 `python scripts/ci/verify-vendor.py`。
`product-lock.json` 固定外部词库 Release、来源提交与 SHA-256；模型使用各自的锁文件。

## 0.8.0 正式构建

在 `Build and Package Release` 工作流派发 `version=0.8.0`、`package_type=full`。
`package-release.yml` 使用 GitHub-hosted `windows-2025` 构建 x64/Win32 TSF、Server 和
Engine，运行 ctest 后打包完整词库与模型，再发布 `v0.8.0`。也支持 `v*` 标签触发。
版本请求必须与 `version.txt` 和 TSF 资源一致。

产物为 `ArongStudyIme_Setup_v0.8.0.exe` 及 `.exe.sha256`。当前流水线不签名。
只有 workflow 成功、公开 Release 资产存在且 SHA-256 一致时才能确认发布完成。
安装目录与默认数据目录使用 `ArongStudyIme`，不读取或迁移原产品安装与旧数据。

## 可选自动发布路径

`release.yml` 中 release-please 仅在配置 `RELEASE_PLEASE_TOKEN` 后运行。缺少该 secret
时跳过自动元数据路径，使用上面的 `package-release.yml`。签名路径还需要仓库所有者
独立配置证书或 SignPath 项目；上游签名授权不属于本仓。

`main` 是本仓默认与发布分支，`develop` 用于日常开发。当前源码与产品输入需要一并评审，
不可在发布过程中替换已锁定的依赖、词库或模型摘要。历史来源见
[engine/UPSTREAM.md](../engine/UPSTREAM.md)。

## 本地检查

```sh
python scripts/apply_version.py --check
python scripts/ci/verify-vendor.py
python scripts/product_lock.py validate
python -m unittest discover -s tests -p 'test_product_lock.py'
```
