#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QStringList>

namespace ssv {

// Checkable list model over GenreTaxonomy::canonicalGenres(), used for the
// genre picker in SettingsDialog. The same widget/model serves both filter
// modes (allow-list or block-list) — SettingsDialog just changes the label
// and how the checked set is written back into FilterConfig.
class GenreListModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit GenreListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setCheckedGenres(const QStringList& genres);
    QStringList checkedGenres() const;

private:
    QStringList m_genres;
    QSet<QString> m_checked;
};

} // namespace ssv
