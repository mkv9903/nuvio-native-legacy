//! Errors contain neither response bodies nor URLs, which may contain credentials.
use std::fmt;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum AppError {
    Configuration,
    InvalidUrl,
    InvalidRequest,
    Connection,
    Timeout,
    Tls,
    ResponseTooLarge,
    InvalidResponse,
    Http(u16),
    UnsafeRedirect,
    TooManyRedirects,
    Storage,
    SessionExpired,
    InvalidCredentials,
    EmailUnconfirmed,
    LoginExpired,
    ProfileLocked,
    NoProfiles,
    Busy,
}

impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Configuration => {
                f.write_str("Account service configuration is missing or invalid.")
            }
            Self::InvalidUrl => f.write_str("The service address is invalid."),
            Self::InvalidRequest => f.write_str("The request could not be prepared."),
            Self::Connection => f.write_str("The service could not be reached. Try again."),
            Self::Timeout => f.write_str("The service took too long to respond. Try again."),
            Self::Tls => f.write_str("The secure connection could not be verified."),
            Self::ResponseTooLarge => {
                f.write_str("The service response exceeded the memory limit.")
            }
            Self::InvalidResponse => f.write_str("The service returned an unexpected response."),
            Self::Http(429) => f.write_str("Too many requests. Wait a minute and try again."),
            Self::Http(status) => write!(f, "The service rejected the request (HTTP {status})."),
            Self::UnsafeRedirect => {
                f.write_str("The service redirected an authenticated request to an unsafe address.")
            }
            Self::TooManyRedirects => f.write_str("The service redirected too many times."),
            Self::Storage => f.write_str("Local settings could not be read or saved."),
            Self::SessionExpired => f.write_str("Your login expired. Please sign in again."),
            Self::InvalidCredentials => f.write_str("The email address or password is incorrect."),
            Self::EmailUnconfirmed => f.write_str("Confirm your email address before signing in."),
            Self::LoginExpired => f.write_str("The login code expired. Request a new code."),
            Self::ProfileLocked => f.write_str("Enter the correct PIN to open this profile."),
            Self::NoProfiles => f.write_str("No profiles are available for this account."),
            Self::Busy => f.write_str("The app is busy. Try again shortly."),
        }
    }
}

impl std::error::Error for AppError {}

pub type Result<T> = std::result::Result<T, AppError>;
