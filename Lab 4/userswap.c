#include "userswap.h"
#include <sys/mman.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>

#define DIRTY 2
#define EVICTED 3
#define RESIDENT 1
#define INVALID -1
#define NON_RESIDENT 0
#define PAGE_SIZE 4096
#define HAS_NO_INDEX -3
#define NUM_OF_ENTRIES 512
#define UNUSED(x) (void)(x)
#define LORM_DEFAULT_VALUE 8626176
#define ROUND_UP(SIZE) SIZE % PAGE_SIZE != 0 ? ceil(SIZE / PAGE_SIZE) * PAGE_SIZE : SIZE;
#define ALLOCATE_MEMORY(SIZE) mmap(NULL, SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

typedef struct Entry {
    int state;
    int offset;
    struct Node* queue_node;
} Entry;

typedef struct Node {
    void* addr;
    size_t size;
    int file_fd;
    struct Node* next;
    struct Node* prev;
} Node;

typedef struct List {
    size_t size;
    struct Node* head;
    struct Node* tail;
} List;

int swap_fd = INVALID;
int swap_fd_counter = 0;
Entry**** level4[NUM_OF_ENTRIES];
size_t LORM = LORM_DEFAULT_VALUE;
List freed_offsets = {0, NULL, NULL};
List eviction_queue = {0, NULL, NULL};
List allocation_list = {0, NULL, NULL};


void enqueue(List* list, Node* node) {

    if (list -> size == 0) {
        list -> head = node;
    }

    else {
        node -> prev = list -> tail;
        list -> tail -> next = node;
    }

    list -> tail = node;
    list -> size += 1;
}

void* dequeue(List* list) {

    if (list -> size > 0) {

        Node* node = list -> head;

        void*   addr = node -> addr;
        list -> head = node -> next;
        list -> size -= 1;

        if (list -> size == 0) {
            list -> head = NULL;
            list -> tail = NULL;
        }

        else {
            list -> head -> prev = NULL;
        }

        free(node);
        return addr;
    }

    else {
        return NULL;
    }
}

int dequeue2(List* list) {

    if (list -> size > 0) {

        Node* node = list -> head;

        int   offset = node -> size;
        list -> head = node -> next;
        list -> size -= 1;

        if (list -> size == 0) {
            list -> head = NULL;
            list -> tail = NULL;
        }

        else {
            list -> head -> prev = NULL;
        }

        free(node);
        return offset;
    }

    else {
        return INVALID;
    }
}

void* delete_node_at(List* list, Node* node) {

    if (node -> next == NULL && node -> prev == NULL) {
        printf("A\n");
        return dequeue(list);
    }

    else if (node -> next != NULL && node -> prev == NULL) {
        return dequeue(list);
    }

    else if (node -> next == NULL && node -> prev != NULL) {
        Node* ptr = node -> addr;
        list -> tail = list -> tail -> prev;
        list -> tail -> next = NULL;
        free(node);
        list -> size -= 1;
        return ptr;
    }

    else {
        Node* ptr = node -> addr;
        node -> prev -> next = node -> next;
        node -> next -> prev = node -> prev;
        free(node);
        list -> size -= 1;
        return ptr;
    }
}

Node* create_node(void* addr, size_t size, int file_fd) {
    Node* node = (Node*) malloc(sizeof(Node));
    node -> next = NULL;
    node -> prev = NULL;
    node -> addr = addr;
    node -> size = size;
    node -> file_fd = (file_fd == INVALID) ? INVALID : file_fd;
    return node;
}


//-----------------------------------------------------------------------------

