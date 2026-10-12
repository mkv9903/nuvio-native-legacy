use std::fmt;

/// Diagnostics contain categories and status codes, never URLs or service bodies.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Error {
    Configuration,
    InvalidData,
    InvalidSession,
    Storage,
    ProductionData,
    Network,
    Timeout,
    Tls,
    ResponseLimit,
    Http(u16),
    Cancelled,
    LoggedOut,
    InvalidCredentials,
    EmailUnconfirmed,
    CodeExpired,
    WrongPin,
    ProfileUnverified,
    OwnerUnverified,
    AddonEditsChanged,
    AddonJournalUnreadable,
}

impl fmt::Display for Error {
    fn fmt(&self, output: &mut fmt::Formatter<'_>) -> fmt::Result {
        output.write_str(match self {
            Self::Configuration => "Account service configuration is missing or invalid.",
            Self::InvalidData => "The service returned invalid data.",
            Self::InvalidSession => {
                "The saved login is incomplete or invalid. Please sign in again."
            }
            Self::Storage => "The account data could not be saved or read.",
            Self::ProductionData => "This test build cannot use production-app account data.",
            Self::Network => "The account server could not be reached. Please try again.",
            Self::Timeout => "The account server took too long to respond. Please try again.",
            Self::Tls => "The secure connection could not be verified.",
            Self::ResponseLimit => "The service response exceeded the memory limit.",
            Self::Cancelled => "The request was cancelled.",
            Self::LoggedOut => "Please sign in to your Nuvio account.",
            Self::InvalidCredentials => "The email address or password is incorrect.",
            Self::EmailUnconfirmed => "Confirm your email address before signing in.",
            Self::CodeExpired => "The TV login code expired. Please request a new code.",
            Self::WrongPin => "The profile PIN is incorrect.",
            Self::ProfileUnverified => "The profile lock could not be confirmed. Please try again.",
            Self::OwnerUnverified => "The account owner could not be confirmed. Please try again.",
            Self::AddonEditsChanged => {
                "Saved addon edits changed in another app process. Synchronization is paused."
            }
            Self::AddonJournalUnreadable => {
                "Saved addon edits could not be read. Synchronization is paused to preserve them."
            }
            Self::Http(429) => "Too many requests. Wait a minute and try again.",
            Self::Http(status) => {
                return write!(
                    output,
                    "The account server rejected the request (HTTP {status})."
                )
            }
        })
    }
}
impl std::error::Error for Error {}
pub type Result<T> = std::result::Result<T, Error>;
