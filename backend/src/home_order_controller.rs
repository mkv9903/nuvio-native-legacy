//! One background Home-order worker with a bounded latest-context queue.
use crate::{
    content_context::ContentContext,
    error::{Error, Result},
    home_order_backend::HomeOrderBackend,
};
use std::{
    sync::{Arc, Condvar, Mutex},
    thread::{self, JoinHandle},
    time::{Duration, Instant},
};

const RETRY_INTERVAL: Duration = Duration::from_secs(30);
const REFRESH_INTERVAL: Duration = Duration::from_secs(300);
struct Queue {
    context: Option<ContentContext>,
    pending: Option<ContentContext>,
    next_request: Instant,
    clear: bool,
    stopped: bool,
}
struct Worker {
    queue: Mutex<Queue>,
    ready: Condvar,
}
pub struct HomeOrderController {
    pub backend: HomeOrderBackend,
    worker: Arc<Worker>,
    _thread: JoinHandle<()>,
}
impl HomeOrderController {
    pub fn new(backend: HomeOrderBackend) -> Result<Self> {
        let worker = Arc::new(Worker {
            queue: Mutex::new(Queue {
                context: None,
                pending: None,
                next_request: Instant::now(),
                clear: false,
                stopped: false,
            }),
            ready: Condvar::new(),
        });
        let shared = worker.clone();
        let service = backend.clone();
        let thread = thread::Builder::new()
            .name("nuvio-home-order".into())
            .spawn(move || loop {
                let (context, clear, stopped) = {
                    let mut queue = shared.queue.lock().unwrap();
                    while queue.pending.is_none() && !queue.clear && !queue.stopped {
                        queue = shared.ready.wait(queue).unwrap();
                    }
                    let work = (queue.pending.take(), queue.clear, queue.stopped);
                    queue.clear = false;
                    work
                };
                if clear {
                    let _ = service.clear();
                }
                if stopped {
                    break;
                }
                if let Some(context) = context {
                    // Never activate a context on a worker: frame selection owns it.
                    let result = service.synchronize(&context);
                    let mut queue = shared.queue.lock().unwrap();
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
    pub fn step(&self, allow_periodic: bool) {
        let context = self.backend.activate();
        let mut queue = self.worker.queue.lock().unwrap();
        if queue.context != context {
            queue.context = context.clone();
            queue.pending = context;
            queue.next_request = Instant::now() + REFRESH_INTERVAL;
            self.worker.ready.notify_one();
        } else if allow_periodic
            && queue.context.is_some()
            && queue.pending.is_none()
            && Instant::now() >= queue.next_request
        {
            queue.pending = queue.context.clone();
            queue.next_request = Instant::now() + REFRESH_INTERVAL;
            self.worker.ready.notify_one();
        }
    }
    pub fn refresh(&self) {
        let mut queue = self.worker.queue.lock().unwrap();
        queue.pending = queue.context.clone();
        queue.next_request = Instant::now() + REFRESH_INTERVAL;
        self.worker.ready.notify_one();
    }
    pub fn logout(&self) {
        self.backend.forget_runtime();
        let mut queue = self.worker.queue.lock().unwrap();
        queue.context = None;
        queue.pending = None;
        queue.clear = true;
        self.worker.ready.notify_one();
    }
}
impl Drop for HomeOrderController {
    fn drop(&mut self) {
        let mut queue = self.worker.queue.lock().unwrap();
        queue.stopped = true;
        queue.pending = None;
        self.worker.ready.notify_one();
        // Detach; an in-flight request stays guarded and cannot block the UI.
    }
}
