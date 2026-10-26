"""Extract production focus activation probes for a standalone virtual-clock test."""

from pathlib import Path
import sys

# Reuse the source extractor without producing bytecode beside the tests.
sys.dont_write_bytecode = True
from generate_commit_reply_fixture import find_body, find_declaration


def generate(ipc_header, ipc_source, worker_source, output):
    header = Path(ipc_header).read_text(encoding="utf-8")
    source = Path(ipc_source).read_text(encoding="utf-8")
    markers = (
        "void ResetFocusActivationRetry()",
        "bool IsLocalSessionResetPending()",
        "bool IsExpectedWorkerFocusAcknowledged()",
        "void RequireNamedpipeFocusActivation()",
        "bool EnsureNamedpipeFocusSessionActivated(",
    )
    fixture = "// Generated from production focus activation paths; do not edit.\n"
    fixture += find_declaration(header, "bool EnsureNamedpipeFocusSessionActivated(") + "\n"
    for declaration in (
        "constexpr DWORD FOCUS_ACTIVATION_RETRY_INITIAL_MS",
        "constexpr DWORD FOCUS_ACTIVATION_RETRY_MAX_MS",
        "thread_local ULONGLONG focusActivationRetryAtMs",
        "thread_local DWORD focusActivationRetryDelayMs",
    ):
        fixture += find_declaration(source, declaration) + "\n"
    for marker in markers:
        signature, body = find_body(source, marker)
        fixture += f"{signature}\n{body}\n"
    worker = Path(worker_source).read_text(encoding="utf-8")
    _, ready_body = find_body(
        worker,
        "if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::FocusSessionReady)",
    )
    fixture += (
        "void ApplyFocusReadyAck(FocusAckService *pIME, uint64_t focusToken)\n"
        + ready_body + "\n"
    )
    Path(output).write_text(fixture, encoding="utf-8")


if __name__ == "__main__":
    generate(*sys.argv[1:])
