//! GLES2 batches and a bounded, lazily rasterized font atlas.
use ab_glyph::{point, Font, FontArc, ScaleFont};
use glow::HasContext;
use std::{collections::HashMap, path::Path};

pub type Color = [f32; 4];
pub const WHITE: Color = [0.96, 0.96, 0.97, 1.0];
pub const MUTED: Color = [0.56, 0.57, 0.60, 1.0];
pub const ACCENT: Color = [0.91, 0.72, 0.23, 1.0];
pub const PANEL: Color = [0.09, 0.095, 0.11, 1.0];
pub const WIDTH: f32 = 1920.0;
pub const HEIGHT: f32 = 1080.0;
const ATLAS: i32 = 1024;

#[derive(Clone, Copy)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
}
impl Rect {
    pub fn new(x: f32, y: f32, w: f32, h: f32) -> Self {
        Self { x, y, w, h }
    }
}

#[derive(Clone, Copy)]
struct Glyph {
    uv: [f32; 4],
    offset: [f32; 2],
    size: [f32; 2],
    advance: f32,
}

pub struct Renderer {
    pub gl: glow::Context,
    program: glow::NativeProgram,
    buffer: glow::NativeBuffer,
    atlas: glow::NativeTexture,
    mode_uniform: Option<glow::NativeUniformLocation>,
    font: FontArc,
    glyphs: HashMap<(char, u16), Glyph>,
    atlas_x: i32,
    atlas_y: i32,
    shelf_height: i32,
    vertices: Vec<f32>,
    mode: i32,
    texture: Option<glow::NativeTexture>,
    drawable: (i32, i32),
    scale: f32,
}

impl Renderer {
    pub fn new(gl: glow::Context, width: u32, height: u32) -> Result<Self, String> {
        let font = FontArc::try_from_vec(
            include_bytes!("../../../deploy/app/fonts/InterDisplay-Regular.ttf").to_vec(),
        )
        .map_err(|_| "The application font could not be loaded".to_owned())?;
        let (program, buffer, atlas, mode_uniform) = unsafe {
            let program = gl.create_program()?;
            let vertex = compile(
                &gl,
                glow::VERTEX_SHADER,
                r#"#version 100
attribute vec2 a_position;
attribute vec2 a_uv;
attribute vec4 a_color;
varying vec2 v_uv;
varying vec4 v_color;
void main() { gl_Position=vec4(a_position.x/960.0-1.0,1.0-a_position.y/540.0,0.0,1.0); v_uv=a_uv; v_color=a_color; }
"#,
            )?;
            let fragment = compile(
                &gl,
                glow::FRAGMENT_SHADER,
                r#"#version 100
precision mediump float;
varying vec2 v_uv;
varying vec4 v_color;
uniform sampler2D u_texture;
uniform int u_mode;
void main() { vec4 color=v_color; if(u_mode==1) color.a*=texture2D(u_texture,v_uv).a; else if(u_mode==2) color*=texture2D(u_texture,v_uv); gl_FragColor=color; }
"#,
            )?;
            gl.attach_shader(program, vertex);
            gl.attach_shader(program, fragment);
            gl.bind_attrib_location(program, 0, "a_position");
            gl.bind_attrib_location(program, 1, "a_uv");
            gl.bind_attrib_location(program, 2, "a_color");
            gl.link_program(program);
            let linked = gl.get_program_link_status(program);
            let log = gl.get_program_info_log(program);
            gl.delete_shader(vertex);
            gl.delete_shader(fragment);
            if !linked {
                gl.delete_program(program);
                return Err(format!("GLES2 shader link failed: {log}"));
            }
            gl.use_program(Some(program));
            gl.uniform_1_i32(gl.get_uniform_location(program, "u_texture").as_ref(), 0);
            let mode_uniform = gl.get_uniform_location(program, "u_mode");
            let buffer = gl.create_buffer()?;
            let atlas = gl.create_texture()?;
            gl.active_texture(glow::TEXTURE0);
            gl.bind_texture(glow::TEXTURE_2D, Some(atlas));
            texture_parameters(&gl);
            gl.pixel_store_i32(glow::UNPACK_ALIGNMENT, 1);
            gl.tex_image_2d(
                glow::TEXTURE_2D,
                0,
                glow::ALPHA as i32,
                ATLAS,
                ATLAS,
                0,
                glow::ALPHA,
                glow::UNSIGNED_BYTE,
                Some(&vec![0; (ATLAS * ATLAS) as usize]),
            );
            gl.enable(glow::BLEND);
            gl.blend_func(glow::SRC_ALPHA, glow::ONE_MINUS_SRC_ALPHA);
            gl.disable(glow::DEPTH_TEST);
            gl.disable(glow::CULL_FACE);
            (program, buffer, atlas, mode_uniform)
        };
        let mut renderer = Self {
            gl,
            program,
            buffer,
            atlas,
            mode_uniform,
            font,
            glyphs: HashMap::new(),
            atlas_x: 1,
            atlas_y: 1,
            shelf_height: 0,
            vertices: Vec::with_capacity(32768),
            mode: 0,
            texture: None,
            drawable: (1, 1),
            scale: 1.0,
        };
        renderer.resize(width, height);
        Ok(renderer)
    }

