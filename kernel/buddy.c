#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// Buddy allocator

static int nsizes;     // the number of entries in bd_sizes array

#define LEAF_SIZE     16                         // The smallest block size
#define MAXSIZE       (nsizes-1)                 // Largest index in bd_sizes array
#define BLK_SIZE(k)   ((1L << (k)) * LEAF_SIZE)  // Size of block at size k
#define HEAP_SIZE     BLK_SIZE(MAXSIZE) 
#define NBLK(k)       (1 << (MAXSIZE-k))         // Number of block at size k
#define ROUNDUP(n,sz) (((((n)-1)/(sz))+1)*(sz))  // Round up to the next multiple of sz

typedef struct list Bd_list;

// The allocator has sz_info for each size k. Each sz_info has a free
// list, an array alloc to keep track which blocks have been
// allocated, and an split array to to keep track which blocks have
// been split.  The arrays are of type char (which is 1 byte), but the
// allocator uses 1 bit per block (thus, one char records the info of
// 8 blocks).
struct sz_info {
  Bd_list free;
  char *alloc;
  char *split;
};
typedef struct sz_info Sz_info;

static Sz_info *bd_sizes; 
static void *bd_base;   // start address of memory managed by the buddy allocator
static struct spinlock lock;

// Return 1 if bit at position index in array is set to 1
int bit_isset(char *array, int index) {
  char b = array[index/8];
  char m = (1 << (index % 8));
  return (b & m) == m;
}

// Set bit at position index in array to 1
void bit_set(char *array, int index) {
  char b = array[index/8];
  char m = (1 << (index % 8));
  array[index/8] = (b | m);
}
//设置当前bit位为B1_is_free XOR B2_is_free（B1空闲 异或 B2空闲）
void bit_xor_set(char *array, int index) {
  index >>= 1;
  char b = array[index/8];// B1_is_free
  char m = (1 << (index % 8));// B2_is_free
  array[index/8] = (b ^ m);//与0异或不变，与1异或取反，因此清除分配状态也可以用这个函数
}
// Clear bit at position index in array
void bit_clear(char *array, int index) {
  char b = array[index/8];
  char m = (1 << (index % 8));
  array[index/8] = (b & ~m);
}

// Print a bit vector as a list of ranges of 1 bits
void
bd_print_vector(char *vector, int len) {
  int last, lb;
  
  last = 1;
  lb = 0;
  for (int b = 0; b < len; b++) {
    if (last == bit_isset(vector, b))
      continue;
    if(last == 1)
      printf(" [%d, %d)", lb, b);
    lb = b;
    last = bit_isset(vector, b);
  }
  if(lb == 0 || last == 1) {
    printf(" [%d, %d)", lb, len);
  }
  printf("\n");
}

// Print buddy's data structures
void
bd_print() {
  for (int k = 0; k < nsizes; k++) {
    printf("size %d (blksz %d nblk %d): free list: ", k, BLK_SIZE(k), NBLK(k));
    lst_print(&bd_sizes[k].free);
    printf("  alloc:");
    bd_print_vector(bd_sizes[k].alloc, NBLK(k));
    if(k > 0) {
      printf("  split:");
      bd_print_vector(bd_sizes[k].split, NBLK(k));
    }
  }
}

// What is the first k such that 2^k >= n?
int
firstk(uint64 n) {
  int k = 0;
  uint64 size = LEAF_SIZE;

  while (size < n) {
    k++;
    size *= 2;
  }
  return k;
}

// Compute the block index for address p at size k
int
blk_index(int k, char *p) {
  int n = p - (char *) bd_base;
  return n / BLK_SIZE(k);
}

// Convert a block index at size k back into an address
void *addr(int k, int bi) {
  int n = bi * BLK_SIZE(k);
  return (char *) bd_base + n;
}

// allocate nbytes, but malloc won't return anything smaller than LEAF_SIZE
void *
bd_malloc(uint64 nbytes)
{
  int fk, k;

  acquire(&lock);

  // Find a free block >= nbytes, starting with smallest k possible
  fk = firstk(nbytes);
  for (k = fk; k < nsizes; k++) {
    if(!lst_empty(&bd_sizes[k].free))//lst_empty：list空返回1
      break;
  }
  if(k >= nsizes) { // No free blocks?
    release(&lock);
    return 0;
  }

  // Found a block; pop it and potentially split it.
  char *p = lst_pop(&bd_sizes[k].free);

  // bit_set(bd_sizes[k].alloc, blk_index(k, p)/2);
  bit_xor_set(bd_sizes[k].alloc, blk_index(k, p));//异或设置，若该位已经为1，则重置为0，表示B1和B2都被分配了；若只有一个被分配了，则置为1
  for(; k > fk; k--) {
    // split a block at size k and mark one half allocated at size k-1
    // and put the buddy on the free list at size k-1
    char *q = p + BLK_SIZE(k-1);   // p's buddy
    bit_set(bd_sizes[k].split, blk_index(k, p));
    // bit_set(bd_sizes[k-1].alloc, blk_index(k-1, p)/2);
    bit_xor_set(bd_sizes[k-1].alloc, blk_index(k-1, p));//异或设置
    lst_push(&bd_sizes[k-1].free, q);//优先分配的都是左边的内存块
  }
  release(&lock);

  return p;
}

