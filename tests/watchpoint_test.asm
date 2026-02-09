; a quick routine to help test new watchpoints in beastem

; *NB* cant actually use these if we've mapped the BIOS out!!
get_page_mapping EQU	0x0FDDC 	; Return the logical (cpu) page C (0-2) in A
set_page_mapping EQU	0x0FDDF 	; Set the logical (cpu) page in A (0-2) to the physical (RAM/ROM) page in E

; we'll have to talk directly to the port instead
IO_MEM_0            EQU    070h      ; Page 0: 0000h - 3fffh
IO_MEM_1            EQU    071h      ; Page 1: 4000h - 7fffh
IO_MEM_2            EQU    072h      ; Page 2: 8000h - bfffh
IO_MEM_3            EQU    073h      ; Page 3: c000h - ffffh
IO_MEM_CTRL         EQU    074h      ; Paging enable register
IO_MEM_ENABLE       EQU    1
IO_MEM_DISABLE      EQU    0 

		OUTPUT	wptest.com
		ORG	0x0100

; first some basic watchpoint fodder
; loop reads from 0x10xx & writes to 0x20xx
; then reads from 0x20xx & writes to 0x10xx
copy1
		LD	HL, 0x1000
		LD	DE, 0x2000
		LD	B, 0xFF

loop		LD	A, (HL)
		LD	(DE), A

		LD	A, (DE)
		LD	(HL), A

		INC	HL
		INC	DE
		DJNZ	loop

; now we move on to some more advanced shenanigans

mappo_worko
; now do some freaky mapping so that we can test
; *physical* watchpoints are working separately from
; logical ones.
;
; FIRST make sure that mapping is enabled!
		LD	A, IO_MEM_ENABLE
		OUT	(IO_MEM_CTRL), A

; a "crossover" mapping:
; we map 8000-bfff => 84000-87fff
;    and c000-ffff => 80000-83fff
;    
; set watchpoints on: 0x84000,ff 
;                     0x80000,ff
; observe that when you break the logical address is
; different from the lower 16 bits of the watchpoint address

		LD	C, IO_MEM_2	; logical page 2
		LD	E, 0x21	; physical page 21
		OUT	(C), E

		LD	C, IO_MEM_3	; logical page 3
		LD	E, 0x20	; physical page 20
		OUT	(C), E

		JR	copy2

; BROKEN! can't call BIOS if it is mapped out!!!
mappo_no_worko
; now do some freaky mapping so that we can test
; *physical* watchpoints are working separately from
; logical ones.
;
; we map 8000-bfff => 84000-87fff
;    and c000-ffff => 80000-83fff
		LD	A, 2	; logical page 2
		LD	E, 0x21	; physical page 21
		CALL	set_page_mapping
		JP	NC, bad_mapping

		LD	A, 3	; logical page 3
		LD	E, 0x20	; physical page 20
		CALL	set_page_mapping
		JP	NC, bad_mapping

copy2
		LD	HL, 0x8000
		LD	DE, 0xc000
		LD	B, 0xFF

loop2		LD	A, (HL)
		LD	(DE), A

		LD	A, (DE)
		LD	(HL), A

		INC	HL
		INC	DE
		DJNZ	loop2
		
bad_mapping	JR	bad_mapping


