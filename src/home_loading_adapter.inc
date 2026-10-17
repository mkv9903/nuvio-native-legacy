/* Native projections and inputs for Rust-owned Home loading. The collection,
 * progress and tracking services retain their implementations for now. */
typedef struct {
  CatItem **batch;
  int *capacity, *count;
  int prefix;
  unsigned generation;
  HomeContexto *source;
  ListasTrakt *lists;
  int *lists_visible, *progressive, *published;
  int first_row;
} HomeLoadingBridge;

static void update_home_manifest_progress(size_t ready, size_t total, void *context) {
  if ((unsigned)(uintptr_t)context!=montagemGeracao) return;
  pthread_mutex_lock(&cargaTrava);
  carga.addonsTotal=total>INT_MAX?INT_MAX:(int)total;
  if (!ready || ready>(size_t)carga.addonsProntos) carga.addonsProntos=ready>INT_MAX?INT_MAX:(int)ready;
  pthread_mutex_unlock(&cargaTrava);
}

static int publish_home_loading(const RustSceneSnapshot *scene, int complete,
                                const RustHomeLoadStats *status, void *context) {
  HomeLoadingBridge *bridge=context;
  size_t title_count=nuvio_scene_title_count(scene), row_count=nuvio_scene_row_count(scene), index;
  int fixed=bridge->prefix>0;
  if (bridge->generation!=montagemGeracao || !fonteIntacta(bridge->source) || !catalog_runtime_current()) return 0;
  if (complete==2) {
    pthread_mutex_lock(&cargaTrava);
    carga.fileiras=(int)row_count;
    carga.falhas=status->failed>INT_MAX?INT_MAX:(int)status->failed;
    pthread_mutex_unlock(&cargaTrava);
    return 1;
  }
  if (title_count>(size_t)(INT_MAX-bridge->prefix) || row_count>(size_t)(CAT_FIL_MAX-fixed)) return 0;
  if ((int)title_count+bridge->prefix>*bridge->capacity) {
    int capacity=(int)title_count+bridge->prefix;
    CatItem *batch=realloc(*bridge->batch,sizeof *batch*(size_t)capacity);
    if (!batch) return 0;
    *bridge->batch=batch; *bridge->capacity=capacity;
  }
  catalog_items_clear(*bridge->batch+bridge->prefix,(size_t)(*bridge->count-bridge->prefix));
  *bridge->count=bridge->prefix;
  for (index=0;index<title_count;index++) {
    const RustTitleSnapshot *title=nuvio_scene_title(scene,index);
    if (!catalog_item_from_rust(*bridge->batch+*bridge->count,title)) { nuvio_title_release(title); return 0; }
    ++*bridge->count;
  }
  catalog_rows_clear(filsLote,CAT_FIL_MAX); nFilsLote=0;
  if (fixed) {
    CatFileira *row=filsLote+nFilsLote++;
    snprintf(row->chave,sizeof row->chave,"continue_watching");
    snprintf(row->titulo,sizeof row->titulo,"Continue Watching");
    snprintf(row->tipo,sizeof row->tipo,"movie");
    row->ini=0; row->n=bridge->prefix;
  }
  for (index=0;index<row_count;index++) {
    const RustRowSnapshot *source=nuvio_scene_row(scene,index);
    if (!catalog_row_from_rust(filsLote+nFilsLote,source)) { nuvio_row_release(source); return 0; }
    filsLote[nFilsLote++].ini+=bridge->prefix;
  }
  pthread_mutex_lock(&cargaTrava);
  carga.fileiras=(int)row_count;
  carga.falhas=status->failed>INT_MAX?INT_MAX:(int)status->failed;
  pthread_mutex_unlock(&cargaTrava);
  if (cat_do_cache() && !cat_home_apenas_fixas()) *bridge->progressive=0;
  if (!complete && *bridge->progressive) {
    nFileirasMontadas=nFilsLote;
    catalog_rows_project(filsMontadas,filsLote,(size_t)nFilsLote,CAT_FIL_MAX);
    *bridge->published=*bridge->count;
    publicarParcial(bridge->batch,bridge->capacity,*bridge->count,filsLote,nFilsLote,bridge->lists,bridge->lists_visible);
  }
  if (!bridge->first_row && row_count) {
    bridge->first_row=1;
    marco(*bridge->progressive ? "primeira fileira da rede na tela" : "primeira fileira da rede pronta (publicacao pendente)");
  }
  return 1;
}