    pub fn resize(&mut self, width: u32, height: u32) {
        self.drawable = (width.max(1) as i32, height.max(1) as i32);
        self.scale = (width as f32 / WIDTH).min(height as f32 / HEIGHT).max(0.1);
    }

    pub fn begin(&mut self) {
        self.vertices.clear();
        self.mode = 0;
        self.texture = None;
        unsafe {
            self.gl.disable(glow::SCISSOR_TEST);
            self.gl.clear_color(0.025, 0.03, 0.04, 1.0);
            self.gl.clear(glow::COLOR_BUFFER_BIT);
            self.gl.viewport(
                (self.drawable.0 - (WIDTH * self.scale) as i32) / 2,
                (self.drawable.1 - (HEIGHT * self.scale) as i32) / 2,
                (WIDTH * self.scale) as i32,
                (HEIGHT * self.scale) as i32,
            );
        }
    }

    pub fn flush(&mut self) {
        if self.vertices.is_empty() {
            return;
        }
        unsafe {
            self.gl.use_program(Some(self.program));
            self.gl.active_texture(glow::TEXTURE0);
            self.gl.bind_texture(glow::TEXTURE_2D, self.texture);
            self.gl.uniform_1_i32(self.mode_uniform.as_ref(), self.mode);
            self.gl.bind_buffer(glow::ARRAY_BUFFER, Some(self.buffer));
            let bytes = std::slice::from_raw_parts(
                self.vertices.as_ptr().cast::<u8>(),
                self.vertices.len() * std::mem::size_of::<f32>(),
            );
            self.gl
                .buffer_data_u8_slice(glow::ARRAY_BUFFER, bytes, glow::STREAM_DRAW);
            for index in 0..3 {
                self.gl.enable_vertex_attrib_array(index);
            }
            self.gl
                .vertex_attrib_pointer_f32(0, 2, glow::FLOAT, false, 32, 0);
            self.gl
                .vertex_attrib_pointer_f32(1, 2, glow::FLOAT, false, 32, 8);
            self.gl
                .vertex_attrib_pointer_f32(2, 4, glow::FLOAT, false, 32, 16);
            self.gl
                .draw_arrays(glow::TRIANGLES, 0, (self.vertices.len() / 8) as i32);
        }
        self.vertices.clear();
    }

