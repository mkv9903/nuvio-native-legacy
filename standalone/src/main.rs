fn main() {
    if let Err(error) = nuvio_rust_app::ui::runtime::run() {
        eprintln!("[startup] {error}");
        std::process::exit(1);
    }
}
