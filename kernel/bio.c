// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13

struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  struct buf hashbucket[NBUCKETS];
} bcache;

void binit(void)
{
  struct buf *b;

  // Initialize each hash bucket with a lock
  for (int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.lock[i], "bcache");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];  
  }
  
  // Initialize all buffer blocks and distribute them among hash buckets
  int i = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++, i++) {
    int bucket = i % NBUCKETS;
    b->next = bcache.hashbucket[bucket].next;
    b->prev = &bcache.hashbucket[bucket];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[bucket].next->prev = b;
    bcache.hashbucket[bucket].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf* bget(uint dev, uint blockno) {
  struct buf *b;
  int hash_index = blockno % NBUCKETS;
  acquire(&bcache.lock[hash_index]);
  // 命中
  for (b = bcache.hashbucket[hash_index].next; b != &bcache.hashbucket[hash_index]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock[hash_index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 未命中
  // 先在当前哈希桶中寻找空闲的缓存块
  for (b = bcache.hashbucket[hash_index].prev; b != &bcache.hashbucket[hash_index]; b = b->prev) {
    // 找到了空闲缓存块
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[hash_index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // 当前哈希桶没有空闲缓存块，在其他哈希桶中寻找，需要释放当前拥有的锁去访问其他哈希桶
  release(&bcache.lock[hash_index]);

  // 遍历所有哈希桶，当然省去当前的哈希桶
  for (int i = 0; i < NBUCKETS; i++) {
    if (i == hash_index)
      continue;

    acquire(&bcache.lock[i]);
    // 在哈希桶 i 中寻找空闲的缓存块
    for (b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev) {
      if (b->refcnt == 0) {
        // 将缓存块从原哈希桶中移除
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.lock[i]);

        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // 将缓存块加入当前哈希桶
        acquire(&bcache.lock[hash_index]);
        b->next = bcache.hashbucket[hash_index].next;
        b->prev = &bcache.hashbucket[hash_index];
        bcache.hashbucket[hash_index].next->prev = b;
        bcache.hashbucket[hash_index].next = b;
        release(&bcache.lock[hash_index]);

        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[i]);
  }
  // 找不到则报错
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void 
brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int hash_index = b->blockno % NBUCKETS;
  acquire(&bcache.lock[hash_index]);

  b->refcnt--;
  // 如果没有其他进程在使用这个缓存块，将它从链表中移除
  if (b->refcnt == 0) {
    // 从缓存链表中移出
    b->next->prev = b->prev;
    b->prev->next = b->next;
    // 移到哈希桶头部
    b->next = bcache.hashbucket[hash_index].next;
    b->prev = &bcache.hashbucket[hash_index];
    bcache.hashbucket[hash_index].next->prev = b;
    bcache.hashbucket[hash_index].next = b;
  }
  release(&bcache.lock[hash_index]);
}

void 
bpin(struct buf *b) {
  int hash_index = b->blockno % NBUCKETS;
  acquire(&bcache.lock[hash_index]);
  b->refcnt++;
  release(&bcache.lock[hash_index]);
}

void 
bunpin(struct buf *b) {
  int hash_index = b->blockno % NBUCKETS;
  acquire(&bcache.lock[hash_index]);
  b->refcnt--;
  release(&bcache.lock[hash_index]);
}



