// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmen{
  struct spinlock lock;
  struct run *freelist;
};

struct kmen kmems[NCPU];//为8个cpu分配独立的freelist
void
kinit()
{
  //初始化8个锁
  initlock(&kmems[0].lock, "kmem_zero");
  initlock(&kmems[1].lock, "kmem_one");
  initlock(&kmems[2].lock, "kmem_two");
  initlock(&kmems[3].lock, "kmem_three");
  initlock(&kmems[4].lock, "kmem_four");
  initlock(&kmems[5].lock, "kmem_five");
  initlock(&kmems[6].lock, "kmem_six");
  initlock(&kmems[7].lock, "kmem_seven");
  freerange(end, (void*)PHYSTOP);//#define PHYSTOP (KERNBASE + 128*1024*1024)
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  struct run *r;
  int i=0,j;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    // kfree(p);
    // 按分组分别初始化空闲链表
    r = (struct run*)p;
    j = i%NCPU;
    acquire(&kmems[j].lock);
    r->next = kmems[j].freelist;
    kmems[j].freelist = r;
    release(&kmems[j].lock);
    i++;
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int cid;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();//关中断，在调用cpuid()并使用其返回值的过程中需要关闭中断
  cid = cpuid();
  pop_off();//开中断
  acquire(&kmems[cid].lock);
  r->next = kmems[cid].freelist;
  kmems[cid].freelist = r;
  release(&kmems[cid].lock);
  
  // acquire(&kmem.lock);
  // r->next = kmem.freelist;
  // kmem.freelist = r;
  // release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cid,i;
  push_off();//关中断，在调用cpuid()并使用其返回值的过程中需要关闭中断
  cid = cpuid();
  pop_off();//开中断
  acquire(&kmems[cid].lock);
  r = kmems[cid].freelist;
  if(r){//若能成功分配到内存块
    kmems[cid].freelist = r->next;
    release(&kmems[cid].lock);
  }
  else{//否则要从别的cpu的freelist窃取内存块
    release(&kmems[cid].lock);
    for(i = 0; i < NCPU; i++){
      if(i != cid){
        acquire(&kmems[i].lock);
        r = kmems[i].freelist;//给当前准备窃取的内存块
        if(r){//窃取成功
          kmems[i].freelist = r->next;
          release(&kmems[i].lock);
          break;
        }
        else{
          release(&kmems[i].lock);
          continue;//若此cpu也没有freelist了，则继续窃取下一个
        } 
        
      }
    }
    if(!r) return 0;//表示所有cpu都没有空闲块
  }
  
  
  
  
  // acquire(&kmem.lock);
  // r = kmem.freelist;
  // if(r)
  //   kmem.freelist = r->next;
  // release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
