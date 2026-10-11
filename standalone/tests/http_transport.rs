mod support;
use nuvio_rust_app::{
    error::AppError,
    http::{parse_service_url, HttpClient, HttpRequest, Transport},
};
use std::time::Duration;
use support::{Reply, Server};

#[test]
fn relative_redirect_keeps_authentication_on_the_same_origin() {
    let server = Server::new(vec![
        Reply::redirect("/result"),
        Reply::new(200, "{\"ready\":true}"),
    ]);
    let mut request =
        HttpRequest::get(parse_service_url(&format!("{}/start", server.base)).unwrap());
    request
        .header("authorization", "Bearer fixture-token")
        .unwrap();
    let response = HttpClient::default().send(request).unwrap();
    assert_eq!(response.json::<serde_json::Value>().unwrap()["ready"], true);
    assert_eq!(
        server.request().headers["authorization"],
        "Bearer fixture-token"
    );
    let second = server.request();
    assert_eq!(second.target, "/result");
    assert_eq!(second.headers["authorization"], "Bearer fixture-token");
}

#[test]
fn credentialed_cross_origin_redirect_is_rejected_before_following() {
    let server = Server::new(vec![Reply::redirect("http://127.0.0.1:1/private")]);
    let mut request = HttpRequest::get(parse_service_url(&server.base).unwrap());
    request.header("apikey", "fixture-key").unwrap();
    assert!(matches!(
        HttpClient::default().send(request),
        Err(AppError::UnsafeRedirect)
    ));
    assert_eq!(server.request().headers["apikey"], "fixture-key");
}

#[test]
fn language_header_allows_public_cross_origin_redirects() {
    let target = Server::new(vec![Reply::new(200, "{}")]);
    let server = Server::new(vec![Reply::redirect(format!("{}/catalog", target.base))]);
    let mut request = HttpRequest::get(parse_service_url(&server.base).unwrap());
    request.header("accept-language", "en-US").unwrap();
    assert_eq!(HttpClient::default().send(request).unwrap().status, 200);
    assert_eq!(target.request().target, "/catalog");
}

#[test]
fn oversized_response_is_rejected_instead_of_truncated() {
    let server = Server::new(vec![Reply::new(200, "123456789")]);
    let mut request = HttpRequest::get(parse_service_url(&server.base).unwrap());
    request.limit = 8;
    assert!(matches!(
        HttpClient::default().send(request),
        Err(AppError::ResponseTooLarge)
    ));
}

#[test]
fn slow_response_obeys_timeout_without_exposing_the_url() {
    let mut reply = Reply::new(200, "{}");
    reply.delay = Duration::from_millis(150);
    let server = Server::new(vec![reply]);
    let request = HttpRequest::get(
        parse_service_url(&format!("{}/private?token=fixture-secret", server.base)).unwrap(),
    );
    let error = match HttpClient::with_timeout(Duration::from_millis(30)).send(request) {
        Err(error) => error,
        Ok(_) => panic!("expected timeout"),
    };
    assert_eq!(error, AppError::Timeout);
    assert!(!error.to_string().contains("fixture-secret"));
}

#[test]
fn service_urls_reject_local_files_and_embedded_passwords() {
    for url in [
        "file:///tmp/playlist",
        "ftp://example.invalid",
        "https://user:password@example.invalid",
    ] {
        assert_eq!(parse_service_url(url).unwrap_err(), AppError::InvalidUrl);
    }
}
