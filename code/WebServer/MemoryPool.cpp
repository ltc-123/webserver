#include "MemoryPool.h"
#include <assert.h>
MemoryPool::MemoryPool() {}

MemoryPool::~MemoryPool() {
    Slot* cur = currentBlock_;
    while(cur) {
        Slot* next = cur->next;

        //转化为void指针，因为void类型不需要调用析构函数，只释放空间
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

void MemoryPool::init(int size) {
    assert(size > 0);
    slotSize_ = size;
    currentBlock_ = NULL;
    currentSlot_ = NULL;
    lastSlot_ = NULL;
    freeSlot_ = NULL;
}

inline size_t MemoryPool::padPointer(char* p, size_t align) {
    size_t result = reinterpret_cast<size_t>(p);
    return ((align - result) & align);
}
//当前Block不够了 向操作系统申请内存
Slot* MemoryPool::allocateBlock() {
    char* newBlock = reinterpret_cast<char *>(operator new(BlockSize));

    char * body = newBlock + sizeof(Slot*);
    size_t bodyPadding = padPointer(body, static_cast<size_t>(slotSize_));

    Slot* useSlot;
    {
        MutexLockGuard lock(mutex_other_);
        //newBlock接到Slock链表的头部
        reinterpret_cast<Slot *>(newBlock)->next = currentBlock_;
        currentBlock_ = reinterpret_cast<Slot *>(newBlock);

        currentSlot_ = reinterpret_cast<Slot *>(body + bodyPadding);
        lastSlot_ = reinterpret_cast<Slot *>(newBlock + BlockSize - slotSize_ + 1);
        useSlot = currentSlot_;

        //slot一次移动8个字节
        currentSlot_ += (slotSize_ >> 3);
    }

    return useSlot;
}
//free_solve没有空的了，到当前Block将current所指的内存分配出去
Slot* MemoryPool::nofree_solve() {
    if(currentSlot_ >= lastSlot_)//满了的话
        return allocateBlock();
    Slot* useSlot;
    {
        MutexLockGuard lock(mutex_other_);
        useSlot = currentSlot_;
        currentSlot_ += (slotSize_ >> 3);//每个槽所占的字节数乘8
    }
    return useSlot;
}
//有free_solve直接分进去，没有的话到当前Block将current所指的内存分配出去，还不够的话，再向操作系统申请内存。
Slot* MemoryPool::allocate() {
    if (freeSlot_) {
        MutexLockGuard lock(mutex_freeSlot_);
        if (freeSlot_) {
            //头插法
            Slot* result = freeSlot_;
            freeSlot_ = freeSlot_->next;
            return result;
        }
    }

    return nofree_solve();
}

inline void MemoryPool::deAllocate(Slot* p) {
    if (p) {
        MutexLockGuard lock(mutex_freeSlot_);
        p->next = freeSlot_;
        freeSlot_ = p;
    }
}

MemoryPool& get_MemoryPool(int id) {
    static MemoryPool memorypool_[64];
    return memorypool_[id];
}

void init_MemoryPool() {
    for (int i = 0; i < 64; ++i) {
        get_MemoryPool(i).init((i + 1) << 3);
    }
}

void* use_Memory(size_t size) {
    if (!size)
        return nullptr;
    if (size > 512)
        return operator new(size);

    //(size / 8)向上取整
    return reinterpret_cast<void *>(get_MemoryPool(((size + 7) >> 3) - 1).allocate());
}

void free_Memory(size_t size, void* p) {
    if (!p) return ;
    if (size > 512) {
        operator delete (p);
        return ;
    }
    get_MemoryPool(((size + 1) >> 3) - 1).deAllocate(reinterpret_cast<Slot *>(p));
}