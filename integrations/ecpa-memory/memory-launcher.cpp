#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <string>
#include <vector>

namespace
{
bool IsRunning(const std::wstring &application)
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return true;
    PROCESSENTRY32W entry{sizeof(entry)};
    bool running = false;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process)
                continue;
            std::vector<wchar_t> path(32768);
            DWORD length = static_cast<DWORD>(path.size());
            if (QueryFullProcessImageNameW(process, 0, path.data(), &length) &&
                CompareStringOrdinal(path.data(), static_cast<int>(length), application.data(),
                                     static_cast<int>(application.size()), TRUE) == CSTR_EQUAL)
                running = true;
            CloseHandle(process);
        } while (!running && Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return running;
}

int Error(const wchar_t *message)
{
    MessageBoxW(nullptr, message, L"ECPA 内存预算启动器", MB_OK | MB_ICONINFORMATION);
    return 1;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int count = 0;
    wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments)
        return 1;
    std::wstring application;
    std::wstring budget = L"512MiB";
    bool selfTest = false;
    for (int index = 1; index < count; ++index)
    {
        const std::wstring argument(arguments[index]);
        if (argument == L"--app" && index + 1 < count)
            application = arguments[++index];
        else if (argument == L"--budget" && index + 1 < count)
            budget = arguments[++index];
        else if (argument == L"--self-test")
            selfTest = true;
        else
        {
            LocalFree(arguments);
            return Error(L"参数无效。用法：--app EasyCLIProxyAPI.exe的完整路径 --budget 512MiB");
        }
    }
    LocalFree(arguments);
    if (budget != L"256MiB" && budget != L"384MiB" && budget != L"512MiB" && budget != L"768MiB" &&
        budget != L"1024MiB")
        return selfTest ? 2 : Error(L"内存预算可选 256、384、512、768 或 1024MiB。");
    if (selfTest)
        return 0;
    if (application.empty() || application.find(L'"') != std::wstring::npos)
        return Error(L"请通过快捷方式启动，或使用 --app 指定 ECPA 程序的完整路径。");
    std::vector<wchar_t> path(32768);
    const DWORD length = GetFullPathNameW(application.c_str(), static_cast<DWORD>(path.size()), path.data(), nullptr);
    if (!length || length >= path.size())
        return Error(L"ECPA 程序路径无效。");
    application.assign(path.data(), length);
    const DWORD attributes = GetFileAttributesW(application.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return Error(L"找不到 ECPA 程序，请更新快捷方式中的路径。");
    if (IsRunning(application))
        return Error(L"ECPA 已运行。请先从 ECPA 托盘菜单退出，再打开此快捷方式。正在运行的请求不会被启动器中止。");
    if (!SetEnvironmentVariableW(L"GOMEMLIMIT", budget.c_str()))
        return Error(L"无法设置本次启动的 Go 内存预算。");
    const size_t separator = application.find_last_of(L"\\/");
    const std::wstring directory = application.substr(0, separator);
    const std::wstring command = L"\"" + application + L"\"";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(application.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE,
                        CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT, nullptr, directory.c_str(),
                        &startup, &process))
        return Error(L"ECPA 启动失败。请检查程序路径和当前账户权限。");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
