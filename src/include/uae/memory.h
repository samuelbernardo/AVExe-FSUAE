/*
* UAE - The Un*x Amiga Emulator
*
* memory management
*
* Copyright 1995 Bernd Schmidt
*/

extern void memory_reset (void); //TODO memory.cpp*
extern void a1000_reset (void);

#ifdef JIT
extern int special_mem;
#define S_READ 1
#define S_WRITE 2

extern uae_u8 *cache_alloc (int);
extern void cache_free (uae_u8*);
#endif

bool init_shm (void);
void free_shm (void);
bool preinit_shm (void);
extern bool canbang;
extern int candirect;

#ifdef ADDRESS_SPACE_24BIT
#define MEMORY_BANKS 256
#define MEMORY_RANGE_MASK ((1<<24)-1)
#else
#define MEMORY_BANKS 65536
#define MEMORY_RANGE_MASK (~0)
#endif

typedef uae_u32 (REGPARAM3 *mem_get_func)(uaecptr) REGPARAM;
typedef void (REGPARAM3 *mem_put_func)(uaecptr, uae_u32) REGPARAM;
typedef uae_u8 *(REGPARAM3 *xlate_func)(uaecptr) REGPARAM;
typedef int (REGPARAM3 *check_func)(uaecptr, uae_u32) REGPARAM;

extern uae_u8 *address_space, *good_address_map;
extern uae_u8 *chipmemory;

extern uae_u32 allocated_chipmem;
extern uae_u32 allocated_fastmem;
extern uae_u32 allocated_bogomem;
extern uae_u32 allocated_gfxmem;
extern uae_u32 allocated_z3fastmem, allocated_z3fastmem2, allocated_z3chipmem;
extern uae_u32 max_z3fastmem;
extern uae_u32 allocated_a3000mem;
extern uae_u32 allocated_cardmem;

extern uae_u32 wait_cpu_cycle_read (uaecptr addr, int mode);
extern void wait_cpu_cycle_write (uaecptr addr, int mode, uae_u32 v);
extern uae_u32 wait_cpu_cycle_read_ce020 (uaecptr addr, int mode);
extern void wait_cpu_cycle_write_ce020 (uaecptr addr, int mode, uae_u32 v);

#undef DIRECT_MEMFUNCS_SUCCESSFUL
#include "machdep/maccess.h"

#define chipmem_start 0x00000000
#define bogomem_start 0x00C00000
#define cardmem_start 0x00E00000
#define kickmem_start 0x00F80000
extern uaecptr z3fastmem_start, z3fastmem2_start, z3chipmem_start;
extern uaecptr p96ram_start;
extern uaecptr fastmem_start;
extern uaecptr a3000lmem_start, a3000hmem_start;

extern bool ersatzkickfile;
extern bool cloanto_rom, kickstart_rom;
extern uae_u16 kickstart_version;
extern bool uae_boot_rom;
extern int uae_boot_rom_size;
extern uaecptr rtarea_base;

extern uae_u8* baseaddr[];

enum { ABFLAG_UNK = 0, ABFLAG_RAM = 1, ABFLAG_ROM = 2, ABFLAG_ROMIN = 4, ABFLAG_IO = 8, ABFLAG_NONE = 16, ABFLAG_SAFE = 32 };
typedef struct {
	/* These ones should be self-explanatory... */
	mem_get_func lget, wget, bget;
	mem_put_func lput, wput, bput;
	/* Use xlateaddr to translate an Amiga address to a uae_u8 * that can
	* be used to address memory without calling the wget/wput functions.
	* This doesn't work for all memory banks, so this function may call
	* abort(). */
	xlate_func xlateaddr;
	/* To prevent calls to abort(), use check before calling xlateaddr.
	* It checks not only that the memory bank can do xlateaddr, but also
	* that the pointer points to an area of at least the specified size.
	* This is used for example to translate bitplane pointers in custom.c */
	check_func check;
	/* For those banks that refer to real memory, we can save the whole trouble
	of going through function calls, and instead simply grab the memory
	ourselves. This holds the memory address where the start of memory is
	for this particular bank. */
	uae_u8 *baseaddr;
	const TCHAR *name;
	/* for instruction opcode/operand fetches */
	mem_get_func lgeti, wgeti;
	int flags;
} addrbank;

