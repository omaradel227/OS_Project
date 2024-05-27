#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#define MEMORY_SIZE 1024

typedef struct MemoryBlock {
    int start;
    int size;
    bool allocated;
    struct MemoryBlock* next;
} MemoryBlock;

typedef struct {
    MemoryBlock* head;
    pthread_mutex_t lock;
    void* cache;
} MemoryAllocator;

MemoryAllocator* initializeMemoryAllocator(int size);
void* allocateMemory(MemoryAllocator* allocator, int size, const char* strategy);
void deallocateMemory(MemoryAllocator* allocator, void* ptr);
void coalesceMemory(MemoryAllocator* allocator);
void printMemoryBlocks(MemoryAllocator* allocator);
void freeMemoryAllocator(MemoryAllocator* allocator);

void* allocateFirstFit(MemoryAllocator* allocator, int size);
void* allocateBestFit(MemoryAllocator* allocator, int size);
void* allocateWorstFit(MemoryAllocator* allocator, int size);

double measurePerformance(MemoryAllocator* allocator, const char* strategy, int* blockSizes, int numBlocks, int numSizes);

int main() {
    MemoryAllocator* allocator = initializeMemoryAllocator(MEMORY_SIZE);

    printf("Initial memory blocks:\n");
    printMemoryBlocks(allocator);

    int blockSizes[] = {50, 100, 200, 75, 125};
    int numSizes = sizeof(blockSizes) / sizeof(blockSizes[0]);

    // Perform multiple allocations and deallocations to create fragmentation
    void* blocks[5];
    blocks[0] = allocateMemory(allocator, 100, "FirstFit");
    blocks[1] = allocateMemory(allocator, 200, "FirstFit");
    blocks[2] = allocateMemory(allocator, 50, "FirstFit");
    deallocateMemory(allocator, blocks[1]);
    blocks[3] = allocateMemory(allocator, 75, "FirstFit");
    deallocateMemory(allocator, blocks[0]);
    blocks[4] = allocateMemory(allocator, 125, "FirstFit");

    printf("Memory blocks after creating fragmentation with First Fit:\n");
    printMemoryBlocks(allocator);

    freeMemoryAllocator(allocator);
    allocator = initializeMemoryAllocator(MEMORY_SIZE);

    blocks[0] = allocateMemory(allocator, 100, "BestFit");
    blocks[1] = allocateMemory(allocator, 200, "BestFit");
    blocks[2] = allocateMemory(allocator, 50, "BestFit");
    deallocateMemory(allocator, blocks[1]);
    blocks[3] = allocateMemory(allocator, 75, "BestFit");
    deallocateMemory(allocator, blocks[0]);
    blocks[4] = allocateMemory(allocator, 125, "BestFit");

    printf("Memory blocks after creating fragmentation with Best Fit:\n");
    printMemoryBlocks(allocator);

    freeMemoryAllocator(allocator);
    allocator = initializeMemoryAllocator(MEMORY_SIZE);

    blocks[0] = allocateMemory(allocator, 100, "WorstFit");
    blocks[1] = allocateMemory(allocator, 200, "WorstFit");
    blocks[2] = allocateMemory(allocator, 50, "WorstFit");
    deallocateMemory(allocator, blocks[1]);
    blocks[3] = allocateMemory(allocator, 75, "WorstFit");
    deallocateMemory(allocator, blocks[0]);
    blocks[4] = allocateMemory(allocator, 125, "WorstFit");

    printf("Memory blocks after creating fragmentation with Worst Fit:\n");
    printMemoryBlocks(allocator);

    // Measure and compare performance
    allocator = initializeMemoryAllocator(MEMORY_SIZE);
    double firstFitTime = measurePerformance(allocator, "FirstFit", blockSizes, 100, numSizes);
    freeMemoryAllocator(allocator);

    allocator = initializeMemoryAllocator(MEMORY_SIZE);
    double bestFitTime = measurePerformance(allocator, "BestFit", blockSizes, 100, numSizes);
    freeMemoryAllocator(allocator);

    allocator = initializeMemoryAllocator(MEMORY_SIZE);
    double worstFitTime = measurePerformance(allocator, "WorstFit", blockSizes, 100, numSizes);
    freeMemoryAllocator(allocator);

    printf("First Fit Allocation Time: %f seconds\n", firstFitTime);
    printf("Best Fit Allocation Time: %f seconds\n", bestFitTime);
    printf("Worst Fit Allocation Time: %f seconds\n", worstFitTime);

    return 0;
}

MemoryAllocator* initializeMemoryAllocator(int size) {
    MemoryAllocator* allocator = (MemoryAllocator*)malloc(sizeof(MemoryAllocator));
    allocator->head = (MemoryBlock*)malloc(sizeof(MemoryBlock));
    allocator->head->start = 0;
    allocator->head->size = size;
    allocator->head->allocated = false;
    allocator->head->next = NULL;
    pthread_mutex_init(&allocator->lock, NULL);
    allocator->cache = NULL;
    return allocator;
}

