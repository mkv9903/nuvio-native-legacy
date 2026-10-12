use super::{
    render::{Rect, Renderer},
    workers::Workers,
};
use crate::{
    error::{AppError, Result},
    http::{parse_service_url, HttpRequest, Transport},
};
use std::{
    collections::HashMap,
    io::Cursor,
    sync::{mpsc::Receiver, Arc},
    time::Instant,
};

const TEXTURE_BUDGET: usize = 32 * 1024 * 1024;

pub struct Decoded {
    width: u32,
    height: u32,
    rgba: Vec<u8>,
}
struct Reply {
    key: String,
    result: Result<Decoded>,
}
struct Texture {
    handle: glow::NativeTexture,
    bytes: usize,
    last_frame: u64,
}

pub struct Artwork {
    // Receivers must drop before workers so shutdown cannot block on a full reply queue.
    replies: Receiver<Reply>,
    workers: Workers<Reply>,
    transport: Arc<dyn Transport>,
    textures: HashMap<String, Texture>,
    pending: HashMap<String, ()>,
    failures: HashMap<String, Instant>,
    bytes: usize,
    frame: u64,
}

impl Artwork {
    pub fn new(transport: Arc<dyn Transport>) -> Self {
        let (workers, replies) = Workers::new(1, 8);
        Self {
            replies,
            workers,
            transport,
            textures: HashMap::new(),
            pending: HashMap::new(),
            failures: HashMap::new(),
            bytes: 0,
            frame: 0,
        }
    }
    pub fn begin(&mut self, renderer: &mut Renderer) {
        self.frame = self.frame.wrapping_add(1);
        while let Ok(reply) = self.replies.try_recv() {
            self.pending.remove(&reply.key);
            match reply.result {
                Ok(decoded) => {
                    let bytes = decoded.rgba.len();
                    while self.bytes + bytes > TEXTURE_BUDGET {
                        let Some(key) = self
                            .textures
                            .iter()
                            .min_by_key(|(_, texture)| texture.last_frame)
                            .map(|(key, _)| key.clone())
                        else {
                            break;
                        };
                        let texture = self.textures.remove(&key).expect("cached texture");
                        self.bytes -= texture.bytes;
                        renderer.delete_texture(texture.handle);
                    }
                    if let Ok(handle) =
                        renderer.upload(decoded.width, decoded.height, &decoded.rgba)
                    {
                        self.bytes += bytes;
                        self.textures.insert(
                            reply.key,
                            Texture {
                                handle,
                                bytes,
                                last_frame: self.frame,
                            },
                        );
                    }
                }
                Err(_) => {
                    self.failures.insert(reply.key, Instant::now());
                }
            }
        }
        if self.failures.len() > 512 {
            self.failures
                .retain(|_, time| time.elapsed().as_secs() < 60);
        }
    }
    pub fn draw(
        &mut self,
        renderer: &mut Renderer,
        url: Option<&str>,
        rect: Rect,
        background: bool,
    ) -> bool {
        let Some(url) = url.filter(|url| !url.is_empty()) else {
            return false;
        };
        let key = format!("{}:{url}", if background { "background" } else { "poster" });
        if let Some(texture) = self.textures.get_mut(&key) {
            texture.last_frame = self.frame;
            renderer.texture(rect, texture.handle);
            return true;
        }
        if self.pending.contains_key(&key)
            || self
                .failures
                .get(&key)
                .is_some_and(|time| time.elapsed().as_secs() < 60)
        {
            return false;
        }
        let transport = self.transport.clone();
        let address = url.to_owned();
        let reply_key = key.clone();
        if self
            .workers
            .submit(move |output| {
                let result = decode(&*transport, &address, background);
                let _ = output.send(Reply {
                    key: reply_key,
                    result,
                });
            })
            .is_ok()
        {
            self.pending.insert(key, ());
        }
        false
    }
    pub fn clear(&mut self, renderer: &mut Renderer) {
        for (_, texture) in self.textures.drain() {
            renderer.delete_texture(texture.handle);
        }
        self.bytes = 0;
    }
    pub fn is_idle(&self) -> bool {
        self.pending.is_empty()
    }
}

fn decode(transport: &dyn Transport, address: &str, background: bool) -> Result<Decoded> {
    let mut url = parse_service_url(address)?;
    if url.host_str() == Some("image.tmdb.org") && url.path().contains("/original/") {
        url.set_path(
            &url.path()
                .replace("/original/", if background { "/w1280/" } else { "/w342/" }),
        );
    }
    let mut request = HttpRequest::get(url);
    request.limit = 8 * 1024 * 1024;
    let response = transport.send(request)?;
    response.success()?;
    let mut reader = image::io::Reader::new(Cursor::new(response.body.as_slice()))
        .with_guessed_format()
        .map_err(|_| AppError::InvalidResponse)?;
    let mut limits = image::io::Limits::default();
    limits.max_image_width = Some(4096);
    limits.max_image_height = Some(4096);
    limits.max_alloc = Some(64 * 1024 * 1024);
    reader.limits(limits);
    let image = reader.decode().map_err(|_| AppError::InvalidResponse)?;
    let image = if background {
        image.thumbnail(1280, 720)
    } else {
        image.thumbnail(342, 513)
    };
    let rgba = image.into_rgba8();
    Ok(Decoded {
        width: rgba.width(),
        height: rgba.height(),
        rgba: rgba.into_raw(),
    })
}