// Find the size of the block that p points to.
int
size(char *p) {
  for (int k = 0; k < nsizes; k++) {
    if(bit_isset(bd_sizes[k+1].split, blk_index(k+1, p))) {
      return k;
    }
  }
  return 0;
}

// Free memory pointed to by p, which was earlier allocated using
// bd_malloc.
void
bd_free(void *p) {
  void *q;
  int k;

  acquire(&lock);
  for (k = size(p); k < MAXSIZE; k++) {
    int bi = blk_index(k, p);
    int buddy = (bi % 2 == 0) ? bi+1 : bi-1;
    // bit_clear(bd_sizes[k].alloc, bi);  // free p at size k
    bit_xor_set(bd_sizes[k].alloc, bi);
    if (bit_isset(bd_sizes[k].alloc, bi/2)) {  // is buddy allocated?
      break;   // break out of loop
    }
    // budy is free; merge with buddy
    q = addr(k, buddy);
    lst_remove(q);    // remove buddy from free list
    if(buddy % 2 == 0) {
      p = q;
    }
    // at size k+1, mark that the merged buddy pair isn't split
    // anymore
    bit_clear(bd_sizes[k+1].split, blk_index(k+1, p));
  }
  lst_push(&bd_sizes[k].free, p);
  release(&lock);
}

// Compute the first block at size k that doesn't contain p
int
blk_index_next(int k, char *p) {
  int n = (p - (char *) bd_base) / BLK_SIZE(k);
  if((p - (char*) bd_base) % BLK_SIZE(k) != 0)
      n++;
  return n ;
}

int
log2(uint64 n) {
  int k = 0;
  while (n > 1) {
    k++;
    n = n >> 1;
  }
  return k;
}

// Mark memory from [start, stop), starting at size 0, as allocated. 
void
bd_mark(void *start, void *stop)
{
  int bi, bj;

  if (((uint64) start % LEAF_SIZE != 0) || ((uint64) stop % LEAF_SIZE != 0))
    panic("bd_mark");

  for (int k = 0; k < nsizes; k++) {
    bi = blk_index(k, start);//计算内存块索引,k层，从start到db_base的中间相隔的block数
    bj = blk_index_next(k, stop);//k层，从stop到db_base的中间相隔的block数
    for(; bi < bj; bi++) {
      if(k > 0) {
        // if a block is allocated at size k, mark it as split too.
        bit_set(bd_sizes[k].split, bi);
      }
      // bit_set(bd_sizes[k].alloc, bi);
      bit_xor_set(bd_sizes[k].alloc, bi);
    }
  }
}
int addr_in_range(void *addr, void *left, void *right,int size) {
  return (addr>=left)&&((addr+size)<right);
}
// If a block is marked as allocated and the buddy is free, put the
// buddy on the free list at size k.
int
bd_initfree_pair(int k, int bi, void *left, void *right) {
  int buddy = (bi % 2 == 0) ? bi+1 : bi-1;
  int free = 0;
  if(bit_isset(bd_sizes[k].alloc, bi/2)) {
    // one of the pair is free,只需要该位为1即可
    free = BLK_SIZE(k);
    if(addr_in_range(addr(k,bi),left,right,free)){
      // printf("left-buddy\n");
      lst_push(&bd_sizes[k].free, addr(k, bi));      // put buddy on free list
    } else{
      // printf("left-bi\n");
      lst_push(&bd_sizes[k].free, addr(k, buddy));      // put bi on free list
    }
    // if(bit_isset(bd_sizes[k].alloc, bi/2))
    //   lst_push(&bd_sizes[k].free, addr(k, buddy));   // put buddy on free list
    // else
    //   lst_push(&bd_sizes[k].free, addr(k, bi));      // put bi on free list
  }
  return free;
}

