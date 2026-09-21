#include "sources/steam/TrailerHeuristics.h"

#include <QRegularExpression>
#include <QSet>

namespace ssv::TrailerHeuristics {

namespace {

// UseUnicodePropertiesOption so \w / \b cover non-ASCII letters (accented
// Latin, Cyrillic, CJK, ...) instead of just [A-Za-z0-9_].
QRegularExpression makeRegex(const QString& pattern)
{
    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption
                                           | QRegularExpression::UseUnicodePropertiesOption);
}

const QRegularExpression& trailerWordPattern()
{
    // Not \b-anchored on purpose: several of these are CJK (no word
    // boundaries) or part of compounds ("trailer" inside "gametrailer").
    static const auto re = makeRegex(QStringLiteral(
        "trailer|teaser|announce|reveal|cinematic|coming soon|out now|available now|wishlist|game intro"
        "|official (\\w+ ){0,2}(video|preview|showcase|intro)"
        "|bande[- ]annonce|tr[aá]iler|трейлер|予告|预告|預告|トレーラー|티저"
        "|nintendo direct|state of play|inside xbox"));
    return re;
}

const QRegularExpression& nonTrailerPattern()
{
    static const auto re = makeRegex(QStringLiteral(
        "\\b(walkthrough|let'?s play|playthrough|reactions?|reacts?|breakdown|unpacks?|explained|analysis"
        "|review|interview|behind the scenes|deep dive|talk|panel|podcast|documentary|making of"
        "|commentary|impressions?|tutorial|guide|ranked|everything we know|leaks?|rumou?rs?|vs)\\b"));
    return re;
}

const QRegularExpression& movieTitlePattern()
{
    static const auto re = makeRegex(QStringLiteral(
        "\\b(movie|film|tv series|tv spot|season \\d+|episode \\d+)\\b"));
    return re;
}

const QRegularExpression& movieSignalPatterns_(int index)
{
    static const QList<QRegularExpression> res{
        makeRegex(QStringLiteral("\\bin (theaters|theatres|cinemas)\\b")),
        makeRegex(QStringLiteral("\\b(movie|film) trailer\\b|\\bofficial (movie|film)\\b")),
        makeRegex(QStringLiteral("\\btv (series|show)\\b")),
        makeRegex(QStringLiteral("\\b(netflix|hbo|prime video|disney\\+|hulu|apple tv\\+)")),
        makeRegex(QStringLiteral("\\b(paramount|warner bros\\.? pictures|universal pictures|sony pictures"
                                 "|20th century|lionsgate|a24|marvel studios|box office)\\b")),
    };
    return res[index];
}
constexpr int kMovieSignalKinds = 5;

const QRegularExpression& gameContextPattern()
{
    static const auto re = makeRegex(QStringLiteral(
        "game|steam|playstation|\\bps[45]\\b|xbox|nintendo|\\bswitch\\b|\\bpc\\b|indie|\\bgog\\b|epic games"
        "|\\bdlc\\b|early access|wishlist|developer|studio"));
    return re;
}

// Lowercased letters/digits only — for comparing "3050 A.D." against
// "3050AD" and "Spider-Man" against "Spider Man".
QString normalized(const QString& s)
{
    static const auto nonWord = makeRegex(QStringLiteral("[^\\w]+"));
    QString out = s.toLower();
    out.remove(nonWord);
    return out;
}

QStringList meaningfulWords(const QString& gameTitle)
{
    static const auto wordSplit = makeRegex(QStringLiteral("[^\\w]+"));
    QStringList out;
    for (const auto& w : gameTitle.toLower().split(wordSplit, Qt::SkipEmptyParts)) {
        if (w.length() >= 3) // short/common words would match almost anything
            out << w;
    }
    return out;
}

QString firstMatchNotInGameTitle(const QRegularExpression& re, const QString& gameTitle, const QString& text)
{
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QString word = it.next().captured(0);
        if (!gameTitle.contains(word, Qt::CaseInsensitive))
            return word;
    }
    return {};
}

} // namespace

bool titleMatchesGame(const QString& gameTitle, const QString& videoTitle)
{
    const QStringList words = meaningfulWords(gameTitle);
    if (words.isEmpty())
        return true; // all short/symbolic words — nothing meaningful to check, don't block

    const QString lowerVideo = videoTitle.toLower();
    const QString normVideo = normalized(videoTitle);

    int matched = 0;
    for (const auto& w : words) {
        if (lowerVideo.contains(w) || normVideo.contains(normalized(w)))
            ++matched;
    }

    return words.size() <= 2 ? matched == words.size() : matched * 2 >= words.size();
}

bool hasTrailerWord(const QString& videoTitle)
{
    return trailerWordPattern().match(videoTitle).hasMatch();
}

QString nonTrailerWord(const QString& gameTitle, const QString& videoTitle)
{
    return firstMatchNotInGameTitle(nonTrailerPattern(), gameTitle, videoTitle);
}

QString movieWordInTitle(const QString& gameTitle, const QString& videoTitle)
{
    return firstMatchNotInGameTitle(movieTitlePattern(), gameTitle, videoTitle);
}

bool isFilmCategory(const QStringList& categories)
{
    static const QSet<QString> film{
        QStringLiteral("Film & Animation"), QStringLiteral("Movies"),
        QStringLiteral("Shows"), QStringLiteral("Trailers"),
    };
    for (const auto& c : categories) {
        if (film.contains(c))
            return true;
    }
    return false;
}

int movieSignalCount(const QString& text)
{
    int count = 0;
    for (int i = 0; i < kMovieSignalKinds; ++i) {
        if (movieSignalPatterns_(i).match(text).hasMatch())
            ++count;
    }
    return count;
}

bool lacksGameContext(const QStringList& categories, const QString& text)
{
    if (categories.isEmpty() || categories.contains(QStringLiteral("Gaming")))
        return false;
    return !gameContextPattern().match(text).hasMatch();
}

} // namespace ssv::TrailerHeuristics
