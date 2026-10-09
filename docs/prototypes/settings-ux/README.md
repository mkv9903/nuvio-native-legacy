# Nuvio Settings Prototype

Local prototype to review remote-control navigation prior to implementing in the native app. Slice: **Trailers, Languages & Subtitles, TV Performance**, with 18 options, search, and demo defaults comparison.

## Launch

In this folder, run:

```bash
python3 -m http.server 4179 --bind 127.0.0.1
```

Open [the local prototype](http://127.0.0.1:4179).
Serving over HTTP is required because the page uses JavaScript ES modules.
Requires no build step, backend, or dependency installation to run.

## Test Workflows

- Arrow keys navigate categories and options. Right enters category; Left returns.
- Enter acts as OK: opens selection sheet and confirms focused choice.
- Escape goes back / cancels. The ✓ continues marking the saved value while navigating.
- Mouse click and Tab also function; dialogs trap focus within controls.
- Search "memory" to find hidden advanced option. Back restores query.
- Disable top hero trailer and open its audio setting to inspect dependency.
- In "Modified from default", open setting and select "Restore default".
- In Performance, test fewer effects: Keep confirms; Escape, Back, or 15 s timeout discards batch. Application failure has separate 5 s timeout from user decision window. Hiding tab also cancels trial.

## Scope and Limits

Data, defaults, and profiles are illustrative. Artwork and Inter font come from assets present in repository. All requests target local server. Confirmed preferences use localStorage key `nuvio-ajustes-ux-demo-v1`; Reset Demo only clears that key. Prototype makes zero API calls, does not sync accounts, and alters no native app files.

Images illustrate effects without playing trailers or measuring memory, GPU, physical output resolution, or TV performance. Switching interface language updates the sample; study UI remains in English. Per-platform capability detection is a subsequent phase. Cancellation and reload verified in browser; does not validate persistence or behavior on physical TV hardware.

## Verification

```bash
node --test model.test.mjs
node --test browser.test.mjs
```

Browser test suite requires Playwright resolvable by Node, Google Chrome installed, and local server running above. If Playwright is installed globally, use `NODE_PATH` pointing to the `node_modules` containing it. `PROTOTYPE_URL` allows overriding test server address.

As of 10/02/2026:

- 10 model tests: draft, confirmation, cancellation, validation, search, independent languages, and recommendations limited to local preferences.
- 10 browser tests: navigation, save/reopen, search with query restore, dependencies, reset, and experiment exits including apply timeout.
- Visual inspection at 1440×900, 1280×720, and 390×844; zero horizontal scrolling. On narrow viewports, help summary moves into sheet.

Files: `index.html` and `app.css` handle layout/styles; `app.js` manages focus, dialogs, and interactions; `model.mjs` manages in-memory options and transactions. See also the [revised proposal](../../settings-ux.md).
