#pragma once

struct memMemStats
{
    uint32_t dword0;
    uint32_t dword4;
    uint32_t dword8;
    uint32_t dwordC;
    uint32_t dword10;
    uint32_t dword14;
};

class memMemoryAllocator
{
public:
    memMemoryAllocator* Prev {nullptr};
    void *HeapData {nullptr};
    int TotalSize {0};
    int Alignment {0};
    int Locked {0};
    bool field_14 {false};
    bool field_15 {false};
    bool CheckAlloc {false};
    bool field_17 {false};
    int Array1[32] {};
    int Array2[16] {};
    float fieldD8 {0.0f};

    memMemoryAllocator();
    ~memMemoryAllocator();

    void Init(void * heapData, uint32_t heapSize, bool a3, bool checkalloc);
    void Kill(void);

    void GetStats(memMemStats * stats, bool unused);

    static void DisplayUsed(char const* status);

    declstatic(memMemoryAllocator*, First);
    declstatic(memMemoryAllocator*, Current);
};

declvar(void(*)(char const *), datDisplayUsed);

declvar(int, datCurrentMemoryAlign);
declvar(int, datCurrentMemoryBucket);

check_size(memMemoryAllocator, 0xDC);

void InitMemoryHooks();