#define CE_MEMBANK_FAST 0
#define CE_MEMBANK_CHIP 1
#define CE_MEMBANK_CIA 2
#define CE_MEMBANK_FAST16BIT 3
extern uae_u8 ce_banktype[65536], ce_cachable[65536];

extern uae_u8 *filesysory;
extern uae_u8 *rtarea;

extern addrbank chipmem_bank;
extern addrbank chipmem_agnus_bank;
extern addrbank chipmem_bank_ce2;
extern addrbank kickmem_bank;
extern addrbank custom_bank;
extern addrbank clock_bank;
extern addrbank cia_bank;
extern addrbank rtarea_bank;
extern addrbank expamem_bank;
extern addrbank fastmem_bank;
extern addrbank gfxmem_bank;
extern addrbank gayle_bank;
extern addrbank gayle2_bank;
extern addrbank mbres_bank;
extern addrbank akiko_bank;
extern addrbank cardmem_bank;

extern void rtarea_init (void);
extern void rtarea_init_mem (void);
extern void rtarea_setup (void);
extern void expamem_init (void);
extern void expamem_reset (void);
extern void expamem_next (void);

extern uae_u32 gfxmem_start;
extern uae_u8 *gfxmemory;
extern uae_u32 gfxmem_mask;
extern uae_u16 last_custom_value1;

/* Default memory access functions */

extern int REGPARAM3 default_check(uaecptr addr, uae_u32 size) REGPARAM; //TODO memory.cpp
extern uae_u8 *REGPARAM3 default_xlate(uaecptr addr) REGPARAM; //TODO memory.cpp
/* 680x0 opcode fetches */
extern uae_u32 REGPARAM3 dummy_lgeti (uaecptr addr) REGPARAM; //TODO memory.cpp
extern uae_u32 REGPARAM3 dummy_wgeti (uaecptr addr) REGPARAM; //TODO memory.cpp

#define bankindex(addr) (((uaecptr)(addr)) >> 16) //TODO may be important

//extern addrbank *mem_banks[MEMORY_BANKS];//not used directly out of memory.cpp, no neet to wrap it

#ifdef JIT
extern uae_u8 *baseaddr[MEMORY_BANKS];
#endif
extern addrbank avex_get_membank(uaecptr addr);
#define get_mem_bank(addr) (avex_get_membank(bankindex(addr))) //TODO this is how mem_banks is accessed from the outside, needs wrapper

#ifdef JIT
#define put_mem_bank(addr, b, realstart) do { \
	(mem_banks[bankindex(addr)] = (b)); \
	if ((b)->baseaddr) \
	baseaddr[bankindex(addr)] = (b)->baseaddr - (realstart); \
	else \
	baseaddr[bankindex(addr)] = (uae_u8*)(((uae_u8*)b)+1); \
} while (0)
#else
#define put_mem_bank(addr, b, realstart) \
	(mem_banks[bankindex(addr)] = (b)); //TODO mem write
#endif

extern void memory_init (void); //TODO memory.cpp*
extern void memory_cleanup (void); //TODO memory.cpp*
extern void map_banks (addrbank *bank, int first, int count, int realsize); //TODO memory.cpp*
extern void map_overlay (int chip); //TODO memory.cpp*
extern void memory_hardreset (int); //TODO memory.cpp*
extern void memory_clear (void); //TODO memory.cpp*
extern void free_fastmemory (void); //TODO memory.cpp

