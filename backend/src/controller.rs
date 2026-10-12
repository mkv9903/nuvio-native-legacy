use crate::{
    account::AccountBackend,
    error::{Error, Result},
    model::{ProfileId, Secret},
};
use std::{
    sync::{Arc, Condvar, Mutex},
    thread::{self, JoinHandle},
};

enum Command {
    Email {
        generation: u64,
        email: Secret,
        password: Secret,
    },
    Tv {
        generation: u64,
    },
    Poll {
        generation: u64,
    },
    Select {
        action: u64,
        id: ProfileId,
        pin: Option<Secret>,
    },
}
struct Queue {
    pending: Option<Command>,
    stopped: bool,
}
struct Worker {
    queue: Mutex<Queue>,
    ready: Condvar,
}

/// One bounded account worker. A newer user action replaces queued work;
/// generations discard old results. Submitted login/profile actions perform
/// their network requests and persistence on the account worker.
pub struct AccountController {
    pub backend: AccountBackend,
    worker: Arc<Worker>,
    _thread: JoinHandle<()>,
}
impl AccountController {
    pub fn new(backend: AccountBackend) -> Result<Self> {
        let worker = Arc::new(Worker {
            queue: Mutex::new(Queue {
                pending: None,
                stopped: false,
            }),
            ready: Condvar::new(),
        });
        let queue = worker.clone();
        let account = backend.clone();
        let thread = thread::Builder::new()
            .name("nuvio-account".to_owned())
            .spawn(move || loop {
                let command = {
                    let mut state = queue
                        .queue
                        .lock()
                        .unwrap_or_else(|error| error.into_inner());
                    while state.pending.is_none() && !state.stopped {
                        state = queue
                            .ready
                            .wait(state)
                            .unwrap_or_else(|error| error.into_inner());
                    }
                    if state.stopped {
                        break;
                    }
                    state.pending.take().expect("pending account command")
                };
                match command {
                    Command::Email {
                        generation,
                        email,
                        password,
                    } => {
                        let _ = account.finish_email(generation, email, password);
                    }
                    Command::Tv { generation } => {
                        let _ = account.finish_tv_start(generation);
                    }
                    Command::Poll { generation } => {
                        let _ = account.finish_poll(generation);
                    }
                    Command::Select { action, id, pin } => {
                        let _ = account.finish_selection(action, id, pin);
                    }
                }
            })
            .map_err(|_| Error::Network)?;
        Ok(Self {
            backend,
            worker,
            _thread: thread,
        })
    }
    fn enqueue(&self, command: Command) {
        let mut queue = self
            .worker
            .queue
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        queue.pending = Some(command);
        self.worker.ready.notify_one();
    }
    pub fn email_login(&self, email: Secret, password: Secret) {
        let generation = self.backend.begin_email();
        self.enqueue(Command::Email {
            generation,
            email,
            password,
        });
    }
    pub fn tv_login(&self) {
        let generation = self.backend.begin_tv();
        self.enqueue(Command::Tv { generation });
    }
    pub fn step(&self) {
        if let Some(generation) = self.backend.claim_poll() {
            self.enqueue(Command::Poll { generation });
        }
    }
    pub fn select(&self, id: ProfileId, pin: Option<Secret>) -> u64 {
        let action = self.backend.begin_selection();
        self.enqueue(Command::Select { action, id, pin });
        action
    }
    pub fn cancel(&self) {
        self.backend.cancel();
        self.clear_pending();
    }
    pub fn cancel_selection(&self) {
        self.backend.cancel_selection();
        self.clear_pending();
    }
    pub fn logout(&self) {
        self.backend.logout();
        self.clear_pending();
    }
    fn clear_pending(&self) {
        self.worker
            .queue
            .lock()
            .unwrap_or_else(|error| error.into_inner())
            .pending = None;
    }
}
impl Drop for AccountController {
    fn drop(&mut self) {
        self.backend.cancel();
        let mut queue = self
            .worker
            .queue
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        queue.stopped = true;
        queue.pending = None;
        self.worker.ready.notify_one();
        // The bounded HTTP timeout lets an in-flight worker finish; dropping
        // the controller must not freeze the drawing thread while it does.
    }
}
