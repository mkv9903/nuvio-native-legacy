// webapis.avplay STUB for Tizen target.
//
// There is no Samsung TV in this testbench and webapis.avplay does not exist in Chrome, so
// video_tizen.c had never been EXECUTED — only compiled. This stub does not
// prove that video plays; it proves that the app speaks to AVPlay in the ORDER and with the
// ARGUMENTS that Samsung documents. That is what can be verified here.
(function () {
  // MAIN THREAD ONLY. --pre-js is prepended to Emscripten glue, and that
  // same glue runs INSIDE EACH pthread WORKER — where `window` does not
  // exist. The first version referenced `window` directly and crashed
  // all workers with "Uncaught ReferenceError: window is not defined" at
  // startup; the app did not even draw a frame and test seemed to do
  // nothing. The stub only makes sense where DOM exists.
  if (typeof window === "undefined" || typeof document === "undefined") return;

  var reg = [];
  function log(nome, args) {
    var a = Array.prototype.slice.call(args).map(function (v) {
      if (typeof v === "function") return "fn";
      if (typeof v === "string" && v.length > 60) return v.slice(0, 60) + "...";
      return JSON.stringify(v);
    }).join(", ");
    reg.push(nome + "(" + a + ")");
    if (typeof Module !== "undefined" && Module.print)
      Module.print("[AV] " + nome + "(" + a + ")");
  }
  var estado = "NONE", ouvinte = null, preparoSincrono = false;
  window.webapis = window.webapis || {};
  window.webapis.avplay = {
    open:            function (u) { log("open", arguments); estado = "IDLE"; preparoSincrono = false; },
    close:           function ()  { log("close", arguments); estado = "NONE"; },
    stop:            function ()  { log("stop", arguments); estado = "IDLE"; },
    play:            function ()  { log("play", arguments); estado = "PLAYING";
                                    if (ouvinte && ouvinte.oncurrentplaytime) ouvinte.oncurrentplaytime(0); },
    pause:           function ()  { log("pause", arguments); estado = "PAUSED"; },
    prepare:         function ()  { log("prepare", arguments); estado = "READY"; preparoSincrono = true; },
    prepareAsync:    function (ok, err) {
                       log("prepareAsync", arguments); preparoSincrono = false;
                       setTimeout(function () { estado = "READY"; if (ok) ok(); }, 10);
                     },
    seekTo:          function (ms, ok, err) { log("seekTo", arguments); if (ok) setTimeout(ok, 5); },
    setDisplayRect:  function (x, y, w, h) { log("setDisplayRect", arguments); },
    setDisplayMethod:function (m) { log("setDisplayMethod", arguments); },
    setListener:     function (l) { log("setListener", arguments); ouvinte = l; },
    getState:        function ()  { return estado; },
    getCurrentTime:  function ()  { return 0; },
    getDuration:     function ()  { return 7200000; },
    setSelectTrack:  function (t, i) { log("setSelectTrack", arguments); },
    setExternalSubtitlePath: function (p) { log("setExternalSubtitlePath", arguments); },
    setSilentSubtitle: function (b) { log("setSilentSubtitle", arguments); },
    getTotalTrackInfo: function () {
      log("getTotalTrackInfo", arguments);
      // Samsung permits READY here only after synchronous prepare(). A fake
      // that accepts every call cannot detect an invalid production sequence.
      if (estado !== "PLAYING" && estado !== "PAUSED" &&
          !(estado === "READY" && preparoSincrono))
        throw new Error("InvalidStateError: getTotalTrackInfo");
      return [
        { index: 0, type: "VIDEO", extra_info: '{"fourCC":"HEVC","Width":3840,"Height":2160}' },
        { index: 1, type: "AUDIO", extra_info: '{"language":"eng","fourCC":"EAC3","channels":6}' },
        { index: 2, type: "AUDIO", extra_info: '{"language":"por","fourCC":"AC-3","channels":6}' },
        { index: 3, type: "TEXT",  extra_info: '{"track_lang":"por"}' }
      ];
    }
  };
  window.__avReg = function () { return reg; };
  // Installed listener, for test to trigger onsubtitlechange (#122).
  window.__avOuvinte = function () { return ouvinte; };
  window.__avLimpar = function () { reg = []; };
})();
