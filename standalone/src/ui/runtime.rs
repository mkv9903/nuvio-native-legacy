use super::{
    app::{App, Input},
    artwork::Artwork,
    render::Renderer,
    screens,
    smoke::Smoke,
};
use crate::{
    account::{BackendConfig, CloudClient, SessionVault},
    error::AppError,
    http::{HttpClient, HttpRequest, HttpResponse, Transport},
    storage::DataStore,
    APP_ID, APP_NAME,
};
use sdl2::{
    event::Event,
    keyboard::{Keycode, Mod},
    video::GLProfile,
};
use std::{path::PathBuf, sync::Arc, time::Duration};

struct MissingConfiguration;
impl Transport for MissingConfiguration {
    fn send(&self, _: HttpRequest) -> crate::error::Result<HttpResponse> {
        Err(AppError::Configuration)
    }
}
pub fn run() -> Result<(), String> {
    let mut fixture = false;
    let mut smoke_dir = None;
    let mut args = std::env::args().skip(1);
    while let Some(argument) = args.next() {
        match argument.as_str() {
            "--fixture" if cfg!(debug_assertions) => fixture = true,
            "--smoke-dir" if cfg!(debug_assertions) => {
                smoke_dir = Some(PathBuf::from(
                    args.next()
                        .ok_or("--smoke-dir requires an output directory")?,
                ));
                fixture = true;
            }
            "--help" => {
                println!("{APP_NAME}\nArrows: move  Enter: OK  Escape: Back  S: search  P: profiles\nText editor: type normally, Tab or Ctrl+Enter to confirm\nDebug Linux builds also accept --fixture and --smoke-dir PATH.");
                return Ok(());
            }
            _ => return Err("Unknown command-line option".to_owned()),
        }
    }
    println!("[startup] {APP_NAME}: standalone Rust, id={APP_ID}");
    #[cfg(feature = "webos")]
    {
        std::env::set_var("APPID", APP_ID);
        std::env::set_var("LS2_APPID", APP_ID);
        sdl2::hint::set("SDL_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
    }
    let (config, transport, missing): (BackendConfig, Arc<dyn Transport>, bool) = if fixture {
        #[cfg(debug_assertions)]
        {
            (
                BackendConfig::new(
                    "https://backend.example.invalid",
                    "fixture-anon",
                    "https://login.example.invalid/tv",
                )
                .map_err(|error| error.to_string())?,
                crate::fixtures::transport(),
                false,
            )
        }
        #[cfg(not(debug_assertions))]
        {
            return Err("Fixtures are unavailable in release builds".to_owned());
        }
    } else {
        match BackendConfig::embedded() {
            Ok(config) => (config, Arc::new(HttpClient::default()), false),
            Err(_) => (
                BackendConfig::new(
                    "https://unconfigured.example.invalid",
                    "unused",
                    "https://unconfigured.example.invalid/tv",
                )
                .map_err(|error| error.to_string())?,
                Arc::new(MissingConfiguration),
                true,
            ),
        }
    };
    let store = if fixture {
        let root = match &smoke_dir {
            Some(path) => path.join("fixture-data"),
            None => {
                std::env::temp_dir().join(format!("nuvio-rust-preview-{}", uuid::Uuid::new_v4()))
            }
        };
        let store = DataStore::new(root).map_err(|error| error.to_string())?;
        store
            .remove("session.json")
            .map_err(|error| error.to_string())?;
        store
    } else {
        DataStore::discover().map_err(|error| error.to_string())?
    };
    let vault = Arc::new(SessionVault::new(store).map_err(|error| error.to_string())?);
    let cloud = Arc::new(CloudClient::new(Arc::new(config), transport.clone(), vault));
    let mut app = App::new(cloud, transport.clone());
    if missing {
        app.banner = AppError::Configuration.to_string();
    }
    let sdl = sdl2::init()?;
    crate::platform::input::install();
    let video = sdl.video()?;
    {
        let attributes = video.gl_attr();
        attributes.set_context_profile(GLProfile::GLES);
        attributes.set_context_version(2, 0);
        attributes.set_red_size(8);
        attributes.set_green_size(8);
        attributes.set_blue_size(8);
        attributes.set_alpha_size(8);
        attributes.set_double_buffer(true);
    }
    let mut builder = video.window(APP_NAME, 1280, 720);
    builder.opengl().position_centered();
    #[cfg(feature = "webos")]
    builder.fullscreen_desktop();
    #[cfg(not(feature = "webos"))]
    builder.resizable();
    let window = builder.build().map_err(|error| error.to_string())?;
    let _context = window.gl_create_context()?;
    let _ = video.gl_set_swap_interval(sdl2::video::SwapInterval::VSync);
    let gl = unsafe {
        glow::Context::from_loader_function(|name| video.gl_get_proc_address(name).cast())
    };
    let (width, height) = window.drawable_size();
    let mut renderer = Renderer::new(gl, width, height)?;
    let mut artwork = Artwork::new(transport);
    let mut events = sdl.event_pump()?;
    video.text_input().start();
    let mut smoke = smoke_dir.map(Smoke::new).transpose()?;
    'running: loop {
        for event in events.poll_iter() {
            match event {
                Event::Quit { .. } => break 'running,
                Event::TextInput { text, .. } => app.text_input(&text),
                Event::KeyDown {
                    keycode: Some(key),
                    keymod,
                    ..
                } => {
                    let control = keymod.intersects(Mod::LCTRLMOD | Mod::RCTRLMOD);
                    let input = match key {
                        Keycode::Up => Some(Input::Up),
                        Keycode::Down => Some(Input::Down),
                        Keycode::Left => Some(Input::Left),
                        Keycode::Right => Some(Input::Right),
                        Keycode::Return | Keycode::KpEnter => Some(if control {
                            Input::Confirm
                        } else {
                            Input::Accept
                        }),
                        Keycode::Escape | Keycode::AcBack => Some(Input::Back),
                        Keycode::Backspace => Some(Input::Delete),
                        Keycode::Tab => Some(Input::Confirm),
                        Keycode::S => Some(Input::Search),
                        Keycode::P => Some(Input::Profiles),
                        Keycode::L if control => {
                            app.logout();
                            artwork.clear(&mut renderer);
                            None
                        }
                        _ => None,
                    };
                    if let Some(input) = input {
                        app.input(input);
                    }
                }
                Event::Window {
                    win_event: sdl2::event::WindowEvent::SizeChanged(_, _),
                    ..
                } => {
                    let (width, height) = window.drawable_size();
                    renderer.resize(width, height);
                }
                _ => {}
            }
        }
        app.tick();
        screens::draw(&app, &mut renderer, &mut artwork);
        if let Some(smoke) = &mut smoke {
            if smoke.advance(&mut app, &mut renderer, artwork.is_idle())? {
                break 'running;
            }
        }
        if app.take_playback().is_some() {
            println!("[player] Manual source selection received");
            app.banner = "Playback is not available in this development preview yet.".to_owned();
        }
        window.gl_swap_window();
        std::thread::sleep(Duration::from_millis(2));
    }
    artwork.clear(&mut renderer);
    println!("[shutdown] Standalone UI closed");
    Ok(())
}
