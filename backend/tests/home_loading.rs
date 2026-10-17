use nuvio_backend::{
    addon_snapshot::{AddonRecord, AddonSnapshot},
    addons::{AddonUrl, InstalledAddon, Manifest},
    catalog_order::HomeOrder,
    catalog_runtime::CatalogResult,
    catalog_scene::{CatalogRow, CatalogRowState, CatalogScene, CatalogSource, SceneTitle},
    catalog_titles::CatalogTitle,
    error::Error,
    home_loading::{HomePlan, UpdateKind, CATALOG_WORKERS},
    scene_snapshot::SceneSnapshot,
};
use serde_json::json;
use std::{
    collections::HashMap,
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc, Condvar, Mutex,
    },
};

fn source(providers: usize, catalogs: usize) -> AddonSnapshot {
    AddonSnapshot {
        context:None,revision:1,list_revision:1,authoritative:true,pending:false,
        records:(0..providers).map(|provider| AddonRecord {
            installed:InstalledAddon {name:format!("Addon {provider}"),url:AddonUrl::parse(&format!("https://provider{provider}.invalid/config")).unwrap(),enabled:true},
            manifest:Some(Arc::new(Manifest::decode(json!({"id":format!("provider{provider}"),"resources":["catalog"],"catalogs":(0..catalogs).map(|id| json!({"type":"movie","id":format!("c{id}"),"name":format!("Catalog {id}")})).collect::<Vec<_>>()} ).to_string().as_bytes()).unwrap())), silent_queries:0,
        }).collect::<Vec<_>>().into(),
    }
}
fn plan(providers: usize, catalogs: usize) -> HomePlan {
    HomePlan::new(
        source(providers, catalogs),
        Arc::new(HomeOrder::default()),
        None,
    )
}
fn result(plan: &HomePlan, index: usize, empty: bool) -> CatalogResult {
    let entry = &plan.catalogs[index];
    let addon = plan.source.records[entry.addon_index].installed.url.clone();
    let title=SceneTitle::from(CatalogTitle::from_meta(&json!({"id":format!("title-{index}"),"name":format!("Title {index}"),"poster":"https://art.invalid/poster"}),"movie").unwrap());
    let titles = if empty { Vec::new() } else { vec![title] };
    let count = titles.len();
    CatalogResult {
        responded: true,
        scene: SceneSnapshot::new(CatalogScene {
            titles,
            rows: vec![CatalogRow {
                key: entry.key.clone(),
                name: entry.name.clone(),
                media_type: entry.media_type.clone(),
                source: Some(CatalogSource {
                    addon,
                    media_type: entry.media_type.clone(),
                    catalog_id: entry.id.clone(),
                    extras: vec![],
                }),
                titles: 0..count,
                state: if empty {
                    CatalogRowState::ConfirmedEmpty
                } else {
                    CatalogRowState::Ready
                },
            }],
        })
        .unwrap(),
    }
}
fn keys(plan: &HomePlan) -> Vec<String> {
    plan.order()
        .iter()
        .map(|index| plan.catalogs[*index].key.clone())
        .collect()
}