    fn select(&mut self, mode: i32, texture: Option<glow::NativeTexture>) {
        if mode != self.mode || texture != self.texture || self.vertices.len() >= 32768 {
            self.flush();
        }
        self.mode = mode;
        self.texture = texture;
    }
    fn vertex(&mut self, x: f32, y: f32, u: f32, v: f32, color: Color) {
        self.vertices
            .extend_from_slice(&[x, y, u, v, color[0], color[1], color[2], color[3]]);
    }
    fn quad(&mut self, rect: Rect, uv: [f32; 4], color: Color) {
        for (x, y, u, v) in [
            (rect.x, rect.y, uv[0], uv[1]),
            (rect.x + rect.w, rect.y, uv[2], uv[1]),
            (rect.x + rect.w, rect.y + rect.h, uv[2], uv[3]),
            (rect.x, rect.y, uv[0], uv[1]),
            (rect.x + rect.w, rect.y + rect.h, uv[2], uv[3]),
            (rect.x, rect.y + rect.h, uv[0], uv[3]),
        ] {
            self.vertex(x, y, u, v, color);
        }
    }
    pub fn rect(&mut self, rect: Rect, color: Color) {
        self.select(0, None);
        self.quad(rect, [0.0; 4], color);
    }
    pub fn line(&mut self, start: [f32; 2], end: [f32; 2], width: f32, color: Color) {
        let dx = end[0] - start[0];
        let dy = end[1] - start[1];
        let length = dx.hypot(dy);
        if length < 0.01 {
            return;
        }
        self.select(0, None);
        let nx = -dy / length * width / 2.0;
        let ny = dx / length * width / 2.0;
        for (x, y) in [
            (start[0] + nx, start[1] + ny),
            (end[0] + nx, end[1] + ny),
            (end[0] - nx, end[1] - ny),
            (start[0] + nx, start[1] + ny),
            (end[0] - nx, end[1] - ny),
            (start[0] - nx, start[1] - ny),
        ] {
            self.vertex(x, y, 0.0, 0.0, color);
        }
    }
    pub fn circle(&mut self, center: [f32; 2], radius: f32, width: f32, color: Color) {
        for index in 0..32 {
            let first = index as f32 * std::f32::consts::TAU / 32.0;
            let last = (index + 1) as f32 * std::f32::consts::TAU / 32.0;
            self.line(
                [
                    center[0] + first.cos() * radius,
                    center[1] + first.sin() * radius,
                ],
                [
                    center[0] + last.cos() * radius,
                    center[1] + last.sin() * radius,
                ],
                width,
                color,
            );
        }
    }
    pub fn gradient(&mut self, rect: Rect, first: Color, last: Color, horizontal: bool) {
        self.select(0, None);
        for (x, y) in [
            (0.0, 0.0),
            (1.0, 0.0),
            (1.0, 1.0),
            (0.0, 0.0),
            (1.0, 1.0),
            (0.0, 1.0),
        ] {
            let mix = if horizontal { x } else { y };
            let mut color = [0.0; 4];
            for index in 0..4 {
                color[index] = first[index] * (1.0 - mix) + last[index] * mix;
            }
            self.vertex(rect.x + x * rect.w, rect.y + y * rect.h, 0.0, 0.0, color);
        }
    }
    pub fn rounded(&mut self, rect: Rect, radius: f32, color: Color) {
        self.select(0, None);
        let radius = radius.min(rect.w / 2.0).min(rect.h / 2.0).max(0.0);
        let mut outline = Vec::with_capacity(24);
        for (cx, cy, start) in [
            (rect.x + rect.w - radius, rect.y + radius, -90.0f32),
            (rect.x + rect.w - radius, rect.y + rect.h - radius, 0.0),
            (rect.x + radius, rect.y + rect.h - radius, 90.0),
            (rect.x + radius, rect.y + radius, 180.0),
        ] {
            for step in 0..=5 {
                let angle = (start + step as f32 * 18.0).to_radians();
                outline.push((cx + angle.cos() * radius, cy + angle.sin() * radius));
            }
        }
        for i in 0..outline.len() {
            self.vertex(
                rect.x + rect.w / 2.0,
                rect.y + rect.h / 2.0,
                0.0,
                0.0,
                color,
            );
            self.vertex(outline[i].0, outline[i].1, 0.0, 0.0, color);
            let next = outline[(i + 1) % outline.len()];
            self.vertex(next.0, next.1, 0.0, 0.0, color);
        }
    }