#define longget(addr) (call_mem_get_func(get_mem_bank(addr).lget, addr))
#define wordget(addr) (call_mem_get_func(get_mem_bank(addr).wget, addr))
#define byteget(addr) (call_mem_get_func(get_mem_bank(addr).bget, addr))
#define longgeti(addr) (call_mem_get_func(get_mem_bank(addr).lgeti, addr))
#define wordgeti(addr) (call_mem_get_func(get_mem_bank(addr).wgeti, addr))
#define longput(addr,l) (call_mem_put_func(get_mem_bank(addr).lput, addr, l))
#define wordput(addr,w) (call_mem_put_func(get_mem_bank(addr).wput, addr, w))
#define byteput(addr,b) (call_mem_put_func(get_mem_bank(addr).bput, addr, b))

#include "client.h"

STATIC_INLINE uae_u32 get_long (uaecptr addr)
{
	readServer(addr);
	return longget (addr);
}
STATIC_INLINE uae_u32 get_word (uaecptr addr)
{
	readServer(addr);
	return wordget (addr);
}
STATIC_INLINE uae_u32 get_byte (uaecptr addr)
{
	readServer(addr);
	return byteget (addr);
}
STATIC_INLINE uae_u32 get_longi(uaecptr addr)
{
	readServer(addr);
	return longgeti (addr);
}
STATIC_INLINE uae_u32 get_wordi(uaecptr addr)
{
	readServer(addr);
	return wordgeti (addr);
}

/*
* Read a host pointer from addr
*/
#if SIZEOF_VOID_P == 4
# define get_pointer(addr) ((void *)get_long (addr))
#else
# if SIZEOF_VOID_P == 8
STATIC_INLINE void *get_pointer (uaecptr addr)
{
	const unsigned int n = SIZEOF_VOID_P / 4;
	union {
		void    *ptr;
		uae_u32  longs[SIZEOF_VOID_P / 4];
	} p;
	unsigned int i;

	for (i = 0; i < n; i++) {
#ifdef WORDS_BIGENDIAN
		p.longs[i]     = get_long (addr + i * 4);
#else
		p.longs[n - 1 - i] = get_long (addr + i * 4);
#endif
	}
	return p.ptr;
}
# else
#  error "Unknown or unsupported pointer size."
# endif
#endif

STATIC_INLINE void put_long (uaecptr addr, uae_u32 l)
{
	writeServer(addr, l);
	longput(addr, l);
}
STATIC_INLINE void put_word (uaecptr addr, uae_u32 w)
{
	writeServer(addr, w);
	wordput(addr, w);
}
STATIC_INLINE void put_byte (uaecptr addr, uae_u32 b)
{
	writeServer(addr, b);
	byteput(addr, b);
}

extern void put_long_slow (uaecptr addr, uae_u32 v);
extern void put_word_slow (uaecptr addr, uae_u32 v);
extern void put_byte_slow (uaecptr addr, uae_u32 v);
extern uae_u32 get_long_slow (uaecptr addr);
extern uae_u32 get_word_slow (uaecptr addr);
extern uae_u32 get_byte_slow (uaecptr addr);


/*
* Store host pointer v at addr
*/
#if SIZEOF_VOID_P == 4
# define put_pointer(addr, p) (put_long ((addr), (uae_u32)(p)))
#else
# if SIZEOF_VOID_P == 8
STATIC_INLINE void put_pointer (uaecptr addr, void *v)
{
	const unsigned int n = SIZEOF_VOID_P / 4;
	union {
		void    *ptr;
		uae_u32  longs[SIZEOF_VOID_P / 4];
	} p;
	unsigned int i;

	p.ptr = v;

	for (i = 0; i < n; i++) {
#ifdef WORDS_BIGENDIAN
		put_long (addr + i * 4, p.longs[i]);
#else
		put_long (addr + i * 4, p.longs[n - 1 - i]);
#endif
	}
}
# endif
#endif

