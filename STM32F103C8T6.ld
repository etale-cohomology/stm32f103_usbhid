ENTRY(Reset_Handler)  /* entry point. must be defined elsewhere, eg. in startup_stm32f10x_md.s */

_estack = 0x20005000;    /* highest address of the user mode stack */  /* end of RAM */
/* generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size  = 0x200;  /* heap bdim.  the heap  lives on RAM, it starts at the beginning and grows by increasing addresses */
_Min_Stack_Size = 0x400;  /* stack bdim. the stack lives on RAM; it starts at the end       and grows by decreasing addresses */

MEMORY{  /* specify the memory areas */
	RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
	FLASH  (rx) : ORIGIN = 0x08000000, LENGTH = 128K
}

SECTIONS{  /* define output sections */
	/*----------------------------------------------------------------------*/
	.isr_vector :{  /* the startup code goes first into FLASH */
		. = ALIGN(4);
		KEEP(*(.isr_vector))  /* startup code */
		. = ALIGN(4);
	}>FLASH

	/*----------------------------------------------------------------------*/
	.text :{  /* the program code and other data goes into FLASH */
		. = ALIGN(4);
		*(.text)           /* .text sections (code) */
		*(.text*)          /* .text* sections (code) */
		*(.glue_7)         /* glue arm to thumb code */
		*(.glue_7t)        /* glue thumb to arm code */
		*(.eh_frame)

		KEEP(*(.init))
		KEEP(*(.fini))

		. = ALIGN(4);
		_etext = .;        /* define a global symbols at end of code */
	}>FLASH

	/*----------------------------------------------------------------------*/
	.rodata :{  /* constant data goes into FLASH */
		. = ALIGN(4);
		*(.rodata)   /* .rodata sections (constants, strings, etc.) */
		*(.rodata*)  /* .rodata* sections (constants, strings, etc.) */
		. = ALIGN(4);
	}>FLASH

	/*----------------------------------------------------------------------*/
	.ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
	.ARM :{
		__exidx_start = .;
		*(.ARM.exidx*)
		__exidx_end = .;
	}>FLASH

	/*----------------------------------------------------------------------*/
	.preinit_array :{
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array*))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	}>FLASH
	.init_array :{
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array*))
		PROVIDE_HIDDEN (__init_array_end = .);
	}>FLASH
	.fini_array :{
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array*))
		PROVIDE_HIDDEN (__fini_array_end = .);
	}>FLASH

	_sidata = LOADADDR(.data);  /* used by the startup to initialize data */

	/*----------------------------------------------------------------------*/
	.data :{  /* initialized data sections goes into RAM, load LMA copy after code */
		. = ALIGN(4);
		_sdata = .;        /* create a global symbol at data start */
		*(.data)           /* .data sections */
		*(.data*)          /* .data* sections */

		. = ALIGN(4);
		_edata = .;        /* define a global symbol at data end */
	}>RAM AT> FLASH

	/*----------------------------------------------------------------------*/
	. = ALIGN(4);  /* uninitialized data section */
	.bss :{  /* this is used by the startup in order to initialize the .bss secion */
		_sbss         = .;  /* define a global symbol at bss start */
		__bss_start__ = _sbss;
		*(.bss)
		*(.bss*)
		*(COMMON)

		. = ALIGN(4);
		_ebss       = .;  /* define a global symbol at bss end */
		__bss_end__ = _ebss;
	}>RAM

	/*----------------------------------------------------------------------*/
	._user_heap_stack :{  /* user_heap_stack section, used to check that there is enough RAM left */
		. = ALIGN(8);
		PROVIDE (  end = . );
		PROVIDE ( _end = . );
		. = . + _Min_Heap_Size;
		. = . + _Min_Stack_Size;
		. = ALIGN(8);
	}>RAM

	/*----------------------------------------------------------------------*/
	/DISCARD/ :{  /* remove information from the standard libraries */
		libc.a  (*)
		libm.a  (*)
		libgcc.a(*)
	}

	.ARM.attributes 0:{ *(.ARM.attributes) }
}
