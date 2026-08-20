// Server diagnostic for the unregister path. Run against InDesign Server with
// a fonts folder (e.g. Lato) next to this script:
//
//   C:\fontreg-test\
//     probe-unregister.jsx
//     fonts\Lato-Regular.ttf, Lato-Bold.ttf
//
// It registers, unregisters, then reports (a) whether the font is still
// installed and (b) whether the plug-in's backing copies still exist on disk.
// Together those distinguish the two failure modes:
//   files gone  + font installed  -> the removal rescan is blind (engine bug)
//   files exist + font installed  -> the OS refused the delete (locked file);
//                                    the plug-in's rename-to-trash fallback
//                                    should have handled it -- report paths.
// Results land in probe-unregister-result.txt next to this script.

try { app.scriptPreferences.userInteractionLevel = UserInteractionLevels.NEVER_INTERACT; } catch (e) {}

var here = File($.fileName).parent.fsName;
var lines = [];
function log(s) { lines.push(String(s)); }
function installed(n) {
  var f = app.fonts.itemByName(n);
  return f.isValid && f.status == FontStatus.INSTALLED;
}

// Find the plug-in's session/trash dirs under the user prefs tree.
function findFontRegDirs() {
  var hits = [];
  function walk(folder, depth) {
    if (depth > 6) return;
    var kids = folder.getFiles();
    for (var i = 0; i < kids.length; i++) {
      if (!(kids[i] instanceof Folder)) continue;
      if (/^FontRegister-/.test(kids[i].name)) hits.push(kids[i]);
      else walk(kids[i], depth + 1);
    }
  }
  walk(Folder(Folder.userData + "/Adobe"), 0);
  return hits;
}
function listDir(folder) {
  var files = folder.getFiles(), names = [];
  for (var i = 0; i < files.length; i++) names.push(files[i].name);
  return folder.fsName + " [" + names.join(", ") + "]";
}

try {
  var reg = app.registerFontFolder(here + "/fonts");
  log("registered: " + reg.fontNames.length + " fonts; installed=" + installed("Lato\tRegular"));

  var dirs = findFontRegDirs();
  for (var d = 0; d < dirs.length; d++) log("pre-unregister  " + listDir(dirs[d]));

  reg.unregister();
  log("post-unregister installed=" + installed("Lato\tRegular"));

  dirs = findFontRegDirs();
  if (dirs.length === 0) log("post-unregister: no FontRegister dirs found (unexpected)");
  for (d = 0; d < dirs.length; d++) log("post-unregister " + listDir(dirs[d]));

  // give the engine one more explicit kick and re-check
  app.updateFonts();
  log("after updateFonts installed=" + installed("Lato\tRegular"));
} catch (e) {
  log("THREW: " + e);
}

var o = new File(here + "/probe-unregister-result.txt");
o.encoding = "UTF-8";
o.open("w");
o.write(lines.join("\n"));
o.close();
lines.join(" | ");
