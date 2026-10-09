// OFF-MAIN-THREAD IMAGE DECODER (Tizen target).
//
// Up to 1.3.2 the browser decoded the image on the MAIN THREAD: createImageBitmap
// did run off-thread, but the downscaled drawImage and getImageData that bring
// pixels to the heap ran in `then`, between one app frame and another. On AU7000 (#72)
// this appeared as `swap=1004` ms in [frame] and FPS=2 while a row was loading:
// each artwork cost up to 1 s of frozen main thread, and a row has six.
//
// This Worker does all of that here: receives the request, reads compressed bytes
// DIRECTLY from WASM shared memory, decodes and scales on an OffscreenCanvas,
// and writes pixels into the buffer that C ALREADY ALLOCATED. The main
// thread only forwards the request (a postMessage) and doesn't touch any pixel.
//
// THIS WORKER NEVER WAITS FOR ANYONE (09/22/2026). Up to 1.4.1 it asked C for the
// buffer after decode and stayed in Atomics.wait up to 8 s for the reply;
// when C had already given up on the request, that was 8 s blocked for nobody,
// with the entire queue behind it (128x128 icon taking 17 s on Samsung). Now C
// allocates everything beforehand based on header size, and the Worker only closes the job.
//
// PROTOCOL, in job[0] (int32 in shared memory; full layout
// is in src/webp.c, enum J_*):
//   0  open: C is waiting
//   1  ready: job[1]=w job[2]=h (0 = failed) job[4]=ow job[5]=oh; pixels in
//      job[3], which has job[8] bytes — never write beyond this
//   4  C gave up: nothing was freed, the job is still ours
//   5  dropped: we finished an abandoned job; only now C frees it
// job[7] is the request sequence: if it doesn't match the message, the job is
// no longer this request and nothing is touched.
//
// TWO ROLES, ONE FILE (09/23/2026). src/webp.c creates this file TWICE
// at startup (navegador_iniciar):
//   - the DECODER, which receives requests via main thread (legacy path)
//     or via a MessagePort (`porta`, direct channel);
//   - the SENTINEL (`sentinela: 1`), which sleeps in Atomics.wait at the top of a
//     job stack in shared memory, pops what the C decode threads
//     pushed and forwards through the other end of the port. It never
//     returns to the event loop, and that's why it exists: the decoder needs its
//     event loop free for createImageBitmap promises.
// With this, a request goes from the decode thread to the decoder without passing
// through the main thread, which in Samsung logs stays 1 to 31 s busy. Each role
// sets a bit in canalEstado when up; C only uses the channel with both set.
//
// ES5 only: TV Chromium (76) does not go through esbuild here.
'use strict';
var HEAP32 = null, HEAPU8 = null;

function vivo(pJob, seq) {
  var e = Atomics.load(HEAP32, pJob);
  return HEAP32[pJob + 7] === seq && (e === 0 || e === 4);
}

// `direto`: came through direct channel, and main thread stores nothing of it —
// no `feito` notification needed (would just be extra work there).
function fim(pJob, seq, w, h, ow, oh, direto) {
  HEAP32[pJob + 1] = w; HEAP32[pJob + 2] = h;
  HEAP32[pJob + 4] = ow; HEAP32[pJob + 5] = oh;
  // 0 -> 1 delivers to C; if C already gave up (4), 4 -> 5 drops the job.
  if (Atomics.compareExchange(HEAP32, pJob, 0, 1) === 4) Atomics.compareExchange(HEAP32, pJob, 4, 5);
  Atomics.notify(HEAP32, pJob);
  if (!direto) self.postMessage({ feito: seq });
}

