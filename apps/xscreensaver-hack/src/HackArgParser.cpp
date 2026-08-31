#include "HackArgParser.h"

#include <QRegularExpression>

#include <optional>

namespace ssv {

namespace {

std::optional<WId> parseXidToken(const QString& token)
{
    bool ok = false;
    qulonglong value = 0;
    if (token.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        value = token.toULongLong(&ok, 16);
    else
        value = token.toULongLong(&ok, 10);
    return ok ? std::make_optional(static_cast<WId>(value)) : std::nullopt;
}

// $XSCREENSAVER_WINDOW's exact format varies across xscreensaver versions
// (a bare id, or "<id> (<visual>)"), so this pulls out just the leading
// numeric/hex token rather than assuming one exact format.
std::optional<WId> parseXScreenSaverWindowEnv(const QString& value)
{
    static const QRegularExpression leadingToken(QStringLiteral(R"(^\s*(0x[0-9a-fA-F]+|\d+))"));
    const auto match = leadingToken.match(value);
    if (!match.hasMatch())
        return std::nullopt;
    return parseXidToken(match.captured(1));
}

} // namespace

HackArgs parseHackArgs(const QStringList& args, const QProcessEnvironment& env)
{
    HackArgs result;

    for (int i = 0; i < args.size(); ++i) {
        const QString& arg = args.at(i);

        if (arg == QStringLiteral("--configure")) {
            result.mode = HackMode::Configure;
            return result; // explicit request always wins, ignore $XSCREENSAVER_WINDOW below
        }
        if (arg == QStringLiteral("-window-id") && i + 1 < args.size()) {
            if (const auto xid = parseXidToken(args.at(++i))) {
                result.mode = HackMode::RunEmbedded;
                result.windowId = *xid;
            }
            continue;
        }
        if (arg == QStringLiteral("-max-resolution") && i + 1 < args.size()) {
            result.use480p = (args.at(++i) == QStringLiteral("480"));
            continue;
        }
        if (arg == QStringLiteral("-max-age") && i + 1 < args.size()) {
            bool ok = false;
            const int age = args.at(++i).toInt(&ok);
            if (ok)
                result.maxAge = age;
            continue;
        }
        if (arg == QStringLiteral("-genre-mode") && i + 1 < args.size()) {
            result.blacklistMode = (args.at(++i) == QStringLiteral("blacklist"));
            continue;
        }
        if (arg == QStringLiteral("-genres") && i + 1 < args.size()) {
            const auto list = args.at(++i).split(QLatin1Char(','), Qt::SkipEmptyParts);
            QStringList trimmed;
            for (const auto& g : list)
                trimmed << g.trimmed();
            result.genres = trimmed;
            continue;
        }
    }

    if (result.mode == HackMode::RunEmbedded)
        return result;

    if (const auto xid = parseXScreenSaverWindowEnv(env.value(QStringLiteral("XSCREENSAVER_WINDOW")))) {
        result.mode = HackMode::RunEmbedded;
        result.windowId = *xid;
        return result;
    }

    return result; // no window id available anywhere -> Configure
}

} // namespace ssv
