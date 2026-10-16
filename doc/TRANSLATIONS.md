# UI translations

Modern menus, the HUD and match dialogs use `data/ui-translations.xml` through
`translateUI`. Every entry has an English key and complete `de`, `en`, `cs`,
`es`, `fr`, and `it` attributes. Unknown language identifiers fall back to
English. The original screens continue to use the matching `lang_*.xml` files.

The catalog includes tab labels, hints, confirmation dialogs, speed presets,
and display names for the bundled backgrounds. Background filenames remain
unchanged; user-provided names and player names are not translated.

Runtime Nunito supports Latin Extended-A accents. Typographic apostrophes in
the modern catalog use ASCII apostrophes to stay within that character set.
The compact preset buttons and footer scale long translated labels to fit.

The hardware smoke suite renders all five option tabs in all six languages in
SDL and OpenGL, and checks selected translations including English widescreen.
