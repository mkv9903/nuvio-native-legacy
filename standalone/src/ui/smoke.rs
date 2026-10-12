use super::{
    app::{App, Input, Page},
    render::Renderer,
};
use std::{
    path::PathBuf,
    time::{Duration, Instant},
};

pub struct Smoke {
    directory: PathBuf,
    stage: usize,
    frames: usize,
    started: Instant,
}
impl Smoke {
    pub fn new(directory: PathBuf) -> Result<Self, String> {
        std::fs::create_dir_all(&directory)
            .map_err(|_| "Smoke output directory could not be created")?;
        Ok(Self {
            directory,
            stage: 0,
            frames: 0,
            started: Instant::now(),
        })
    }
    fn capture(&self, renderer: &mut Renderer, name: &str) -> Result<(), String> {
        renderer.screenshot(&self.directory.join(name))
    }
    pub fn advance(
        &mut self,
        app: &mut App,
        renderer: &mut Renderer,
        artwork_ready: bool,
    ) -> Result<bool, String> {
        if self.started.elapsed() > Duration::from_secs(20) {
            return Err(format!(
                "UI fixture smoke timed out at stage {}",
                self.stage
            ));
        }
        self.frames += 1;
        if self.frames < 12 {
            return Ok(false);
        }
        if !artwork_ready {
            return Ok(false);
        }
        match self.stage {
            0 => {
                self.capture(renderer, "login.png")?;
                app.submit_password(
                    "fixture@example.invalid",
                    crate::secret::Secret::new("fixture-password"),
                );
            }
            1 if app.page == Page::Profiles => {
                self.capture(renderer, "profiles.png")?;
                app.select_profile(0);
            }
            2 if home_ready(app) => {
                self.capture(renderer, "home-primary.png")?;
                app.open_title(
                    app.focused_title()
                        .cloned()
                        .ok_or("Home fixture has no title")?,
                );
            }
            3 if !app.detail_loading => {
                self.capture(renderer, "movie-details.png")?;
                app.input(Input::Accept);
            }
            4 if app.sources_loading == 0 && !app.sources.is_empty() => {
                self.capture(renderer, "movie-sources.png")?;
                if app.take_playback().is_some() {
                    return Err("Sources autoplayed before selection".to_owned());
                }
                app.input(Input::Accept);
                if app.take_playback().is_none() {
                    return Err("Manual source selection did not produce playback".to_owned());
                }
                app.open_profiles();
                app.select_profile(1);
            }
            5 if home_ready(app) => {
                if app.home.rows.len() != 1 || app.home.rows[0].catalog.kind != "series" {
                    return Err("Shared profile reused primary Home rows".to_owned());
                }
                self.capture(renderer, "home-shared.png")?;
                app.open_title(
                    app.focused_title()
                        .cloned()
                        .ok_or("Shared fixture has no title")?,
                );
            }
            6 if !app.detail_loading => {
                self.capture(renderer, "series-details.png")?;
                app.input(Input::Down);
                app.input(Input::Accept);
            }
            7 if app.sources_loading == 0 && !app.sources.is_empty() => {
                self.capture(renderer, "episode-sources.png")?;
                if !app.video_id.ends_with(":1:2") {
                    return Err("Series source request used the wrong episode".to_owned());
                }
                if app.take_playback().is_some() {
                    return Err("Episode sources autoplayed".to_owned());
                }
                app.open_profiles();
                app.select_profile(2);
            }
            8 if home_ready(app) => {
                if !app.home.rows[0].name.starts_with("Independent") {
                    return Err("Independent profile reused primary catalogs".to_owned());
                }
                self.capture(renderer, "home-independent.png")?;
                app.search("signal");
            }
            9 if app.search_loading == 0 && !app.search_titles.is_empty() => {
                self.capture(renderer, "search.png")?;
                println!("[smoke] Login, profiles, Home, movie/episode details, search and manual sources passed");
                return Ok(true);
            }
            _ => return Ok(false),
        }
        self.stage += 1;
        self.frames = 0;
        Ok(false)
    }
}
fn home_ready(app: &App) -> bool {
    app.page == Page::Home
        && !app.catalogs_loading()
        && app
            .home
            .rows
            .first()
            .is_some_and(|row| row.loaded && !row.titles.is_empty())
}
