#include "util/ProcessRunner.h"

#include <QProcess>

namespace ssv::ProcessRunner {

ProcessResult run(const QString& program, const QStringList& arguments, int timeoutMs)
{
    ProcessResult result;

    QProcess process;
    process.start(program, arguments);
    result.started = process.waitForStarted(timeoutMs);
    if (!result.started) {
        // Without this, a missing binary (the common case — yt-dlp not
        // installed or not on PATH) logs a completely blank error message,
        // indistinguishable from any other failure.
        result.stdErr = process.errorString().toUtf8();
        return result;
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        result.started = true;
        result.exitCode = -1;
        return result;
    }

    result.exitCode = process.exitCode();
    result.stdOut = process.readAllStandardOutput();
    result.stdErr = process.readAllStandardError();
    return result;
}

} // namespace ssv::ProcessRunner
