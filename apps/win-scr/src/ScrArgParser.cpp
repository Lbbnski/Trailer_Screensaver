#include "ScrArgParser.h"

#include <optional>

namespace ssv {

namespace {

std::optional<WId> tryParseWId(const QString& s)
{
    bool ok = false;
    const qulonglong v = s.toULongLong(&ok);
    if (!ok)
        return std::nullopt;
    return static_cast<WId>(v);
}

} // namespace

ScrArgs parseScrArgs(const QStringList& args)
{
    ScrArgs result;
    if (args.isEmpty())
        return result; // bare invocation -> Configure, per the documented contract

    const QString first = args.first();
    const QString firstLower = first.toLower();

    if (firstLower.startsWith(QStringLiteral("/s"))) {
        result.mode = ScrMode::RunFullscreen;
        return result;
    }

    if (firstLower.startsWith(QStringLiteral("/c"))) {
        result.mode = ScrMode::Configure;
        // Accepts both the "/c:HWND" form (older but still commonly emitted
        // by real screensaver hosts) and a separate "/c HWND" token.
        const int colon = first.indexOf(QLatin1Char(':'));
        QString hwndToken = colon >= 0 ? first.mid(colon + 1) : (args.size() > 1 ? args.at(1) : QString());
        if (const auto hwnd = tryParseWId(hwndToken))
            result.targetWindow = *hwnd;
        return result;
    }

    if (firstLower.startsWith(QStringLiteral("/p"))) {
        result.mode = ScrMode::Preview;
        const int colon = first.indexOf(QLatin1Char(':'));
        QString hwndToken = colon >= 0 ? first.mid(colon + 1) : (args.size() > 1 ? args.at(1) : QString());
        if (const auto hwnd = tryParseWId(hwndToken))
            result.targetWindow = *hwnd;
        return result;
    }

    return result; // unrecognized -> Configure
}

} // namespace ssv
