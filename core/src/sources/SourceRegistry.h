#pragma once

#include "sources/IMetadataSource.h"

#include <QHash>
#include <QString>
#include <memory>
#include <vector>

namespace ssv {

// Holds the set of enabled IMetadataSource instances (config-driven via
// sources.enabled) and is the only thing TrailerResolver / PlaylistEngine
// talk to for source access — neither knows Steam exists specifically.
// Adding a future source is registering it here (typically from a small
// factory in the app's startup code), nothing else in core needs to change.
class SourceRegistry {
public:
    void registerSource(std::unique_ptr<IMetadataSource> source);

    // Sources in registration order; callers that want to express priority
    // between sources (e.g. try Steam before a future GOG source) do so by
    // registering in that order.
    const std::vector<IMetadataSource*>& sources() const;

    IMetadataSource* find(const QString& sourceId) const;

private:
    std::vector<std::unique_ptr<IMetadataSource>> m_owned;
    std::vector<IMetadataSource*> m_sources;
    QHash<QString, IMetadataSource*> m_byId;
};

} // namespace ssv
