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

double measurePerformance(MemoryAllocator* allocator, const char* strategy, int blockSize, int numBlocks);

int main() {
    MemoryAllocator* allocator = initializeMemoryAllocator(MEMORY_SIZE);

    printf("Initial memory blocks:\n");
    printMemoryBlocks(allocator);

    double firstFitTime = measurePerformance(allocator, "FirstFit", 50, 100);
    printf("Memory blocks after First Fit allocation:\n");
    printMemoryBlocks(allocator);

    freeMemoryAllocator(allocator);
    allocator = initializeMemoryAllocator(MEMORY_SIZE);

    double bestFitTime = measurePerformance(allocator, "BestFit", 50, 100);
    printf("Memory blocks after Best Fit allocation:\n");
    printMemoryBlocks(allocator);

    freeMemoryAllocator(allocator);
    allocator = initializeMemoryAllocator(MEMORY_SIZE);

    double worstFitTime = measurePerformance(allocator, "WorstFit", 50, 100);
    printf("Memory blocks after Worst Fit allocation:\n");
    printMemoryBlocks(allocator);

    printf("First Fit Allocation Time: %f seconds\n", firstFitTime);
    printf("Best Fit Allocation Time: %f seconds\n", bestFitTime);
    printf("Worst Fit Allocation Time: %f seconds\n", worstFitTime);

    freeMemoryAllocator(allocator);

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

    fprintf(stderr, "Error: Invalid memory deallocation\n");
    pthread_mutex_unlock(&allocator->lock);
}

void coalesceMemory(MemoryAllocator* allocator) {
    MemoryBlock* current = allocator->head;
    while (current != NULL && current->next != NULL) {
        if (!current->allocated && !current->next->allocated) {
            current->size += current->next->size;
            MemoryBlock* temp = current->next;
            current->next = current->next->next;
            free(temp);
        } else {
            current = current->next;
        }
    }
}

void printMemoryBlocks(MemoryAllocator* allocator) {
    MemoryBlock* current = allocator->head;
    while (current != NULL) {
        printf("Block Start: %d, Size: %d, Allocated: %s\n", current->start, current->size, current->allocated ? "Yes" : "No");
        current = current->next;
    }
}

void freeMemoryAllocator(MemoryAllocator* allocator) {
    MemoryBlock* current = allocator->head;
    while (current != NULL) {
        MemoryBlock* temp = current;
        current = current->next;
        free(temp);
    }
    pthread_mutex_destroy(&allocator->lock);
    free(allocator);
}

double measurePerformance(MemoryAllocator* allocator, const char* strategy, int blockSize, int numBlocks) {
    clock_t start, end;
    double cpu_time_used;

    start = clock();
    for (int i = 0; i < numBlocks; i++) {
        void* block = allocateMemory(allocator, blockSize, strategy);
        if (block == NULL) {
            break;
        }
        deallocateMemory(allocator, block);
    }
    end = clock();

    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    return cpu_time_used;
}

