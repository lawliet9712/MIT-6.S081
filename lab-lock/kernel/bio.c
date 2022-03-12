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

#define BUCKET_CNT 13
#define NBUF (BUCKET_CNT * 3)

struct bcache_bucket{
  struct buf head;
  struct spinlock lock;
};
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least. 
  struct bcache_bucket bucket[BUCKET_CNT];
  //struct buf head;
} bcache;


int 
hash_key(int blockno){
  return blockno % BUCKET_CNT;
}

void
binit(void)
{
  struct buf *b;
  char buf[32];
  int sz = 32;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < BUCKET_CNT; i++){
    snprintf(buf, sz, "bcache.bucket_%d", i);
    initlock(&bcache.bucket[i].lock, buf);
  }

  // Create linked list of buffers
  //bcache.head.prev = &bcache.head;
  //bcache.head.next = &bcache.head;
  int blockcnt = 0;
  struct bcache_bucket* bucket;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    //b->next = bcache.head.next;
    //b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    b->access_time = ticks;
    b->blockno = blockcnt++;
    bucket = &bcache.bucket[hash_key(b->blockno)];
    b->next = bucket->head.next;
    bucket->head.next = b;
    //bcache.head.next->prev = b;
    //bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *lrub;
  struct bcache_bucket* bucket = &bcache.bucket[hash_key(blockno)];

  //acquire(&bcache.lock);
  //printf("acquire %d bucket lock \n", hash_key(blockno));
  acquire(&bucket->lock);
  // Is the block already cached?
  for(b = &bucket->head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      b->access_time = ticks;
      //printf("found cache ... \n");
      //printf("release %d bucket lock \n", hash_key(blockno));
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // find bucket lru buffer
  lrub = 0;
  uint min_time = 0x8ffffff;
  for(b = &bucket->head; b; b = b->next){
    if (b->refcnt == 0 && b->access_time < min_time){
      min_time = b->access_time;
      lrub = b;
    }
  }
  if (lrub) {
    goto setup;
  }

  // Not cached.
  // find in the global array
  acquire(&bcache.lock);
  //printf("acqurie bcache.block blockno=%d \n", hash_key(blockno));

findbucket:
  lrub = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    if(b->refcnt == 0 && b->access_time < min_time) {
      lrub = b;
    }
  }

  ////printf("compare finish\n");
  if (lrub) {
    // step 1 : release from the old bucket
    // need to hold the old bucket lock
    struct bcache_bucket* old_bucket = &bcache.bucket[hash_key(lrub->blockno)];
    //printf("acquire old_bucket %d \n", hash_key(lrub->blockno));
    acquire(&old_bucket->lock);
    // other cpu may use this lrub before we lock the old_bucket lock
    if (lrub->refcnt != 0){
      //printf("release old_bucket %d \n", hash_key(lrub->blockno));
      release(&old_bucket->lock);
      goto findbucket;
    }

    b = &old_bucket->head;
    struct buf* bnext = b->next;
    while (bnext != lrub) {
      b = bnext;
      bnext = bnext->next;
    }
    b->next = bnext->next;

    //printf("release old_bucket %d \n", hash_key(lrub->blockno));
    release(&old_bucket->lock);
    // step 2 : add to target bucket 
    // pay attention, we hold the target bucket lock 
    lrub->next = bucket->head.next;
    bucket->head.next = lrub;
    //printf("release bcache.block blockno=%d\n", hash_key(blockno));
    release(&bcache.lock);
    //printf("update target bucket ... \n");

setup:
    lrub->dev = dev;
    lrub->blockno = blockno;
    lrub->valid = 0;
    lrub->refcnt = 1;
    lrub->access_time = ticks;
    release(&bucket->lock);
    //printf("release %d bucket lock \n", hash_key(blockno));
    acquiresleep(&lrub->lock);
    return lrub;
  }
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
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  struct bcache_bucket* bucket = &bcache.bucket[hash_key(b->blockno)];
  acquire(&bucket->lock);
  b->refcnt--;
  /*
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  */
  release(&bucket->lock);
  
}

void
bpin(struct buf *b) {
  struct bcache_bucket* bucket = &bcache.bucket[hash_key(b->blockno)];
  acquire(&bucket->lock);
  b->refcnt++;
  release(&bucket->lock);
}

void
bunpin(struct buf *b) {
  struct bcache_bucket* bucket = &bcache.bucket[hash_key(b->blockno)];
  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}


