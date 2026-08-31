#include "config/Config.h"
#include "config/ConfigPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace ssv {

namespace {

QString toString(MaxResolution r)
{
    return r == MaxResolution::P480 ? QStringLiteral("480p") : QStringLiteral("max");
}

MaxResolution maxResolutionFromString(const QString& s)
{
    return s == QStringLiteral("480p") ? MaxResolution::P480 : MaxResolution::Max;
}

QString toString(MonitorMode m)
{
    return m == MonitorMode::PrimaryOnly ? QStringLiteral("primaryOnly") : QStringLiteral("independent");
}

MonitorMode monitorModeFromString(const QString& s)
{
    return s == QStringLiteral("primaryOnly") ? MonitorMode::PrimaryOnly : MonitorMode::Independent;
}

QString toString(GenreFilter::Mode m)
{
    return m == GenreFilter::Mode::BlockList ? QStringLiteral("blacklist") : QStringLiteral("allowlist");
}

GenreFilter::Mode filterModeFromString(const QString& s)
{
    return s == QStringLiteral("blacklist") ? GenreFilter::Mode::BlockList : GenreFilter::Mode::AllowList;
}

QStringList toStringList(const QJsonArray& arr)
{
    QStringList out;
    out.reserve(arr.size());
    for (const auto& v : arr)
        out << v.toString();
    return out;
}

QJsonArray toJsonArray(const QStringList& list)
{
    QJsonArray arr;
    for (const auto& s : list)
        arr.append(s);
    return arr;
}

} // namespace

QByteArray Config::toJson() const
{
    QJsonObject playbackObj{
        {"maxResolution", toString(playback.maxResolution)},
        {"monitorMode", toString(playback.monitorMode)},
        {"muted", playback.muted},
        {"hardwareDecode", playback.hardwareDecode},
    };

    QJsonObject filterObj{
        {"mode", toString(filter.mode)},
        {"genres", toJsonArray(filter.genres)},
        {"maxAge", filter.maxAge},
        {"blockedContentDescriptors", toJsonArray(filter.blockedContentDescriptors)},
        {"preferPopular", filter.preferPopular},
    };

    QJsonObject steamObj{
        {"language", sources.steamLanguage},
        {"countryCode", sources.steamCountryCode},
    };
    QJsonObject sourcesObj{
        {"enabled", toJsonArray(sources.enabled)},
        {"steam", steamObj},
    };

    QJsonObject advancedObj{
        {"cacheTtlDaysAppDetails", advanced.cacheTtlDaysAppDetails},
        {"cacheTtlDaysCandidateList", advanced.cacheTtlDaysCandidateList},
        {"maxCatalogRequestsPerRun", advanced.maxCatalogRequestsPerRun},
        {"ytDlpPath", advanced.ytDlpPath},
        {"debugOverlay", advanced.debugOverlay},
    };

    QJsonObject root{
        {"version", version},
        {"playback", playbackObj},
        {"filter", filterObj},
        {"sources", sourcesObj},
        {"advanced", advancedObj},
    };

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

Config Config::fromJson(const QByteArray& json, bool* ok)
{
    Config cfg; // defaults
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (ok) *ok = false;
        return cfg;
    }

    const auto root = doc.object();
    cfg.version = root.value("version").toInt(kCurrentVersion);

    const auto playbackObj = root.value("playback").toObject();
    cfg.playback.maxResolution = maxResolutionFromString(playbackObj.value("maxResolution").toString());
    cfg.playback.monitorMode = monitorModeFromString(playbackObj.value("monitorMode").toString());
    cfg.playback.muted = playbackObj.value("muted").toBool(true);
    cfg.playback.hardwareDecode = playbackObj.value("hardwareDecode").toBool(true);

    const auto filterObj = root.value("filter").toObject();
    cfg.filter.mode = filterModeFromString(filterObj.value("mode").toString());
    cfg.filter.genres = toStringList(filterObj.value("genres").toArray());
    cfg.filter.maxAge = filterObj.value("maxAge").toInt(18);
    cfg.filter.blockedContentDescriptors = toStringList(filterObj.value("blockedContentDescriptors").toArray());
    cfg.filter.preferPopular = filterObj.value("preferPopular").toBool(false);

    const auto sourcesObj = root.value("sources").toObject();
    cfg.sources.enabled = toStringList(sourcesObj.value("enabled").toArray());
    if (cfg.sources.enabled.isEmpty())
        cfg.sources.enabled << QStringLiteral("steam");
    const auto steamObj = sourcesObj.value("steam").toObject();
    cfg.sources.steamLanguage = steamObj.value("language").toString(QStringLiteral("english"));
    cfg.sources.steamCountryCode = steamObj.value("countryCode").toString(QStringLiteral("US"));

    const auto advancedObj = root.value("advanced").toObject();
    cfg.advanced.cacheTtlDaysAppDetails = advancedObj.value("cacheTtlDaysAppDetails").toInt(21);
    cfg.advanced.cacheTtlDaysCandidateList = advancedObj.value("cacheTtlDaysCandidateList").toInt(7);
    cfg.advanced.maxCatalogRequestsPerRun = advancedObj.value("maxCatalogRequestsPerRun").toInt(40);
    cfg.advanced.ytDlpPath = advancedObj.value("ytDlpPath").toString(QStringLiteral("yt-dlp"));
    cfg.advanced.debugOverlay = advancedObj.value("debugOverlay").toBool(false);

    if (ok) *ok = true;
    return cfg;
}

Config Config::load()
{
    QFile file(ConfigPaths::configFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return Config{}; // defaults — first run, or unreadable

    bool ok = false;
    Config cfg = fromJson(file.readAll(), &ok);
    return ok ? cfg : Config{};
}

bool Config::save() const
{
    const QString path = ConfigPaths::configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    return file.write(toJson()) >= 0;
}

} // namespace ssv
