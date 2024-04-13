use std::collections::VecDeque;
use crossbeam_channel::{Sender, Receiver, bounded, TrySendError};

pub struct SyncBufferPool<T> {
    tx_ret: Sender<T>,
    rx_ret: Receiver<T>,
    pool: VecDeque<T>,
}

impl<T> SyncBufferPool<T> {
    pub fn new<F>(count: usize, f: F) -> SyncBufferPool<T>
        where F: FnMut() -> T
    {
        assert_ne!(count, 0);
        let (tx_ret, rx_ret) = bounded(count);
        // force 0 so resize_with will initialize all members with f
        let mut pool = VecDeque::<T>::with_capacity(0);
        pool.resize_with(count, f);
        SyncBufferPool {
            tx_ret,
            rx_ret,
            pool,
        }
    }

    pub fn get(&mut self) -> Option<SyncReturnableBuffer<T>> {
        // check if queue is empty; if it is, go ahead and pump the return queue
        if self.pool.is_empty() {
            while let Ok(item) = self.rx_ret.try_recv() {
                self.pool.push_back(item);
            }
        }
        // let pool return either Some or None; if None, its up to the caller
        // to provide back pressure
        if let Some(item) = self.pool.pop_front() {
            Some(SyncReturnableBuffer::new(item, self.tx_ret.clone()))
        } else {
            None
        }
    }

    pub fn count_enqueued(&mut self) -> usize {
        // for performance reasons, we don't enqueue rx until we are out of buffers; this
        //  method is only used for test anyway...
        self.pool.len() + self.rx_ret.len()
    }

}


pub struct SyncReturnableBuffer<T> {
    tx_ret: Sender<T>,
    data: Option<T>
}

impl<T> SyncReturnableBuffer<T> {
    fn new(data: T, tx_ret: Sender<T>) -> SyncReturnableBuffer<T> {
        SyncReturnableBuffer {
            tx_ret, data: Some(data)
        }
    }
    pub fn get_mut(&mut self) -> &mut T {
        return self.data.as_mut().unwrap()
    }
    pub fn get(&self) -> &T {
        return self.data.as_ref().unwrap()
    }
}

impl<T> Drop for SyncReturnableBuffer<T> {
    fn drop(&mut self) {
        let data = self.data.take().expect("I can't be uninitialized until dropped");
        match self.tx_ret.try_send(data) {
            Err(_) => {
                // should warn here: return channel is either full (which would imply a bug)
                //  or closed down (so I hope you meant to do that)
            },
            _ => {}
        }
    }
}

type StringSyncBufferPool = SyncBufferPool<Vec<u8>>;

pub fn new_string_sync_buffer_pool(count: usize, size: usize) -> StringSyncBufferPool {
    SyncBufferPool::<Vec<u8>>::new(count, || vec![0; size])
}

#[cfg(test)]
mod tests {
    use crate::new_string_sync_buffer_pool;

    #[test]
    fn can_create_buffer_pool() {
        let _ = new_string_sync_buffer_pool(5, 10);
    }

    #[test]
    fn can_exhaust_buffer_pool() {
        let mut pool = new_string_sync_buffer_pool(3, 10);
        let a = pool.get();
        let b = pool.get();
        let c = pool.get();
        let d = pool.get();
        assert_eq!(pool.count_enqueued(), 0);
        assert!(a.is_some());
        assert!(b.is_some());
        assert!(c.is_some());
        assert!(d.is_none());
    }

    #[test]
    fn will_enqueue_when_dropped() {
        let mut pool = new_string_sync_buffer_pool(3, 10);
        assert_eq!(pool.count_enqueued(), 3);
        let a = pool.get();
        assert_eq!(pool.count_enqueued(), 2);
        assert!(a.is_some());
        drop(a);
        assert_eq!(pool.count_enqueued(), 3);
    }

    #[test]
    fn can_mutate() {
        let mut pool = new_string_sync_buffer_pool(3, 10);
        let mut a = pool.get().unwrap();
        let a_inner = a.get_mut();
        assert_eq!(*a_inner, vec![0u8; 10]);
        a_inner.fill(5);
        assert_eq!(*a_inner, vec![5u8; 10]);
        assert_eq!(pool.count_enqueued(), 2);
        drop(a);
        assert_eq!(pool.count_enqueued(), 3);


    }
}