mod support;
use nuvio_backend::{
    config::AccountConfig,
    error::Error,
    http::{Method, Response},
    model::{ProfileLock, Secret},
    service::{email_error, PollResult},
};
use serde_json::{json, Value};
use std::sync::{
    atomic::{AtomicUsize, Ordering},
    Arc,
};
use support::*;
use zeroize::Zeroizing;

#[test]
fn password_login_sends_correct_headers_and_escaped_json_without_fixed_buffers() {
    let email = "person@example.test";
    let password = "pAss\"word\\with spaces and Unicode 日本".repeat(100);
    let expected = password.clone();
    let service = service(move |request| {
        assert_eq!(request.method, Method::Post);
        assert_eq!(request.url.path(), "/auth/v1/token");
        assert_eq!(request.url.query(), Some("grant_type=password"));
        assert_eq!(request.public_key.expose(), "public-test-key");
        assert_eq!(request.authorization.expose(), "public-test-key");
        let body: Value = serde_json::from_slice(&request.body).unwrap();
        assert_eq!(body["email"], "person@example.test");
        assert!(body["password"].as_str().unwrap() == expected);
        response(200, json!({"session": auth("user", None, "sig")}))
    });
    assert_eq!(
        service
            .password_login(&Secret::new(email), &Secret::new(password))
            .unwrap()
            .subject,
        "user"
    );
}

#[test]
fn email_errors_cover_new_legacy_confirmation_rate_limit_and_outage_formats() {
    for (status, body, expected) in [
        (
            400,
            json!({"error_code":"invalid_credentials"}),
            Error::InvalidCredentials,
        ),
        (
            400,
            json!({"error":"invalid_grant","error_description":"Invalid login credentials"}),
            Error::InvalidCredentials,
        ),
        (
            400,
            json!({"error_code":"email_not_confirmed"}),
            Error::EmailUnconfirmed,
        ),
        (
            422,
            json!({"msg":"Email not confirmed"}),
            Error::EmailUnconfirmed,
        ),
        (429, json!({}), Error::Http(429)),
        (
            400,
            json!({"error_code":"over_email_send_rate_limit"}),
            Error::Http(429),
        ),
        (503, json!({}), Error::Http(503)),
        (
            400,
            json!({"error_code":"bad_json","message":"sensitive server message"}),
            Error::Http(400),
        ),
        (
            400,
            json!({"error_code":"validation_failed"}),
            Error::Http(400),
        ),
        (400, json!({}), Error::Http(400)),
        (
            403,
            json!({"message":"sensitive server message"}),
            Error::Http(403),
        ),
    ] {
        let response = response(status, body).unwrap();
        assert_eq!(email_error(&response), expected);
        assert!(!email_error(&response).to_string().contains("sensitive"));
    }
    assert_eq!(
        email_error(&Response {
            status: 503,
            body: Zeroizing::new(b"error code: 503".to_vec())
        }),
        Error::Http(503)
    );
}

#[test]
fn malformed_authentication_never_creates_a_session() {
    for body in [
        json!({"access_token":"invalid","refresh_token":"r"}),
        json!({"access_token":token("", None, "sig"),"refresh_token":"r"}),
        json!({"access_token":token("user", None, "sig"),"refresh_token":""}),
        json!({"session":null}),
    ] {
        let service = service(move |_| response(200, body.clone()));
        assert!(service
            .password_login(&Secret::new("a@b.test"), &Secret::new("password"))
            .is_err());
    }
}

#[test]
fn profile_fields_fallback_ids_legacy_sharing_and_unverified_locks() {
    let service = service(|_| {
        response(
            200,
            json!([
                {"profile_index":1,"name":"Primary","avatar_color_hex":"#123456","profile_background_url":"https://image.invalid/background"},
                {"profile_index":2,"id":"database-profile-key","name":"Second","avatar_id":"avatar","uses_primary_plugins":true},
                {"profile_index":0,"id":3,"name":"Third","uses_primary_addons":false,"uses_primary_plugins":true,"pin_enabled":true},
                {"name":"No valid ID"}, {"profile_index":0}, {"profile_index":-1}, {"profile_index":2,"name":"duplicate"}
            ]),
        )
    });
    let profiles = service.profiles(&Secret::new("token")).unwrap();
    assert_eq!(profiles.len(), 3);
    assert!(profiles[0].primary);
    assert_eq!(profiles[0].background, "https://image.invalid/background");
    assert_eq!(profiles[0].lock, ProfileLock::Unknown);
    assert!(profiles[1].share_primary_addons);
    assert!(!profiles[2].share_primary_addons);
    assert_eq!(profiles[2].lock, ProfileLock::Locked);
}

#[test]
fn profile_locks_use_the_numeric_profile_index_when_another_identifier_is_opaque() {
    let service = service(|_| {
        response(
            200,
            json!([
                {"profile_id":"database-profile-key","profile_index":2,"pin_enabled":true},
                {"profile_id":3,"profile_index":8,"pin_enabled":false}
            ]),
        )
    });
    let locks = service.locks(&Secret::new("token")).unwrap();
    assert_eq!(
        locks.get(&nuvio_backend::model::ProfileId::new(2).unwrap()),
        Some(&ProfileLock::Locked)
    );
    assert_eq!(
        locks.get(&nuvio_backend::model::ProfileId::new(3).unwrap()),
        Some(&ProfileLock::Unlocked)
    );
    assert!(!locks.contains_key(&nuvio_backend::model::ProfileId::new(8).unwrap()));
}

