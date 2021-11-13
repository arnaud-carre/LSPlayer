;*****************************************************************
;
;	Light Speed Player v1.05
;	Fastest Amiga MOD player ever :)
;	Written By Arnaud Carré (aka Leonard / OXYGENE)
;	https://github.com/arnaud-carre/LSPlayer
;	twitter: @leonard_coder
;
;	"small & fast" player version ( average time: 1 scanline )
;	Less than 512 bytes of code!
;	You can also use generated "insane" player code for even more perf
;
;	--------How to use--------- 
;
;	bsr LSP_MusicDriver+0 : Init LSP player code
;		In:	a0: LSP music data(any memory)
;			a1: LSP sound bank(chip memory)
;			a2: DMACON 8bits byte address (should be odd address!)
;		Out:a0: music BPM pointer (16bits)
;			d0: music len in tick count
;
;	bsr LSP_MusicDriver+4 : LSP player tick (call once per frame)
;		In:	a6: should be $dff0a0
;			Scratched regs: d0/d1/d2/a0/a1/a2/a3/a4/a5
;		Out:None
;
;*****************************************************************

	opt o-		; switch off ALL optimizations (we don't want vasm to change some code size, and all optimizations are done!)

LSP_MusicDriver:
			bra.w	.LSP_PlayerInit

;.LSP_MusicDriver+4:						; player tick handle ( call this at music player rate )
			lea		.LSPVars(pc),a1
			move.l	(a1),a0					; byte stream
.process:	moveq	#0,d0
.cloop:		move.b	(a0)+,d0
			bne.s	.swCode
			addi.w	#$0100,d0
			bra.s	.cloop
.swCode:	add.w	d0,d0
			move.l	m_codeTableAddr(a1),a2	; code table
			move.w	0(a2,d0.w),d0			; code
			beq		.noInst
			cmp.w	m_escCodeRewind(a1),d0
			beq		.r_rewind
			cmp.w	m_escCodeSetBpm(a1),d0
			beq		.r_chgbpm

			add.b	d0,d0
			bcc.s	.noVd
			move.b	(a0)+,$d9-$a0(a6)
.noVd:		add.b	d0,d0
			bcc.s	.noVc
			move.b	(a0)+,$c9-$a0(a6)
.noVc:		add.b	d0,d0
			bcc.s	.noVb
			move.b	(a0)+,$b9-$a0(a6)
.noVb:		add.b	d0,d0
			bcc.s	.noVa
			move.b	(a0)+,$a9-$a0(a6)
.noVa:		
			move.l	a0,(a1)+	; store byte stream ptr
			move.l	(a1),a0		; word stream

			tst.b	d0
			beq.s	.noPa

			add.b	d0,d0
			bcc.s	.noPd
			move.w	(a0)+,$d6-$a0(a6)
.noPd:		add.b	d0,d0
			bcc.s	.noPc
			move.w	(a0)+,$c6-$a0(a6)
.noPc:		add.b	d0,d0
			bcc.s	.noPb
			move.w	(a0)+,$b6-$a0(a6)
.noPb:		add.b	d0,d0
			bcc.s	.noPa
			move.w	(a0)+,$a6-$a0(a6)
.noPa:		
			tst.w	d0
			beq.s	.noInst

			moveq	#0,d1
			move.l	m_lspInstruments-4(a1),a2	; instrument table
			lea		.resetv+12(pc),a4

			lea		3*16(a6),a5
			moveq	#4-1,d2

.vloop:		add.w	d0,d0
			bcs.s	.setIns
			add.w	d0,d0
			bcc.s	.skip
			move.l	(a4),a3
			move.l	(a3)+,(a5)
			move.w	(a3)+,4(a5)
			bra.s	.skip
.setIns:	add.w	(a0)+,a2
			add.w	d0,d0
			bcc.s	.noReset
			bset	d2,d1
			move.w	d1,$96-$a0(a6)
.noReset:	move.l	(a2)+,(a5)
			move.w	(a2)+,4(a5)
			move.l	a2,(a4)
.skip:		subq.w	#4,a4
			lea		-16(a5),a5
			dbf		d2,.vloop

			move.l	m_dmaconPatch-4(a1),a3		; dmacon patch
			move.b	d1,(a3)						; dmacon			

.noInst:	move.l	a0,(a1)			; store word stream (or byte stream if coming from early out)
			rts

.r_rewind:	move.l	m_byteStreamLoop(a1),a0
			move.l	m_wordStreamLoop(a1),m_wordStream(a1)
			bra		.process

.r_chgbpm:	move.b	(a0)+,(m_currentBpm+1)(a1)	; BPM
			bra		.process

; a0: music data (any mem)
; a1: sound bank data (chip mem)
; a2: 16bit DMACON word address

.LSP_PlayerInit:
			cmpi.l	#'LSP1',(a0)+
			bne		.dataError
			move.l	(a0)+,d0		; unique id
			cmp.l	(a1),d0			; check that sample bank is this one
			bne.s	.dataError

			lea		.LSPVars(pc),a3
			cmpi.w	#$0104,(a0)+			; minimal major & minor version of latest compatible LSPConvert.exe
			blt.s	.dataError
			move.w	(a0)+,m_currentBpm(a3)	; default BPM
			move.w	(a0)+,m_escCodeRewind(a3)
			move.w	(a0)+,m_escCodeSetBpm(a3)
			move.l	(a0)+,-(a7)
			move.l	a2,m_dmaconPatch(a3)
			move.w	#$8000,-1(a2)			; Be sure DMACon word is $8000 (note: a2 should be ODD address)
			move.w	(a0)+,d0				; instrument count
			lea		-12(a0),a2				; LSP data has -12 offset on instrument tab ( to win 2 cycles in fast player :) )
			move.l	a2,m_lspInstruments(a3)	; instrument tab addr ( minus 4 )
			subq.w	#1,d0
			move.l	a1,d1
