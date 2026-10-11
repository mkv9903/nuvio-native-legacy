#![allow(dead_code)]
use nuvio_rust_app::{
    error::Result,
    http::{HttpRequest, HttpResponse, Transport},
    storage::DataStore,
};
use serde_json::{json, Value};
use std::{
    collections::HashMap,
    io::{Read, Write},
    net::{TcpListener, TcpStream},
    path::PathBuf,
    sync::{mpsc, Arc},
    thread,
    time::Duration,
};
use zeroize::Zeroizing;

pub struct FixtureDirectory(PathBuf);
impl FixtureDirectory {
    pub fn new() -> Self {
        let root =
            std::env::temp_dir().join(format!("nuvio-rust-fixture-{}", uuid::Uuid::new_v4()));
        std::fs::create_dir_all(&root).unwrap();
        Self(root)
    }
    pub fn store(&self) -> DataStore {
        DataStore::new(self.0.clone()).unwrap()
    }
    pub fn path(&self, name: &str) -> PathBuf {
        self.0.join(name)
    }
}
impl Drop for FixtureDirectory {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.0);
    }
}

pub struct FakeTransport(pub Arc<dyn Fn(HttpRequest) -> Result<HttpResponse> + Send + Sync>);
impl Transport for FakeTransport {
    fn send(&self, request: HttpRequest) -> Result<HttpResponse> {
        self.0(request)
    }
}
pub fn fake(
    handler: impl Fn(HttpRequest) -> Result<HttpResponse> + Send + Sync + 'static,
) -> Arc<dyn Transport> {
    Arc::new(FakeTransport(Arc::new(handler)))
}
pub fn response(status: u16, body: Value) -> HttpResponse {
    HttpResponse {
        status,
        body: Zeroizing::new(serde_json::to_vec(&body).unwrap()),
    }
}
pub fn tokens(access: &str, refresh: &str) -> Value {
    json!({"access_token":access,"refresh_token":refresh,"expires_in":3600,"user":{"id":"fixture-account"}})
}

pub struct ReceivedRequest {
    pub target: String,
    pub headers: HashMap<String, String>,
    pub body: Vec<u8>,
}
pub struct Reply {
    pub status: u16,
    pub headers: Vec<(String, String)>,
    pub body: Vec<u8>,
    pub delay: Duration,
}
impl Reply {
    pub fn new(status: u16, body: &str) -> Self {
        Self {
            status,
            headers: Vec::new(),
            body: body.as_bytes().to_vec(),
            delay: Duration::ZERO,
        }
    }
    pub fn redirect(location: impl Into<String>) -> Self {
        let mut reply = Self::new(302, "");
        reply.headers.push(("Location".to_owned(), location.into()));
        reply
    }
}

pub struct Server {
    pub base: String,
    received: mpsc::Receiver<ReceivedRequest>,
    worker: Option<thread::JoinHandle<()>>,
}
impl Server {
    pub fn new(replies: Vec<Reply>) -> Self {
        let listener = TcpListener::bind("127.0.0.1:0").expect("local fixture listener");
        listener.set_nonblocking(true).unwrap();
        let base = format!("http://{}", listener.local_addr().unwrap());
        let (send, received) = mpsc::channel();
        let worker = thread::spawn(move || {
            for reply in replies {
                let deadline = std::time::Instant::now() + Duration::from_secs(3);
                let mut stream = loop {
                    match listener.accept() {
                        Ok((stream, _)) => break stream,
                        Err(error) if error.kind() == std::io::ErrorKind::WouldBlock => {
                            if std::time::Instant::now() > deadline {
                                return;
                            }
                            thread::sleep(Duration::from_millis(2));
                        }
                        Err(_) => return,
                    }
                };
                stream
                    .set_read_timeout(Some(Duration::from_secs(2)))
                    .unwrap();
                let request = receive(&mut stream);
                if send.send(request).is_err() {
                    return;
                }
                thread::sleep(reply.delay);
                let mut headers = format!(
                    "HTTP/1.1 {} Fixture\r\nConnection: close\r\nContent-Length: {}\r\n",
                    reply.status,
                    reply.body.len()
                );
                for (name, value) in reply.headers {
                    headers.push_str(&format!("{name}: {value}\r\n"));
                }
                headers.push_str("\r\n");
                let _ = stream
                    .write_all(headers.as_bytes())
                    .and_then(|_| stream.write_all(&reply.body));
            }
        });
        Self {
            base,
            received,
            worker: Some(worker),
        }
    }
    pub fn request(&self) -> ReceivedRequest {
        self.received.recv_timeout(Duration::from_secs(3)).unwrap()
    }
}
impl Drop for Server {
    fn drop(&mut self) {
        if let Some(worker) = self.worker.take() {
            worker.join().unwrap();
        }
    }
}

fn receive(stream: &mut TcpStream) -> ReceivedRequest {
    let mut bytes = Vec::new();
    while !bytes.ends_with(b"\r\n\r\n") {
        let mut byte = [0];
        stream.read_exact(&mut byte).unwrap();
        bytes.push(byte[0]);
        assert!(bytes.len() < 16_384);
    }
    let headers = String::from_utf8(bytes).unwrap();
    let mut lines = headers.split("\r\n");
    let target = lines
        .next()
        .unwrap()
        .split_whitespace()
        .nth(1)
        .unwrap()
        .to_owned();
    let headers: HashMap<_, _> = lines
        .filter_map(|line| line.split_once(':'))
        .map(|(key, value)| (key.to_ascii_lowercase(), value.trim().to_owned()))
        .collect();
    let length = headers
        .get("content-length")
        .and_then(|text| text.parse::<usize>().ok())
        .unwrap_or(0);
    let mut body = vec![0; length];
    stream.read_exact(&mut body).unwrap();
    ReceivedRequest {
        target,
        headers,
        body,
    }
}