#[test]
fn tv_code_nonce_old_server_retry_poll_and_exchange() {
    let count = Arc::new(AtomicUsize::new(0));
    let requests = count.clone();
    let service = service(move |request| {
        let body: Value = serde_json::from_slice(&request.body).unwrap();
        match requests.fetch_add(1, Ordering::SeqCst) {
            0 => {
                assert_eq!(request.url.path(), "/auth/v1/signup");
                response(200, auth("anon", None, "anon"))
            }
            1 => {
                assert!(uuid::Uuid::parse_str(body["p_device_nonce"].as_str().unwrap()).is_ok());
                assert!(body.get("p_device_name").is_some());
                response(404, json!({"code":"PGRST202"}))
            }
            2 => {
                assert!(body.get("p_device_name").is_none());
                response(
                    200,
                    json!({"code":"login-code","web_url":"https://login.invalid/tv/code","poll_interval_seconds":4}),
                )
            }
            3 => {
                assert_eq!(body["p_code"], "login-code");
                response(200, json!({"status":"pending"}))
            }
            4 => response(200, json!({"status":"authenticated"})),
            5 => {
                assert_eq!(request.url.path(), "/functions/v1/tv-logins-exchange");
                assert_eq!(body["code"], "login-code");
                assert!(body.get("device_nonce").is_some());
                response(200, auth("user", None, "user"))
            }
            _ => panic!("unexpected request"),
        }
    });
    let anonymous = service.anonymous().unwrap();
    assert!(anonymous.anonymous);
    let code = service.request_code(&anonymous).unwrap();
    assert_eq!(code.interval_seconds, 4);
    assert_eq!(
        service.poll_code(&code, &anonymous).unwrap(),
        PollResult::Pending
    );
    assert_eq!(
        service.poll_code(&code, &anonymous).unwrap(),
        PollResult::Authorized
    );
    assert_eq!(
        service.exchange_code(&code, &anonymous).unwrap().subject,
        "user"
    );
    assert_eq!(count.load(Ordering::SeqCst), 6);
}

#[test]
fn tv_code_and_poll_accept_single_row_rpc_arrays_and_objects() {
    for rows in [false, true] {
        for (status, expected) in [
            ("pending", PollResult::Pending),
            ("authorized", PollResult::Authorized),
            ("expired", PollResult::Expired),
            ("cancelled", PollResult::Expired),
        ] {
            let service = service(move |request| {
                let record = match request.url.path() {
                    "/rest/v1/rpc/start_tv_login_session" => json!({
                        "code":"code", "web_url":"https://login.invalid/tv?code=code",
                        "poll_interval_seconds":5
                    }),
                    "/rest/v1/rpc/poll_tv_login_session" => json!({"status":status}),
                    _ => panic!("unexpected request"),
                };
                response(200, if rows { json!([record]) } else { record })
            });
            let anonymous = session("anonymous", None, "anon");
            let code = service.request_code(&anonymous).unwrap();
            assert_eq!(code.interval_seconds, 5);
            assert_eq!(code.code, "code");
            assert_eq!(service.poll_code(&code, &anonymous).unwrap(), expected);
        }
    }
}

#[test]
fn tv_rpc_rejects_empty_ambiguous_and_non_record_replies() {
    let anonymous = session("anonymous", None, "anon");
    let code = nuvio_backend::service::LoginCode {
        code: "code".to_owned(),
        web_url: "https://login.invalid/tv".to_owned(),
        interval_seconds: 3,
        nonce: Secret::new("nonce"),
    };
    for invalid in [
        json!([]),
        json!([{}, {}]),
        json!(null),
        json!(["pending"]),
        json!({"unknown":true}),
    ] {
        let service = service(move |_| response(200, invalid.clone()));
        assert_eq!(
            service.request_code(&anonymous).err(),
            Some(Error::InvalidData)
        );
        assert_eq!(
            service.poll_code(&code, &anonymous).err(),
            Some(Error::InvalidData)
        );
    }
}

#[test]
fn missing_or_empty_tv_url_falls_back_to_the_configured_page_with_the_code() {
    for web_url in [Value::Null, json!("")] {
        let service = service(move |_| {
            response(
                200,
                json!([{
                    "code":"one&two/日本", "web_url":web_url
                }]),
            )
        });
        let code = service
            .request_code(&session("anonymous", None, "anon"))
            .unwrap();
        let url = url::Url::parse(&code.web_url).unwrap();
        assert_eq!(url.host_str(), Some("login.invalid"));
        assert_eq!(url.path(), "/tv");
        assert_eq!(
            url.query_pairs().collect::<Vec<_>>(),
            [("code".into(), "one&two/日本".into())]
        );
    }
}

#[test]
fn invalid_config_and_rpc_names_are_rejected_before_transport() {
    for base in [
        "http://remote.invalid",
        "https://user:password@account.invalid",
        "https://account.invalid/?key=x",
        "ftp://account.invalid",
    ] {
        assert!(AccountConfig::new(base, "key", "https://login.invalid").is_err());
    }
    let service = service(|_| panic!("invalid route must not be sent"));
    for name in ["", "../token", "name?secret=value", "name/header"] {
        assert!(service
            .rpc(name, &json!({}), &Secret::new("token"))
            .is_err());
    }
}

#[test]
fn configuration_covers_every_property_in_the_repository_template() {
    let properties: std::collections::BTreeSet<_> = include_str!("../../local.properties.example")
        .lines()
        .filter_map(|line| {
            let line = line.trim();
            if line.starts_with('#') {
                return None;
            }
            line.split_once('=').map(|(key, _)| key.trim())
        })
        .collect();
    let compiled: std::collections::BTreeSet<_> = nuvio_backend::config::built::CONFIGURATION_KEYS
        .iter()
        .copied()
        .collect();
    assert_eq!(properties, compiled);
    let _ = (
        nuvio_backend::config::built::TMDB_API_KEY,
        nuvio_backend::config::built::SEEKR_API_KEY,
        nuvio_backend::config::built::SIMKL_APP_NAME,
    );
}