void* allocateMemory(MemoryAllocator* allocator, int size, const char* strategy) {
    pthread_mutex_lock(&allocator->lock);
    void* block = NULL;

    if (strcmp(strategy, "FirstFit") == 0) {
        block = allocateFirstFit(allocator, size);
    } else if (strcmp(strategy, "BestFit") == 0) {
        block = allocateBestFit(allocator, size);
    } else if (strcmp(strategy, "WorstFit") == 0) {
        block = allocateWorstFit(allocator, size);
    }

    pthread_mutex_unlock(&allocator->lock);
    return block;
}

void* allocateFirstFit(MemoryAllocator* allocator, int size) {
    MemoryBlock* current = allocator->head;
    while (current != NULL) {
        if (!current->allocated && current->size >= size) {
            if (current->size > size) {
                MemoryBlock* newBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
                if (newBlock == NULL) {
                    fprintf(stderr, "Error: Out of memory\n");
                    return NULL;
                }
                newBlock->start = current->start + size;
                newBlock->size = current->size - size;
                newBlock->allocated = false;
                newBlock->next = current->next;
                current->next = newBlock;
            }
            current->size = size;
            current->allocated = true;
            return (void*)(long)(current->start);
        }
        current = current->next;
    }
    fprintf(stderr, "Error: Out of memory\n");
    return NULL;
}

void* allocateBestFit(MemoryAllocator* allocator, int size) {
    MemoryBlock* current = allocator->head;
    MemoryBlock* bestFit = NULL;

    while (current != NULL) {
        if (!current->allocated && current->size >= size) {
            if (bestFit == NULL || current->size < bestFit->size) {
                bestFit = current;
            }
        }
        current = current->next;
    }

    if (bestFit != NULL) {
        if (bestFit->size > size) {
            MemoryBlock* newBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
            if (newBlock == NULL) {
                fprintf(stderr, "Error: Out of memory\n");
                return NULL;
            }
            newBlock->start = bestFit->start + size;
            newBlock->size = bestFit->size - size;
            newBlock->allocated = false;
            newBlock->next = bestFit->next;
            bestFit->next = newBlock;
        }
        bestFit->size = size;
        bestFit->allocated = true;
        return (void*)(long)(bestFit->start);
    }

    fprintf(stderr, "Error: Out of memory\n");
    return NULL;
}

void* allocateWorstFit(MemoryAllocator* allocator, int size) {
    MemoryBlock* current = allocator->head;
    MemoryBlock* worstFit = NULL;

    while (current != NULL) {
        if (!current->allocated && current->size >= size) {
            if (worstFit == NULL || current->size > worstFit->size) {
                worstFit = current;
            }
        }
        current = current->next;
    }

    if (worstFit != NULL) {
        if (worstFit->size > size) {
            MemoryBlock* newBlock = (MemoryBlock*)malloc(sizeof(MemoryBlock));
            if (newBlock == NULL) {
                fprintf(stderr, "Error: Out of memory\n");
                return NULL;
            }
            newBlock->start = worstFit->start + size;
            newBlock->size = worstFit->size - size;
            newBlock->allocated = false;
            newBlock->next = worstFit->next;
            worstFit->next = newBlock;
        }
        worstFit->size = size;
        worstFit->allocated = true;
        return (void*)(long)(worstFit->start);
    }

    fprintf(stderr, "Error: Out of memory\n");
    return NULL;
}

void deallocateMemory(MemoryAllocator* allocator, void* ptr) {
    pthread_mutex_lock(&allocator->lock);
    int start = (int)(long)ptr;
    MemoryBlock* current = allocator->head;

    while (current != NULL) {
        if (current->start == start && current->allocated) {
            current->allocated = false;
            coalesceMemory(allocator);
            pthread_mutex_unlock(&allocator->lock);
            return;
        }
        current = current->next;
    }

    fprintf(stderr, "Error: Invalid pointer\n");
    pthread_mutex_unlock(&allocator->lock);
}

void coalesceMemory(MemoryAllocator* allocator) {
    MemoryBlock* current = allocator->head;

    while (current != NULL && current->next != NULL) {
        if (!current->allocated && !current->next->allocated) {
            MemoryBlock* nextBlock = current->next;
            current->size += nextBlock->size;
            current->next = nextBlock->next;
            free(nextBlock);
        } else {
            current = current->next;
        }
    }
}

void printMemoryBlocks(MemoryAllocator* allocator) {
    MemoryBlock* current = allocator->head;
    while (current != NULL) {
        printf("Block start: %d, size: %d, allocated: %d\n", current->start, current->size, current->allocated);
        current = current->next;
    }
}

void freeMemoryAllocator(MemoryAllocator* allocator) {
    MemoryBlock* current = allocator->head;
    while (current != NULL) {
        MemoryBlock* next = current->next;
        free(current);
        current = next;
    }
    pthread_mutex_destroy(&allocator->lock);
    free(allocator);
}

double measurePerformance(MemoryAllocator* allocator, const char* strategy, int* blockSizes, int numBlocks, int numSizes) {
    clock_t start = clock();
    for (int i = 0; i < numBlocks; i++) {
        int size = blockSizes[i % numSizes];
        void* block = allocateMemory(allocator, size, strategy);
        if (block != NULL) {
            deallocateMemory(allocator, block);
        }
    }
    clock_t end = clock();
    return (double)(end - start) / CLOCKS_PER_SEC;
}

