#include <memory>


class MemoryPool {
public:
    MemoryPool(
        size_t chunk_size,
        size_t chunk_alignment,
        size_t initial_chunks_per_block
    );


    MemoryPool() = delete;
    void* allocate();
    void deallocate(void* ptr);


    ~MemoryPool();


private:
    struct Chunk {
        Chunk* next;
    };


    struct Block {
        Block* next;
    };


    size_t chunk_size_;
    size_t block_size_;
    size_t chunks_per_block_;


    // linked list between blocks
    Block* current_block_;


    // pointer to the current chunk which should jump by chunk size every allocation
    Chunk* current_chunk_;


    // pointer to mark end of block
    Chunk* last_chunk_;


    // linked list between free chunks
    Chunk* free_chunks_;


    void allocateBlock(size_t blockSize);
    static size_t getAssignment(size_t chunk_size, size_t chunk_alignment) {
        if(chunk_size % chunk_alignment == 0) {
            return chunk_size;
        }
        return chunk_size / chunk_alignment * (chunk_alignment+1);
    }
};


template<class T>
class TypedMemoryPool {
public:
    TypedMemoryPool(size_t initial_chunks_per_block);
    TypedMemoryPool() = delete;
    template<class... Args> T* make(Args&&... args);
    void destroy(T* ptr);


private:
    std::unique_ptr<MemoryPool> memory_pool_;
};


MemoryPool::MemoryPool(
    size_t chunk_size,
    size_t chunk_alignment,
    size_t initial_chunks_per_block)
{
    chunk_size_ = getAssignment(std::max(sizeof(Chunk*), chunk_size), chunk_alignment);
    chunks_per_block_ = initial_chunks_per_block;


    // add pointer for Block pointer and add one extra chunk for initial chunk alignment
    block_size_ = sizeof(Block*) + (chunk_size_ + 1) * initial_chunks_per_block;


    current_block_ = nullptr;
    current_chunk_ = nullptr;
    last_chunk_ = nullptr;
    free_chunks_ = nullptr;
}


void MemoryPool::allocateBlock(size_t blockSize) {
    char* new_block = new char[block_size_];
    reinterpret_cast<Block*>(new_block)->next = current_block_;
    current_block_ = reinterpret_cast<Block*>(new_block);


    char* first_chunk = new_block + sizeof(Block*);


    uintptr_t address = reinterpret_cast<uintptr_t>(first_chunk);
    size_t padding = chunk_size_ - address % chunk_size_;


    current_chunk_ = reinterpret_cast<Chunk*>(first_chunk + padding);
    last_chunk_ = reinterpret_cast<Chunk*>(first_chunk + padding + chunks_per_block_ * chunk_size_);
}


void* MemoryPool::allocate() {
    void* mem;
    if(free_chunks_) {
        // free node either has data to the next node or could be returned
        // to the user to allocate data, so we don't need to allocate extra
        // memory for linked list pointers.
        mem = free_chunks_;
        free_chunks_ = free_chunks_->next;
        return mem;
    }
    if(current_chunk_ >= last_chunk_) {
        allocateBlock(block_size_);
    }
    mem = current_chunk_;


    // move current chunk by chunk size
    current_chunk_ = reinterpret_cast<Chunk*>(reinterpret_cast<char*>(mem)+chunk_size_);
    return mem;
}


void MemoryPool::deallocate(void* ptr) {
    if(ptr) {
        reinterpret_cast<Chunk*>(ptr)->next = free_chunks_;
        free_chunks_ = reinterpret_cast<Chunk*>(ptr);
    }
}


MemoryPool::~MemoryPool() {
    while(current_block_) {
        printf("%d\n", current_block_);
        Block* next = current_block_->next;
        delete[] reinterpret_cast<char*>(current_block_);
        current_block_ = next;
    }
}


template<class T>
TypedMemoryPool<T>::TypedMemoryPool(size_t initial_chunks_per_block) {
    memory_pool_ = std::make_unique<MemoryPool>(sizeof(T), alignof(T), initial_chunks_per_block);
}


template<class T>
template<class... Args> T* TypedMemoryPool<T>::make(Args&&... args) {
    void* mem = memory_pool_->allocate();
    T* obj;
    try {
        obj = new(mem) T{std::forward<Args>(args)...};
    } catch(std::exception& e) {
        // Avoid memory leak if the object creation failed
        obj = nullptr;
        memory_pool_->deallocate(mem);
    }
    return obj;
}


template<class T>
void TypedMemoryPool<T>::destroy(T* ptr) {
    if(ptr) {
        try {
            ptr->~T();
        } catch(std::exception& e) {}
        memory_pool_->deallocate(ptr);
    }
}




// Simple test here


struct DummyPoint{
    int x;
    int y;
    int z;
};


#include<vector>


template class TypedMemoryPool<DummyPoint>;


int main() {
    MemoryPool mm(10, 10, 10);
    TypedMemoryPool<DummyPoint>m(10);
    std::vector<DummyPoint*>v;
    for(int i = 0; i < 20; i++) {
        v.push_back(m.make(10, 20, 30));
    }
    for(auto x: v) {
        printf("%d %d %d\n", x->x, x->y, x->z);
        m.destroy(x);
    }
    v.clear();
    for(int i = 0; i < 20; i++) {
        v.push_back(m.make(40, 50, 60));
    }
    for(auto x: v) {
        printf("%d %d %d\n", x->x, x->y, x->z);
        m.destroy(x);
    }
    return 0;
}