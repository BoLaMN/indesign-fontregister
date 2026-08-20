# Scripting FontRegister

Works identically from ExtendScript (`.jsx`), UXP (`.idjs`) and InDesign
Server -- the plug-in adds its API to the scripting DOM, which all three
share.

## The job-package pattern

A generated build script ships in a package next to its fonts:

```
job-1234/
  build.jsx
  Fonts/
    Foo-Regular.otf
    Foo-Bold.otf
```

Prepend one line and the fonts are available for everything below it:

```js
var reg = app.registerFontFolder(File($.fileName).parent.fsName + "/Fonts");

// fonts resolve immediately, on the very next line
var f = app.fonts.itemByName(reg.fontNames[0]);

// ... build the document, export ...

reg.unregister();   // optional; see "Self-cleaning" below
```

## API

### `app.registerFont(path)` → FontRegistration

Copies one font file into the plug-in's private session directory, tells
InDesign's font system about it, and returns once the font is usable.
Throws if the path doesn't exist or the file yields no fonts.

### `app.registerFontFolder(path)` → FontRegistration

The same for every font file (`.otf`, `.ttf`, `.ttc`, `.otc`) directly in a
folder. A folder with no font files returns an empty registration rather
than throwing. Registering a path that's already actively registered
returns the existing registration instead of double-installing.

### FontRegistration

| Member | Meaning |
|---|---|
| `fontNames` | what actually installed, as `"Family\tStyle"` strings — the exact form `app.fonts.itemByName()` takes. Computed by diffing the font system across the rescan, so pre-installed fonts don't appear. |
| `sourcePath` | the path passed to the register call |
| `isValid` | `false` once unregistered |
| `unregister()` | removes exactly the fonts this call installed; a second call is a no-op |

### `app.fontRegistrations`

The session's active registrations, as a normal collection (`length`,
`item(n)`, `itemByID(id)`, `everyItem()`). Unregistered ones drop out.

## Semantics

- **Synchronous** -- registration returns after the font system rescan, so
  the next script line can use the fonts.
- **Session-scoped** -- everything lives in a per-process temp directory and
  is gone when InDesign / the Server exits. System and InDesign fonts
  folders are never touched.
- **Unregistering** removes the backing files and rescans; open documents
  using the fonts see them become missing fonts, like any uninstall.

## Self-cleaning

An idle task runs every 60 seconds and unregisters any registration whose
`sourcePath` no longer exists on disk. If your proxy deletes the job
directory when a job finishes, the job's fonts follow automatically -- so
`unregister()` in the script is good hygiene, not a requirement. Idle tasks
never run while a script is executing, so the sweep can't pull fonts out
from under a running job (as long as the job dir exists while it runs).

## Errors

All failures throw catchable ExtendScript errors: missing path, unreadable
file, a named file that produces no fonts, or a failed copy (disk full,
permissions).
