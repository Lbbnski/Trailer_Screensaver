#include "GenreListModel.h"

#include "sources/GenreTaxonomy.h"

namespace ssv {

GenreListModel::GenreListModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_genres(GenreTaxonomy::canonicalGenres())
{
}

int GenreListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_genres.size();
}

QVariant GenreListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_genres.size())
        return {};

    const auto& genre = m_genres.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return genre;
    case Qt::CheckStateRole:
        return m_checked.contains(genre) ? Qt::Checked : Qt::Unchecked;
    default:
        return {};
    }
}

bool GenreListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::CheckStateRole)
        return false;

    const auto& genre = m_genres.at(index.row());
    if (value.toInt() == Qt::Checked)
        m_checked.insert(genre);
    else
        m_checked.remove(genre);

    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

Qt::ItemFlags GenreListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

void GenreListModel::setCheckedGenres(const QStringList& genres)
{
    beginResetModel();
    m_checked = QSet<QString>(genres.begin(), genres.end());
    endResetModel();
}

QStringList GenreListModel::checkedGenres() const
{
    QStringList result;
    for (const auto& g : m_genres) {
        if (m_checked.contains(g))
            result << g;
    }
    return result;
}

} // namespace ssv
