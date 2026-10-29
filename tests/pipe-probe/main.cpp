#include <Windows.h>
#include "ipc_negotiation.h"

#include <iostream>
#include <stdexcept>
#include <string>

class Pipe
{
  public:
    explicit Pipe(const wchar_t *name)
    {
        const auto deadline = GetTickCount64() + 15000;
        while (GetTickCount64() < deadline)
        {
            handle_ = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (handle_ != INVALID_HANDLE_VALUE)
            {
                DWORD mode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
                if (SetNamedPipeHandleState(handle_, &mode, nullptr, nullptr))
                    return;
                CloseHandle(handle_);
                handle_ = INVALID_HANDLE_VALUE;
                throw std::runtime_error("Could not configure probe pipe");
            }
            Sleep(10);
        }
        throw std::runtime_error("Server did not open its pipe within 15 seconds");
    }
    ~Pipe()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
    }
    Pipe(const Pipe &) = delete;
    Pipe &operator=(const Pipe &) = delete;

    template <typename T> void Write(const T &packet)
    {
        DWORD written = 0;
        if (!WriteFile(handle_, &packet, sizeof(packet), &written, nullptr) || written != sizeof(packet))
            throw std::runtime_error("Could not write a complete probe frame");
    }

    template <typename T> T Read()
    {
        const auto deadline = GetTickCount64() + 5000;
        while (GetTickCount64() < deadline)
        {
            T packet{};
            DWORD read = 0;
            if (ReadFile(handle_, &packet, sizeof(packet), &read, nullptr))
            {
                if (read == sizeof(packet))
                    return packet;
                if (read != 0)
                    throw std::runtime_error("Truncated probe frame");
            }
            else if (GetLastError() != ERROR_NO_DATA)
                throw std::runtime_error("Probe pipe disconnected before its expected reply");
            Sleep(5);
        }
        throw std::runtime_error("Server did not acknowledge the probe within 5 seconds");
    }

  private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

void Require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

unsigned ReadModeRepliesUntilFocusReady(Pipe &workers, std::uint64_t focus_token)
{
    unsigned mode_replies = 0;
    for (unsigned count = 0; count < 64; ++count)
    {
        const auto reply = workers.Read<FanyImeNamedpipeDataToTsfWorkerThread>();
        if (reply.msg_type == FanyImeWorkerReplyType::InputModeChanged)
            ++mode_replies;
        if (reply.msg_type == FanyImeWorkerReplyType::FocusSessionReady)
        {
            Require(std::wstring(reply.data) == std::to_wstring(focus_token), "Focus token changed");
            return mode_replies;
        }
    }
    throw std::runtime_error("Compatible client did not reach the active session barrier");
}

void ProbeStatusSnapshotConvergence(Pipe &main, Pipe &workers, FanyImeNamedpipeData &activation)
{
    FanyImeNamedpipeData snapshot{};
    snapshot.event_type = FanyImePipeEventType::StatusSnapshot;
    snapshot.client_id = activation.client_id;
    snapshot.keycode = 1;

    const auto check_snapshot = [&](unsigned expected_mode_replies) {
        main.Write(snapshot);
        // Same focus token: this explicit retry is idempotent and supplies an
        // ordered worker-pipe barrier after the preceding status snapshot.
        main.Write(activation);
        Require(ReadModeRepliesUntilFocusReady(workers, activation.request_id) == expected_mode_replies,
                "Repeated status snapshots amplified full policy replies");
    };
    check_snapshot(2); // First snapshot sync plus the explicit activation's sync.
    check_snapshot(1); // Only the explicit activation sync; an old DLL echo stops.
    check_snapshot(1);

    // A genuinely new focus session still gets its own first-snapshot sync.
    ++activation.request_id;
    main.Write(activation);
    Require(ReadModeRepliesUntilFocusReady(workers, activation.request_id) == 1,
            "New focus activation did not synchronize mode exactly once");
    check_snapshot(2);
    check_snapshot(1);
}

void Probe(unsigned sequence, bool legacy, bool incompatible, bool character_set_shortcut = false)
{
    const auto client = (static_cast<std::uint64_t>(GetCurrentProcessId()) << 32) | sequence;
    Pipe replies(FANY_IME_TO_TSF_NAMED_PIPE);
    FanyImePipeHello endpoint{};
    endpoint.client_id = client;
    endpoint.pipe_role = FanyImePipeRole::ToTsf;
    replies.Write(endpoint);
    Require(replies.Read<FanyImeNamedpipeDataToTsf>().msg_type == FanyImeReplyType::PipeReady,
            "Reply endpoint was not registered");
    Pipe workers(FANY_IME_TO_TSF_WORKER_THREAD_NAMED_PIPE);
    endpoint.pipe_role = FanyImePipeRole::ToTsfWorkerThread;
    workers.Write(endpoint);
    Require(workers.Read<FanyImeNamedpipeDataToTsfWorkerThread>().msg_type == FanyImeWorkerReplyType::PipeReady,
            "Worker endpoint was not registered");

    Pipe main(FANY_IME_NAMED_PIPE);
    auto hello = FanyImeProtocol::Hello(client, sequence,
                                        FanyImeProtocol::Capabilities |
                                            (character_set_shortcut ? FanyImeProtocol::CharacterSetShortcut : 0u));
    if (legacy)
    {
        hello = {};
        hello.client_id = client;
        hello.event_type = FanyImePipeEventType::ClientHello;
    }
    if (incompatible)
        ++hello.wch;
    main.Write(hello);
    if (!legacy)
    {
        const auto acknowledgement = replies.Read<FanyImeNamedpipeDataToTsf>();
        Require(acknowledgement.request_id == sequence, "Registration correlation was lost");
        if (incompatible)
        {
            Require(acknowledgement.msg_type == FanyImeReplyType::ProtocolMismatch,
                    "Server accepted an incompatible major protocol");
            return;
        }
        Require(FanyImeProtocol::AcceptReply(acknowledgement, sequence), "Versioned registration failed");
        Require(((FanyImeProtocol::ReplyCapabilities(acknowledgement) & FanyImeProtocol::CharacterSetShortcut) != 0) ==
                    character_set_shortcut,
                "Character-set shortcut capability was not negotiated correctly");
    }

    FanyImeNamedpipeData activation{};
    activation.event_type = FanyImePipeEventType::ClientActivated;
    activation.client_id = client;
    activation.request_id = sequence + 100;
    main.Write(activation);
    bool activated = false;
    for (int count = 0; count < 32; ++count)
    {
        const auto reply = workers.Read<FanyImeNamedpipeDataToTsfWorkerThread>();
        if (reply.msg_type == FanyImeWorkerReplyType::FocusSessionReady)
        {
            Require(std::wstring(reply.data) == std::to_wstring(activation.request_id), "Focus token changed");
            activated = true;
            break;
        }
    }
    Require(activated, "Compatible client did not reach the active session barrier");
    ProbeStatusSnapshotConvergence(main, workers, activation);
    activation.event_type = FanyImePipeEventType::ClientDeactivated;
    main.Write(activation);
}

int main()
{
    try
    {
        Probe(1, false, false);
        Probe(2, true, false);
        Probe(3, false, true);
        Probe(4, false, false); // reconnect after the rejected client
        Probe(5, false, false, true);
        Probe(1, false, false); // A new registration of the original client must sync again.
        std::cout
            << "Real Server: new client, legacy client, version rejection, reconnect, optional shortcut capability "
               "and bounded status synchronization passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
