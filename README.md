# FontRegister

An InDesign plug-in that lets a script register font files at runtime, so a
job package (script + fonts) can run against InDesign Server without the
fonts being installed on the machine.

Registration is session-scoped: fonts are copied into a per-process
subfolder of InDesign's per-user CompositeFont folder -- the only location
the font system rescans synchronously mid-script -- and everything
evaporates at unregister or process exit. System font folders are never
touched.

Structured after [indesign-httplink](../indesign-http), which is the source
of truth for the build and CI patterns.

## Scripting

Same code from `.jsx`, `.idjs` and Server:

```js
var reg = app.registerFontFolder(File($.fileName).parent.fsName + "/Fonts");
reg.fontNames;     // ["Foo\tBold", ...] -- what actually installed
reg.sourcePath;    // the path you passed in
reg.isValid;       // built-in specifier validity; false after unregister()

// fonts are usable on the next line
var f = app.fonts.itemByName(reg.fontNames[0]);

reg.unregister();  // removes exactly what this call installed

app.fontRegistrations;              // active registrations, a collection
app.registerFont("/path/Foo.otf");  // single file; same FontRegistration
```

More in [docs/SCRIPTING.md](docs/SCRIPTING.md).

## Self-cleaning

An idle task sweeps every 60 seconds and unregisters any registration whose
`sourcePath` no longer exists — so when the proxy deletes a finished job's
directory, that job's fonts go too, even if the script never called
`unregister()`. Idle tasks never run mid-script, so a sweep cannot remove a
running job's fonts while its job directory exists.

## Install

Build (see [BUILD.md](BUILD.md)), then point InDesign at the output:

```bash
echo '=Path "<repo>/build/mac/release_cocoa64/SDK"' \
  > ~/Library/Preferences/Adobe\ InDesign/Version\ 21.0/en_US/PluginConfig.txt
```

Restart and check Help ▸ About Plug-ins.

## Layout

```
source/fontregister/  the model plug-in: registry, scripting, idle sweep
build/mac/            Xcode workspace and project
build/win/            Visual Studio project
sdk/                  InDesign SDK, not committed (see BUILD.md)
test/                 scripted integration test + test font
```

## Known gaps

- The plug-in prefix ID is a placeholder; shipping needs a number from Adobe
  Developer Support (must not collide with other plug-ins)
- Windows compiles in CI but has not been loaded into InDesign there