STATIC_INLINE uae_u8 *get_real_address (uaecptr addr)
{
	addrbank tmpfix = get_mem_bank (addr);
	return tmpfix.xlateaddr(addr);
}

STATIC_INLINE int valid_address (uaecptr addr, uae_u32 size)
{
	return get_mem_bank (addr).check(addr, size);
}

extern int addr_valid (const TCHAR*, uaecptr,uae_u32); //TODO memory.cpp

/* For faster access in custom chip emulation.  */
extern void REGPARAM3 chipmem_lput (uaecptr, uae_u32) REGPARAM; //TODO memory.cpp*
extern void REGPARAM3 chipmem_wput (uaecptr, uae_u32) REGPARAM; //TODO memory.cpp*
extern void REGPARAM3 chipmem_bput (uaecptr, uae_u32) REGPARAM; //TODO memory.cpp*

extern uae_u32 REGPARAM3 chipmem_agnus_wget (uaecptr) REGPARAM; //TODO memory.cpp*
extern void REGPARAM3 chipmem_agnus_wput (uaecptr, uae_u32) REGPARAM; //TODO memory.cpp*

extern uae_u32 chipmem_mask, kickmem_mask;
extern uae_u8 *kickmemory;
extern addrbank dummy_bank;

/* 68020+ Chip RAM DMA contention emulation */
extern void REGPARAM3 chipmem_bput_c2 (uaecptr, uae_u32) REGPARAM; //TODO memory.cpp?
//TODO memory.cpp
extern uae_u32 (REGPARAM3 *chipmem_lget_indirect)(uaecptr) REGPARAM;
extern uae_u32 (REGPARAM3 *chipmem_wget_indirect)(uaecptr) REGPARAM;
extern uae_u32 (REGPARAM3 *chipmem_bget_indirect)(uaecptr) REGPARAM;
extern void (REGPARAM3 *chipmem_lput_indirect)(uaecptr, uae_u32) REGPARAM;
extern void (REGPARAM3 *chipmem_wput_indirect)(uaecptr, uae_u32) REGPARAM;
extern void (REGPARAM3 *chipmem_bput_indirect)(uaecptr, uae_u32) REGPARAM;
extern int (REGPARAM3 *chipmem_check_indirect)(uaecptr, uae_u32) REGPARAM;
extern uae_u8 *(REGPARAM3 *chipmem_xlate_indirect)(uaecptr) REGPARAM;

#ifdef NATMEM_OFFSET

typedef struct shmpiece_reg {
	uae_u8 *native_address;
	int id;
	uae_u32 size;
	const TCHAR *name;
	struct shmpiece_reg *next;
	struct shmpiece_reg *prev;
} shmpiece;

extern shmpiece *shm_start;

#endif

extern uae_u8 *mapped_malloc (size_t, const TCHAR*); //TODO memory.cpp*
extern void mapped_free (uae_u8 *); //TODO memory.cpp*
extern void clearexec (void);
extern void mapkick (void);
extern void a3000_fakekick (int);

extern uaecptr strcpyha_safe (uaecptr dst, const uae_char *src); //TODO memory.cpp*
extern uae_char *strcpyah_safe (uae_char *dst, uaecptr src, int maxsize); //TODO memory.cpp*
extern void memcpyha_safe (uaecptr dst, const uae_u8 *src, int size); //TODO memory.cpp*
extern void memcpyha (uaecptr dst, const uae_u8 *src, int size); //TODO memory.cpp*
extern void memcpyah_safe (uae_u8 *dst, uaecptr src, int size); //TODO memory.cpp*
extern void memcpyah (uae_u8 *dst, uaecptr src, int size); //TODO memory.cpp*

extern uae_s32 getz2size (struct uae_prefs *p); //TODO memory.cpp*
extern ULONG getz2endaddr (void); //TODO memory.cpp*

#ifdef FSUAE
int uae_get_memory_checksum(); //TODO memory.cpp*
#endif
