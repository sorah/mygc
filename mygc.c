/*
 * mygc.c
 *
 * Author: Shota Fukumori (sora_h)
 * License: MIT License
 *
 *   (c) Shota Fukumori (sora_h), 2010
 * 
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 * 
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *   THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**** TYPEDEF ****/

typedef struct MYBASIC {
    unsigned long flags;
    struct MYDATA *next_root;
} MYBASIC;

#define TYPE_NONE 0
#define TYPE_OBJ 0x01
typedef struct MYOBJ {
    struct MYBASIC basic;
} MYOBJ;

#define TYPE_CONTAINER 0x02
typedef struct MYCONTAINER {
    struct MYBASIC basic;
    struct MYDATA *data;
    struct MYDATA *next_data;
} MYCONTAINER;

#define TYPE_AB 0x03
typedef struct MYAB {
    struct MYBASIC basic;
    struct MYDATA *a;
    struct MYDATA *b;
} MYAB;

typedef struct MYDATA {
    union {
	struct {
	    struct MYDATA *next;
	    unsigned long flags;
	} free;
	struct MYBASIC basic;
	struct MYOBJ obj;
	struct MYCONTAINER container;
	struct MYAB ab;
    } as;
} MYDATA;

/**** MACROS ****/

#define MBASIC(obj) ((struct MYBASIC*)obj)
#define MCONT(obj)  ((struct MYCONTAINER*)obj)
#define MAB(obj)    ((struct MYAB*)obj)

/**** VARIABLES ****/

#define SLOT_MIN 10000   /* SLOTS in a HEAP */
#define HEAPS_STEP 10    /* HEAPS inclement step */

#define MARK_FLAG (((unsigned long)1)<<5)
#define ROOT_FLAG (((unsigned long)1)<<6)
#define UNROOT_FLAG (((unsigned long)1)<<7)

static int initialized = 0;       /* Count of HEAPS */
static int heaps_length = 0;      /* Count of HEAPS */
static int heaps_used = 0;        /* Count of HEAPS without already used */
static int heap_slots = SLOT_MIN; /* When add heap to HEAPS, it will be 1.8x */
static int *heap_lengths = NULL;  /* Lengths of HEAP in HEAPS */
static MYDATA **heaps = NULL;     /* HEAPS */

static MYDATA *freelist = NULL;
static MYDATA *rootlist = NULL;


void mygc_gc(void);
static void fatal_error(char *err);

/**** HEAPS ****/

static void
alloc_heaps(void) {
    heaps_length += HEAPS_STEP;
    if(heaps == NULL) {
	heaps = malloc(sizeof(MYDATA*) * heaps_length);
	heap_lengths = malloc(sizeof(int) * heaps_length);
    } else {
	heaps = realloc(heaps, sizeof(MYDATA*) * heaps_length);
	heap_lengths = realloc(heap_lengths, sizeof(int) * heaps_length);
    }
    if(heaps == NULL || heap_lengths == NULL)
	fatal_error("can't allocate heaps or heap_lengths at alloc_heaps()");
}

static void
add_heap(void) {
    MYDATA *pf, *pe;

    if(heaps_length == heaps_used)
	alloc_heaps();
    
    pf = heaps[heaps_used] = malloc(sizeof(MYDATA) * heap_slots);
    if(pf == NULL)
	fatal_error("can't allocate heap on HEAPS at add_heap()");
    heap_lengths[heaps_used] = heap_slots;
    pe = pf + heap_slots;

    heaps_used++;
    heap_slots *= 1.8;

    while(pf < pe) {
	pf->as.free.flags = 0;
	pf->as.free.next = freelist;
	freelist = pf;
	pf++;
    }
}

/**** ROOT ****/

void
mygc_add_root(MYDATA* d) {
    d->as.basic.flags |= ROOT_FLAG;
    d->as.basic.next_root = rootlist;
    rootlist = d;
}

void
mygc_remove_root(MYDATA* d) {
    MYDATA *p, *k;
    MBASIC(d)->flags &= ~ROOT_FLAG;
    MBASIC(d)->flags |= UNROOT_FLAG;

    p = rootlist;
    while(p) {
	if(MBASIC(p)->flags & UNROOT_FLAG) {
	    MBASIC(k)->next_root = MBASIC(p)->next_root;
	}else{
	    k = p;
	}
	p = MBASIC(p)->next_root;
    }
}
/**** FREE & ALLOC ****/

MYDATA*
mygc_alloc(int type) {
    MYDATA* obj;

    if(!freelist) mygc_gc();
    if(!freelist) add_heap();
    if(!freelist)
	fatal_error("gced but...");

    obj = freelist;
    freelist = freelist->as.free.next;

    memset((void*)obj, 0, sizeof(MYDATA));

    MBASIC(obj)->flags = type;

    return obj;
}

void
mygc_free(MYDATA* d) {
    d->as.free.next = freelist;
    freelist = d;
}

/**** MARK & SWEEP ****/
static void
gc_mark_m(MYDATA* d) {
    MBASIC(d)->flags |= MARK_FLAG;
    switch(MBASIC(d)->flags & 0x1f) {
	case TYPE_CONTAINER:
	    gc_mark_m(MCONT(d)->data);
	    gc_mark_m(MCONT(d)->next_data);
	    break;
	case TYPE_AB:
	    gc_mark_m(MAB(d)->a);
	    gc_mark_m(MAB(d)->b);
	    break;
	case TYPE_OBJ:
	default:
	    break;
    }
}

static void
gc_mark(void) {
    MYDATA *p;
    p = rootlist;
    while(p) {
	gc_mark_m(p);
	p = MBASIC(p)->next_root;
    }
}

static void
gc_sweep(void) {
    MYDATA* d;
    int i,j;
    for(i=0; i < heaps_used; i++) {
	for(j=0; j < heap_lengths[i]; j++) {
	    d = &heaps[i][j];
	    if(MBASIC(d)->flags != 0 && \
	       MBASIC(d)->flags & MARK_FLAG) {
		MBASIC(d)->flags &= ~MARK_FLAG;
	    }else{
		mygc_free(d);
	    }
	}
    }
    /*int i,j;
    for(i=0; i < heaps_used; i++) {
	for(j=0; j < heap_lengths[i]; j++) {
	    if(MBASIC(heaps[i][j])->flags != 0 && \
	       MBASIC(heaps[i][j])->flags & MARK_FLAG) {
		MBASIC(heaps[i][j])->flags &= ~MARK_FLAG;
	    }else{
		mygc_free(heaps[i][j]);
	    }
	}
    }*/
}

void
mygc_gc(void) {
    gc_mark();
    gc_sweep();
}

/**** INIT & FINAL ****/

void
mygc_init(void) {
    if(initialized == 1)
	return;
    alloc_heaps();
    add_heap();
    initialized = 1;
}

void
mygc_final(void) {
    int i;
    for(i=0; i < heaps_used; i++) {
	free(heaps[i]);
    }
    free(heaps);
}


/**** MISC ****/

static void
fatal_error(char* err) {
    fprintf(stderr, "Fatal: %s", err);
    mygc_final();
    exit(1);
}

/**** TEST + MAIN ****/

static void
test(void) {
    mygc_init();
    mygc_final();
}

int
main(void) {
    test();
    return 0;
}
