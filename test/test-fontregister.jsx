// FontRegister integration test. Run from ExtendScript (desktop) or against
// InDesign Server. Registers the fonts packaged next to this script, composes
// with one, exports a PNG, unregisters, and checks the negatives. Results are
// written to test-result.txt next to this script (and alerted on desktop).
//
// The test fonts (Lato, SIL OFL) may already be installed on a machine
// (macOS ships none, but dev machines and Windows base images often have
// them); the assertions adapt: pre-installed fonts won't appear in
// fontNames (the registration diff only reports what actually installed),
// which is itself correct behaviour and reported as a SKIP, but it means the
// install path went untested -- use a font the machine doesn't have (one
// the machine lacks) in test/fonts to make the run conclusive.

var __failures = [];
var __skips = [];
function assert(cond, label) {
  if (!cond) __failures.push(label);
}
function skip(label) { __skips.push(label); }

var here = File($.fileName).parent.fsName;
var fontsDir = here + "/fonts";

app.scriptPreferences.measurementUnit = MeasurementUnits.POINTS;
try { app.scriptPreferences.userInteractionLevel = UserInteractionLevels.NEVER_INTERACT; } catch (e) {}

// --- register a folder -------------------------------------------------------
var preInstalled = app.fonts.itemByName("Lato\tRegular").isValid
  && app.fonts.itemByName("Lato\tRegular").status == FontStatus.INSTALLED;

var reg = app.registerFontFolder(fontsDir);
assert(reg !== null && reg !== undefined, "registerFontFolder returns something");
assert(reg.isValid === true, "registration starts valid");
assert(reg.sourcePath == fontsDir, "sourcePath echoes the argument");
assert(app.fontRegistrations.length === 1, "fontRegistrations has the registration");

if (preInstalled) {
  skip("Lato already installed on this machine; the install path went UNTESTED -- swap in a non-installed font");
  reg.unregister();
} else {
  assert(reg.fontNames.length >= 2, "both faces installed, got " + reg.fontNames.length);
  var f = app.fonts.itemByName(reg.fontNames[0]);
  assert(f.isValid && f.status == FontStatus.INSTALLED, "registered font usable via itemByName");

  // --- compose and export ----------------------------------------------------
  var doc = app.documents.add();
  doc.documentPreferences.pageWidth = 300;
  doc.documentPreferences.pageHeight = 100;
  doc.documentPreferences.facingPages = false;
  var tf = doc.spreads.item(0).pages.item(0).textFrames.add();
  tf.geometricBounds = [10, 10, 90, 290];
  tf.parentStory.insertionPoints.item(-1).contents = "FontRegister test";
  tf.parentStory.characters.everyItem().appliedFont = f;
  assert(tf.parentStory.characters.item(0).appliedFont.status == FontStatus.INSTALLED,
         "composed with the registered font");
  var out = new File(here + "/test-output.png");
  app.pngExportPreferences.exportResolution = 72;
  doc.exportFile(ExportFormat.PNG_FORMAT, out, false);
  assert(out.exists, "PNG exported");
  doc.close(SaveOptions.NO);

  // --- unregister -------------------------------------------------------------
  var name0 = reg.fontNames[0];
  reg.unregister();
  assert(reg.isValid === false, "isValid false after unregister");
  assert(app.fontRegistrations.length === 0, "collection empty after unregister");
  var gone = app.fonts.itemByName(name0);
  assert(!(gone.isValid && gone.status == FontStatus.INSTALLED), "font no longer installed");
  // an unregistered registration is a deleted object; using it throws
  var deletedThrew = false;
  try { reg.unregister(); } catch (e) { deletedThrew = true; }
  assert(deletedThrew, "second unregister throws (object deleted)");
}

// --- negatives ----------------------------------------------------------------
var threw = false;
try { app.registerFont(here + "/does-not-exist.otf"); } catch (e) { threw = true; }
assert(threw, "missing file throws");

threw = false;
try { app.registerFont($.fileName); } catch (e) { threw = true; }  // a .jsx is not a font
assert(threw, "non-font file throws");

var emptyDir = new Folder(here + "/empty-folder");
if (!emptyDir.exists) emptyDir.create();
var emptyReg = app.registerFontFolder(emptyDir.fsName);
assert(emptyReg.fontNames.length === 0, "empty folder registers with no fonts");
emptyReg.unregister();
emptyDir.remove();

// --- report -------------------------------------------------------------------
var msg = __failures.length === 0
  ? "FontRegister test PASSED"
  : "FontRegister test FAILED:\n- " + __failures.join("\n- ");
if (__skips.length > 0) msg += "\nSkipped:\n- " + __skips.join("\n- ");
var rf = new File(here + "/test-result.txt");
rf.encoding = "UTF-8";
rf.open("w");
rf.write(msg);
rf.close();
try { if (app.name.indexOf("Server") < 0) alert(msg); } catch (e) {}
msg;
