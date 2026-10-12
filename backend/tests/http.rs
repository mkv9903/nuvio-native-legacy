mod support;
use nuvio_backend::{
    config::AccountConfig,
    error::Error,
    http::{HttpTransport, Method, Request, Transport},
    model::Secret,
    service::AccountService,
};
use serde_json::{json, Value};
use std::{
    io::{Read, Write},
    net::{TcpListener, TcpStream},
    sync::{mpsc, Arc},
    thread,
    time::Duration,
};
use support::*;
use zeroize::Zeroizing;

fn read_request(stream: &mut TcpStream) -> (String, Vec<u8>) {
    stream
        .set_read_timeout(Some(Duration::from_secs(5)))
        .unwrap();
    let mut bytes = Vec::new();
    let mut buffer = [0; 1024];
    let end = loop {
        let read = stream.read(&mut buffer).unwrap();
        assert!(read > 0);
        bytes.extend_from_slice(&buffer[..read]);
        if let Some(end) = bytes.windows(4).position(|part| part == b"\r\n\r\n") {
            break end + 4;
        }
        assert!(bytes.len() < 64 * 1024);
    };
    let headers = String::from_utf8(bytes[..end].to_vec()).unwrap();
    let length: usize = headers
        .lines()
        .find_map(|line| {
            line.split_once(':')
                .filter(|(name, _)| name.eq_ignore_ascii_case("content-length"))
                .map(|(_, value)| value.trim().parse().unwrap())
        })
        .unwrap_or(0);
    while bytes.len() < end + length {
        let read = stream.read(&mut buffer).unwrap();
        assert!(read > 0);
        bytes.extend_from_slice(&buffer[..read]);
    }
    (headers, bytes[end..end + length].to_vec())
}

#[test]
fn real_http_password_login_sends_expected_wire_headers_and_json() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap();
    let (sender, receiver) = mpsc::channel();
    let server = thread::spawn(move || {
        let (mut stream, _) = listener.accept().unwrap();
        sender.send(read_request(&mut stream)).unwrap();
        let body = serde_json::to_vec(&auth("user", None, "sig")).unwrap();
        write!(stream, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n", body.len()).unwrap();
        stream.write_all(&body).unwrap();
    });
    let service = AccountService::new(
        AccountConfig::new(
            &format!("http://{address}"),
            "public-test-key",
            "https://login.invalid/tv",
        )
        .unwrap(),
        Arc::new(HttpTransport::default()),
    );
    let session = service
        .password_login(
            &Secret::new("person@example.test"),
            &Secret::new("pAss\"word\\ 日本"),
        )
        .unwrap();
    assert_eq!(session.subject, "user");
    let (headers, body) = receiver.recv_timeout(Duration::from_secs(5)).unwrap();
    let headers = headers.to_ascii_lowercase();
    assert!(headers.starts_with("post /auth/v1/token?grant_type=password http/1.1\r\n"));
    assert!(headers.contains("apikey: public-test-key\r\n"));
    assert!(headers.contains("authorization: bearer public-test-key\r\n"));
    assert!(headers.contains("content-type: application/json\r\n"));
    let body: Value = serde_json::from_slice(&body).unwrap();
    assert_eq!(
        body,
        json!({"email":"person@example.test","password":"pAss\"word\\ 日本"})
    );
    server.join().unwrap();
}

#[test]
fn redirects_do_not_forward_credentials_and_body_limits_are_enforced() {
    for (status, body, limit, expected) in [
        (302, "redirect", 1024, None),
        (
            200,
            "a response larger than the configured limit",
            8,
            Some(Error::ResponseLimit),
        ),
    ] {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        let target = TcpListener::bind("127.0.0.1:0").unwrap();
        target.set_nonblocking(true).unwrap();
        let target_address = target.local_addr().unwrap();
        let server = thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            read_request(&mut stream);
            write!(stream,"HTTP/1.1 {status} Test\r\nLocation: http://{target_address}/unexpected\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{body}",body.len()).unwrap();
        });
        let result = HttpTransport::default().send(Request {
            method: Method::Get,
            url: format!("http://{address}/test").parse().unwrap(),
            public_key: Secret::new("public-key"),
            authorization: Secret::new("secret-user-token"),
            body: Zeroizing::new(Vec::new()),
            response_limit: limit,
        });
        if let Some(expected) = expected {
            assert_eq!(result.err(), Some(expected));
        } else {
            assert_eq!(result.unwrap().status, status);
        }
        server.join().unwrap();
        assert_eq!(
            target.accept().err().unwrap().kind(),
            std::io::ErrorKind::WouldBlock
        );
    }
}

#[test]
fn actual_connection_failure_is_an_english_transport_error() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap();
    drop(listener);
    let result = HttpTransport::default().send(Request {
        method: Method::Get,
        url: format!("http://{address}/test").parse().unwrap(),
        public_key: Secret::new("public-key"),
        authorization: Secret::new("secret-token"),
        body: Zeroizing::new(Vec::new()),
        response_limit: 1024,
    });
    let error = result.err().unwrap();
    assert_eq!(error, Error::Network);
    assert!(!error.to_string().contains(&address.to_string()));
    assert!(!error.to_string().contains("secret-token"));
}
