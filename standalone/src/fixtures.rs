//! Synthetic developer fixtures. Excluded from release builds.
use crate::{
    error::{AppError, Result},
    http::{HttpRequest, HttpResponse, Transport},
};
use image::ImageEncoder;
use serde_json::{json, Value};
use std::sync::Arc;
use zeroize::Zeroizing;

pub fn transport() -> Arc<dyn Transport> {
    Arc::new(FixtureTransport)
}
struct FixtureTransport;
impl Transport for FixtureTransport {
    fn send(&self, request: HttpRequest) -> Result<HttpResponse> {
        if request.url.host_str() == Some("art.example.invalid") {
            let image = illustration(request.url.path());
            return Ok(HttpResponse {
                status: 200,
                body: Zeroizing::new(image),
            });
        }
        let path = request.url.path();
        let body: Value = serde_json::from_slice(request.body_bytes()).unwrap_or(Value::Null);
        let value = match path {
            "/auth/v1/token" | "/auth/v1/signup" | "/functions/v1/tv-logins-exchange" => {
                json!({"access_token":"fixture-access","refresh_token":"fixture-refresh","expires_in":3600,"user":{"id":"fixture-account"}})
            }
            "/rest/v1/rpc/get_sync_owner" => json!("fixture-owner"),
            "/rest/v1/rpc/sync_pull_profiles" => json!([
                {"profile_index":1,"name":"Primary","is_primary":true,"avatar_color_hex":"#305A82"},
                {"profile_index":2,"name":"Shared addons","uses_primary_addons":true,"avatar_color_hex":"#7B4778"},
                {"profile_index":3,"name":"Independent","avatar_color_hex":"#3F756C"}]),
            "/rest/v1/rpc/sync_pull_profile_locks" => json!([]),
            "/rest/v1/rpc/verify_profile_pin" => json!(body["p_pin"] == "1234"),
            "/rest/v1/rpc/start_tv_login_session" => {
                json!({"code":"TEST123","web_url":"https://login.example.invalid/tv?code=TEST123","poll_interval_seconds":60})
            }
            "/rest/v1/rpc/poll_tv_login_session" => json!({"status":"pending"}),
            "/rest/v1/addons" => {
                let own = request
                    .url
                    .query_pairs()
                    .any(|(key, value)| key == "profile_id" && value == "eq.3");
                json!([{ "url":if own {"https://addon.example.invalid/independent/manifest.json"}else{"https://addon.example.invalid/primary/manifest.json"},"name":"Fixture addon","enabled":true }])
            }
            "/rest/v1/rpc/sync_pull_home_catalog_settings" => {
                let items = if body["p_profile_id"] == 2 {
                    json!([{ "addon_id":"fixture","type":"movie","catalog_id":"movies","enabled":false,"order":0 }])
                } else {
                    json!([])
                };
                json!([{ "settings_json":{"items":items} }])
            }
            _ if path.ends_with("manifest.json") => {
                let own = path.contains("/independent/");
                json!({"id":"fixture","name":"Fixture catalog","types":["movie","series"],"resources":["catalog","meta","stream"],"catalogs":[
                    {"id":"movies","type":"movie","name":if own {"Independent movie picks"}else{"Featured movies"},"extra":[{"name":"search"},{"name":"skip"}]},
                    {"id":"series","type":"series","name":if own {"Independent series picks"}else{"Popular series"},"extra":[{"name":"search"},{"name":"skip"}]}]})
            }
            _ if path.contains("/catalog/") => {
                let kind = if path.contains("/series/") {
                    "series"
                } else {
                    "movie"
                };
                let own = path.contains("/independent/");
                let start = if own { 10 } else { 0 };
                let titles: Vec<_> = (start..start + 12)
                    .map(|index| title(kind, index))
                    .collect();
                json!({"metas":titles})
            }
            _ if path.contains("/meta/") => {
                let kind = if path.contains("/series/") {
                    "series"
                } else {
                    "movie"
                };
                let index = path
                    .rsplit('/')
                    .next()
                    .unwrap_or_default()
                    .trim_end_matches(".json")
                    .trim_start_matches("ttfixture")
                    .parse::<usize>()
                    .unwrap_or(0);
                let mut value = title(kind, index);
                if kind == "series" {
                    value["videos"] = json!([
                    {"id":format!("ttfixture{index}:1:1"),"season":1,"episode":1,"title":"A signal in the dark"},
                    {"id":format!("ttfixture{index}:1:2"),"season":1,"episode":2,"title":"The second horizon"},
                    {"id":format!("ttfixture{index}:2:1"),"season":2,"episode":1,"title":"Home again"}]);
                }
                json!({"meta":value})
            }
            _ if path.contains("/stream/") => json!({"streams":[
                {"name":"Fixture · 1080p","title":"1080p · English audio","url":"https://media.example.invalid/selected.mkv"},
                {"name":"Fixture · 720p","title":"720p · English audio","url":"https://media.example.invalid/alternate.mkv"}]}),
            _ => return Err(AppError::Http(404)),
        };
        Ok(HttpResponse {
            status: 200,
            body: Zeroizing::new(
                serde_json::to_vec(&value).map_err(|_| AppError::InvalidResponse)?,
            ),
        })
    }
}
fn title(kind: &str, index: usize) -> Value {
    let names = [
        "The Last Signal",
        "Across the Stars",
        "A Quiet Horizon",
        "After the Rain",
        "The Long Way Home",
        "Beyond the North",
        "A New Dawn",
        "Hidden Worlds",
        "Winter Lights",
        "Second Chances",
        "The Far Side",
        "Midnight Journey",
    ];
    json!({"id":format!("ttfixture{index}"),"type":kind,"name":names[index%names.len()],"releaseInfo":"2026","runtime":"112 min","genres":["Drama","Adventure"],"imdbRating":"8.2",
        "description":"An unexpected discovery brings a group of strangers together. As they follow the clues, their journey leads them beyond the familiar and toward a world of new possibilities.",
        "poster":format!("https://art.example.invalid/poster/{index}.png"),"background":format!("https://art.example.invalid/background/{index}.png")})
}
fn illustration(path: &str) -> Vec<u8> {
    let (w, h) = if path.contains("background") {
        (640u32, 360u32)
    } else {
        (200u32, 300u32)
    };
    let seed = path
        .bytes()
        .fold(0u32, |sum, byte| sum.wrapping_add(byte as u32));
    let mut image = image::RgbaImage::new(w, h);
    for (x, y, pixel) in image.enumerate_pixels_mut() {
        let halo = (((x as f32 / w as f32 - 0.68).powi(2) + (y as f32 / h as f32 - 0.40).powi(2))
            .sqrt()
            * 255.0) as u8;
        let light = 170u8.saturating_sub(halo);
        let star = if (x * 31 + y * 71 + seed) % 1031 < 3 {
            80
        } else {
            0
        };
        *pixel = image::Rgba([
            15u8.saturating_add(light / 3).saturating_add(star),
            20u8.saturating_add(light / 2).saturating_add(star),
            40u8.saturating_add(light).saturating_add(star),
            255,
        ]);
    }
    let mut bytes = Vec::new();
    image::codecs::png::PngEncoder::new(&mut bytes)
        .write_image(image.as_raw(), w, h, image::ColorType::Rgba8)
        .expect("fixture image");
    bytes
}
