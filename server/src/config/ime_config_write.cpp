// 把设置写回 config.toml（批量、保留格式、先校验再原子替换），并通知 Server 重新加载配置或切换输入方案。
#include "config/ime_config_internal.h"
#include <Windows.h>
#include <cwchar>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
#include "defines/defines.h"
#include "ipc/ipc.h"

using namespace ime_config_detail;

namespace ime_config_detail
{
bool WriteConfiguredValues(const std::vector<ConfigValueUpdate> &updates)
{
    ConfigFileLock lock;
    if (!lock)
        return false;
    std::error_code exists_error;
    const bool config_exists = std::filesystem::is_regular_file(g_config_path, exists_error);
    const auto config_size =
        config_exists ? std::filesystem::file_size(g_config_path, exists_error) : std::uintmax_t{0};
    std::string text = ReadFileText(g_config_path);
    if (!TomlTextIsParseable(text))
    {
        if (config_exists && config_size > 0 && text.empty())
        {
            return false;
        }
        text = ReadFileText(g_config_path.parent_path() / kConfigTemplateFileName);
        if (!TomlTextIsParseable(text))
        {
            return false;
        }
    }

    for (const auto &update : updates)
    {
        // 所有凭证写入都经过这里，在此统一加密，不必让每个 setter 各自记得。
        const auto value = ProtectCredentialLiteral(update.key, update.value);
        if (!value)
            return false;
        if (!ReplaceTomlValuePreservingFormatting(text, update.section, update.key, *value) &&
            !InsertTomlValuePreservingFormatting(text, update.section, update.key, *value))
        {
            return false;
        }
    }

    try
    {
        (void)toml::parse(text);
    }
    catch (const toml::parse_error &)
    {
        return false;
    }

    // Write a temp file and rename it so a crash can never leave the user with a truncated config. This only makes the
    // directory entry swap atomic: there is no FlushFileBuffers, so a power loss can still lose the contents.
    if (!WriteFileTextAtomically(g_config_path, text))
    {
        return false;
    }

    RememberConfigWriteTime();
    NotifyImeServerConfigChanged();
    return true;
}

bool WriteConfiguredValue(const std::string &section, const std::string &key, const std::string &replacement)
{
    return WriteConfiguredValues({{section, key, replacement}});
}
} // namespace ime_config_detail

namespace
{
HWND FindImeServerCandidateWindow()
{
    // Candidate, tray menu, and floating toolbar share the same class. FindWindow
    // without a title often hits the toolbar, which does not apply config.
    if (const HWND hwnd = FindWindowW(L"ArongStudyIme_windows", L"ArongStudyImecandwnd"))
    {
        return hwnd;
    }
    return FindWindowW(L"ArongStudyIme_windows", nullptr);
}

bool SendAuxConfigNotification(const wchar_t *message, bool waitForAcknowledgement = false)
{
    const DWORD access = waitForAcknowledgement ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_WRITE;
    HANDLE pipe = CreateFileW(FANY_IME_AUX_NAMED_PIPE, access, 0, nullptr, OPEN_EXISTING,
                              SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    if (pipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY &&
        WaitNamedPipeW(FANY_IME_AUX_NAMED_PIPE, 500))
    {
        pipe = CreateFileW(FANY_IME_AUX_NAMED_PIPE, access, 0, nullptr, OPEN_EXISTING,
                           SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    }
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD bytesWritten = 0;
    const DWORD byteCount = static_cast<DWORD>(wcslen(message) * sizeof(wchar_t));
    const bool sent = WriteFile(pipe, message, byteCount, &bytesWritten, nullptr) != FALSE && bytesWritten == byteCount;
    if (!sent || !waitForAcknowledgement)
    {
        CloseHandle(pipe);
        return sent;
    }

    DWORD pipeMode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
    if (!SetNamedPipeHandleState(pipe, &pipeMode, nullptr, nullptr))
    {
        CloseHandle(pipe);
        return false;
    }

    bool acknowledged = false;
    const ULONGLONG deadline = GetTickCount64() + 500;
    while (GetTickCount64() < deadline)
    {
        wchar_t acknowledgement[2] = {};
        DWORD bytesRead = 0;
        if (ReadFile(pipe, acknowledgement, sizeof(acknowledgement), &bytesRead, nullptr))
        {
            acknowledged =
                bytesRead == sizeof(acknowledgement) && acknowledgement[0] == L'O' && acknowledgement[1] == L'K';
            break;
        }
        if (GetLastError() != ERROR_NO_DATA)
            break;
        Sleep(1);
    }
    CloseHandle(pipe);
    return acknowledged;
}
} // namespace

namespace ime_config_detail
{
void NotifyImeServer(UINT windowMessage, const wchar_t *auxMessage, WPARAM wParam)
{
    const HWND hwnd = FindImeServerCandidateWindow();
    DWORD serverProcessId = 0;
    if (hwnd)
    {
        GetWindowThreadProcessId(hwnd, &serverProcessId);
    }

    if (serverProcessId == GetCurrentProcessId())
    {
        PostMessageW(hwnd, windowMessage, wParam, 0);
        return;
    }

    // ArongStudyImeServer runs with uiAccess while the standalone Settings
    // process does not. Cross-process WM_USER delivery can therefore be
    // rejected by UIPI. The session-less Aux pipe is already the supported
    // cross-integrity control path; use it for config invalidation as well.
    if (!SendAuxConfigNotification(auxMessage) && hwnd)
    {
        // Retain the old route as a best-effort fallback. The Server's periodic
        // file watcher remains the final recovery path if both transports are
        // temporarily unavailable during startup.
        PostMessageW(hwnd, windowMessage, wParam, 0);
    }
}
} // namespace ime_config_detail

void NotifyImeServerConfigChanged()
{
    NotifyImeServer(WM_APPLY_IME_CONFIG, L"ConfigChanged");
}

void NotifyImeServerCandidateSkinRefresh()
{
    NotifyImeServer(WM_APPLY_IME_CONFIG, L"CandidateSkinRefresh", 1);
}

void NotifyImeServerInputSchemeChanged()
{
    NotifyImeServer(WM_APPLY_IME_INPUT_SCHEME, L"InputSchemeChanged");
}

bool NotifyImeServerRestart()
{
    // No window message carries a restart, so there is no PostMessage fallback
    // here: if the Aux pipe is unavailable the Server is not running anyway, and
    // the caller reports that instead of silently doing nothing.
    return SendAuxConfigNotification(L"RestartServer", true);
}
