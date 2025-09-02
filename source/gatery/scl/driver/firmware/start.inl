#pragma once

void __attribute__((weak)) start_memory();
void __attribute__((weak)) start_devices();

extern char __init_array_start;
extern char __init_array_end;
extern char __bss_start;
extern char __BSS_END__;

#pragma GCC push_options
#pragma GCC optimize ("O1")

extern "C" __attribute__((naked)) void _start()
{
	asm volatile(
		".option push;"
		".option norelax;"
		"la gp, __global_pointer$;"
		"mv sp, %0;"
		".option pop;"
		: : "r" (START_STACK_POINTER)
		);

	if(start_memory) 
		start_memory();

	// __bss_start may not be aligned :( but __BSS_END is
	// slow but small solution
	for (char* it = &__bss_start; it != &__BSS_END__; ++it)
	{
		asm volatile(""); // HACK to prevent gcc from replacing bss init by memset
		*it = 0;
	}

	typedef void(*init_t)(void);
	for (init_t* it = (init_t*)&__init_array_start; it != (init_t*)&__init_array_end; ++it)
		(*it)();

	if(start_devices) 
		start_devices();
	main();
	asm volatile("ecall");
}

#pragma GCC pop_options
