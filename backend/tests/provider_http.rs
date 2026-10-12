use nuvio_backend::{
    addons::{AddonUrl, ADDON_RESPONSE_LIMIT},
    error::Error,
    provider_http::{ProviderHttp, ProviderTransport},
};
use std::{
    io::{Read, Write},
    net::TcpListener,
    sync::mpsc,
    thread,
    time::Duration,
};
use url::Url;

fn request(stream: &mut std::net::TcpStream) -> String {
    stream
        .set_read_timeout(Some(Duration::from_secs(5)))
        .unwrap();
    let mut bytes = Vec::new();
    let mut buffer = [0u8; 4096];
    while !bytes.windows(4).any(|part| part == b"\r\n\r\n") {
        let count = stream.read(&mut buffer).unwrap();
        assert!(count > 0);
        bytes.extend_from_slice(&buffer[..count]);
        assert!(bytes.len() < 128 * 1024);
    }
    String::from_utf8(bytes).unwrap()
}

#[test]
fn external_resource_requests_preserve_full_provider_configuration_and_encode_extras() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap();
    let (sent, received) = mpsc::channel();
    let server = thread::spawn(move || {
        let (mut stream, _) = listener.accept().unwrap();
        sent.send(request(&mut stream)).unwrap();
        stream
            .write_all(
                b"HTTP/1.1 200 OK\r\nContent-Length: 12\r\nConnection: close\r\n\r\n{\"metas\":[]}",
            )
            .unwrap();
    });
    let base = AddonUrl::parse(&format!(
        "http://{address}/{}?provider-key=keep%26all",
        "config".repeat(1000)
    ))
    .unwrap();
    let url = base
        .resource_url_with_extras(
            "catalog",
            "series",
            "kitsu:41370/日",
            &[("search", "A & B/日?=+"), ("skip", "100")],
        )
        .unwrap();
    assert!(url.as_str().contains(
        "/search=A%20%26%20B%2F%E6%97%A5%3F%3D%2B&skip=100.json?provider-key=keep%26all"
    ));
    let response = ProviderHttp::default()
        .get(&url, Duration::from_secs(5))
        .unwrap();
    response.success().unwrap();
    assert_eq!(response.body.as_slice(), br#"{"metas":[]}"#);
    let headers = received.recv_timeout(Duration::from_secs(5)).unwrap();
    assert!(headers.starts_with(&format!(
        "GET {}?{} HTTP/1.1\r\n",
        url.path(),
        url.query().unwrap()
    )));
    for forbidden in ["authorization:", "apikey:", "cookie:"] {
        assert!(!headers.to_ascii_lowercase().contains(forbidden));
    }
    server.join().unwrap();
}

#[test]
fn addon_redirects_work_without_account_headers_on_either_host() {
    let first = TcpListener::bind("127.0.0.1:0").unwrap();
    let first_address = first.local_addr().unwrap();
    let second = TcpListener::bind("127.0.0.1:0").unwrap();
    let second_address = second.local_addr().unwrap();
    let (sent, received) = mpsc::channel();
    let server = thread::spawn(move || {
        let (mut stream, _) = first.accept().unwrap();
        sent.send(request(&mut stream)).unwrap();
        write!(stream, "HTTP/1.1 302 Found\r\nLocation: http://{second_address}/manifest.json\r\nContent-Length: 0\r\nConnection: close\r\n\r\n").unwrap();
        let (mut stream, _) = second.accept().unwrap();
        sent.send(request(&mut stream)).unwrap();
        stream
            .write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}")
            .unwrap();
    });
    let url = Url::parse(&format!(
        "http://{first_address}/manifest.json?provider=private"
    ))
    .unwrap();
    let response = ProviderHttp::default()
        .get(&url, Duration::from_secs(5))
        .unwrap();
    assert_eq!(response.status, 200);
    for _ in 0..2 {
        let headers = received
            .recv_timeout(Duration::from_secs(5))
            .unwrap()
            .to_ascii_lowercase();
        for forbidden in ["authorization:", "apikey:", "cookie:"] {
            assert!(!headers.contains(forbidden));
        }
    }
    server.join().unwrap();
}

#[test]
fn oversized_provider_bodies_are_rejected_and_http_failures_remain_distinct_from_empty_data() {
    for (status, size) in [(200, ADDON_RESPONSE_LIMIT + 1), (503, 2)] {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        let server = thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            let _ = request(&mut stream);
            write!(
                stream,
                "HTTP/1.1 {status} Test\r\nContent-Length: {size}\r\nConnection: close\r\n\r\n"
            )
            .unwrap();
            let _ = stream.write_all(&vec![b'x'; size]);
        });
        let url = Url::parse(&format!("http://{address}/manifest.json")).unwrap();
        let response = ProviderHttp::default().get(&url, Duration::from_secs(5));
        if status == 200 {
            assert!(matches!(response, Err(Error::ResponseLimit)));
        } else {
            assert_eq!(response.unwrap().success(), Err(Error::Http(503)));
        }
        server.join().unwrap();
    }
    assert!(AddonUrl::parse("https://provider.invalid")
        .unwrap()
        .resource_url_with_extras("catalog", "movie", "id", &[("search", "bad\0value")])
        .is_err());
    assert!(matches!(
        ProviderHttp::default().get(
            &Url::parse("file:///tmp/catalog.json").unwrap(),
            Duration::from_secs(1)
        ),
        Err(Error::InvalidData)
    ));
}
