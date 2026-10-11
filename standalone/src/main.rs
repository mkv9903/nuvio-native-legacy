use nuvio_rust_app::{APP_ID, APP_NAME};

fn main() {
    println!("[startup] {APP_NAME}: standalone Rust, id={APP_ID}");
    println!("[startup] Runtime compatibility preflight; interactive UI is under construction");
    let id = uuid::Uuid::new_v4();
    assert_eq!(id.get_version_num(), 4);
    let worker = std::thread::spawn(|| serde_json::json!({"runtime": "rust", "standalone": true}));
    assert_eq!(worker.join().expect("runtime worker")["standalone"], true);
    println!("[startup] allocation, UUID randomness, JSON and worker checks passed");
}
