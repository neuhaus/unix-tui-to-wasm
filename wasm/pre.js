/* pre.js - runs inside the Emscripten module before main().
 *
 * Optional persistent filesystem: when Module.persist is not false, the
 * directory Module.persistDir (default /data) is mounted on IDBFS (browser
 * IndexedDB), so files written there survive a page reload.
 *
 * Startup blocks on the initial sync from IndexedDB. Periodic flushes only
 * start once that load has succeeded: an un-restored (empty) directory must
 * never be reconciled over IndexedDB, or one failed read would delete every
 * stored file. The page (storage.js) performs one final awaited flush when
 * the program exits.
 *
 * NOTE: this file is concatenated into the module by --pre-js, so `Module`,
 * `FS` and `IDBFS` are already in scope. Do not redeclare them.
 */
Module["preRun"] = Module["preRun"] || [];
Module["preRun"].push(function () {
  if (Module["persist"] === false) return;

  var dir = Module["persistDir"] || "/data";
  try {
    FS.mkdir(dir);
  } catch (e) {}

  try {
    FS.mount(IDBFS, {}, dir);
  } catch (e) {
    // No IndexedDB (private browsing, non-browser runtime, ...): the
    // program still works, files just last for this session (MEMFS).
    console.error("utw: persistent filesystem unavailable:", e);
    Module["storagePersistent"] = false;
    return;
  }
  Module["storagePersistent"] = true;

  // One flush is one IndexedDB transaction; the guard keeps flushes
  // sequential. A flush that does not finish within 15s (its transaction
  // blocked behind one from a page being torn down, ...) is abandoned so
  // later flushes still run.
  var syncing = false,
    syncStarted = 0;
  function flush() {
    if (syncing && Date.now() - syncStarted < 15000) return;
    syncing = true;
    syncStarted = Date.now();
    FS.syncfs(false, function (err) {
      syncing = false;
      if (err) console.error("utw: could not write to IndexedDB:", err);
    });
  }
  // Promise variant for storage.js: wait out any in-flight flush, then
  // perform a final one. Resolves with the error (null on success).
  Module["flushStorage"] = function () {
    return new Promise(function (resolve) {
      (function go() {
        if (syncing && Date.now() - syncStarted < 15000)
          return setTimeout(go, 200);
        syncing = true;
        syncStarted = Date.now();
        FS.syncfs(false, function (err) {
          syncing = false;
          resolve(err);
        });
      })();
    });
  };

  // Initial sync from IndexedDB; main() waits on this run dependency.
  try {
    Module["addRunDependency"]("utw-idbfs-load");
    FS.syncfs(true, function (err) {
      Module["removeRunDependency"]("utw-idbfs-load");
      if (err) {
        // Stored files could not be read. Keep flushes disabled for this
        // session: the directory is not a faithful copy of IndexedDB, and
        // reconciling it would delete the stored files, which stay intact
        // for the next load.
        console.error("utw: could not load from IndexedDB:", err);
        Module["storagePersistent"] = false;
        return;
      }
      setInterval(flush, 5000);
      if (typeof window !== "undefined" && window.addEventListener)
        window.addEventListener("pagehide", flush);
    });
  } catch (e) {
    console.error("utw: persistent filesystem unavailable:", e);
    Module["storagePersistent"] = false;
    try {
      Module["removeRunDependency"]("utw-idbfs-load");
    } catch (e2) {}
  }
});
