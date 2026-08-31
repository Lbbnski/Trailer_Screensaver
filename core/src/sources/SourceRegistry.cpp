#include "sources/SourceRegistry.h"

namespace ssv {

void SourceRegistry::registerSource(std::unique_ptr<IMetadataSource> source)
{
    IMetadataSource* raw = source.get();
    m_byId.insert(raw->id(), raw);
    m_sources.push_back(raw);
    m_owned.push_back(std::move(source));
}

const std::vector<IMetadataSource*>& SourceRegistry::sources() const
{
    return m_sources;
}

IMetadataSource* SourceRegistry::find(const QString& sourceId) const
{
    return m_byId.value(sourceId, nullptr);
}

} // namespace ssv
