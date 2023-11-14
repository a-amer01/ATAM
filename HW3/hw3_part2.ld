/* TODO: Add the other needed fields.*/

ENTRY(_hw3_unicorn);

EXTERN(purple);
EXTERN(white);

PHDRS {
  text PT_LOAD FLAGS(3);
  data PT_LOAD FLAGS(6);
  rodata PT_LOAD FLAGS(5);
}

SECTIONS
{
  .text 0x400000:
  {
	*(.text)
  } :text

  .data 0x60000:
  { 
	*(.data)
	*(.bss)
  } :data
  
  .rodata 0x80000000:
  { 
	*(.rodata) 
  } :rodata
}

