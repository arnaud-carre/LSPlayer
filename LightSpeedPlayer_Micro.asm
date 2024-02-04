;*****************************************************************
;
;	Light Speed Player v1.20
;	Fastest Amiga MOD player ever :)
;	Written By Arnaud Carré (aka Leonard / OXYGENE)
;	https://github.com/arnaud-carre/LSPlayer
;	twitter: @leonard_coder
;
;	"micro mode" player version ( average time: 1.5 scanline )
;	This mode focus on music data compression ratio. Suited for 4KiB or small intros
;	Do not support various BPM, music getPos/setPos and sample without a note
;
;	You can also use classic "standard" player to support all features
;	Or you can use generated "ultra fast" player code for half scanline replayer ("-insane" option)
;
;	LSP_MusicInitMicro		Initialize a LSP driver + relocate score&bank music data
;	LSP_MusicPlayTickMicro	Play a LSP music (call it per frame)
;
;*****************************************************************

;------------------------------------------------------------------
;
;	LSP_MusicInitMicro
;
;		In:	a0: LSP music data(any memory)
;			a1: LSP sound bank(chip memory)
;			a2: DMACON patching value low byte address (should be odd address!)
;		Out:none
;
;------------------------------------------------------------------
LSP_dataError:	illegal

LSP_MusicInitMicro:
			cmpi.l	#'LSPm',(a0)+	; LSP "micro" mode signature
			bne.s	LSP_dataError
			cmpi.w	#$0114,(a0)+			; this play routine supports v1.20 as minimal version of LPConvert.exe
			blt.s	LSP_dataError
			lea		LSPMicroVars(pc),a3
			clr.w	m_lastDmacon(a3)
			move.l	a2,m_dmaconPatch(a3)
			move.w	(a0)+,d0				; instrument count
			move.l	a0,m_lspInstruments(a3)	; instrument tab addr ( minus 4 )
			subq.w	#1,d0
			move.l	a1,d1
.relocLoop:	add.l	d1,(a0)
			add.l	d1,6(a0)
			lea		12(a0),a0
			dbf		d0,.relocLoop
			lea		m_streams(a3),a2
			lea		16*4(a0),a1
		; do not stress about this rept, your exe packer will enjoy it
		rept	16
			move.l	(a0)+,d1
			add.l	a1,d1
			move.l	d1,m_loopStreams-m_streams(a2)		; set loopStreams at 0 by default
			move.l	d1,(a2)+				
		endr
			bset.b	#1,$bfe001				; disabling this fucking Low pass filter!!
			rts



;------------------------------------------------------------------
;
;	LSP_MusicPlayTickMicro
;
;		In:	a6: should be $dff0a0
;		Out:None
;
;------------------------------------------------------------------
LSP_MusicPlayTickMicro:
			lea		LSPMicroVars(pc),a2
			move.w	m_lastDmacon(a2),d0
			beq.s	.skip
			lea		m_resetv(a2),a3
			lea		16*4(a6),a4
			moveq	#4-1,d1
.rLoop:		lea		-16(a4),a4
			btst	d1,d0
			beq.s	.norst
			move.l	(a3)+,(a4)
			move.w	(a3)+,4(a4)
.norst:		dbf		d1,.rLoop

.skip:		lea		m_streams(a2),a1
			moveq	#4-1,d7
			moveq	#0,d6
			lea		m_resetv(a2),a3
			lea		16*4(a6),a6
			
.vLoop:		lea		-16(a6),a6
			move.l	(a1),a0
			move.b	(a0)+,d0		; cmd for current voice
			move.l	a0,(a1)+		; update cmd stream ptr
			add.b	d0,d0
			bcc.s	.noVol
			move.l	4*4*1-4(a1),a0
			move.b	(a0)+,$9(a6)	; volume
			move.l	a0,4*4*1-4(a1)
.noVol:		add.b	d0,d0
			bcc.s	.noPer
			move.l	4*4*2-4(a1),a0
			move.w	(a0)+,$6(a6)		; period
			move.l	a0,4*4*2-4(a1)
.noPer:		add.b	d0,d0
			bcc.s	.noInstr
			move.l	4*4*3-4(a1),a0
			moveq	#0,d1
			move.b	(a0)+,d1
			move.l	a0,4*4*3-4(a1)
			
		; prepare instrument
		IF	1
			mulu.w	#12,d1
		ELSE
			move.w	d1,d2
			lsl.w	#2,d1		; x*4
			lsl.w	#3,d2		; x*8
			add.w	d2,d1		; x*(4+8)
		ENDIF
			move.l	m_lspInstruments(a2),a0
			add.w	d1,a0
			bset	d7,d6
			move.l	(a0)+,(a6)
			move.w	(a0)+,4(a6)
			move.l	(a0)+,(a3)+
			move.w	(a0)+,(a3)+

.noInstr:
			dbf		d7,.vLoop

			move.w	d6,$dff096
			move.w	d6,m_lastDmacon(a2)
			move.l	m_dmaconPatch(a2),a0
			move.b	d6,(a0)

			add.b	d0,d0
			bcc.s	.noLoopCmd
			
		; backup or restore current song position
			lea		m_streams(a2),a0
			lea		m_loopStreams(a2),a1
			add.b	d0,d0
			bcc.s	.skipRestore
			exg		a0,a1
.skipRestore:
			; do not stress about this rept, your exe packer will enjoy it
			rept	16
			move.l	(a0)+,(a1)+
			endr
.noLoopCmd:
			rts

	rsreset

m_streams:			rs.l	16
m_loopStreams:		rs.l	16
m_dmaconPatch:		rs.l	1	;  8 m_lfmDmaConPatch
m_lspInstruments:	rs.l	1	; 16 LSP instruments table addr
m_lastDmacon:		rs.w	1
m_resetv:			rs.b	4*6
sizeof_LSPVars:		rs.w	0

LSPMicroVars:		ds.b	sizeof_LSPVars
