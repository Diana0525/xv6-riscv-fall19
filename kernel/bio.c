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
  //自旋锁，用于互斥访问
  //用于表示bcache链表是否被锁住
  struct spinlock lock[NBUCKETS];
  //buf数组的每一个元素都是一块块缓存块=磁盘块
  //buf.lock是睡眠锁
  //buf.lock用于表示缓存数据块是否被锁住，针对每一块数据块
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  // struct buf head;
  struct buf hashbucket[NBUCKETS];//每个哈希队列一个linked list 以及一个lock
} bcache;


void
binit(void)
{
  struct buf *b;
  int count = 0,num;
  //初始化13个锁
  initlock(&bcache.lock[0], "bcache_zero");
  initlock(&bcache.lock[1], "bcache_one");
  initlock(&bcache.lock[2], "bcache_two");
  initlock(&bcache.lock[3], "bcache_three");
  initlock(&bcache.lock[4], "bcache_four");
  initlock(&bcache.lock[5], "bcache_five");
  initlock(&bcache.lock[6], "bcache_six");
  initlock(&bcache.lock[7], "bcache_seven");
  initlock(&bcache.lock[8], "bcache_eight");
  initlock(&bcache.lock[9], "bcache_nine");
  initlock(&bcache.lock[10], "bcache_ten");
  initlock(&bcache.lock[11], "bcache_eleven");
  initlock(&bcache.lock[12], "bcache_twelve");

  // Create linked list of buffers
  // 将缓存块区域分为13个链表
  for(int i=0;i<NBUCKETS;i++){
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  //13个链表通过取余运算分别进行头插法初始化
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){//头插法插入缓存块
    num = count % NBUCKETS;
    b->next = bcache.hashbucket[num].next;
    b->prev = &bcache.hashbucket[num];//b插入到head 和 head.next中间
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[num].next->prev = b;
    bcache.hashbucket[num].next = b;
    count++;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// 从磁盘中获得了缓存块
static struct buf*
bget(uint dev, uint blockno)//dev是设备号，blockno是缓存数据块号
{
  struct buf *b;

  int i;
  // 哈希函数用取余的方式构造
  // 判断该块是否已经在cache里
  i = blockno % NBUCKETS;
  acquire(&bcache.lock[i]);
  for(b = bcache.hashbucket[i].next; b != &bcache.hashbucket[i]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[i]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // acquire(&bcache.lock);

  // // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // Not cached; recycle an unused buffer.
  //若缓存块不在cache中,找一个空的cache块存储该数据块
  for(b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev){
    if(b->refcnt == 0){
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[i]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 若当前链表已满，则进行窃取
  for(int j = 0; j < NBUCKETS; j++){
    if(j != i){
      acquire(&bcache.lock[j]);
      for(b = bcache.hashbucket[j].prev; b != &bcache.hashbucket[j]; b = b->prev){
        if(b->refcnt == 0){
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          //将b从j的链表中摘去
          b->prev->next = b->next;
          b->next->prev = b->prev;
          release(&bcache.lock[j]);
          //将b插入i的链表中
          b->next = bcache.hashbucket[i].next;
          b->prev = &bcache.hashbucket[i];//b插入到head 和 head.next中间
          bcache.hashbucket[i].next->prev = b;
          bcache.hashbucket[i].next = b;
          release(&bcache.lock[i]);
          acquiresleep(&b->lock);
          return b;
        }
      }
      release(&bcache.lock[j]);
    }
  }
  
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
// 读取指定设备号、数据块号的数据，判断是在cache中还是要从内存中读取
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);//把磁盘内容读取到缓存区
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
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))//若检测出未锁则报错
    panic("brelse");

  releasesleep(&b->lock);//解缓存块的锁

  int key;
  key = b->blockno % NBUCKETS;//计算哈希函数 
  acquire(&bcache.lock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[key].next;
    b->prev = &bcache.hashbucket[key];
    bcache.hashbucket[key].next->prev = b;
    bcache.hashbucket[key].next = b;
  }
  
  release(&bcache.lock[key]);
}

void
bpin(struct buf *b) {
  int key;
  key = b->blockno % NBUCKETS;
  acquire(&bcache.lock[key]);
  b->refcnt++;
  release(&bcache.lock[key]);
}

void
bunpin(struct buf *b) {
  int key;
  key = b->blockno % NBUCKETS;
  acquire(&bcache.lock[key]);
  b->refcnt--;
  release(&bcache.lock[key]);
}


