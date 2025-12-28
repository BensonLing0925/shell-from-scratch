// arena_test.c
// Build (example):
//   gcc -std=c11 -Wall -Wextra -O0 -g arena.c arena_test.c -o arena_test
// If you have ARENA_DEBUG features:
//   gcc -std=c11 -Wall -Wextra -O0 -g -DARENA_DEBUG arena.c arena_test.c -o arena_test

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdalign.h>

#include "arena.h"

// ---- helpers ----
static void check_aligned(const void *p, size_t align, const char *msg) {
    uintptr_t v = (uintptr_t)p;
    if ((v & (align - 1)) != 0) {
        fprintf(stderr, "[FAIL] %s: ptr=%p align=%zu\n", msg, p, align);
        abort();
    }
}

static void fill_bytes(void *p, size_t n, unsigned char val) {
    memset(p, val, n);
}

static void expect_bytes(const void *p, size_t n, unsigned char val, const char *msg) {
    const unsigned char *b = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++) {
        if (b[i] != val) {
            fprintf(stderr, "[FAIL] %s: mismatch at i=%zu got=0x%02X expected=0x%02X\n",
                    msg, i, b[i], val);
            abort();
        }
    }
}

// ---- tests ----
static void test_alignment_basic(struct arena *a) {
    puts("[TEST] alignment_basic");

    // power-of-two aligns; you can add more
    size_t maxa = alignof(max_align_t);
    printf("max align: %zu\n", maxa);
    const size_t aligns[] = { 1, 2, 4, 8, maxa };
    for (size_t i = 0; i < sizeof(aligns)/sizeof(aligns[0]); i++) {
        size_t al = aligns[i];
        void *p = arena_alloc_align(a, 13, al);
        assert(p != NULL);
        // Your arena may bump align to sizeof(void*), so 1/2/4 may come back at ptr-alignment >= sizeof(void*)
        // Still: it MUST be aligned to max(al, sizeof(void*)) effectively.
        size_t effective = al;
        if (effective < sizeof(void*)) effective = sizeof(void*);
        check_aligned(p, effective, "arena_alloc_align");
    }

    puts("  OK");
}

static void test_non_overlapping(struct arena *a) {
    puts("[TEST] non_overlapping");

    // Allocate a bunch of small chunks and ensure they don't overlap.
    enum { N = 2000 };
    void *ptrs[N];
    size_t sizes[N];

    for (int i = 0; i < N; i++) {
        sizes[i] = (size_t)(1 + (i % 37)); // 1..37 bytes
        ptrs[i] = arena_alloc_align(a, sizes[i], 8);
        assert(ptrs[i] != NULL);
        check_aligned(ptrs[i], 8 < sizeof(void*) ? sizeof(void*) : 8, "ptr alignment");
        fill_bytes(ptrs[i], sizes[i], (unsigned char)(i & 0xFF));
    }

    // Spot-check a few earlier buffers stayed intact
    for (int i = 0; i < N; i += 199) {
        expect_bytes(ptrs[i], sizes[i], (unsigned char)(i & 0xFF), "buffer intact");
    }

    puts("  OK");
}

static void test_cross_block(struct arena *a) {
    puts("[TEST] cross_block");

    // Force allocator to allocate multiple blocks:
    // allocate a big chunk close to (or bigger than) BLOCK_SIZE to trigger new block.
    // If your BLOCK_SIZE is 64KB, 80KB definitely triggers.
    size_t big = (size_t)(80u * 1024u);
    void *p1 = arena_alloc_align(a, big, 16);
    assert(p1 != NULL);
    check_aligned(p1, 16 < sizeof(void*) ? sizeof(void*) : 16, "big alloc");

    void *p2 = arena_alloc_align(a, 1024, 16);
    assert(p2 != NULL);
    check_aligned(p2, 16 < sizeof(void*) ? sizeof(void*) : 16, "alloc after big");

    // Write to ensure no segfault and memory is accessible
    fill_bytes(p1, 64, 0xAA);
    fill_bytes(p2, 64, 0xBB);

    expect_bytes(p1, 64, 0xAA, "p1 accessible");
    expect_bytes(p2, 64, 0xBB, "p2 accessible");

    puts("  OK");
}

static void test_reset(struct arena *a) {
    puts("[TEST] reset");

    // Allocate something, remember pointer, reset, allocate again
    void *p1 = arena_alloc_align(a, 256, 16);
    assert(p1 != NULL);
    fill_bytes(p1, 256, 0x11);

    arena_reset(a);

    void *p2 = arena_alloc_align(a, 256, 16);
    assert(p2 != NULL);

    // Not guaranteed by spec, but in common bump arenas reset causes first alloc to return same address.
    // We'll treat "same pointer" as a nice-to-have and print it.
    printf("  after reset: p1=%p p2=%p (may be same)\n", p1, p2);

    // Write and check it doesn't crash
    fill_bytes(p2, 256, 0x22);
    expect_bytes(p2, 256, 0x22, "p2 writable");

    puts("  OK");
}

static void test_strdup_if_available(struct arena *a) {
    // If your arena.h provides arena_strdup, keep this.
    // If not, comment out this test.
    puts("[TEST] strdup (if implemented)");

    const char *s = "hello arena";
    char *p = arena_strdup(a, s);
    assert(p != NULL);
    assert(strcmp(p, s) == 0);

    puts("  OK");
}

static void test_calloc_if_available(struct arena *a) {
    // Only if you implemented a real calloc(count,size).
    // If your current arena_calloc signature differs, adjust or comment out.
    puts("[TEST] calloc (if implemented)");

    // Example assumes: void* arena_calloc(struct arena* a, size_t count, size_t size);
    // If your arena_calloc is different, comment this out.
    void *p = arena_calloc(a, 128, 4);
    assert(p != NULL);
    // should be all zeros
    const unsigned char *b = (const unsigned char*)p;
    for (size_t i = 0; i < 128 * 4; i++) {
        if (b[i] != 0) {
            fprintf(stderr, "[FAIL] calloc not zeroed at i=%zu\n", i);
            abort();
        }
    }
    puts("  OK");
}

int main(void) {
    struct arena a;
    arena_init(&a);

    // Run tests on a fresh arena
    test_alignment_basic(&a);
    arena_reset(&a);

    test_non_overlapping(&a);
    arena_reset(&a);

    test_cross_block(&a);
    arena_reset(&a);

    test_reset(&a);
    arena_reset(&a);

    // Optional tests: comment out if you don't have these APIs
    test_strdup_if_available(&a);
    arena_reset(&a);

    // If your arena_calloc signature differs, comment out.
    test_calloc_if_available(&a);
    arena_reset(&a);

    arena_destroy(&a);
    puts("\nAll arena tests passed.");
    return 0;
}

