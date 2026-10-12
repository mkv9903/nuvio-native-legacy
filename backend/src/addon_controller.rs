//! A bounded worker for addon storage and account synchronization.
use crate::{
    addon_backend::AddonBackend,
    addon_snapshot::AddonSnapshot,
    addons::InstalledAddon,
    content_context::ContentContext,
    error::{Error, Result},
};
use std::{
    sync::{Arc, Condvar, Mutex},
    thread::{self, JoinHandle},
    time::{Duration, Instant},
};

const RETRY_INTERVAL: Duration = Duration::from_secs(30);
const REFRESH_INTERVAL: Duration = Duration::from_secs(300);
struct Queue {
    pending: Option<ContentContext>,
    context: Option<ContentContext>,
    next_request: Instant,
    clear: bool,
    flush: bool,
    stopped: bool,
}
struct Worker {
    queue: Mutex<Queue>,
    ready: Condvar,
}
pub struct AddonController {
    pub backend: AddonBackend,
    worker: Arc<Worker>,
    _thread: JoinHandle<()>,
}
impl AddonController {
    pub fn new(backend: AddonBackend) -> Result<Self> {
        let worker = Arc::new(Worker {
            queue: Mutex::new(Queue {
                pending: None,
                context: None,
                next_request: Instant::now(),
                clear: false,
                flush: false,
                stopped: false,
            }),
            ready: Condvar::new(),
        });
        let shared = worker.clone();
        let service = backend.clone();
        let thread = thread::Builder::new()
            .name("nuvio-addons".into())
            .spawn(move || loop {
                let (context, clear, flush) = {
                    let mut queue = shared
                        .queue
                        .lock()
                        .unwrap_or_else(|error| error.into_inner());
                    while queue.pending.is_none() && !queue.clear && !queue.flush && !queue.stopped
                    {
                        queue = shared
                            .ready
                            .wait(queue)
                            .unwrap_or_else(|error| error.into_inner());
                    }
                    if queue.stopped {
                        let clear = queue.clear;
                        drop(queue);
                        // Persist accepted actions without making a final network request.
                        if clear {
                            let _ = service.clear();
                        } else {
                            let _ = service.flush_edits();
                        }
                        break;
                    }
                    let work = (queue.pending.take(), queue.clear, queue.flush);
                    queue.clear = false;
                    queue.flush = false;
                    work
                };
                if clear {
                    let _ = service.clear();
                    if let Some(context) = &context {
                        service.activate(context.force_primary);
                    }
                }
                if flush {
                    let _ = service.flush_edits();
                }
                if let Some(context) = context {
                    service.activate(context.force_primary);
                    let result = service.synchronize(&context);
                    let mut queue = shared
                        .queue
                        .lock()
                        .unwrap_or_else(|error| error.into_inner());
                    if queue.context.as_ref() == Some(&context) {
                        queue.next_request = Instant::now()
                            + if result.is_ok() {
                                REFRESH_INTERVAL
                            } else {
                                RETRY_INTERVAL
                            };
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
    /// Constant-time queue bookkeeping at each frame, after context selection.
    pub fn step(&self, force_primary: bool) {
        let context = self.backend.activate(force_primary);
        let mut queue = self
            .worker
            .queue
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        if queue.context != context {
            queue.context = context.clone();
            queue.pending = context;
            queue.flush = true;
            queue.next_request = Instant::now() + REFRESH_INTERVAL;
            self.worker.ready.notify_one();
        } else if queue.context.is_some()
            && queue.pending.is_none()
            && Instant::now() >= queue.next_request
        {
            queue.pending = queue.context.clone();
            queue.next_request = Instant::now() + REFRESH_INTERVAL;
            self.worker.ready.notify_one();
        }
    }
    pub fn refresh(&self) {
        let mut queue = self
            .worker
            .queue
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        queue.pending = queue.context.clone();
        queue.flush = true;
        queue.next_request = Instant::now() + REFRESH_INTERVAL;
        self.worker.ready.notify_one();
    }
    pub fn set_enabled(&self, snapshot: &AddonSnapshot, index: usize, enabled: bool) -> Result<()> {
        self.backend.set_enabled(snapshot, index, enabled)?;
        self.refresh();
        Ok(())
    }
    pub fn install(&self, snapshot: &AddonSnapshot, addon: InstalledAddon) -> Result<bool> {
        let installed = self.backend.install(snapshot, addon)?;
        if installed {
            self.refresh();
        }
        Ok(installed)
    }
    pub fn logout(&self) {
        // The account must already be logged out. Hide old content immediately.
        self.backend.forget_runtime();
        let mut queue = self
            .worker
            .queue
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        queue.context = None;
        queue.pending = None;
        queue.clear = true;
        queue.flush = false;
        self.worker.ready.notify_one();
    }
}
impl Drop for AddonController {
    fn drop(&mut self) {
        let mut queue = self
            .worker
            .queue
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        queue.stopped = true;
        queue.pending = None;
        self.worker.ready.notify_one();
        // A request already in flight finishes under its context guard. Drop
        // detaches instead of blocking the UI on a network timeout.
    }
}