Entry* page_table(int ref_4, int ref_3, int ref_2, int ref_1) {

    Entry**** l3 = level4[ref_4];
    if (l3 == NULL) {
        l3 = calloc(NUM_OF_ENTRIES, sizeof(Entry****));
        level4[ref_4] = l3;
    }

    Entry*** l2 = l3[ref_3];
    if (l2 == NULL) {
        l2 = calloc(NUM_OF_ENTRIES, sizeof(Entry***));
        l3[ref_3] = l2;
    }

    Entry** l1 = l2[ref_2];
    if (l1 == NULL) {
        l1 = calloc(NUM_OF_ENTRIES, sizeof(Entry**));
        l2[ref_2] = l1;
    }

    Entry* l0 = l1[ref_1];
    if (l0 == NULL) {
        l0 = (Entry*) malloc(sizeof(Entry));
        l0 -> state = NON_RESIDENT;
        l0 -> offset = HAS_NO_INDEX;
        l0 -> queue_node = NULL;
        l1[ref_1] = l0;
    }

    return l0;
}

void exceed_lorm(Node* node) {
    while (eviction_queue.size * PAGE_SIZE > LORM) {

        void* addr = dequeue(&eviction_queue);
        int l4 = ((uintptr_t) addr >> 39) & 0x1FF;
        int l3 = ((uintptr_t) addr >> 30) & 0x1FF;
        int l2 = ((uintptr_t) addr >> 21) & 0x1FF;
        int l1 = ((uintptr_t) addr >> 12) & 0x1FF;
        Entry* entry = level4[l4][l3][l2][l1];

        if (entry -> state == DIRTY) {
            if (node -> file_fd != INVALID) {
                if (pwrite(node -> file_fd, addr, PAGE_SIZE, entry -> offset) == -1) perror("pwrite");
            }

            else {
                if (entry -> offset == HAS_NO_INDEX) {
                    int offset = dequeue2(&freed_offsets);

                    if (offset == INVALID) {
                        entry -> offset = swap_fd_counter;
                        swap_fd_counter += PAGE_SIZE;
                    }

                    else {
                        entry -> offset = offset;
                    }
                }
                    
                if (pwrite(swap_fd, addr, PAGE_SIZE, entry -> offset) == -1) perror("pwrite");
            }
        }

        entry -> state = EVICTED;
        entry -> queue_node = NULL;
        mprotect(addr, PAGE_SIZE, PROT_NONE);
        madvise(addr, PAGE_SIZE, MADV_DONTNEED);
    }
}

void page_fault_handler(Node* mem_node, void* addr) {

    int l4 = ((uintptr_t) addr >> 39) & 0x1FF;
    int l3 = ((uintptr_t) addr >> 30) & 0x1FF;
    int l2 = ((uintptr_t) addr >> 21) & 0x1FF;
    int l1 = ((uintptr_t) addr >> 12) & 0x1FF;
    Entry* entry = page_table(l4, l3, l2, l1);

    if (entry -> state == NON_RESIDENT) {
        exceed_lorm(mem_node);
        entry -> state = RESIDENT;

        if (mem_node -> file_fd != INVALID) {
            mprotect(addr, PAGE_SIZE, PROT_WRITE);
            entry -> offset = addr - (mem_node -> addr);
            if (pread(mem_node -> file_fd, addr, PAGE_SIZE, entry -> offset) == -1) perror("pread");
        }

        mprotect(addr, PAGE_SIZE, PROT_READ);

        Node* queue_node = create_node(addr, PAGE_SIZE, INVALID);
        entry -> queue_node = queue_node;
        enqueue(&eviction_queue, queue_node);
    }

    else if (entry -> state == RESIDENT) {
        entry -> state = DIRTY;
        mprotect(addr, PAGE_SIZE, PROT_READ | PROT_WRITE);
    }

    else if (entry -> state == EVICTED) {
        exceed_lorm(mem_node);
        entry -> state = RESIDENT;

        if (entry -> offset != HAS_NO_INDEX) {
            mprotect(addr, PAGE_SIZE, PROT_WRITE);
            int fd = (mem_node -> file_fd != INVALID) ? mem_node -> file_fd : swap_fd;
            if (pread(fd, addr, PAGE_SIZE, entry -> offset) == -1) perror("pread");
        }

        mprotect(addr, PAGE_SIZE, PROT_READ);

        Node* queue_node = create_node(addr, PAGE_SIZE, INVALID);
        entry -> queue_node = queue_node;
        enqueue(&eviction_queue, queue_node);
    }
}

