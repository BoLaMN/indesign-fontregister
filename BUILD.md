# Building

```bash
cd build/mac/prj
xcodebuild -workspace FontReg.xcworkspace -scheme Release -configuration Default build
```

Or open `build/mac/prj/FontReg.xcworkspace` in Xcode and press Build. There is
a `Debug` scheme as well.

## You need

- Xcode (26.6 works; the SDK docs ask for 16.2 but nothing is pinned)
- The InDesign 2026 Plug-in SDK, unzipped to `<repo>/sdk`
- InDesign 21.4 or newer to load the result

The SDK is licence-restricted and about 4 GB, so it isn't committed. Get it
from <https://developer.adobe.com/console/downloads>.

If you keep the SDK elsewhere, change `ID_SDK_ROOT` in
`build/mac/prj/_shared_build_settings/plugin.sdk.xcconfig`.

## Output

`build/mac/release_cocoa64/SDK/FontReg.sdk.InDesignPlugin`, universal
(arm64 + x86_64). Debug plug-ins only load into a debug InDesign.

## Out-of-tree builds

The SDK's shared xcconfigs assume your project lives inside the SDK and
derive paths from `ID_PRJ_DIR`. Ours doesn't, so the files in
`build/mac/prj/_shared_build_settings/` include the SDK's originals and
re-point `ID_SDK_ROOT`. xcconfig references resolve lazily so inherited
paths follow. Three link-time search paths are re-rooted at the SDK's build
folder for the same reason. This mirrors indesign-httplink exactly.

## Windows

Open `build/win/prj/FontReg.sdk.vcxproj` in Visual Studio, or build from the
command line. CI (`.github/workflows/build.yml`) builds both platforms; give
a runner the SDK via the `SDK_PATH` repository variable (self-hosted) or the
`SDK_ZIP_URL` secret (pre-signed URL), as documented in the workflow.
