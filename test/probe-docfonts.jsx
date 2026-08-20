// Diagnostic for InDesign Server: does the document-fonts mechanism install
// job fonts synchronously? Run against the Server with a job dir laid out as:
//
//   C:\fontreg-test\
//     probe-docfonts.jsx        (this file)
//     Document fonts\
//       Lato-Regular.ttf
//       Lato-Bold.ttf
//
// It creates a document, saves it into the job dir (so "Document fonts" is
// adjacent), reopens it, and reports whether Lato resolves and composes.
// Results land in probe-docfonts-result.txt next to the script.

try { app.scriptPreferences.userInteractionLevel = UserInteractionLevels.NEVER_INTERACT; } catch (e) {}

var here = File($.fileName).parent.fsName;
var lines = [];
function log(s) { lines.push(String(s)); }
function st(n) { var f = app.fonts.itemByName(n); return f.isValid ? String(f.status) : "invalid"; }

try {
  log("pre-open app Lato Regular: " + st("Lato\tRegular"));

  var indd = new File(here + "/probe-build.indd");
  var d0 = app.documents.add();
  d0.save(indd);
  d0.close(SaveOptions.NO);

  var doc = app.open(indd);
  log("post-open app Lato Regular: " + st("Lato\tRegular"));

  var tf = doc.spreads.item(0).pages.item(0).textFrames.add();
  tf.geometricBounds = [10, 10, 140, 290];
  tf.parentStory.insertionPoints.item(-1).contents = "Hamburgefonstiv";

  var applied = "none";
  var tries = ["Lato\tRegular", "Lato (TT)\tRegular"];
  for (var i = 0; i < tries.length && applied == "none"; i++) {
    try {
      tf.parentStory.characters.everyItem().appliedFont = tries[i];
      applied = tries[i];
    } catch (e) {}
  }
  log("applied via: " + applied.replace("\t", "<TAB>"));
  if (applied != "none") {
    var got = tf.parentStory.characters.item(0).appliedFont;
    log("composed: " + got.name.replace("\t", "<TAB>")
      + " ps=" + got.postscriptName
      + " installed=" + (got.status == FontStatus.INSTALLED));
    app.pngExportPreferences.exportResolution = 144;
    doc.exportFile(ExportFormat.PNG_FORMAT, new File(here + "/probe-docfonts.png"), false);
    log("png exported (check the glyphs are Lato, not a substitute)");
  }
  doc.close(SaveOptions.NO);
} catch (e) {
  log("THREW: " + e);
}

var o = new File(here + "/probe-docfonts-result.txt");
o.encoding = "UTF-8";
o.open("w");
o.write(lines.join("\n"));
o.close();
lines.join(" | ");
