use super::{
    app::{App, EditPurpose, Page, KEYS},
    artwork::Artwork,
    render::{Color, Rect, Renderer, ACCENT, MUTED, PANEL, WHITE},
};
use crate::catalog::Title;

pub fn draw(app: &App, renderer: &mut Renderer, artwork: &mut Artwork) {
    renderer.begin();
    artwork.begin(renderer);
    match app.page {
        Page::Login => login(app, renderer),
        Page::Profiles => profiles(app, renderer, artwork),
        Page::Home => home(app, renderer, artwork),
        Page::Detail => detail(app, renderer, artwork),
        Page::Sources => sources(app, renderer),
        Page::Search => search(app, renderer, artwork),
    }
    if !app.banner.is_empty() {
        renderer.rounded(
            Rect::new(150.0, 985.0, 1620.0, 58.0),
            14.0,
            [0.17, 0.13, 0.07, 0.96],
        );
        renderer.text(
            &app.banner,
            174.0,
            1001.0,
            22.0,
            [1.0, 0.85, 0.52, 1.0],
            1570.0,
        );
    }
    if app.editor.is_some() {
        keyboard(app, renderer);
    }
    renderer.flush();
}

fn button(renderer: &mut Renderer, label: &str, rect: Rect, focused: bool) {
    renderer.rounded(
        rect,
        rect.h / 2.0,
        if focused {
            ACCENT
        } else {
            [0.14, 0.15, 0.17, 1.0]
        },
    );
    let width = renderer.measure(label, 25.0);
    renderer.text(
        label,
        rect.x + (rect.w - width) / 2.0,
        rect.y + (rect.h - 28.0) / 2.0,
        25.0,
        if focused {
            [0.025, 0.03, 0.035, 1.0]
        } else {
            WHITE
        },
        rect.w - 30.0,
    );
}
fn logo(renderer: &mut Renderer, x: f32, y: f32) {
    renderer.text("nuvio", x, y, 52.0, WHITE, 260.0);
}
fn login(app: &App, renderer: &mut Renderer) {
    renderer.rect(
        Rect::new(0.0, 0.0, 810.0, 1080.0),
        [0.07, 0.075, 0.095, 1.0],
    );
    logo(renderer, 155.0, 130.0);
    renderer.text(
        "Your world of entertainment.",
        155.0,
        360.0,
        54.0,
        WHITE,
        610.0,
    );
    renderer.paragraph(
        "Sign in to bring your profiles, catalogs and watch progress to this TV.",
        Rect::new(155.0, 515.0, 540.0, 160.0),
        29.0,
        MUTED,
    );
    renderer.text("Welcome back", 995.0, 175.0, 50.0, WHITE, 730.0);
    renderer.text(
        "Sign in to your Nuvio account",
        995.0,
        253.0,
        27.0,
        MUTED,
        730.0,
    );
    let masked = "•".repeat(app.password.expose().chars().count().min(32));
    for (index, label, value, y) in [
        (0, "Email address", app.email.as_str(), 360.0),
        (1, "Password", masked.as_str(), 492.0),
    ] {
        renderer.text(label, 995.0, y - 38.0, 23.0, MUTED, 720.0);
        renderer.rounded(
            Rect::new(991.0, y - 4.0, 724.0, 76.0),
            12.0,
            if app.login_focus == index {
                ACCENT
            } else {
                PANEL
            },
        );
        renderer.rounded(Rect::new(995.0, y, 716.0, 68.0), 10.0, PANEL);
        renderer.text(
            if value.is_empty() {
                "Press OK to enter"
            } else {
                value
            },
            1017.0,
            y + 18.0,
            27.0,
            if value.is_empty() { MUTED } else { WHITE },
            668.0,
        );
    }
    button(
        renderer,
        if app.busy { "Signing in…" } else { "Sign in" },
        Rect::new(995.0, 620.0, 716.0, 68.0),
        app.login_focus == 2,
    );
    button(
        renderer,
        "Link with a TV code",
        Rect::new(995.0, 720.0, 716.0, 68.0),
        app.login_focus == 3,
    );
    renderer.text(
        "Use arrows to move and OK to select",
        995.0,
        848.0,
        23.0,
        MUTED,
        730.0,
    );
    if let Some(login) = &app.login_code {
        renderer.rect(Rect::new(0.0, 0.0, 1920.0, 1080.0), [0.0, 0.0, 0.0, 0.83]);
        renderer.rounded(Rect::new(440.0, 170.0, 1040.0, 720.0), 28.0, PANEL);
        renderer.text("Link your Nuvio account", 510.0, 227.0, 43.0, WHITE, 900.0);
        renderer.paragraph(
            "Scan this code with your phone, or open the address below and enter the TV code.",
            Rect::new(945.0, 355.0, 450.0, 180.0),
            27.0,
            MUTED,
        );
        if let Some(qr) = &login.qr {
            let cell = 350.0 / (qr.width() + 8) as f32;
            renderer.rect(Rect::new(515.0, 350.0, 350.0, 350.0), [1.0; 4]);
            for y in 0..qr.width() {
                for x in 0..qr.width() {
                    if qr[(x, y)] == qrcode::Color::Dark {
                        renderer.rect(
                            Rect::new(
                                515.0 + (x + 4) as f32 * cell,
                                350.0 + (y + 4) as f32 * cell,
                                cell,
                                cell,
                            ),
                            [0.0, 0.0, 0.0, 1.0],
                        );
                    }
                }
            }
        }
        renderer.text(&login.code, 945.0, 590.0, 66.0, ACCENT, 450.0);
        renderer.text(&login.web_url, 510.0, 755.0, 21.0, WHITE, 900.0);
        renderer.text(
            "Waiting for approval…    Back to cancel",
            510.0,
            815.0,
            24.0,
            MUTED,
            900.0,
        );
    }
}
fn profiles(app: &App, renderer: &mut Renderer, artwork: &mut Artwork) {
    logo(renderer, 130.0, 75.0);
    renderer.text("Who’s watching?", 660.0, 275.0, 64.0, WHITE, 900.0);
    let Some(account) = &app.account else {
        renderer.text("Loading your profiles…", 660.0, 430.0, 32.0, MUTED, 900.0);
        return;
    };
    let start = (app.profile_focus / 6) * 6;
    let shown = account.profiles.len().saturating_sub(start).min(6);
    let left = (1920.0 - shown as f32 * 240.0) / 2.0;
    for (offset, profile) in account.profiles.iter().skip(start).take(6).enumerate() {
        let x = left + offset as f32 * 240.0;
        let focused = app.profile_focus == start + offset;
        renderer.rounded(
            Rect::new(x - 5.0, 465.0, 200.0, 200.0),
            22.0,
            if focused { WHITE } else { PANEL },
        );
        renderer.rounded(
            Rect::new(x, 470.0, 190.0, 190.0),
            18.0,
            profile_color(&profile.color_hex),
        );
        if !artwork.draw(
            renderer,
            Some(&profile.avatar_url),
            Rect::new(x + 8.0, 478.0, 174.0, 174.0),
            false,
        ) {
            let first = profile.name.chars().next().unwrap_or('P').to_string();
            renderer.text(&first, x + 57.0, 515.0, 96.0, WHITE, 150.0);
        }
        renderer.text(
            &profile.name,
            x,
            710.0,
            29.0,
            if focused { WHITE } else { MUTED },
            220.0,
        );
        if profile.has_pin {
            renderer.text("PIN required", x, 758.0, 20.0, MUTED, 210.0);
        }
    }
    renderer.text(
        "Choose a profile with the arrows, then press OK",
        570.0,
        875.0,
        26.0,
        MUTED,
        1000.0,
    );
}
fn rail(app: &App, renderer: &mut Renderer) {
    renderer.rect(
        Rect::new(0.0, 0.0, 106.0, 1080.0),
        [0.025, 0.03, 0.04, 0.97],
    );
    for index in 0..3 {
        let y = 395.0 + index as f32 * 94.0;
        let focused = app.rail_focus == Some(index);
        renderer.rounded(
            Rect::new(20.0, y, 66.0, 66.0),
            18.0,
            if focused { ACCENT } else { PANEL },
        );
        let color = if focused {
            [0.03, 0.03, 0.03, 1.0]
        } else {
            WHITE
        };
        match index {
            0 => {
                for (first, last) in [
                    ([35.0, y + 31.0], [53.0, y + 16.0]),
                    ([53.0, y + 16.0], [71.0, y + 31.0]),
                    ([39.0, y + 30.0], [39.0, y + 49.0]),
                    ([39.0, y + 49.0], [67.0, y + 49.0]),
                    ([67.0, y + 49.0], [67.0, y + 30.0]),
                ] {
                    renderer.line(first, last, 3.0, color);
                }
            }
            1 => {
                renderer.circle([49.0, y + 29.0], 12.0, 3.0, color);
                renderer.line([59.0, y + 39.0], [70.0, y + 50.0], 3.0, color);
            }
            _ => {
                renderer.circle([53.0, y + 25.0], 7.0, 3.0, color);
                for (first, last) in [
                    ([38.0, y + 50.0], [38.0, y + 44.0]),
                    ([38.0, y + 44.0], [43.0, y + 38.0]),
                    ([43.0, y + 38.0], [63.0, y + 38.0]),
                    ([63.0, y + 38.0], [68.0, y + 44.0]),
                    ([68.0, y + 44.0], [68.0, y + 50.0]),
                ] {
                    renderer.line(first, last, 3.0, color);
                }
            }
        }
    }
}
fn backdrop(title: Option<&Title>, renderer: &mut Renderer, artwork: &mut Artwork) {
    if let Some(title) = title {
        artwork.draw(
            renderer,
            title.background.as_deref(),
            Rect::new(105.0, 0.0, 1815.0, 650.0),
            true,
        );
    }
    renderer.gradient(
        Rect::new(105.0, 0.0, 1815.0, 650.0),
        [0.025, 0.03, 0.04, 0.98],
        [0.025, 0.03, 0.04, 0.12],
        true,
    );
    renderer.gradient(
        Rect::new(105.0, 430.0, 1815.0, 220.0),
        [0.025, 0.03, 0.04, 0.0],
        [0.025, 0.03, 0.04, 1.0],
        false,
    );
}
fn hero(title: &Title, renderer: &mut Renderer, y: f32) {
    renderer.text(
        if title.kind == "series" {
            "TV SERIES"
        } else {
            "MOVIE"
        },
        158.0,
        y,
        21.0,
        ACCENT,
        850.0,
    );
    renderer.text(&title.name, 154.0, y + 43.0, 66.0, WHITE, 1100.0);
    let facts = [
        title.release_info.clone(),
        title.runtime.clone(),
        title
            .genres
            .iter()
            .take(3)
            .cloned()
            .collect::<Vec<_>>()
            .join(" · "),
    ]
    .into_iter()
    .filter(|text| !text.is_empty())
    .collect::<Vec<_>>()
    .join("  ·  ");
    renderer.text(&facts, 158.0, y + 140.0, 23.0, MUTED, 1050.0);
    if !title.imdb_rating.is_empty() {
        renderer.rounded(
            Rect::new(158.0, y + 184.0, 140.0, 36.0),
            9.0,
            [0.15, 0.13, 0.065, 1.0],
        );
        renderer.text(
            &format!("IMDb  {}", title.imdb_rating),
            169.0,
            y + 190.0,
            21.0,
            ACCENT,
            130.0,
        );
    }
    renderer.paragraph(
        &title.description,
        Rect::new(158.0, y + 243.0, 870.0, 120.0),
        24.0,
        [0.74, 0.75, 0.78, 1.0],
    );
}
fn poster(
    renderer: &mut Renderer,
    artwork: &mut Artwork,
    title: &Title,
    x: f32,
    y: f32,
    focused: bool,
) {
    renderer.rounded(
        Rect::new(x - 4.0, y - 4.0, 208.0, 302.0),
        14.0,
        if focused { WHITE } else { PANEL },
    );
    renderer.rounded(Rect::new(x, y, 200.0, 294.0), 10.0, [0.12, 0.14, 0.19, 1.0]);
    if !artwork.draw(
        renderer,
        title.poster.as_deref(),
        Rect::new(x + 4.0, y + 4.0, 192.0, 286.0),
        false,
    ) {
        renderer.paragraph(
            &title.name,
            Rect::new(x + 17.0, y + 106.0, 167.0, 110.0),
            25.0,
            WHITE,
        );
    }
    renderer.text(
        &title.name,
        x,
        y + 313.0,
        23.0,
        if focused { WHITE } else { MUTED },
        210.0,
    );
}
fn home(app: &App, renderer: &mut Renderer, artwork: &mut Artwork) {
    backdrop(app.focused_title(), renderer, artwork);
    rail(app, renderer);
    logo(renderer, 154.0, 50.0);
    if let Some(profile) = app.active_profile.and_then(|index| {
        app.account
            .as_ref()
            .and_then(|account| account.profiles.get(index))
    }) {
        renderer.text(&profile.name, 1540.0, 68.0, 26.0, WHITE, 310.0);
    }
    if let Some(title) = app.focused_title() {
        hero(title, renderer, 162.0);
    } else {
        renderer.text(
            if app.catalogs_loading() {
                "Loading your catalogs…"
            } else {
                "Your Home"
            },
            158.0,
            210.0,
            58.0,
            WHITE,
            1300.0,
        );
        renderer.paragraph(if app.catalogs_loading(){"Your profile’s addons and catalog rows are being loaded."}else{"Catalog addons from your account appear here. Choose another profile or check the addons configured in Nuvio."},Rect::new(158.0,325.0,900.0,150.0),27.0,MUTED);
    }
    for (offset, row) in app.home.rows.iter().skip(app.row_focus).take(2).enumerate() {
        let y = 630.0 + offset as f32 * 407.0;
        renderer.text(&row.name, 158.0, y - 51.0, 30.0, WHITE, 1570.0);
        let first = if offset == 0 {
            app.card_focus.saturating_sub(5)
        } else {
            0
        };
        if row.titles.is_empty() {
            renderer.text(
                if row.loading {
                    "Loading…"
                } else if row.error.is_some() {
                    "This catalog could not be loaded"
                } else {
                    "No titles in this catalog"
                },
                158.0,
                y + 50.0,
                26.0,
                MUTED,
                1200.0,
            );
        }
        for (position, title) in row.titles.iter().skip(first).take(7).enumerate() {
            poster(
                renderer,
                artwork,
                title,
                158.0 + position as f32 * 233.0,
                y,
                offset == 0 && position + first == app.card_focus && app.rail_focus.is_none(),
            );
        }
    }
}
fn detail(app: &App, renderer: &mut Renderer, artwork: &mut Artwork) {
    backdrop(app.detail.as_ref(), renderer, artwork);
    renderer.text("‹  Back", 100.0, 60.0, 28.0, MUTED, 450.0);
    let Some(title) = &app.detail else {
        return;
    };
    hero(title, renderer, 155.0);
    if app.detail_loading {
        renderer.text("Loading title details…", 158.0, 580.0, 26.0, MUTED, 1500.0);
    } else if title.kind == "series" {
        renderer.text("Episodes", 158.0, 585.0, 33.0, WHITE, 1500.0);
        for (offset, episode) in app
            .episodes
            .iter()
            .skip(app.detail_focus.saturating_sub(2))
            .take(5)
            .enumerate()
        {
            let index = app.detail_focus.saturating_sub(2) + offset;
            let y = 650.0 + offset as f32 * 67.0;
            renderer.rounded(
                Rect::new(158.0, y, 1590.0, 57.0),
                12.0,
                if index == app.detail_focus {
                    [0.20, 0.18, 0.11, 1.0]
                } else {
                    PANEL
                },
            );
            renderer.text(
                &format!(
                    "S{}  E{}",
                    episode.season.unwrap_or(0),
                    episode.episode.unwrap_or(0)
                ),
                178.0,
                y + 16.0,
                22.0,
                ACCENT,
                170.0,
            );
            renderer.text(&episode.name, 380.0, y + 15.0, 24.0, WHITE, 1210.0);
        }
    } else {
        button(
            renderer,
            "Choose a source",
            Rect::new(158.0, 600.0, 330.0, 68.0),
            true,
        );
    }
}
fn sources(app: &App, renderer: &mut Renderer) {
    renderer.text("‹  Back", 100.0, 60.0, 28.0, MUTED, 450.0);
    renderer.text("Choose a source", 150.0, 150.0, 56.0, WHITE, 1500.0);
    if let Some(title) = &app.detail {
        renderer.text(&title.name, 153.0, 235.0, 28.0, MUTED, 1580.0);
    }
    let start = app.source_focus.saturating_sub(3);
    for (offset, choice) in app.sources.iter().skip(start).take(6).enumerate() {
        let focused = offset + start == app.source_focus;
        let y = 325.0 + offset as f32 * 98.0;
        renderer.rounded(
            Rect::new(150.0, y, 1610.0, 86.0),
            14.0,
            if focused {
                [0.20, 0.18, 0.11, 1.0]
            } else {
                PANEL
            },
        );
        renderer.text(
            &format!("{}  ·  {}", choice.addon_name, choice.source.name),
            172.0,
            y + 13.0,
            26.0,
            if focused { ACCENT } else { WHITE },
            1550.0,
        );
        let description = if choice.source.description.is_empty() {
            &choice.source.title
        } else {
            &choice.source.description
        };
        renderer.text(description, 172.0, y + 49.0, 22.0, MUTED, 1550.0);
    }
    if app.sources.is_empty() {
        renderer.text(
            if app.sources_loading > 0 {
                "Loading sources…"
            } else {
                "No sources were returned for this title"
            },
            155.0,
            400.0,
            30.0,
            MUTED,
            1510.0,
        );
    }
    renderer.text(
        "Select a source and press OK. Playback starts only after your selection.",
        155.0,
        945.0,
        24.0,
        MUTED,
        1540.0,
    );
}
fn search(app: &App, renderer: &mut Renderer, artwork: &mut Artwork) {
    renderer.text("Search", 150.0, 75.0, 54.0, WHITE, 1500.0);
    renderer.text(&app.search_term, 152.0, 157.0, 30.0, ACCENT, 1500.0);
    if app.search_titles.is_empty() {
        renderer.text(
            if app.search_loading > 0 {
                "Searching your catalogs…"
            } else {
                "No matching titles"
            },
            152.0,
            310.0,
            30.0,
            MUTED,
            1500.0,
        );
    }
    let first = (app.search_focus / 7) * 7;
    for (position, title) in app.search_titles.iter().skip(first).take(14).enumerate() {
        poster(
            renderer,
            artwork,
            title,
            154.0 + (position % 7) as f32 * 234.0,
            262.0 + (position / 7) as f32 * 364.0,
            first + position == app.search_focus,
        );
    }
}
fn keyboard(app: &App, renderer: &mut Renderer) {
    let Some(editor) = &app.editor else {
        return;
    };
    renderer.rect(Rect::new(0.0, 0.0, 1920.0, 1080.0), [0.0, 0.0, 0.0, 0.83]);
    renderer.rounded(Rect::new(320.0, 170.0, 1280.0, 750.0), 24.0, PANEL);
    let label = match editor.purpose {
        EditPurpose::Email => "Email address",
        EditPurpose::Password => "Password",
        EditPurpose::Search => "Search",
        EditPurpose::Pin => "Profile PIN",
    };
    renderer.text(label, 370.0, 217.0, 42.0, WHITE, 1150.0);
    renderer.rounded(
        Rect::new(370.0, 300.0, 1180.0, 75.0),
        12.0,
        [0.14, 0.15, 0.18, 1.0],
    );
    let value = if matches!(editor.purpose, EditPurpose::Password | EditPurpose::Pin) {
        "•".repeat(editor.value.chars().count().min(48))
    } else {
        editor.value.to_string()
    };
    renderer.text(&value, 392.0, 322.0, 29.0, WHITE, 1130.0);
    for (index, key) in KEYS.iter().enumerate() {
        let x = 370.0 + (index % 12) as f32 * 99.0;
        let y = 435.0 + (index / 12) as f32 * 75.0;
        let focused = index == editor.focus;
        renderer.rounded(
            Rect::new(x, y, 88.0, 62.0),
            10.0,
            if focused {
                ACCENT
            } else {
                [0.14, 0.15, 0.18, 1.0]
            },
        );
        let text = if *key == " " {
            "Space".to_owned()
        } else if editor.shift {
            key.to_uppercase()
        } else {
            key.to_string()
        };
        let size = if text.len() > 2 { 19.0 } else { 28.0 };
        let width = renderer.measure(&text, size);
        renderer.text(
            &text,
            x + (88.0 - width) / 2.0,
            y + 17.0,
            size,
            if focused {
                [0.035, 0.035, 0.035, 1.0]
            } else {
                WHITE
            },
            86.0,
        );
    }
    renderer.text(
        "OK to select    Done to confirm    Back to cancel",
        370.0,
        840.0,
        23.0,
        MUTED,
        1150.0,
    );
}
fn profile_color(value: &str) -> Color {
    let Some(value) = value
        .strip_prefix('#')
        .filter(|value| value.len() == 6 && value.bytes().all(|byte| byte.is_ascii_hexdigit()))
    else {
        return [0.12, 0.28, 0.50, 1.0];
    };
    [
        u8::from_str_radix(&value[0..2], 16).unwrap_or(30) as f32 / 255.0,
        u8::from_str_radix(&value[2..4], 16).unwrap_or(60) as f32 / 255.0,
        u8::from_str_radix(&value[4..6], 16).unwrap_or(110) as f32 / 255.0,
        1.0,
    ]
}