.relocLoop:	bset.b	#0,3(a0)				; bit0 is relocation done flag
			bne.s	.relocated
			add.l	d1,(a0)
			add.l	d1,6(a0)
.relocated:	lea		12(a0),a0
			dbf		d0,.relocLoop
			move.w	(a0)+,d0				; codes count (+2)
			move.l	a0,m_codeTableAddr(a3)	; code table
			add.w	d0,d0
			add.w	d0,a0
			move.l	(a0)+,d0				; word stream size
			move.l	(a0)+,d1				; byte stream loop point
			move.l	(a0)+,d2				; word stream loop point

			move.l	a0,m_wordStream(a3)
			lea		0(a0,d0.l),a1			; byte stream
			move.l	a1,m_byteStream(a3)
			add.l	d2,a0
			add.l	d1,a1
			move.l	a0,m_wordStreamLoop(a3)
			move.l	a1,m_byteStreamLoop(a3)
			bset.b	#1,$bfe001				; disabling this fucking Low pass filter!!
			lea		m_currentBpm(a3),a0
			move.l	(a7)+,d0				; music len in frame ticks
			rts

.dataError:	illegal

	rsreset
	
m_byteStream:		rs.l	1	;  0 byte stream
m_wordStream:		rs.l	1	;  4 word stream
m_dmaconPatch:		rs.l	1	;  8 m_lfmDmaConPatch
m_codeTableAddr:	rs.l	1	; 12 code table addr
m_escCodeRewind:	rs.w	1	; 16 rewind special escape code
m_escCodeSetBpm:	rs.w	1	; 18 set BPM escape code
m_lspInstruments:	rs.l	1	; 20 LSP instruments table addr
m_relocDone:		rs.w	1	; 24 reloc done flag
m_currentBpm:		rs.w	1	; 26 current BPM
m_byteStreamLoop:	rs.l	1	; 28 byte stream loop point
m_wordStreamLoop:	rs.l	1	; 32 word stream loop point
sizeof_LSPVars:		rs.w	0

.LSPVars:	ds.b	sizeof_LSPVars
			
.resetv:	dc.l	0,0,0,0
