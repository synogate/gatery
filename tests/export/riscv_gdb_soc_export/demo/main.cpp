#include "../firmware/gen_mmap.h"
#include <errno.h>

struct Tile
{
    unsigned char color[4];
    unsigned int index;
};
struct Tile g_tilemap[31][112];

extern "C" __attribute__((used))
void* _sbrk(int incr)
{
    extern char _end;
    static char* heap_end = &_end;

    char* ret = heap_end;
    heap_end += incr;
    if(heap_end - &_end > 1024*1024)
    {
        errno = ENOMEM;
        return (void*)-1;
    }
    return ret;
}

int main()
{
    dev::root().display.framebuffer = (unsigned)g_tilemap;
    dev::root().display.backlight_level = 192;
    dev::root().display.enable = 1;

    for(struct Tile* it = &g_tilemap[0][0]; it != &g_tilemap[0][0] + sizeof(g_tilemap)/sizeof(struct Tile); ++it)
    {
        it->color[0] = 0;
        it->color[1] = 240;
        it->color[2] = 249;
        it->color[3] = 15;
        it->index = 0;
    }

    for(int i = 0; i < 100; ++i)
        g_tilemap[0][i].index = '0' + i % 10;
    for(int i = 0; i < 100; ++i)
        g_tilemap[28][i].index = 'a' + i % 26;
    for(int i = 0; i < 100; ++i)
        g_tilemap[29][i].index = 'A' + i % 26;
}

#if 0

#pragma GCC push_options
#pragma GCC optimize ("O0")

void _start()
{
	asm volatile(
		"mv t0, %0;"
		"jr t0;"
		: : "r" (0x09000000)
	);
	asm volatile(
        ".option push;"
        ".option norelax;"
        "la gp, __global_pointer$;"
        ".option pop;"
		: : 
		);

	main();
}

#pragma GCC pop_options

__attribute__((optimize("no-tree-loop-distribute-patterns")))
__attribute__((used))
void* memcpy ( void * destination, const void * source, unsigned num )
{
	char* dst = destination;
	const char* src = source;
	for(char* it = dst; it != dst + num;)
		*it++ = *src++;
	return destination;
}

// int math

// tested by checking all inputs
static unsigned int countLeadingZeros(unsigned int v)
{
    if (v == 0)
        return 32;

    unsigned int res = 0;
    if((v & 0xFFFF0000) == 0) { res += 16; v <<= 16; }
    if((v & 0xFF000000) == 0) { res += 8; v <<= 8; }
    if((v & 0xF0000000) == 0) { res += 4; v <<= 4; }
    if((v & 0xC0000000) == 0) { res += 2; v <<= 2; }
    if((v & 0x80000000) == 0) { res += 1; v <<= 1; }
    return res;
}

// tested by fuzzing
unsigned int __udivsi3(unsigned int N, unsigned int D)
{
    unsigned int Q = 0;
    unsigned int R = N;

    int i_start = countLeadingZeros(D);
    for(int i = i_start; i >= 0; i--) {
        Q <<= 1;

        unsigned int DShift = (D << i);
        if(R >= DShift) {
            Q |= 1;
            R -= DShift;
        }
    }
    return Q;
}

// tested by fuzzing
unsigned int __umodsi3(unsigned int N, unsigned int D)
{
    return N - __udivsi3(N, D) * D;
}

// tested by fuzzing
__attribute__((used)) int __divsi3(int N, int D)
{
    int change_sign = N ^ D;
    if(N < 0) N = -N;
    if(D < 0) D = -D;

    unsigned int Q = 0;
    unsigned int R = N;

    int i_start = countLeadingZeros(D);
    for(int i = i_start; i >= 0; i--) {
        Q <<= 1;

        unsigned int DShift = (D << i);
        if(R >= DShift) {
            Q |= 1;
            R -= DShift;
        }
    }

    if((change_sign & 0x80000000) != 0)
        Q = ~Q + 1;
    return Q;
}

// tested by fuzzing
__attribute__((used)) int __modsi3(int N, int D)
{
    return N - __divsi3(N, D) * D;
}

// tested by fuzzing
static unsigned int __umulsi3(unsigned int a, unsigned int b)
{
    unsigned int r = 0;

    while (a)
    {
        if (a & 1)
            r += b;
        a >>= 1;
        b <<= 1;
    }
    return r;
}

// tested by fuzzing
// used in f_wipe only
__attribute__((used)) int __mulsi3(int a, int b)
{
    unsigned int change_sign = a ^ b;
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    unsigned int r = __umulsi3(a, b);
    if (change_sign & 0x80000000)
        r = ~r + 1;
    return (int)r;
}

typedef union {
    long long full;
    struct {
       unsigned long low;
       long high; 
    } s;
} WORD64;

long long __lshrdi3(long long a, int b)
{
    if (b == 0)
        return a;

    WORD64 result;
    WORD64 in;
    in.full = a;

    if ((b & 32))
    {
        result.s.high = in.s.high >> 31;
        result.s.low = in.s.high >> b;
    }
    else
    {
        result.s.high = in.s.high >> b;
        result.s.low = (in.s.high << (64 - b)) | (in.s.low >> b);
    }
    return result.full;
}

#if 0 // unsued functions
unsigned long long __umuldi3(unsigned long long a, unsigned long long b)
{
    unsigned long long r = 0;

    while (a)
    {
        if (a & 1)
            r += b;
        a >>= 1;
        b <<= 1;
    }
    return r;
}


long long __muldi3(long long a, long long b)
{
    unsigned long long change_sign = a ^ b;
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    unsigned long long r = __umuldi3(a, b);
    if (change_sign & 0x8000000000000000ll)
        r = -r;
    return r;
}

static unsigned int countLeadingZerosD(unsigned long long v)
{
    unsigned int res = 0;
    if ((v & 0xFFFFFFFF00000000ll) == 0) { res += 32; v <<= 32; }
    if ((v & 0xFFFF000000000000ll) == 0) { res += 16; v <<= 16; }
    if ((v & 0xFF00000000000000ll) == 0) { res += 8; v <<= 8; }
    if ((v & 0xF000000000000000ll) == 0) { res += 4; v <<= 4; }
    if ((v & 0xC000000000000000ll) == 0) { res += 2; v <<= 2; }
    if ((v & 0x8000000000000000ll) == 0) { res += 1; v <<= 1; }
    return res;
}

long long __divdi3(long long N, long long D)
{
    long long change_sign = N ^ D;
    if (N < 0) N = -N;
    if (D < 0) D = -D;

    unsigned long long Q = 0;
    unsigned long long R = N;

    int i_start = countLeadingZerosD(D);
    for (int i = i_start; i >= 0; i--) {
        Q <<= 1;

        unsigned long long DShift = (D << i);
        if (R >= DShift) {
            Q |= 1;
            R -= DShift;
        }
    }

    if ((change_sign & 0x8000000000000000ll) != 0)
        Q = -Q;
    return Q;
}

long long __ashldi3(long long a, int b) {
    if (b == 0)
        return a;
    unsigned int a_low = (unsigned int) a;
    unsigned int a_high = (unsigned int) (a >> 32);
    unsigned int shift_over = a_low >> (32 - b);
    
    unsigned int result_low = a_low << b;
    unsigned int result_high = (a_high << b) | shift_over;

    return result_low | ((long long)result_high << 32);
}

#endif

#endif