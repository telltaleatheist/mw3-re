# Launcher 1.7.1 (2026-11-26)

A launcher fix. The games are unchanged.

## "Older than the launcher" when it isn't

- The launcher could say a game was "older than the launcher" and ask you to
  update it when the game was current (MechWarrior 1 Remake issue #2). This
  happened whenever the launcher could not get an answer from the game: an
  AppImage you downloaded yourself and linked on Linux, a game that took
  longer than 10 seconds to answer (for example while an antivirus scanned a
  fresh install), or any other failure.
- Now an unanswered game is never called older. On PLAY the launcher asks it
  again and says "Checking..." meanwhile; if the game still doesn't answer,
  the launcher says so and the log gives the reason.
- Games get more time to answer, and MechWarrior 3 and Pirate's Moon are
  asked a second time if the first ask gets nothing.
- The launcher's window comes up without waiting for MechWarrior 1 Remake's
  answer.
- When a newer copy of a game turns up somewhere else on your computer, the
  launcher uses it instead of an older copy it had linked before.

## Upgrading

Press UPDATE AVAILABLE in your launcher, or install over it. Your games stay
as they are.