function decodificar(m) {
  var pJob = m.job >> 2, seq = m.seq;
  var largMax = m.largMax, d = !!m.direto;
  if (!vivo(pJob, seq)) return;
  // Abandoned before starting: do not waste decode, just drop.
  if (Atomics.load(HEAP32, pJob) === 4) { fim(pJob, seq, 0, 0, 0, 0, d); return; }
  // `slice`: copies to a standard ArrayBuffer. Blob doesn't accept view over
  // SharedArrayBuffer. job[9]/job[10] is the C COPY, which lives with the job.
  var bytes = HEAPU8.slice(HEAP32[pJob + 9], HEAP32[pJob + 9] + HEAP32[pJob + 10]);
  createImageBitmap(new Blob([bytes], { type: m.mime })).then(function (bmp) {
    var ow = bmp.width, oh = bmp.height, w = ow, h = oh;
    if (!vivo(pJob, seq)) { if (bmp.close) bmp.close(); return; }
    if (Atomics.load(HEAP32, pJob) === 4) { if (bmp.close) bmp.close(); fim(pJob, seq, 0, 0, 0, 0, d); return; }
    if (largMax > 0 && ow > largMax) {
      w = largMax;
      h = Math.max(1, Math.round(oh * largMax / ow));
    }
    if (!(w > 0 && h > 0) || w * h * 4 > HEAP32[pJob + 8] || !HEAP32[pJob + 3]) {
      if (bmp.close) bmp.close(); fim(pJob, seq, 0, 0, ow, oh, d); return;
    }
    var cv = new OffscreenCanvas(w, h);
    var cx = cv.getContext('2d');
    cx.imageSmoothingEnabled = true;
    if ('imageSmoothingQuality' in cx) cx.imageSmoothingQuality = 'high';
    cx.drawImage(bmp, 0, 0, w, h);
    if (bmp.close) bmp.close();
    HEAPU8.set(cx.getImageData(0, 0, w, h).data, HEAP32[pJob + 3]);
    fim(pJob, seq, w, h, ow, oh, d);
  }).catch(function () { if (vivo(pJob, seq)) fim(pJob, seq, 0, 0, 0, 0, d); });
}

// ANIMATED GIF: no longer goes through here (1.4.7, #84). The Worker composed the
// frames that the main thread sliced; since then GIF is decoded in C,
// in an app pthread (src/gif.c), and this file only decodes still images.

// Job layout read by sentinel (mirror of enum J_* in src/webp.c).
var J_SEQ = 7, J_FILA = 12, J_MIME = 13, J_LARG = 14;

// SENTINEL. Treiber stack: C threads push via CAS in job[J_FILA];
// here we take the ENTIRE stack at once (exchange with 0), hence no
// ABA. The stack comes out from newest to oldest; forwards in arrival order.
// Job fields are only read BEFORE it is forwarded: after that the
// decoder can close it and C can free it.
function sentinela(m) {
  var H = new Int32Array(m.memoria);
  var cab = m.cab >> 2, porta = m.porta;
  Atomics.or(H, m.canal >> 2, 2);
  for (;;) {
    var p = Atomics.exchange(H, cab, 0);
    if (!p) { Atomics.wait(H, cab, 0); continue; }
    var lista = [];
    while (p) { lista.push(p); p = H[(p >> 2) + J_FILA]; }
    for (var i = lista.length - 1; i >= 0; i--) {
      var j = lista[i] >> 2;
      var mc = H[j + J_MIME];
      porta.postMessage({ job: lista[i], seq: H[j + J_SEQ], mime: mc === 3 ? 'image/gif' : mc === 2 ? 'image/png' : 'image/webp',
                          largMax: H[j + J_LARG], direto: 1 });
    }
  }
}

function pedido(m) {
  if (!HEAP32) { return; }
  try { decodificar(m); } catch (e) { if (vivo(m.job >> 2, m.seq)) fim(m.job >> 2, m.seq, 0, 0, 0, 0, !!m.direto); }
}

self.onmessage = function (ev) {
  var m = ev.data;
  if (m.sentinela) { sentinela(m); return; }
  if (m.memoria) {
    HEAP32 = new Int32Array(m.memoria);
    HEAPU8 = new Uint8Array(m.memoria);
    if (m.porta) {
      m.porta.onmessage = function (e) { pedido(e.data); };
      Atomics.or(HEAP32, m.canal >> 2, 1);
    }
    return;
  }
  pedido(m);
};
