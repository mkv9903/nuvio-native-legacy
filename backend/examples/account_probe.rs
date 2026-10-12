//! Diagnose the anonymous QR flow without credentials or persistent account data.
use nuvio_backend::{
    config::AccountConfig,
    error::{Error, Result},
    http::{HttpTransport, Request, Response, Transport},
    service::AccountService,
};
use serde_json::Value;
use std::sync::Arc;

struct ProbeTransport(HttpTransport);
impl Transport for ProbeTransport {
    fn send(&self, request: Request) -> Result<Response> {
        // Only fixed account endpoint paths are reported. Never log query
        // values, headers, request bodies, raw replies, tokens or login URLs.
        let endpoint = request.url.path().to_owned();
        let response = self.0.send(request)?;
        let value = serde_json::from_slice::<Value>(&response.body).ok();
        let (shape, object) = match &value {
            Some(Value::Object(object)) => ("object".to_owned(), Some(object)),
            Some(Value::Array(items)) => (
                format!("array({})", items.len()),
                items.first().and_then(Value::as_object),
            ),
            Some(_) => ("scalar".to_owned(), None),
            None => ("non-JSON".to_owned(), None),
        };
        let present: Vec<_> = [
            "access_token",
            "refresh_token",
            "session",
            "code",
            "web_url",
            "poll_interval_seconds",
            "status",
            "error_code",
            "error",
            "message",
        ]
        .into_iter()
        .filter(|field| object.is_some_and(|object| object.contains_key(*field)))
        .collect();
        println!(
            "{endpoint}: HTTP {}, shape={shape}, fields={present:?}",
            response.status
        );
        Ok(response)
    }
}

fn main() -> Result<()> {
    let service = AccountService::new(
        AccountConfig::built()?,
        Arc::new(ProbeTransport(HttpTransport::default())),
    );
    let anonymous = service.anonymous().map_err(|error| {
        eprintln!("Anonymous authentication failed: {error}");
        error
    })?;
    println!("Anonymous authentication decoded successfully");
    let code = service.request_code(&anonymous).map_err(|error| {
        eprintln!("TV code request failed: {error}");
        error
    })?;
    println!("TV code decoded successfully");
    let status = service.poll_code(&code, &anonymous).map_err(|error| {
        eprintln!("TV code polling failed: {error}");
        error
    })?;
    println!("TV polling decoded successfully: {status:?}; no account data was saved");
    Ok::<(), Error>(())
}