    fn glyph(&mut self, character: char, size: u16) -> Option<Glyph> {
        if let Some(glyph) = self.glyphs.get(&(character, size)) {
            return Some(*glyph);
        }
        let scaled = self.font.as_scaled(size as f32);
        let id = scaled.glyph_id(character);
        let advance = scaled.h_advance(id);
        let outlined =
            scaled.outline_glyph(id.with_scale_and_position(size as f32, point(0.0, 0.0)));
        let mut glyph = Glyph {
            uv: [0.0; 4],
            offset: [0.0; 2],
            size: [0.0; 2],
            advance,
        };
        if let Some(outlined) = outlined {
            let bounds = outlined.px_bounds();
            let w = bounds.width() as i32;
            let h = bounds.height() as i32;
            if w + 2 >= ATLAS || h + 2 >= ATLAS {
                return None;
            }
            if self.atlas_x + w + 1 >= ATLAS {
                self.atlas_x = 1;
                self.atlas_y += self.shelf_height + 1;
                self.shelf_height = 0;
            }
            if self.atlas_y + h + 1 >= ATLAS {
                self.flush();
                self.glyphs.clear();
                self.atlas_x = 1;
                self.atlas_y = 1;
                self.shelf_height = 0;
                unsafe {
                    self.gl.bind_texture(glow::TEXTURE_2D, Some(self.atlas));
                    self.gl.tex_image_2d(
                        glow::TEXTURE_2D,
                        0,
                        glow::ALPHA as i32,
                        ATLAS,
                        ATLAS,
                        0,
                        glow::ALPHA,
                        glow::UNSIGNED_BYTE,
                        Some(&vec![0; (ATLAS * ATLAS) as usize]),
                    );
                }
            }
            let mut bytes = vec![0; (w * h) as usize];
            outlined.draw(|x, y, coverage| {
                bytes[(y * w as u32 + x) as usize] = (coverage * 255.0) as u8;
            });
            // Texture uploads change the binding used by pending batches.
            self.flush();
            unsafe {
                self.gl.bind_texture(glow::TEXTURE_2D, Some(self.atlas));
                self.gl.tex_sub_image_2d(
                    glow::TEXTURE_2D,
                    0,
                    self.atlas_x,
                    self.atlas_y,
                    w,
                    h,
                    glow::ALPHA,
                    glow::UNSIGNED_BYTE,
                    glow::PixelUnpackData::Slice(&bytes),
                );
            }
            glyph.uv = [
                self.atlas_x as f32 / ATLAS as f32,
                self.atlas_y as f32 / ATLAS as f32,
                (self.atlas_x + w) as f32 / ATLAS as f32,
                (self.atlas_y + h) as f32 / ATLAS as f32,
            ];
            glyph.offset = [bounds.min.x, bounds.min.y];
            glyph.size = [w as f32, h as f32];
            self.atlas_x += w + 1;
            self.shelf_height = self.shelf_height.max(h);
        }
        self.glyphs.insert((character, size), glyph);
        Some(glyph)
    }

    pub fn text(
        &mut self,
        text: &str,
        x: f32,
        top: f32,
        size: f32,
        color: Color,
        max_width: f32,
    ) -> f32 {
        let pixels = (size * self.scale).round().clamp(1.0, 200.0) as u16;
        let baseline = top + self.font.as_scaled(pixels as f32).ascent() / self.scale;
        let mut position = x;
        for character in text.chars().filter(|c| !c.is_control()) {
            let Some(glyph) = self.glyph(character, pixels) else {
                continue;
            };
            if position + glyph.advance / self.scale > x + max_width {
                break;
            }
            self.select(1, Some(self.atlas));
            if glyph.size[0] > 0.0 {
                self.quad(
                    Rect::new(
                        position + glyph.offset[0] / self.scale,
                        baseline + glyph.offset[1] / self.scale,
                        glyph.size[0] / self.scale,
                        glyph.size[1] / self.scale,
                    ),
                    glyph.uv,
                    color,
                );
            }
            position += glyph.advance / self.scale;
        }
        position - x
    }

    pub fn paragraph(&mut self, text: &str, rect: Rect, size: f32, color: Color) {
        let mut line = String::new();
        let mut y = rect.y;
        for word in text.split_whitespace() {
            let candidate = if line.is_empty() {
                word.to_owned()
            } else {
                format!("{line} {word}")
            };
            if self.measure(&candidate, size) > rect.w && !line.is_empty() {
                self.text(&line, rect.x, y, size, color, rect.w);
                y += size * 1.35;
                line.clear();
                if y + size > rect.y + rect.h {
                    return;
                }
            }
            if !line.is_empty() {
                line.push(' ');
            }
            line.push_str(word);
        }
        if !line.is_empty() && y + size <= rect.y + rect.h {
            self.text(&line, rect.x, y, size, color, rect.w);
        }
    }
    pub fn measure(&self, text: &str, size: f32) -> f32 {
        let scaled = self.font.as_scaled((size * self.scale).round().max(1.0));
        text.chars()
            .map(|c| scaled.h_advance(scaled.glyph_id(c)) / self.scale)
            .sum()
    }

