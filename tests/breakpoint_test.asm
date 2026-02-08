; a quick routine to help test new breakpoints in beastem

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

CODE_START1	EQU	0x4000
CODE_START2	EQU	0x8000

		OUTPUT	bptest.com
		ORG	0x0100
;
; FIRST make sure that mapping is enabled!
		LD	A, IO_MEM_ENABLE
		OUT	(IO_MEM_CTRL), A

; the basic idea:
; - we are executing from page 0x0 (0000-3fff)
; - we set up mappings:
;      page 0x1 (4000-7fff)  ==> phys page 0x20 (80000-83fff)
;      page 0x2 (8000-bfff)  ==> phys page 0x21 (84000-87fff)
; - we copy some executable code into each of those pages
; - we set breakpoints in each of the physical pages
; - we run code in page 1 and verify that one physical BP triggers
; - we map the other physical page into page 1
; - we run code in page 1 and verify that the other physical BP triggers

; FIRST: setup page 0x1 (4000-7fff)  ==> phys page 0x20 (80000-83fff)
		LD	C, IO_MEM_1	; page 1
		LD	E, 0x20	; physical page 20
		OUT	(C), E

;        and   page 0x2 (8000-bfff)  ==> phys page 0x21 (84000-87fff)
		LD	C, IO_MEM_2	; page 2
		LD	E, 0x21	; physical page 21
		OUT	(C), E

; 	also set up page 0x3 (c000-ffff) -> phys page 0x23 (8c000-8ffff)
;       so that we've got some RAM for our stack
		LD	C, IO_MEM_3	; page 3
		LD	E, 0x23	; physical page 23
		OUT	(C), E

; SECOND: copy code blocks into their intended destinations
		LD	DE, CODE_START1
		LD	HL, code_block_1
		LD	BC, end_code_block_1 - code_block_1
		LDIR

		LD	DE, CODE_START2
		LD	HL, code_block_2
		LD	BC, end_code_block_2 - code_block_2
		LDIR
		
; THIRD: start executing code_block_1
; if we break in here we should see A=0x55
		CALL	CODE_START1

; FOURTH: map the *other* physical page (0x21) into page 0x1
		LD	C, IO_MEM_1	; page 1
		LD	E, 0x21	; physical page 21
		OUT	(C), E

; FIFTH: call the same address again, but now it's code_block_2 running
; if we break in here we should see A=0xAA
		CALL	CODE_START1

; we are done
halt		JR	halt		





code_block_1
		DISP	CODE_START1
		LD	B, 8		; go round 8 times
1
		LD	A, 0x55		; quickly identify this code loop
		DJNZ	1_B
		RET
		ENT
end_code_block_1

code_block_2
		DISP	CODE_START1	; NB yes CODE_START_1
					; this code will be copied to CODE_START_2
					; but it will *run* from CODE_START_1
					; ...I need a lie down.

		LD	B, 8		; go round 8 times
1
		LD	A, 0xAA		; quickly identify this code loop
		DJNZ	1_B

		RET
		ENT
end_code_block_2

		LD	A, 0x21
		OUT	(0x70), A
