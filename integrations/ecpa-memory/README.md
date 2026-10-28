# ECPA 的 Go 内存预算

`memory-launcher.cpp` 编译为无控制台窗口的启动器。它只给本次启动的 ECPA 及其子进程设置 `GOMEMLIMIT=512MiB`，然后退出；ECPA 不依赖此启动器或开发工具继续运行。它不会中止已经运行的 ECPA。

Go 将这个数值视为运行时管理内存的**软预算**，会据此更积极地回收可回收的堆。它不限制上游模型的上下文或输出，不改变 API 请求、重试、路由、超时、缓存语义和请求统计。原生插件、系统映射和 Go 以外的内存不在此预算内；进程总内存仍可能超过预算。过低预算可能增加 GC CPU 开销，不能将进程内存必定低于512MiB或请求必定更快作为承诺。

```text
EcpaMemoryLauncher.exe --app "D:\完整路径\EasyCLIProxyAPI.exe" --budget 512MiB
```

可选预算为256、384、512、768、1024MiB。启动器不会写系统环境变量或修改 ECPA 配置。需要退出旧 ECPA 并从此入口重新启动，已经运行的进程不会自动获得新环境变量。

首次验收应在没有进行中请求的时段重启，再检查 `cli-proxy-api.exe` 的 `GOMEMLIMIT` 环境变量，并以相同请求比较空闲、连续翻译和长请求后的工作集、私有提交、CPU和延迟。当前运行中的代理是多个应用可共用的服务，重启验收需由用户选择时机。

来源：[Go GC Guide — Memory limit](https://go.dev/doc/gc-guide#Memory_limit)。已核对 ECPA 的 `start_core_process_once` 通过普通子进程继承环境，未覆盖 `GOMEMLIMIT`。本轮不是 ECPA 上游源码版本升级。
