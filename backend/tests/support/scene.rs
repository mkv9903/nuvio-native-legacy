use nuvio_backend::{addons::AddonUrl, catalog_scene::*, catalog_titles::CatalogTitle};
use serde_json::json;

pub fn complete_scene() -> CatalogScene {
    let metadata = CatalogTitle::from_meta(
        &json!({
            "id": format!("kitsu:{}", "complete-identity".repeat(400)),
            "imdb_id": "tt1234567", "type": "series", "name": "Title 日".repeat(100),
            "poster": format!("https://art.invalid/{}", "complete-path".repeat(500)),
            "background": "https://art.invalid/effective", "logo": "https://art.invalid/logo",
            "description": "Complete synopsis 日".repeat(100), "imdbRating": 10,
            "moviedb_id": 987, "genres": ["Drama"], "runtime": "48m", "releaseInfo": "2026",
        }),
        "series",
    )
    .unwrap();
    let mut title = SceneTitle::from(metadata);
    title.origin = "provider-id".into();
    title.artwork = ArtworkVariants {
        catalog_background: "https://art.invalid/catalog".into(),
        tmdb_background: "https://art.invalid/tmdb".into(),
        trakt_background: "https://art.invalid/trakt".into(),
        logo_language: "en".into(),
        logo_language_url: "https://art.invalid/logo".into(),
    };
    title.display = TitleDisplay {
        genre_text: Some("Drama · Adventure".into()),
        metadata_text: Some("2026 · 80 seasons".into()),
        classification: "TV-14".into(),
        director: "Director 日".repeat(30),
    };
    title.cast = (0..25)
        .map(|index| CastMember {
            name: format!("Actor {index} {}", "日".repeat(50)),
            role: format!("Role {index}"),
            photo: format!(
                "https://art.invalid/actor-{index}/{}",
                "full-photo".repeat(100)
            ),
            tmdb_id: Some(5_000_000_000 + index),
        })
        .collect();
    title.availability = Availability {
        subscription: Some(CommercialProvider {
            name: "Subscription".into(),
            logo: "subscription-logo".into(),
        }),
        rental: Some(CommercialProvider {
            name: "Rental".into(),
            logo: "rental-logo".into(),
        }),
        purchase: Some(CommercialProvider {
            name: "Purchase".into(),
            logo: "purchase-logo".into(),
        }),
    };
    title.resume = ResumeInfo {
        percentage: 47,
        season: 5,
        episode: 13,
        remaining_minutes: 24,
        episode_name: "Episode 日".repeat(50),
        last_watched_ms: 1_234_567_890_123,
    };
    title.seasons = (0..80).collect();
    title.on_watchlist = true;
    title.in_collection = true;
    CatalogScene {
        titles: vec![title],
        rows: vec![CatalogRow {
            key: "complete-row-key".repeat(300),
            name: "Series row 日".into(),
            media_type: "series".into(),
            source: Some(CatalogSource {
                addon: AddonUrl::parse(&format!(
                    "https://provider.invalid/{}?private=keep",
                    "config".repeat(1200)
                ))
                .unwrap(),
                media_type: "series".into(),
                catalog_id: "catalog/日".repeat(100),
                extras: vec![
                    ("genre".into(), "Sci-Fi 日".into()),
                    ("skip".into(), "24".into()),
                ],
            }),
            titles: 0..1,
            state: CatalogRowState::Ready,
        }],
    }
}
