use crate::error::{AppError, Result};
use std::{
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc::{self, Receiver, SyncSender},
        Arc, Mutex,
    },
    thread::{self, JoinHandle},
};

type Job<T> = Box<dyn FnOnce(&SyncSender<T>) + Send + 'static>;

pub struct Workers<T: Send + 'static> {
    jobs: Option<SyncSender<Job<T>>>,
    stopped: Arc<AtomicBool>,
    threads: Vec<JoinHandle<()>>,
}

impl<T: Send + 'static> Workers<T> {
    pub fn new(count: usize, pending: usize) -> (Self, Receiver<T>) {
        let (jobs, input) = mpsc::sync_channel::<Job<T>>(pending);
        let (output, events) = mpsc::sync_channel(pending + count);
        let input = Arc::new(Mutex::new(input));
        let stopped = Arc::new(AtomicBool::new(false));
        let mut threads = Vec::new();
        for _ in 0..count {
            let input = input.clone();
            let output = output.clone();
            let stop = stopped.clone();
            threads.push(thread::spawn(move || loop {
                let task = input.lock().expect("worker queue").recv();
                let Ok(task) = task else {
                    break;
                };
                if !stop.load(Ordering::Acquire) {
                    task(&output);
                }
            }));
        }
        (
            Self {
                jobs: Some(jobs),
                stopped,
                threads,
            },
            events,
        )
    }

    pub fn submit(&self, job: impl FnOnce(&SyncSender<T>) + Send + 'static) -> Result<()> {
        self.jobs
            .as_ref()
            .ok_or(AppError::Busy)?
            .try_send(Box::new(job))
            .map_err(|_| AppError::Busy)
    }
}

impl<T: Send + 'static> Drop for Workers<T> {
    fn drop(&mut self) {
        // The owner drops its event receiver first, releasing any blocked replies.
        self.stopped.store(true, Ordering::Release);
        self.jobs.take();
        for thread in self.threads.drain(..) {
            let _ = thread.join();
        }
    }
}