#[test]
fn complete_manifests_have_no_addon_or_declaration_quota() {
    let mut plan = plan(80, 703);
    assert_eq!(plan.catalogs.len(), 80 * 703);
    let wanted = 79 * 703 + 702;
    plan.catalogs[wanted].choice.local_position = Some(0);
    assert_eq!(plan.order()[0], wanted);
    assert_eq!(plan.order().len(), 80 * 703);
}
#[test]
fn saved_account_order_and_explicit_local_order_precede_new_rows() {
    let mut plan = plan(3, 3);
    plan.account_order = Arc::new(HomeOrder {
        keys: vec![
            "provider0_movie_c2".into(),
            "provider0_movie_c1".into(),
            "provider0_movie_c0".into(),
        ],
        ..Default::default()
    });
    assert_eq!(
        &keys(&plan)[..3],
        &[
            "provider0_movie_c2",
            "provider0_movie_c1",
            "provider0_movie_c0"
        ]
    );
    plan.catalogs[8].choice.local_position = Some(0);
    assert_eq!(plan.order()[0], 8);
    assert_eq!(
        &keys(&plan)[1..4],
        &[
            "provider0_movie_c2",
            "provider0_movie_c1",
            "provider0_movie_c0"
        ]
    );
}
#[test]
fn installed_addons_do_not_displace_the_previously_accepted_home() {
    let mut plan = plan(2, 7);
    let previous = result(&plan, 0, false).scene;
    plan.previous = Some(Arc::new(previous));
    assert_eq!(plan.order()[0], 0);
    plan.previous = Some(Arc::new(result(&plan, 5, false).scene));
    assert_eq!(plan.order()[0], 5);
    let loaded = plan
        .load(
            1,
            &|index| Ok(result(&plan, index, false)),
            &|| true,
            |_, _, _| true,
        )
        .unwrap();
    assert_eq!(loaded.scene.rows[0].record.key, "provider0_movie_c5");
}
#[test]
fn visibility_covers_local_account_alias_and_visible_collections() {
    let mut plan = plan(1, 5);
    plan.catalogs[0].choice.hidden = true;
    plan.catalogs[1].choice.represented_by_collection = true;
    plan.account_order = Arc::new(HomeOrder {
        hidden: [plan.catalogs[2].key.clone(), plan.disable_key(3)].into(),
        ..Default::default()
    });
    assert_eq!(plan.order(), vec![4]);
    plan.catalogs[1].choice.represented_by_collection = false;
    assert_eq!(plan.order(), vec![1, 4]);
}
#[test]
fn disabled_addons_and_duplicate_catalogs_never_produce_rows() {
    let mut snapshot = source(2, 2);
    let mut records = snapshot.records.to_vec();
    records[1].installed.enabled = false;
    snapshot.records = records.into();
    let mut plan = HomePlan::new(snapshot, Arc::new(HomeOrder::default()), None);
    plan.catalogs.push(plan.catalogs[0].clone());
    assert_eq!(plan.order(), vec![0, 1]);
}
#[test]
fn required_extras_and_unselected_channels_do_not_consume_home_slots() {
    let mut snapshot = source(1, 0);
    let mut records = snapshot.records.to_vec();
    records[0].manifest=Some(Arc::new(Manifest::decode(br#"{"id":"p","resources":["catalog"],"catalogs":[{"type":"movie","id":"search","extra":[{"name":"search","isRequired":true}]},{"type":"movie","id":"genre","extraRequired":["genre"]},{"type":"channel","id":"live"},{"type":"movie","id":"normal"}]}"#).unwrap()));
    snapshot.records = records.into();
    let mut plan = HomePlan::new(snapshot, Arc::new(HomeOrder::default()), None);
    assert!(plan.catalogs[0].searchable);
    assert_eq!(plan.order(), vec![3]);
    plan.catalogs[2].choice.channel_selected = true;
    assert_eq!(plan.order(), vec![2, 3]);
}
#[test]
fn empty_and_failed_catalogs_yield_their_slots_to_later_candidates() {
    let plan = plan(1, 8);
    let calls = Mutex::new(Vec::new());
    let loaded = plan
        .load(
            3,
            &|index| {
                calls.lock().unwrap().push(index);
                if index == 0 {
                    Err(Error::Timeout)
                } else {
                    Ok(result(&plan, index, index == 1))
                }
            },
            &|| true,
            |_, _, _| true,
        )
        .unwrap();
    assert_eq!(loaded.stats.requested, 5);
    assert_eq!(loaded.stats.empty, 1);
    assert_eq!(loaded.stats.failed, 1);
    assert_eq!(loaded.stats.rows, 3);
    assert_eq!(loaded.stats.remaining, 3);
    assert_eq!(
        loaded
            .scene
            .rows
            .iter()
            .map(|r| r.record.key.as_str())
            .collect::<Vec<_>>(),
        vec![
            "provider0_movie_c2",
            "provider0_movie_c3",
            "provider0_movie_c4"
        ]
    );
    assert!(calls.lock().unwrap().iter().all(|index| *index < 5));
}
#[test]
fn failures_preserve_valid_previous_rows_but_confirmed_empty_clears_them() {
    let mut plan = plan(1, 1);
    plan.previous = Some(Arc::new(result(&plan, 0, false).scene));
    let preserved = plan
        .load(1, &|_| Err(Error::InvalidData), &|| true, |_, _, _| true)
        .unwrap();
    assert_eq!(preserved.scene.titles.len(), 1);
    let emptied = plan
        .load(
            1,
            &|index| Ok(result(&plan, index, true)),
            &|| true,
            |_, _, _| true,
        )
        .unwrap();
    assert!(emptied.scene.rows.is_empty());
    assert!(emptied.scene.titles.is_empty());
}
#[test]
fn first_load_is_progressive_and_out_of_order_replies_finish_in_saved_order() {
    let plan = plan(1, 3);
    let gate = Arc::new((Mutex::new(false), Condvar::new()));
    let callbacks = Mutex::new(Vec::new());
    let result = plan
        .load(
            3,
            &|index| {
                if index == 0 {
                    let mut ready = gate.0.lock().unwrap();
                    while !*ready {
                        ready = gate.1.wait(ready).unwrap();
                    }
                }
                Ok(result(&plan, index, false))
            },
            &|| true,
            |scene, kind, _| {
                callbacks.lock().unwrap().push(kind);
                if kind == UpdateKind::Partial {
                    let mut ready = gate.0.lock().unwrap();
                    if !*ready {
                        assert!(!scene
                            .rows
                            .iter()
                            .any(|row| row.record.key == "provider0_movie_c0"));
                    }
                    *ready = true;
                    gate.1.notify_all();
                }
                true
            },
        )
        .unwrap();
    assert_eq!(
        result
            .scene
            .rows
            .iter()
            .map(|r| r.record.key.as_str())
            .collect::<Vec<_>>(),
        vec![
            "provider0_movie_c0",
            "provider0_movie_c1",
            "provider0_movie_c2"
        ]
    );
    assert_eq!(
        callbacks.lock().unwrap().last(),
        Some(&UpdateKind::Complete)
    );
}
#[test]
fn cached_home_and_saved_list_only_home_stay_stable_until_completion() {
    for saved_only in [false, true] {
        let mut plan = plan(1, 2);
        let mut previous = result(&plan, 0, false).scene.to_scene();
        if saved_only {
            previous.rows.clear();
            previous.titles[0].on_watchlist = true;
        }
        plan.previous = Some(Arc::new(SceneSnapshot::new(previous).unwrap()));
        let mut updates = Vec::new();
        plan.load(
            2,
            &|index| Ok(result(&plan, index, false)),
            &|| true,
            |_, kind, _| {
                updates.push(kind);
                true
            },
        )
        .unwrap();
        assert!(updates[..updates.len() - 1]
            .iter()
            .all(|kind| *kind == UpdateKind::Progress));
        assert_eq!(updates.last(), Some(&UpdateKind::Complete));
    }
}
#[test]
fn network_workers_are_bounded_and_snapshot_titles_share_owners() {
    let plan = plan(1, 20);
    let active = AtomicUsize::new(0);
    let peak = AtomicUsize::new(0);
    let owners = Mutex::new(HashMap::new());
    let loaded = plan
        .load(
            20,
            &|index| {
                let running = active.fetch_add(1, Ordering::SeqCst) + 1;
                peak.fetch_max(running, Ordering::SeqCst);
                std::thread::sleep(std::time::Duration::from_millis(2));
                let scene = result(&plan, index, false);
                owners
                    .lock()
                    .unwrap()
                    .insert(index, Arc::downgrade(&scene.scene.titles[0]));
                active.fetch_sub(1, Ordering::SeqCst);
                Ok(scene)
            },
            &|| true,
            |_, _, _| true,
        )
        .unwrap();
    assert!(peak.load(Ordering::SeqCst) <= CATALOG_WORKERS);
    assert!(owners
        .lock()
        .unwrap()
        .values()
        .all(|owner| owner.upgrade().is_some()));
    drop(loaded);
    assert!(owners
        .lock()
        .unwrap()
        .values()
        .all(|owner| owner.upgrade().is_none()));
}
#[test]
fn cancellation_never_emits_more_visible_results_or_a_final_scene() {
    let plan = plan(1, 20);
    let mut callbacks = 0;
    assert_eq!(
        plan.load(
            20,
            &|index| Ok(result(&plan, index, false)),
            &|| true,
            |_, kind, _| {
                assert_ne!(kind, UpdateKind::Complete);
                callbacks += 1;
                false
            }
        )
        .err(),
        Some(Error::Cancelled)
    );
    assert_eq!(callbacks, 1);
    assert_eq!(
        plan.load(
            2,
            &|_| panic!("cancelled work fetched"),
            &|| false,
            |_, _, _| panic!("cancelled work published")
        )
        .err(),
        Some(Error::Cancelled)
    );
}
#[test]
fn missing_manifests_keep_useful_same_source_scene_rows_without_requests() {
    let first = plan(1, 1);
    let previous = Arc::new(result(&first, 0, false).scene);
    let mut source = first.source.clone();
    let mut records = source.records.to_vec();
    records[0].manifest = None;
    source.records = records.into();
    let plan = HomePlan::new(source, Arc::new(HomeOrder::default()), Some(previous));
    let loaded = plan
        .load(
            1,
            &|_| panic!("unknown manifest requires cached scene only"),
            &|| true,
            |_, _, _| true,
        )
        .unwrap();
    assert_eq!(loaded.scene.titles.len(), 1);
    assert_eq!(loaded.scene.rows[0].record.key, "provider0_movie_c0");
}
#[test]
fn an_explicitly_removed_catalog_is_not_recovered_from_previous_scene() {
    let first = plan(1, 1);
    let previous = Arc::new(result(&first, 0, false).scene);
    let plan = HomePlan::new(source(1, 0), Arc::new(HomeOrder::default()), Some(previous));
    assert!(plan.order().is_empty());
    let loaded = plan
        .load(
            5,
            &|_| panic!("removed catalog fetched"),
            &|| true,
            |_, _, _| true,
        )
        .unwrap();
    assert!(loaded.scene.rows.is_empty());
}

#[test]
fn failed_preferred_rows_are_retried_before_successful_replacements() {
    let mut first = plan(1, 3);
    first.saved_keys = first
        .declared_order()
        .iter()
        .map(|index| first.catalogs[*index].key.clone())
        .collect();
    let loaded = first
        .load(
            1,
            &|index| {
                if index == 0 {
                    Err(Error::Network)
                } else {
                    Ok(result(&first, index, false))
                }
            },
            &|| true,
            |_, _, _| true,
        )
        .unwrap();
    assert_eq!(loaded.scene.rows[0].record.key, "provider0_movie_c1");
    let mut next = HomePlan::new(
        first.source.clone(),
        first.account_order.clone(),
        Some(loaded.scene),
    );
    next.saved_keys = first.saved_keys.clone();
    assert_eq!(
        next.order()[0],
        0,
        "a failed row keeps its original retry position"
    );
    next.account_order = Arc::new(HomeOrder {
        keys: vec!["provider0_movie_c2".into()],
        ..Default::default()
    });
    assert_eq!(
        next.order()[0],
        2,
        "a later explicit account choice still wins"
    );
}