void sigsegv_handler(int sig, siginfo_t *info, void *ucontext) {

    UNUSED(ucontext);

    if (sig == SIGSEGV) {

        Node* node_to_send = NULL;
        Node* node_ptr = allocation_list.head;

        while (node_ptr != NULL) {
            
            uintptr_t base  = (uintptr_t) node_ptr -> addr;
            uintptr_t limit = (uintptr_t) node_ptr -> size + base;
            uintptr_t check = (uintptr_t) info -> si_addr;

            if (check >= base && check < limit) {
                node_to_send = node_ptr;
                break;
            }

            node_ptr = node_ptr -> next;
        }

        if (node_to_send != NULL)
            page_fault_handler(node_to_send, info -> si_addr);

        else
            if (signal(SIGSEGV, SIG_DFL) == SIG_ERR) perror("signal");
    }
}

void sigaction_init() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sigsegv_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) perror("sigaction");
}


//---------------------------------------------------------

int swap_fd_init() {

    char pid[10];
    char cwd[PATH_MAX];
    char* slash = "/";
    char* swap = ".swap";

    if (getcwd(cwd, sizeof(cwd)) == NULL) perror("getcwd");
    strcat(cwd, slash);
    snprintf(pid, 10, "%d", (int) getpid());
    strcat(cwd, pid);
    strcat(cwd, swap);

    int fd = open(cwd, O_RDWR|O_CREAT|O_TRUNC, 0666);
    if (fd == -1) perror("open");
    return fd;
}

void userswap_set_size(size_t size) {
    LORM = ROUND_UP(size);
}

void* userswap_alloc(size_t size) {
    sigaction_init();
    size_t alloc_size = ROUND_UP(size);
    void* addr = ALLOCATE_MEMORY(alloc_size);
    if (swap_fd == INVALID) swap_fd = swap_fd_init();
    enqueue(&allocation_list, create_node(addr, alloc_size, INVALID));
    return addr;
}

void* userswap_map(int fd, size_t size) {

    sigaction_init();
    size_t alloc_size = ROUND_UP(size);

    struct stat buf;
    if (fstat(fd, &buf) == -1) perror("fstat");

    if ((size_t) buf.st_size < alloc_size)
        if (ftruncate(fd, alloc_size) == -1) perror("ftruncate");

    void* addr = ALLOCATE_MEMORY(alloc_size);
    enqueue(&allocation_list, create_node(addr, alloc_size, fd));
    return addr;
}

void userswap_free(void *mem) {

    Node* node = NULL;
    Node* node_ptr = allocation_list.head;

    while (node_ptr != NULL) {
        if (node_ptr -> addr == mem) {
            node = node_ptr;
            break;
        }

        node_ptr = node_ptr -> next;
    }

    if (node == NULL) return;

    for (size_t i = 0; i < (node -> size / PAGE_SIZE); i++) {

        void* addr = node -> addr + PAGE_SIZE * i;
        int l4 = ((uintptr_t) addr >> 39) & 0x1FF;
        int l3 = ((uintptr_t) addr >> 30) & 0x1FF;
        int l2 = ((uintptr_t) addr >> 21) & 0x1FF;
        int l1 = ((uintptr_t) addr >> 12) & 0x1FF;
        Entry* entry = level4[l4][l3][l2][l1];

        if (entry != NULL) {
            if (entry -> state == DIRTY && node -> file_fd != INVALID)
                if (pwrite(node -> file_fd, addr, PAGE_SIZE, entry -> offset) == -1) perror("pwrite");

            if (entry -> state == RESIDENT || entry -> state == DIRTY)
                delete_node_at(&eviction_queue, entry -> queue_node);

            if (node -> file_fd == INVALID && entry -> offset != HAS_NO_INDEX)
                enqueue(&freed_offsets, create_node(NULL, entry -> offset, INVALID));

            free(entry);
            level4[l4][l3][l2][l1] = NULL;
        }
    }

    munmap(node -> addr, node -> size);
    delete_node_at(&allocation_list, node);
}