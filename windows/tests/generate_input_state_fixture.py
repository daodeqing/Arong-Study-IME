"""Extract worker state snapshots and the owner-thread translation mode handler."""

from pathlib import Path
import sys

sys.dont_write_bytecode = True
from generate_commit_reply_fixture import find_body


def generate(worker_source, window_source, output):
    worker = Path(worker_source).read_text(encoding="utf-8")
    window = Path(window_source).read_text(encoding="utf-8")
    fixture = "// Generated from production input-state snapshot paths; do not edit.\n"
    _, generation_body = find_body(
        worker, "if (inputStateSnapshotGeneration != workerGeneration)"
    )
    fixture += (
        "void WorkerSnapshots::ResetGeneration(UINT workerGeneration)\n{\n"
        + "if (inputStateSnapshotGeneration != workerGeneration)\n"
        + generation_body + "\n}\n"
    )
    for name, opcode in (
        ("InputMode", "InputModeChanged"),
        ("OutputLanguage", "OutputLanguageChanged"),
        ("SentenceTranslation", "SentenceTranslationChanged"),
    ):
        _, body = find_body(
            worker,
            "else if (buf.msg_type == Global::DataToTsfWorkerThreadMsgType::"
            + opcode + ")",
        )
        # continue in a worker branch ends this one-frame fixture invocation.
        fixture += (
            f"void WorkerSnapshots::{name}(CMetasequoiaIME *pIME, const Frame &buf)\n"
            + "{\ndo\n" + body + "\nwhile (false);\n}\n"
        )
    _, apply_body = find_body(window, "case WM_ApplySentenceTranslation:")
    # The extracted case ends with break, which exits this single-shot scope.
    fixture += (
        "void ApplySentenceTranslation(CMetasequoiaIME *pIME)\n{\ndo\n"
        + apply_body + "\nwhile (false);\n}\n"
    )
    Path(output).write_text(fixture, encoding="utf-8")


if __name__ == "__main__":
    generate(*sys.argv[1:])
