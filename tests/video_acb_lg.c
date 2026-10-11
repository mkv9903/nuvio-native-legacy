/* Compile the actual webOS path; no bus, decoder or TV is contacted. */
#include <time.h>
#include <assert.h>
#include <unistd.h>
static int fake_sleep(const struct timespec *, struct timespec *);
#define nanosleep fake_sleep
#include "../src/video.c"
#undef nanosleep

void legenda_carregar(const char *url) { (void)url;assert(0); }
int esmaecer_segura_protetor_tv(void) { return 0; }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int dados_gravar(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 1; }
void marco(const char *name) { (void)name; }
const char *i18n(const char *s) { return s; }
const char *rede_url_publica(const char *url,char *dst,unsigned n) { snprintf(dst,n,"%s",url);return dst; }
static int exported_windows;
static int fake_exported_window(const char *id,SDL_Rect *src,SDL_Rect *dst) {
  assert(!strcmp(id,"webos6-window") && src->w==1920 && src->h==1080 && dst->w && dst->h);
  exported_windows++;return 1;
}
static int creates, destroys, finalizes, inits, media_calls, state_calls, windows, connects;
static int init_type, init_ok = 1, change_at_sleep, sleeps;
static long next_handle = 100;
static char bound_media[64];
static int fake_sleep(const struct timespec *a, struct timespec *b) {
  (void)a;(void)b;sleeps++;
  if (change_at_sleep && sleeps==change_at_sleep) {
    __atomic_add_fetch(&sessao,1,__ATOMIC_ACQ_REL);
    snprintf(midia,sizeof midia,"new-media");
  }
  return 0;
}
static long fake_create(void) { creates++;return next_handle++; }
static int fake_init(long h,int type,const char *app,void *cb) {
  assert(h && app && cb);inits++;init_type=type;return init_ok;
}
static void fake_destroy(long h) { assert(h);assert(!bindAtivo());destroys++; }
static int fake_finalize(long h) { assert(h);assert(!bindAtivo());finalizes++;return 1; }
static int fake_sink(long h,int type) { assert(h && type==0);return 1; }
static int fake_media(long h,const char *id) { assert(h==acb);media_calls++;snprintf(bound_media,sizeof bound_media,"%s",id);return 1; }
static int fake_state(long h,int app,int play,long *task) { assert(h==acb && app==1 && play>=1 && task);state_calls++;return 1; }
static int fake_window(long h,long x,long y,long w,long height,int full,long *task) {
  (void)x;(void)y;(void)full;assert(h==acb && w && height && task);windows++;return 1;
}
static int fake_connect(long h,int sink,long *task) { assert(h==acb && !sink && task);connects++;return 1; }
static int fake_data(long h,const char *data,long *task) { assert(h==acb && data && task);return 1; }
static AcbBind *snapshot(void) {
  AcbBind *b=malloc(sizeof *b);assert(b);b->sessao=sessao;b->acb=acb;b->tipo=acbTipoAtual;snprintf(b->midia,sizeof b->midia,"%s",midia);return b;
}
static void reset_calls(void) { media_calls=state_calls=windows=connects=sleeps=0;change_at_sleep=0; }
static int native_audio_calls;
static int fake_native_audio(LSHandle *h,const char *uri,const char *payload,
                             Filtro cb,void *ctx,unsigned long *token,void *error) {
  (void)h;(void)cb;(void)ctx;(void)token;(void)error;
  assert(!strcmp(uri,"luna://com.webos.media/selectTrack"));
  assert(strstr(payload,"\"type\":\"audio\""));native_audio_calls++;return 1;
}
static int native_subtitle_index = -1, native_subtitle_calls;
static int fake_native_subtitle(LSHandle *h,const char *uri,const char *payload,
                                Filtro cb,void *ctx,unsigned long *token,void *error) {
  (void)h;(void)cb;(void)ctx;(void)token;(void)error;
  if (!strcmp(uri,"luna://com.webos.media/setSubtitleEnable")) return 1;
  assert(!strcmp(uri,"luna://com.webos.media/selectTrack"));
  assert(strstr(payload,"\"type\":\"text\""));
  native_subtitle_index=(int)js_num(payload,NULL,"index",-1);
  assert(native_subtitle_index>=0);native_subtitle_calls++;return 1;
}
int main(void) {
  /* OLED A1: LG rejects DTS and returns an empty string, followed by a video
   * array. That video must never become a fake H264 audio track. */
  eventoPayload("{\"sourceInfo\":{\"programInfo\":[{\"numAudioTracks\":0,\"audioTrackInfo\":\"\",\"videoTrackInfo\":[{\"codec\":\"H264\"}],\"subtitleTrackInfo\":\"\"}]}}",sessao);
  assert(nAudio == 0 && nLeg == 0);
  MkvFaixa tracks[3] = {
    {.numero=1,.tipo=1,.codec="V_MPEG4/ISO/AVC"},
    {.numero=2,.tipo=2,.codec="A_DTS",.nome="DTS-HD Master Audio",.canais=6},
    {.numero=3,.tipo=17,.codec="S_HDMV/PGS"}
  };
  eventoPayload("{\"error\":{\"errorCode\":200,\"errorText\":\"Audio Codec Not Supported\"}}",sessao);
  assert(audioNaoSup && !video_faixa_dts(video_audio(audioAtual)));
  completarAudioMkv(tracks,3);
  assert(nAudio == 1 && audioAtual == 0 && video_faixa_dts(video_audio(0)) > 0);
  assert(audioNaoSup && video_faixa_dts(video_audio(audioAtual))); /* rejected native track is still identified */
  assert(!strcmp(faixaAudio[0].codec,"dts") && faixaAudio[0].canais == 6);
  /* Empty arrays also stay empty; multiple hidden tracks are ambiguous. */
  eventoPayload("{\"sourceInfo\":{\"audioTrackInfo\":[],\"videoTrackInfo\":[{\"codec\":\"H264\"}],\"subtitleTrackInfo\":[]}}",sessao);
  assert(nAudio == 0 && nLeg == 0);
  tracks[2] = (MkvFaixa){.tipo=2,.codec="A_DTS"};
  completarAudioMkv(tracks,3);assert(nAudio == 0);
  /* Error 200 also covers TrueHD; metadata must retain its codec. */
  snprintf(tracks[1].codec,sizeof tracks[1].codec,"A_TRUEHD");
  snprintf(tracks[1].nome,sizeof tracks[1].nome,"TrueHD");
  completarAudioMkv(tracks,2);assert(nAudio == 1 && !video_faixa_dts(video_audio(0)));
  /* Real native audio remains authoritative; iteration stops at the array. */
  eventoPayload("{\"sourceInfo\":{\"audioTrackInfo\":[{\"codec\":\"aac\",\"channels\":2}],\"videoTrackInfo\":[{\"codec\":\"H264\"}],\"subtitleTrackInfo\":[]}}",sessao);
  assert(nAudio == 1 && !strcmp(faixaAudio[0].codec,"aac"));
  completarAudioMkv(tracks,2);assert(!strcmp(faixaAudio[0].codec,"aac"));
  nAudio = nLeg = 0; audioNaoSup = 0;

  /* 32LK628BPTF / webOS 4.4.3: all seven subtitle entries omit trackNum.
   * Each must have its own native index, UI identity and MKV metadata match. */
  eventoPayload("{\"sourceInfo\":{\"programInfo\":[{\"numSubtitleTracks\":7,\"subtitleTrackInfo\":["
                "{\"language\":\"(null)\",\"type\":\"text\"},{\"language\":\"(null)\",\"type\":\"text\"},"
                "{\"language\":\"(null)\",\"type\":\"text\"},{\"language\":\"(null)\",\"type\":\"text\"},"
                "{\"language\":\"(null)\",\"type\":\"text\"},{\"language\":\"(null)\",\"type\":\"text\"},"
                "{\"language\":\"(null)\",\"type\":\"text\"}]}]}}",sessao);
  assert(nLeg==7);
  int tv_subs[7], matched[7];
  MkvFaixa file_tracks[9]={{.numero=1,.tipo=1},{.numero=2,.tipo=2}};
  for (int i=0;i<7;i++) {
    const VideoFaixa *f=video_legenda(i);
    char label[32];snprintf(label,sizeof label,"Legenda %d",i+1);
    assert(f && f->numero==i && !strcmp(f->rotulo,label) && !f->idioma[0]);
    assert(f->ordinalMkv==-1);tv_subs[i]=f->numero;
    file_tracks[i+2]=(MkvFaixa){.numero=i+3,.tipo=17,.codec="S_TEXT/UTF8"};
    snprintf(file_tracks[i+2].idioma,sizeof file_tracks[i+2].idioma,"%s",i==2 || i==3?"hi":"en");
  }
  assert(mkv_casar_legendas(file_tracks,9,tv_subs,7,matched)==MKV_CASA_ORDINAL);
  for (int i=0;i<7;i++) assert(matched[i]==i+2);
  ligado=1;snprintf(midia,sizeof midia,"native-subtitles");lsCall=fake_native_subtitle;
  video_escolher_legenda(0);assert(native_subtitle_calls==1 && native_subtitle_index==0 && legAtual==0);
  video_escolher_legenda(4);assert(native_subtitle_calls==2 && native_subtitle_index==4 && legAtual==4);
  video_escolher_legenda(-1);assert(native_subtitle_calls==2 && legAtual==-1);
  ligado=0;lsCall=NULL;midia[0]=0;
  /* A missing value must not borrow trackNum from the following object;
   * explicit track numbers remain authoritative, including zero. */
  eventoPayload("{\"sourceInfo\":{\"subtitleTrackInfo\":[{\"type\":\"text\"},"
                "{\"trackNum\":12},{\"trackNum\":2}]}}",sessao);
  assert(nLeg==3 && faixaLeg[0].numero==0 && faixaLeg[1].numero==12 && faixaLeg[2].numero==2);
  eventoPayload("{\"sourceInfo\":{\"subtitleTrackInfo\":[{\"trackNum\":2},"
                "{\"trackNum\":0},{\"trackNum\":1}]}}",sessao);
  assert(nLeg==3 && faixaLeg[0].numero==2 && faixaLeg[1].numero==0 && faixaLeg[2].numero==1);
  eventoPayload("{\"sourceInfo\":{\"subtitleTrackInfo\":[{\"trackNum\":-1},"
                "{\"trackNum\":null},{\"trackNum\":\"unknown\"},{\"trackNum\":1e30},{\"trackNum\":4.5}]}}",sessao);
  assert(nLeg==5);
  for (int i=0;i<nLeg;i++) assert(faixaLeg[i].numero==i);
  nAudio=nLeg=0;

  /* An unsupported error
   * from another track must not follow the user onto a compatible DTS track. */
  ligado=1;snprintf(midia,sizeof midia,"native-media");nAudio=2;audioAtual=0;
  snprintf(faixaAudio[0].codec,sizeof faixaAudio[0].codec,"eac3");
  snprintf(faixaAudio[1].codec,sizeof faixaAudio[1].codec,"dts");
  faixaAudio[0].numero=0;faixaAudio[1].numero=1;
  lsCall=fake_native_audio;
  eventoPayload("{\"errorCode\":200,\"errorText\":\"Audio Codec Not Supported\"}",sessao);
  assert(audioNaoSup);
  video_escolher_audio(0);assert(audioNaoSup); /* same rejected track */
  video_escolher_audio(1);
  assert(audioAtual==1 && !audioNaoSup && native_audio_calls==2);
  eventoPayload("{\"playing\":{},\"currentTime\":1000,\"errorCode\":0,\"errorText\":\"No Error\"}",sessao);
  assert(video_tocando() && !audioNaoSup);
  eventoPayload("{\"errorCode\":200,\"errorText\":\"Audio Codec Not Supported\"}",sessao);
  assert(audioNaoSup); /* a fresh DTS failure remains visible */
  audioNaoSup=0;ligado=0;nAudio=0;midia[0]=0;posSeg=0;tocando=0;lsCall=NULL;

  acbCriar=fake_create;acbIniciar=fake_init;acbDestruir=fake_destroy;acbFinalizar=fake_finalize;
  acbSink=fake_sink;acbMidia=fake_media;acbEstado=fake_state;acbJanela=fake_window;acbConectar=fake_connect;
  acbVideoData=acbAudioData=fake_data;
  assert(acbConfigurarTipo() && init_type==0 && creates==1);long original=acb;
  tipoJogador=4;bindVivo=1;
  assert(!acbConfigurarTipo() && acb==original && creates==1 && destroys==0);
  bindVivo=0;assert(acbConfigurarTipo() && init_type==4 && creates==2 && finalizes==1 && destroys==1);
  assert(acbConfigurarTipo() && creates==2); // stable playback retains its handle
  tipoJogador=0;assert(acbConfigurarTipo() && init_type==0 && creates==3);
  init_ok=0;tipoJogador=4;
  assert(!acbConfigurarTipo() && !acb);int before=creates;
  assert(!acbConfigurarTipo() && creates==before); // no per-frame failed-init loop
  init_ok=1;tipoJogador=0;assert(acbConfigurarTipo());

  sessao=7;snprintf(midia,sizeof midia,"old-media");AcbBind *b=snapshot();
  sessao=8;snprintf(midia,sizeof midia,"new-media");bindVivo=1;prenderPlano(b);
  assert(!bindAtivo() && !media_calls && !state_calls && !windows && !connects);
  // A new native ID can also appear within the same source session.
  reset_calls();b=snapshot();snprintf(midia,sizeof midia,"replacement-id");bindVivo=1;prenderPlano(b);
  assert(!media_calls && !state_calls);
  // Changing generation during a sleep must stop before the next native stage.
  reset_calls();b=snapshot();change_at_sleep=1;bindVivo=1;prenderPlano(b);
  assert(media_calls==1 && !state_calls && !connects && !windows);
  // A current snapshot binds normally and retains the captured identity.
  reset_calls();b=snapshot();bindVivo=1;prenderPlano(b);
  assert(media_calls==1 && !strcmp(bound_media,"new-media") && state_calls==2 && connects==1 && windows==1);
  assert(!bindAtivo());

  /* Native webOS 6 playback exports a window instead of binding ACB. */
  acb=0;snprintf(expWin,sizeof expWin,"webos6-window");sdlExpJanela=fake_exported_window;
  pronto=tocando=viuVideo=0;snprintf(midia,sizeof midia,"native-exported-media");bindPendente=0;
  expSrcW=1280;expSrcH=720;fonX=-1;
  eventoPayload("{\"videoInfo\":{\"width\":1920,\"height\":1080,\"hdrType\":\"HDR10\"}}",sessao);
  eventoPayload("{\"loadCompleted\":{}}",sessao);
  eventoPayload("{\"playing\":{}}",sessao);
  assert(video_pronto() && video_tocando() && video_decoder_anunciou());
  assert(!strcmp(vidHdr,"HDR10") && exported_windows>0);
  puts("webOS native tracks, subtitle indices, ACB lifetime, stale generations, overrides, failed-init retry and exported-window tests passed");
  return 0;
}
