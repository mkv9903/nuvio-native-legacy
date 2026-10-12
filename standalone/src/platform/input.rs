//! Normalize the TV's SDL extensions before Rust SDL2 converts enum values.
use sdl2::sys::{SDL_Event, SDL_EventType, SDL_KeyCode, SDL_Scancode};
use std::os::raw::{c_int, c_void};

pub fn install() {
    unsafe {
        sdl2::sys::SDL_SetEventFilter(Some(normalize), std::ptr::null_mut());
    }
}

// SDL may invoke this on another thread. It accesses only the supplied event;
// no application state, callbacks, allocation or locks.
unsafe extern "C" fn normalize(_: *mut c_void, event: *mut SDL_Event) -> c_int {
    if event.is_null() {
        return 1;
    }
    let kind = (*event).type_;
    if kind == SDL_EventType::SDL_KEYDOWN as u32 || kind == SDL_EventType::SDL_KEYUP as u32 {
        // The LG scancode is absent from the generated C enum. Read its storage
        // as an integer, without materializing an invalid Rust enum value.
        let code = std::ptr::addr_of!((*event).key.keysym.scancode)
            .cast::<i32>()
            .read_unaligned();
        if code == 482 {
            (*event).key.keysym.scancode = SDL_Scancode::SDL_SCANCODE_ESCAPE;
            (*event).key.keysym.sym = SDL_KeyCode::SDLK_ESCAPE as i32;
        } else if sdl2::keyboard::Scancode::from_i32(code).is_none() {
            (*event).key.keysym.scancode = SDL_Scancode::SDL_SCANCODE_UNKNOWN;
        }
    }
    1
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn tv_back_extension_becomes_a_supported_sdl_key() {
        unsafe {
            let mut event: SDL_Event = std::mem::zeroed();
            event.type_ = SDL_EventType::SDL_KEYDOWN as u32;
            std::ptr::addr_of_mut!(event.key.keysym.scancode)
                .cast::<i32>()
                .write_unaligned(482);
            assert_eq!(normalize(std::ptr::null_mut(), &mut event), 1);
            assert_eq!(event.key.keysym.sym, SDL_KeyCode::SDLK_ESCAPE as i32);
            assert_eq!(event.key.keysym.scancode, SDL_Scancode::SDL_SCANCODE_ESCAPE);
        }
    }
    #[test]
    fn unknown_scancodes_do_not_reach_rust_enum_conversion() {
        unsafe {
            let mut event: SDL_Event = std::mem::zeroed();
            event.type_ = SDL_EventType::SDL_KEYUP as u32;
            std::ptr::addr_of_mut!(event.key.keysym.scancode)
                .cast::<i32>()
                .write_unaligned(489);
            normalize(std::ptr::null_mut(), &mut event);
            assert_eq!(
                event.key.keysym.scancode,
                SDL_Scancode::SDL_SCANCODE_UNKNOWN
            );
        }
    }
}