    pub fn texture(&mut self, rect: Rect, texture: glow::NativeTexture) {
        self.select(2, Some(texture));
        self.quad(rect, [0.0, 0.0, 1.0, 1.0], WHITE);
    }
    pub fn upload(
        &mut self,
        width: u32,
        height: u32,
        rgba: &[u8],
    ) -> Result<glow::NativeTexture, String> {
        if width == 0
            || height == 0
            || width > 2048
            || height > 2048
            || width
                .checked_mul(height)
                .and_then(|size| size.checked_mul(4))
                .map(|size| size as usize)
                != Some(rgba.len())
        {
            return Err("Invalid artwork dimensions".to_owned());
        }
        self.flush();
        unsafe {
            let texture = self.gl.create_texture()?;
            self.gl.bind_texture(glow::TEXTURE_2D, Some(texture));
            texture_parameters(&self.gl);
            self.gl.tex_image_2d(
                glow::TEXTURE_2D,
                0,
                glow::RGBA as i32,
                width as i32,
                height as i32,
                0,
                glow::RGBA,
                glow::UNSIGNED_BYTE,
                Some(rgba),
            );
            Ok(texture)
        }
    }
    pub fn delete_texture(&mut self, texture: glow::NativeTexture) {
        self.flush();
        unsafe {
            self.gl.delete_texture(texture);
        }
    }

    pub fn screenshot(&mut self, path: &Path) -> Result<(), String> {
        self.flush();
        let (w, h) = self.drawable;
        let mut bytes = vec![0; (w * h * 4) as usize];
        unsafe {
            self.gl.pixel_store_i32(glow::PACK_ALIGNMENT, 1);
            self.gl.read_pixels(
                0,
                0,
                w,
                h,
                glow::RGBA,
                glow::UNSIGNED_BYTE,
                glow::PixelPackData::Slice(&mut bytes),
            );
        }
        let mut image = image::RgbaImage::from_raw(w as u32, h as u32, bytes)
            .ok_or("Screenshot allocation failed")?;
        image::imageops::flip_vertical_in_place(&mut image);
        image
            .save(path)
            .map_err(|_| "Screenshot could not be saved".to_owned())
    }
}

impl Drop for Renderer {
    fn drop(&mut self) {
        unsafe {
            self.gl.delete_texture(self.atlas);
            self.gl.delete_buffer(self.buffer);
            self.gl.delete_program(self.program);
        }
    }
}

unsafe fn compile(
    gl: &glow::Context,
    kind: u32,
    source: &str,
) -> Result<glow::NativeShader, String> {
    let shader = gl.create_shader(kind)?;
    gl.shader_source(shader, source);
    gl.compile_shader(shader);
    if !gl.get_shader_compile_status(shader) {
        let log = gl.get_shader_info_log(shader);
        gl.delete_shader(shader);
        return Err(format!("GLES2 shader compilation failed: {log}"));
    }
    Ok(shader)
}
unsafe fn texture_parameters(gl: &glow::Context) {
    gl.tex_parameter_i32(
        glow::TEXTURE_2D,
        glow::TEXTURE_MIN_FILTER,
        glow::LINEAR as i32,
    );
    gl.tex_parameter_i32(
        glow::TEXTURE_2D,
        glow::TEXTURE_MAG_FILTER,
        glow::LINEAR as i32,
    );
    gl.tex_parameter_i32(
        glow::TEXTURE_2D,
        glow::TEXTURE_WRAP_S,
        glow::CLAMP_TO_EDGE as i32,
    );
    gl.tex_parameter_i32(
        glow::TEXTURE_2D,
        glow::TEXTURE_WRAP_T,
        glow::CLAMP_TO_EDGE as i32,
    );
}
