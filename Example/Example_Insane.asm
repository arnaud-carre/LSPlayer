;
; LightSpeedPlayer usage example
; 

		
		code
		

			move.w	#$7fff,$dff09e
			move.w	#(1<<5)|(1<<6)|(1<<7)|(1<<8),$dff096
			move.w	#(1<<5)|(1<<6)|(1<<13),$dff09a

			bsr		clearSprites

		; Init LSP player
			lea		LSPMusic,a0
			lea		LSPBank,a1
			lea		copperDMAConPatch+3,a2
			bsr		LSP_MusicInitInsane

		; setup copper list & interrupt
			move.l	#copperInterrupt,$6c.w
			move.l	#copperMain,$dff080
			move.w	#(1<<4),$dff09e
			move.w	#$c000|(1<<4),$dff09a
			move.w	#$8000|(1<<7),$dff096		; Copper DMA

		; infinite loop ( LSP player tick is called from copper interrupt)
mainLoop:	bra.s	mainLoop

		; Include dedicated ultra fast LSP player (generated by LSPConvert.exe)
			include	"rink-a-dink_insane.asm"
		
copperInterrupt:

			movem.l	d0/a0/a1/a2/a3,-(a7)
			
			move.w	#$f00,$dff180
			lea		$dff0a0,a6					; always set a6 to dff0a0 before calling LSP tick
			bsr		LSP_MusicPlayTickInsane		; player music tick
			move.w	#$0,$dff180
			
			movem.l	(a7)+,d0/a0/a1/a2/a3


			move.w	#1<<4,$dff09c		;clear copper interrupt bit
			move.w	#1<<4,$dff09c		;clear VBL interrupt bit
			nop
			rte
			
		

clearSprites:
			lea		$dff140,a0
			moveq	#8-1,d0			; 8 sprites to clear
			moveq	#0,d1
.clspr:		move.l	d1,(a0)+
			move.l	d1,(a0)+
			dbf		d0,.clspr
			rts

		data_c

copperMain:
			dc.l	$01fc0000
			dc.l	$01000200
			
			dc.l	(50<<24)|($09fffe)			; wait scanline 50
			dc.l	$009c8000|(1<<4)			; fire copper interrupt

			dc.l	((50+11)<<24)|($09fffe)		; wait scanline 50+11
			dc.l	$0180000f
copperDMAConPatch:
			dc.l	$00968000
			dc.l	$01800000
			dc.l	-2



LSPBank:	incbin	"rink-a-dink.lsbank"
			even

		data

LSPMusic:	incbin	"rink-a-dink.lsmusic"
			even

