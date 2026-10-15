#include <cstddef>
#include <cstdint>
extern "C" { extern unsigned char __heap_base; }
struct Block{ size_t size; Block* next; };
static Block* freeList=nullptr;
static size_t heapTop=0;
extern "C" void* malloc(size_t len){
 len=(len+15)&~(size_t)15;if(!len)len=16;
 Block** p=&freeList;
 while(*p){Block* b=*p;if(b->size>=len){*p=b->next;return b+1;}p=&b->next;}
 if(!heapTop)heapTop=((size_t)&__heap_base+15)&~(size_t)15;
 size_t next=heapTop+sizeof(Block)+len;
 size_t bytes=(size_t)__builtin_wasm_memory_size(0)*65536;
 if(next>bytes){size_t pages=(next-bytes+65535)/65536;if(__builtin_wasm_memory_grow(0,pages)==(size_t)-1)return nullptr;}
 auto* b=(Block*)heapTop;b->size=len;b->next=nullptr;heapTop=next;return b+1;
}
extern "C" void free(void* v){if(!v)return;auto* b=(Block*)v-1;b->next=freeList;freeList=b;}
extern "C" void* memcpy(void* dst,const void* src,size_t n){auto*d=(volatile unsigned char*)dst;auto*s=(const volatile unsigned char*)src;for(size_t i=0;i<n;++i)d[i]=s[i];return dst;}
extern "C" void* memset(void* dst,int c,size_t n){auto*d=(volatile unsigned char*)dst;for(size_t i=0;i<n;++i)d[i]=(unsigned char)c;return dst;}
extern "C" void* memmove(void* dst,const void* src,size_t n){auto*d=(volatile unsigned char*)dst;auto*s=(const volatile unsigned char*)src;if((size_t)d<(size_t)s)for(size_t i=0;i<n;++i)d[i]=s[i];else for(size_t i=n;i>0;--i)d[i-1]=s[i-1];return dst;}
