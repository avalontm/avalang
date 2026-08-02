# AvaHost

AvaHost is the piece that turns an app written in AvaLang into a
real website, available in a browser.

Think of it as the "server" of the ecosystem: it takes the pages you
designed, converts them into HTML, serves the images/styles/scripts
that go along with them, and responds when someone visits an address
like `/` or `/products`. It fills the same role that tools like
PHP, ASP.NET Core, or Node.js fill for other languages, but built
from scratch for AvaLang and for AvaUI, the ecosystem's screen
system.

## What it solves

When someone visits your site, something has to decide which
screen to show, assemble the final document that reaches the
browser, and deliver everything else (an image, a stylesheet, a
font) exactly as it's stored. AvaHost handles all three:

- **Finds the right screen** based on the visited address, simply
  from how your screens are organized and named in the project —
  no need to keep a separate list of which address maps to which
  file.
- **Assembles the final page**, including your site's header,
  footer, and any element that repeats across several screens,
  without you having to copy and paste it into each one.
- **Serves everything else** — images, styles, icons — directly and
  fast, without going through the AvaLang engine.

## Built for day-to-day work

While you're building the site, AvaHost can watch your files and
refresh the browser as soon as you save a change, so you don't have
to go back and forth manually. And once the site is ready to
publish, that same project gets hosted as-is, with no extra
conversion steps.

## Who it's for

For anyone who has already built something in AvaLang / Ava Studio
and wants other people to see it in a browser — without having to
learn a separate web framework or step outside the ecosystem.

---

⬅️ [Back to the main menu](../README.md)