static int prepare_home_choices(RustHomePlan *plan) {
  size_t index, count=nuvio_home_count(plan);
  unsigned generation=fil_perfil_geracao();
  int profile=(int)nuvio_home_profile(plan);
  RustAddonSnapshot *addons=nuvio_home_addons(plan);
  addon_registry_use(addons); nuvio_addons_snapshot_destroy(addons);
  desc_alvos_busca_zerar();
  for (index=0;index<count;index++) {
    if (!catalog_runtime_current() || generation!=fil_perfil_geracao()) return 0;
    char *key=nuvio_home_text(plan,index,0), *name=nuvio_home_text(plan,index,1);
    char *base=nuvio_home_text(plan,index,2), *type=nuvio_home_text(plan,index,3);
    char *id=nuvio_home_text(plan,index,4), *addon=nuvio_home_text(plan,index,5);
    uint32_t features=nuvio_home_flags(plan,index), flags=0;
    int64_t position=-1;
    int row, was_selected=fil_escolhida(key)>=0;
    if (!key || !name || !base || !type || !id || !addon) {
      nuvio_text_destroy(key); nuvio_text_destroy(name); nuvio_text_destroy(base);
      nuvio_text_destroy(type); nuvio_text_destroy(id); nuvio_text_destroy(addon); return 0;
    }
    registrarNomeCatalogo(base,type,id,name);
    if (features & 1) desc_alvo_busca(base,type,id,name,addon);
    int accepted=(features&2) || register_profile_row(profile,generation,key,name,addon,type,-1,(features&4)&&!was_selected);
    if (fil_oculta(key)) flags|=1;
    if (dentroDeColecaoVisivelBase(base,type,id)) flags|=2;
    if (was_selected) flags|=4;
    if (fil_tem_ordem()) {
      for (row=0;row<fil_n();row++) if (!strcmp(fil_chave(row),key)) { position=row; break; }
    }
    if (accepted) nuvio_home_choice(plan,index,position,flags);
    nuvio_text_destroy(key); nuvio_text_destroy(name); nuvio_text_destroy(base);
    nuvio_text_destroy(type); nuvio_text_destroy(id); nuvio_text_destroy(addon);
    if (!accepted) return 0;
  }
  fil_gravar_registro();
  return 1;
}

#ifdef NV_NATIVE_UI_TEST
/* The Rust driver owns provider replies and assertions. This invokes the same
 * choices/publication adapter used by the app, with optional C service inputs. */
int discovery_test_home_loading(RustHomePlan *plan, const RustCatalogWork *work,
                                const CatItem *continuing, const CatItem *saved,
                                int limit, RustHomeLoadStats *stats) {
  int capacity=1, count=continuing?1:0, lists_visible=0, progressive=1, published=-1;
  CatItem *batch=calloc(1,sizeof *batch);
  ListasTrakt *lists=NULL;
  HomeContexto source;
  int result=0;
  if (!batch) return 0;
  if (continuing) catalog_item_copy(batch,continuing);
  if (saved) {
    lists=calloc(1,sizeof *lists);
    if (!lists) goto cleanup;
    lists->wl=calloc(1,sizeof *lists->wl);
    if (!lists->wl) { free(lists); lists=NULL; goto cleanup; }
    catalog_item_copy(lists->wl,saved); lists->wl[0].naLista=1; lists->nWl=1; lists->pronto=1;
  }
  catalog_runtime_use(work);
  if (!prepare_home_choices(plan)) goto cleanup;
  homeestado_contexto(&source);
  HomeLoadingBridge bridge={&batch,&capacity,&count,continuing?1:0,montagemGeracao,&source,lists,&lists_visible,&progressive,&published,0};
  result=nuvio_home_load(plan,work,(size_t)limit,24,publish_home_loading,&bridge,stats);
  if (result>0) {
    nFileirasMontadas=nFilsLote;
    catalog_rows_project(filsMontadas,filsLote,(size_t)nFilsLote,CAT_FIL_MAX);
    publicarParcial(&batch,&capacity,count,filsLote,nFilsLote,lists,&lists_visible);
  }
cleanup:
  catalog_items_clear(batch,(size_t)count); free(batch);
  if (lists) listasAbandonar(lists);
  clear_home_worker_rows();
  return result;
}
#endif