// Initialize the free lists for each size k.  For each size k, there
// are only two pairs that may have a buddy that should be on free list:
// bd_left and bd_right.
int
bd_initfree(void *bd_left, void *bd_right) {
  int free = 0;

  for (int k = 0; k < MAXSIZE; k++) {   // skip max size
    int left = blk_index_next(k, bd_left);//(bd_left-bd_base)/size(k),若不能整除，则结果+1
    int right = blk_index(k, bd_right);//(bd_right-bd_base)/size(k)
    free += bd_initfree_pair(k, left,bd_left,bd_right);
    if(right <= left)
      continue;
    free += bd_initfree_pair(k, right,bd_left,bd_right);
  }
  return free;
}

// Mark the range [bd_base,p) as allocated
int
bd_mark_data_structures(char *p) {
  int meta = p - (char*)bd_base;
  printf("bd: %d meta bytes for managing %d bytes of memory\n", meta, BLK_SIZE(MAXSIZE));
  bd_mark(bd_base, p);
  return meta;
}

// Mark the range [end, HEAPSIZE) as allocated
int
bd_mark_unavailable(void *end, void *left) {
  int unavailable = BLK_SIZE(MAXSIZE)-(end-bd_base);
  if(unavailable > 0)
    unavailable = ROUNDUP(unavailable, LEAF_SIZE);
  printf("bd: 0x%x bytes unavailable\n", unavailable);

  void *bd_end = bd_base+BLK_SIZE(MAXSIZE)-unavailable;
  bd_mark(bd_end, bd_base+BLK_SIZE(MAXSIZE));
  return unavailable;
}

// Initialize the buddy allocator: it manages memory from [base, end).
void
bd_init(void *base, void *end) {
  char *p = (char *) ROUNDUP((uint64)base, LEAF_SIZE);
  int sz;

  initlock(&lock, "buddy");
  bd_base = (void *) p;
  // size的数目是偏大的，则表示有一些内存块是不可用的
  // compute the number of sizes we need to manage [base, end)
  nsizes = log2(((char *)end-p)/LEAF_SIZE) + 1;
  if((char*)end-p > BLK_SIZE(MAXSIZE)) {
    nsizes++;  // round up to the next power of 2
  }

  printf("bd: memory sz is %d bytes; allocate an size array of length %d\n",
         (char*) end - p, nsizes);

  // allocate bd_sizes array
  bd_sizes = (Sz_info *) p;//获取需要分配数据块的首地址
  p += sizeof(Sz_info) * nsizes;//p指向数据块尾部
  memset(bd_sizes, 0, sizeof(Sz_info) * nsizes);//所有的内存块清零

  //为每层size分配alloc数组，用于存放内存块是否被占据的信息
  //按要求，两块内存块使用1个bit，1个字节8个bit
  // initialize free list and allocate the alloc array for each size k
  for (int k = 0; k < nsizes; k++) {
    lst_init(&bd_sizes[k].free);
    sz = sizeof(char)* ROUNDUP(NBLK(k), 16)/16;
    bd_sizes[k].alloc = p;//该数组在后面
    memset(bd_sizes[k].alloc, 0, sz);//分配给alloc的空间除以2
    p += sz;
  }

  //为每层size分配split数组，用于存放内存块是否分裂的信息
  //一个内存块使用1个bit，一个字节8个bit
  // allocate the split array for each size k, except for k = 0, since
  // we will not split blocks of size k = 0, the smallest size.
  for (int k = 1; k < nsizes; k++) {
    sz = sizeof(char)* (ROUNDUP(NBLK(k), 8))/8;
    bd_sizes[k].split = p;//该数组接在后面
    memset(bd_sizes[k].split, 0, sz);
    p += sz;
  }
  p = (char *) ROUNDUP((uint64) p, LEAF_SIZE);

  //标记元数据区
  // done allocating; mark the memory range [base, p) as allocated, so
  // that buddy will not hand out that memory.
  int meta = bd_mark_data_structures(p);
  
  //标记无效区
  // mark the unavailable memory range [end, HEAP_SIZE) as allocated,
  // so that buddy will not hand out that memory.
  int unavailable = bd_mark_unavailable(end, p);
  void *bd_end = bd_base+BLK_SIZE(MAXSIZE)-unavailable;
  
  // 初始化各层空闲链表
  // initialize free lists for each size k
  int free = bd_initfree(p, bd_end);
  // bd_print();
  // check if the amount that is free is what we expect
  if(free != BLK_SIZE(MAXSIZE)-meta-unavailable) {
    printf("free %d %d\n", free, BLK_SIZE(MAXSIZE)-meta-unavailable);
    panic("bd_init: free mem");
  }
}

