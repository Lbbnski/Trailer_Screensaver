#pragma once

#include <QString>
#include <QStringList>

namespace ssv::TrailerHeuristics {

// Pure text checks used by YoutubeFallbackResolver to tell "an actual trailer
// for this game" apart from everything else a YouTube search returns (a
// same-named movie's trailer, a dev talk, a review, a streamer's gameplay
// video, ...). Kept free of any I/O so each rule can be unit-tested against
// real examples — every rule here exists because of a concrete bad match
// found in a real user's diagnostics log or bad-trailer reports, not
// guessed at in the abstract.

// True if the video's title plausibly names this game. Punctuation-insensitive
// ("3050 A.D." matches "3050AD"). Short game titles (1-2 meaningful words)
// must match in full — that's what stops "Gun Beat" from accepting *Top
// Gun: Maverick*'s trailer on a single shared word — while longer titles
// only need half their words, since a real trailer's title routinely drops
// a subtitle or edition suffix ("Demo", "Soundtrack") the store entry has.
bool titleMatchesGame(const QString& gameTitle, const QString& videoTitle);

// True if the title contains a word that marks it as a trailer/announcement
// at all (in several languages). A video with none of these is far more
// likely a streamer's gameplay, an OST, a fan video, or an unrelated upload
// that merely mentions the game.
bool hasTrailerWord(const QString& videoTitle);

// The first word in `videoTitle` marking it as something *about* a trailer
// or game rather than the trailer itself (review, reaction, breakdown,
// "unpacks", interview, walkthrough, ...), or an empty string if none. A
// word that's part of the game's own title is ignored ("Review Board: The
// Game" shouldn't be penalized for containing "review").
QString nonTrailerWord(const QString& gameTitle, const QString& videoTitle);

// The first word in `videoTitle` marking it as film/TV content (movie, film,
// tv series, season/episode N, ...), ignoring words in the game's own title.
QString movieWordInTitle(const QString& gameTitle, const QString& videoTitle);

// True for YouTube categories that mean film/TV content, never a game trailer.
bool isFilmCategory(const QStringList& categories);

// How many distinct strong film/TV signals ("in theaters", "netflix", a film
// studio name, ...) `text` (a video's description + tags) contains.
int movieSignalCount(const QString& text);

// True if `categories` is known, isn't "Gaming", and nothing in `text` (title
// + description + tags) mentions games/platforms at all — i.e. it looks like
// a non-game video. An unknown (empty) category list never counts as lacking
// game context: missing data isn't evidence.
bool lacksGameContext(const QStringList& categories, const QString& text);

} // namespace ssv::TrailerHeuristics
