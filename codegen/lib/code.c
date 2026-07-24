#include "code.h"

void Start(void) {
  sei(); // pretty standard 6502 type init here
  cld();
  lda_imm(0b00010000); // init PPU control register 1
  ppu_ctrl = a;
  ldx_imm(0xff); // reset stack pointer
  sp = x;
  
VBlank1:
  lda_abs(PPU_STATUS); // wait two frames
  if (!neg_flag) { goto VBlank1; }
  
VBlank2:
  lda_abs(PPU_STATUS);
  if (!neg_flag) { goto VBlank2; }
  ldy_imm(ColdBootOffset); // load default cold boot pointer
  ldx_imm(0x5); // this is where we check for a warm boot
  
WBootCheck:
  lda_absx(TopScoreDisplay); // check each score digit in the top score
  cmp_imm(10); // to see if we have a valid digit
  // if not, give up and proceed with cold boot
  if (!carry_flag) {
    dex();
    if (!neg_flag) { goto WBootCheck; }
    lda_abs(WarmBootValidation); // second checkpoint, check to see if
    cmp_imm(0xa5); // another location has a specific value
    if (zero_flag) {
      ldy_imm(WarmBootOffset); // if passed both, load warm boot pointer
    }
  }
  // ColdBoot:
  InitializeMemory(); // clear memory using pointer in Y
  dynamic_ram_write(SND_DELTA_REG + 1, a); // reset delta counter load register
  ram[OperMode] = a; // reset primary mode of operation
  lda_imm(0xa5); // set warm boot flag
  ram[WarmBootValidation] = a;
  ram[PseudoRandomBitReg] = a; // set seed for pseudorandom register
  lda_imm(0b00001111);
  apu_write(SND_MASTERCTRL_REG, a); // enable all sound channels except dmc
  lda_imm(0b00000110);
  ppu_mask = a; // turn off clipping for OAM and background
  MoveAllSpritesOffscreen();
  InitializeNameTables(); // initialize both name tables
  inc_abs(DisableScreenFlag); // set flag to disable screen output
  lda_abs(Mirror_PPU_CTRL_REG1);
  ora_imm(0b10000000); // enable NMIs
  WritePPUReg1();
  return; // <rti> //  EndlessLoop: jmp EndlessLoop ; endless loop, need I say more?
  // -------------------------------------------------------------------------------------
  // $00 - vram buffer address table low, also used for pseudorandom bit
  // $01 - vram buffer address table high
}

void NonMaskableInterrupt(void) {
  lda_abs(Mirror_PPU_CTRL_REG1); // disable NMIs in mirror reg
  and_imm(0b01111111); // save all other bits
  ram[Mirror_PPU_CTRL_REG1] = a;
  and_imm(0b01111110); // alter name table address to be $2800
  ppu_ctrl = a; // (essentially $2000) but save other bits
  lda_abs(Mirror_PPU_CTRL_REG2); // disable OAM and background display by default
  and_imm(0b11100110);
  ldy_abs(DisableScreenFlag); // get screen disable flag
  if (!zero_flag) { goto ScreenOff; } // if set, used bits as-is
  lda_abs(Mirror_PPU_CTRL_REG2); // otherwise reenable bits and save them
  ora_imm(0b00011110);
  
ScreenOff:
  ram[Mirror_PPU_CTRL_REG2] = a; // save bits for later but not in register at the moment
  and_imm(0b11100111); // disable screen for now
  ppu_mask = a;
  ldx_abs(PPU_STATUS); // reset flip-flop and reset scroll registers to zero
  lda_imm(0x0);
  InitScroll();
  oam_addr = a; // reset spr-ram address register
  lda_imm(0x2); // perform spr-ram DMA access on $0200-$02ff
  ppu_transfer_oam((uint16_t)(a << 8));
  ldx_abs(VRAM_Buffer_AddrCtrl); // load control for pointer to buffer contents
  lda_absx(VRAM_AddrTable_Low); // set indirect at $00 to pointer
  ram[0x0] = a;
  lda_absx(VRAM_AddrTable_High);
  ram[0x1] = a;
  UpdateScreen(); // update screen with buffer contents
  ldy_imm(0x0);
  ldx_abs(VRAM_Buffer_AddrCtrl); // check for usage of $0341
  cpx_imm(0x6);
  if (!zero_flag) { goto InitBuffer; }
  iny(); // get offset based on usage
  
InitBuffer:
  ldx_absy(VRAM_Buffer_Offset);
  lda_imm(0x0); // clear buffer header at last location
  ram[VRAM_Buffer1_Offset + x] = a;
  ram[VRAM_Buffer1 + x] = a;
  ram[VRAM_Buffer_AddrCtrl] = a; // reinit address control to $0301
  lda_abs(Mirror_PPU_CTRL_REG2); // copy mirror of $2001 to register
  ppu_mask = a;
  SoundEngine(); // play sound
  ReadJoypads(); // read joypads
  PauseRoutine(); // handle pause
  UpdateTopScore();
  lda_abs(GamePauseStatus); // check for pause status
  lsr_acc();
  if (carry_flag) { goto PauseSkip; }
  lda_abs(TimerControl); // if master timer control not set, decrement
  if (zero_flag) { goto DecTimers; } // all frame and interval timers
  dec_abs(TimerControl);
  if (!zero_flag) { goto NoDecTimers; }
  
DecTimers:
  ldx_imm(0x14); // load end offset for end of frame timers
  dec_abs(IntervalTimerControl); // decrement interval timer control,
  if (!neg_flag) { goto DecTimersLoop; } // if not expired, only frame timers will decrement
  lda_imm(0x14);
  ram[IntervalTimerControl] = a; // if control for interval timers expired,
  ldx_imm(0x23); // interval timers will decrement along with frame timers
  
DecTimersLoop:
  lda_absx(Timers); // check current timer
  if (zero_flag) { goto SkipExpTimer; } // if current timer expired, branch to skip,
  dec_absx(Timers); // otherwise decrement the current timer
  
SkipExpTimer:
  dex(); // move onto next timer
  if (!neg_flag) { goto DecTimersLoop; } // do this until all timers are dealt with
  
NoDecTimers:
  inc_zp(FrameCounter); // increment frame counter
  
PauseSkip:
  ldx_imm(0x0);
  ldy_imm(0x7);
  lda_abs(PseudoRandomBitReg); // get first memory location of LSFR bytes
  and_imm(0b00000010); // mask out all but d1
  ram[0x0] = a; // save here
  lda_abs(PseudoRandomBitReg + 1); // get second memory location
  and_imm(0b00000010); // mask out all but d1
  eor_zp(0x0); // perform exclusive-OR on d1 from first and second bytes
  carry_flag = false; // if neither or both are set, carry will be clear
  if (zero_flag) { goto RotPRandomBit; }
  carry_flag = true; // if one or the other is set, carry will be set
  
RotPRandomBit:
  ror_absx(PseudoRandomBitReg); // rotate carry into d7, and rotate last bit into carry
  inx(); // increment to next byte
  dey(); // decrement for loop
  if (!zero_flag) { goto RotPRandomBit; }
  lda_abs(Sprite0HitDetectFlag); // check for flag here
  if (zero_flag) { goto SkipSprite0; }
  
Sprite0Clr:
  lda_abs(PPU_STATUS); // wait for sprite 0 flag to clear, which will
  and_imm(0b01000000); // not happen until vblank has ended
  if (!zero_flag) { goto Sprite0Clr; }
  lda_abs(GamePauseStatus); // if in pause mode, do not bother with sprites at all
  lsr_acc();
  if (carry_flag) { goto Sprite0Hit; }
  MoveSpritesOffscreen();
  SpriteShuffler();
  
Sprite0Hit:
  lda_abs(PPU_STATUS); // do sprite #0 hit detection
  and_imm(0b01000000);
  if (zero_flag) { goto Sprite0Hit; }
  ldy_imm(0x14); // small delay, to wait until we hit horizontal blank time
  
HBlankDelay:
  dey();
  if (!zero_flag) { goto HBlankDelay; }
  
SkipSprite0:
  lda_abs(HorizontalScroll); // set scroll registers from variables
  ppu_write_scroll(a);
  lda_abs(VerticalScroll);
  ppu_write_scroll(a);
  lda_abs(Mirror_PPU_CTRL_REG1); // load saved mirror of $2000
  pha();
  ppu_ctrl = a;
  lda_abs(GamePauseStatus); // if in pause mode, do not perform operation mode stuff
  lsr_acc();
  if (carry_flag) { goto SkipMainOper; }
  OperModeExecutionTree(); // otherwise do one of many, many possible subroutines
  
SkipMainOper:
  lda_abs(PPU_STATUS); // reset flip-flop
  pla();
  ora_imm(0b10000000); // reactivate NMIs
  ppu_ctrl = a;
  return; // <rti> // we are done until the next frame!
  // -------------------------------------------------------------------------------------
}

void PauseRoutine(void) {
  lda_abs(OperMode); // are we in victory mode?
  cmp_imm(VictoryModeValue); // if so, go ahead
  if (zero_flag) { goto ChkPauseTimer; }
  cmp_imm(GameModeValue); // are we in game mode?
  if (!zero_flag) { return; } // if not, leave
  lda_abs(OperMode_Task); // if we are in game mode, are we running game engine?
  cmp_imm(0x3);
  if (!zero_flag) { return; } // if not, leave
  
ChkPauseTimer:
  lda_abs(GamePauseTimer); // check if pause timer is still counting down
  if (zero_flag) { goto ChkStart; }
  dec_abs(GamePauseTimer); // if so, decrement and leave
  return;
  
ChkStart:
  lda_abs(SavedJoypad1Bits); // check to see if start is pressed
  and_imm(Start_Button); // on controller 1
  if (zero_flag) { goto ClrPauseTimer; }
  lda_abs(GamePauseStatus); // check to see if timer flag is set
  and_imm(0b10000000); // and if so, do not reset timer (residual,
  if (!zero_flag) { return; } // joypad reading routine makes this unnecessary)
  lda_imm(0x2b); // set pause timer
  ram[GamePauseTimer] = a;
  lda_abs(GamePauseStatus);
  tay();
  iny(); // set pause sfx queue for next pause mode
  ram[PauseSoundQueue] = y;
  eor_imm(0b00000001); // invert d0 and set d7
  ora_imm(0b10000000);
  if (!zero_flag) { goto SetPause; } // unconditional branch
  
ClrPauseTimer:
  lda_abs(GamePauseStatus); // clear timer flag if timer is at zero and start button
  and_imm(0b01111111); // is not pressed
  
SetPause:
  ram[GamePauseStatus] = a;
  // -------------------------------------------------------------------------------------
  // $00 - used for preset value
}

void SpriteShuffler(void) {
  ldy_abs(AreaType); // load level type, likely residual code
  lda_imm(0x28); // load preset value which will put it at
  ram[0x0] = a; // sprite #10
  ldx_imm(0xe); // start at the end of OAM data offsets
  
ShuffleLoop:
  lda_absx(SprDataOffset); // check for offset value against
  cmp_zp(0x0); // the preset value
  // if less, skip this part
  if (carry_flag) {
    ldy_abs(SprShuffleAmtOffset); // get current offset to preset value we want to add
    carry_flag = false;
    adc_absy(SprShuffleAmt); // get shuffle amount, add to current sprite offset
    // if not exceeded $ff, skip second add
    if (carry_flag) {
      carry_flag = false;
      adc_zp(0x0); // otherwise add preset value $28 to offset
    }
    // StrSprOffset:
    ram[SprDataOffset + x] = a; // store new offset here or old one if branched to here
  }
  // NextSprOffset:
  dex(); // move backwards to next one
  if (!neg_flag) { goto ShuffleLoop; }
  ldx_abs(SprShuffleAmtOffset); // load offset
  inx();
  cpx_imm(0x3); // check if offset + 1 goes to 3
  // if offset + 1 not 3, store
  if (zero_flag) {
    ldx_imm(0x0); // otherwise, init to 0
  }
  // SetAmtOffset:
  ram[SprShuffleAmtOffset] = x;
  ldx_imm(0x8); // load offsets for values and storage
  ldy_imm(0x2);
  
SetMiscOffset:
  lda_absy(SprDataOffset + 5); // load one of three OAM data offsets
  ram[Misc_SprDataOffset - 2 + x] = a; // store first one unmodified, but
  carry_flag = false; // add eight to the second and eight
  adc_imm(0x8); // more to the third one
  ram[Misc_SprDataOffset - 1 + x] = a; // note that due to the way X is set up,
  carry_flag = false; // this code loads into the misc sprite offsets
  adc_imm(0x8);
  ram[Misc_SprDataOffset + x] = a;
  dex();
  dex();
  dex();
  dey();
  if (!neg_flag) { goto SetMiscOffset; } // do this until all misc spr offsets are loaded
  // -------------------------------------------------------------------------------------
}

void OperModeExecutionTree(void) {
  lda_abs(OperMode); // this is the heart of the entire program,
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: TitleScreenMode(); return;
    case 1: GameMode(); return;
    case 2: VictoryMode(); return;
    case 3: GameOverMode(); return;
  }
}

void TitleScreenMode(void) {
  lda_abs(OperMode_Task);
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: InitializeGame(); return;
    case 1: ScreenRoutines(); return;
    case 2: PrimaryGameSetup(); return;
    case 3: GameMenuRoutine(); return;
  }
}

void GameMenuRoutine(void) {
  ldy_imm(0x0);
  lda_abs(SavedJoypad1Bits); // check to see if either player pressed
  ora_abs(SavedJoypad2Bits); // only the start button (either joypad)
  cmp_imm(Start_Button);
  if (zero_flag) { goto StartGame; }
  cmp_imm(A_Button + Start_Button); // check to see if A + start was pressed
  if (!zero_flag) { goto ChkSelect; } // if not, branch to check select button
  
StartGame:
  goto ChkContinue; // if either start or A + start, execute here
  
ChkSelect:
  cmp_imm(Select_Button); // check to see if the select button was pressed
  if (zero_flag) { goto SelectBLogic; } // if so, branch reset demo timer
  ldx_abs(DemoTimer); // otherwise check demo timer
  if (!zero_flag) { goto ChkWorldSel; } // if demo timer not expired, branch to check world selection
  ram[SelectTimer] = a; // set controller bits here if running demo
  DemoEngine(); // run through the demo actions
  if (carry_flag) { goto ResetTitle; } // if carry flag set, demo over, thus branch
  goto RunDemo; // otherwise, run game engine for demo
  
ChkWorldSel:
  ldx_abs(WorldSelectEnableFlag); // check to see if world selection has been enabled
  if (zero_flag) { goto NullJoypad; }
  cmp_imm(B_Button); // if so, check to see if the B button was pressed
  if (!zero_flag) { goto NullJoypad; }
  iny(); // if so, increment Y and execute same code as select
  
SelectBLogic:
  lda_abs(DemoTimer); // if select or B pressed, check demo timer one last time
  if (zero_flag) { goto ResetTitle; } // if demo timer expired, branch to reset title screen mode
  lda_imm(0x18); // otherwise reset demo timer
  ram[DemoTimer] = a;
  lda_abs(SelectTimer); // check select/B button timer
  if (!zero_flag) { goto NullJoypad; } // if not expired, branch
  lda_imm(0x10); // otherwise reset select button timer
  ram[SelectTimer] = a;
  cpy_imm(0x1); // was the B button pressed earlier?  if so, branch
  if (zero_flag) { goto IncWorldSel; } // note this will not be run if world selection is disabled
  lda_abs(NumberOfPlayers); // if no, must have been the select button, therefore
  eor_imm(0b00000001); // change number of players and draw icon accordingly
  ram[NumberOfPlayers] = a;
  DrawMushroomIcon();
  goto NullJoypad;
  
IncWorldSel:
  ldx_abs(WorldSelectNumber); // increment world select number
  inx();
  txa();
  and_imm(0b00000111); // mask out higher bits
  ram[WorldSelectNumber] = a; // store as current world select number
  GoContinue();
  
UpdateShroom:
  lda_absx(WSelectBufferTemplate); // write template for world select in vram buffer
  ram[VRAM_Buffer1 - 1 + x] = a; // do this until all bytes are written
  inx();
  cpx_imm(0x6);
  if (neg_flag) { goto UpdateShroom; }
  ldy_abs(WorldNumber); // get world number from variable and increment for
  iny(); // proper display, and put in blank byte before
  ram[VRAM_Buffer1 + 3] = y; // null terminator
  
NullJoypad:
  lda_imm(0x0); // clear joypad bits for player 1
  ram[SavedJoypad1Bits] = a;
  
RunDemo:
  GameCoreRoutine(); // run game engine
  lda_zp(GameEngineSubroutine); // check to see if we're running lose life routine
  cmp_imm(0x6);
  if (!zero_flag) { return; } // if not, do not do all the resetting below
  
ResetTitle:
  lda_imm(0x0); // reset game modes, disable
  ram[OperMode] = a; // sprite 0 check and disable
  ram[OperMode_Task] = a; // screen output
  ram[Sprite0HitDetectFlag] = a;
  inc_abs(DisableScreenFlag);
  return;
  
ChkContinue:
  ldy_abs(DemoTimer); // if timer for demo has expired, reset modes
  if (zero_flag) { goto ResetTitle; }
  asl_acc(); // check to see if A button was also pushed
  if (!carry_flag) { goto StartWorld1; } // if not, don't load continue function's world number
  lda_abs(ContinueWorld); // load previously saved world number for secret
  GoContinue(); // continue function when pressing A + start
  
StartWorld1:
  LoadAreaPointer();
  inc_abs(Hidden1UpFlag); // set 1-up box flag for both players
  inc_abs(OffScr_Hidden1UpFlag);
  inc_abs(FetchNewGameTimerFlag); // set fetch new game timer flag
  inc_abs(OperMode); // set next game mode
  lda_abs(WorldSelectEnableFlag); // if world select flag is on, then primary
  ram[PrimaryHardMode] = a; // hard mode must be on as well
  lda_imm(0x0);
  ram[OperMode_Task] = a; // set game mode here, and clear demo timer
  ram[DemoTimer] = a;
  ldx_imm(0x17);
  lda_imm(0x0);
  
InitScores:
  ram[ScoreAndCoinDisplay + x] = a; // clear player scores and coin displays
  dex();
  if (!neg_flag) { goto InitScores; }
}

void VictoryMode(void) {
  VictoryModeSubroutines(); // run victory mode subroutines
  lda_abs(OperMode_Task); // get current task of victory mode
  // if on bridge collapse, skip enemy processing
  if (!zero_flag) {
    ldx_imm(0x0);
    ram[ObjectOffset] = x; // otherwise reset enemy object offset
    EnemiesAndLoopsCore(); // and run enemy code
  }
  // AutoPlayer:
  RelativePlayerPosition(); // get player's relative coordinates
  PlayerGfxHandler(); return; // draw the player, then leave
}

void ScreenRoutines(void) {
  lda_abs(ScreenRoutineTask); // run one of the following subroutines
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: InitScreen(); return;
    case 1: SetupIntermediate(); return;
    case 2: WriteTopStatusLine(); return;
    case 3: WriteBottomStatusLine(); return;
    case 4: DisplayTimeUp(); return;
    case 5: ResetSpritesAndScreenTimer(); return;
    case 6: DisplayIntermediate(); return;
    case 7: ResetSpritesAndScreenTimer(); return;
    case 8: AreaParserTaskControl(); return;
    case 9: GetAreaPalette(); return;
    case 10: GetBackgroundColor(); return;
    case 11: GetAlternatePalette1(); return;
    case 12: DrawTitleScreen(); return;
    case 13: ClearBuffersDrawIcon(); return;
    case 14: WriteTopScore(); return;
  }
}

void InitScreen(void) {
  MoveAllSpritesOffscreen(); // initialize all sprites including sprite #0
  InitializeNameTables(); // and erase both name and attribute tables
  lda_abs(OperMode);
  // if mode still 0, do not load
  if (zero_flag) {
    NextSubtask();
    return;
  }
  ldx_imm(0x3); // into buffer pointer
  SetVRAMAddr_A();
  // -------------------------------------------------------------------------------------
}

void SetupIntermediate(void) {
  lda_abs(BackgroundColorCtrl); // save current background color control
  pha(); // and player status to stack
  lda_abs(PlayerStatus);
  pha();
  lda_imm(0x0); // set background color to black
  ram[PlayerStatus] = a; // and player status to not fiery
  lda_imm(0x2); // this is the ONLY time background color control
  ram[BackgroundColorCtrl] = a; // is set to less than 4
  GetPlayerColors();
  pla(); // we only execute this routine for
  ram[PlayerStatus] = a; // the intermediate lives display
  pla(); // and once we're done, we return bg
  ram[BackgroundColorCtrl] = a; // color ctrl and player status from stack
  IncSubtask(); // then move onto the next task
  // -------------------------------------------------------------------------------------
}

void GetAreaPalette(void) {
  ldy_abs(AreaType); // select appropriate palette to load
  ldx_absy(AreaPalette); // based on area type
  SetVRAMAddr_A(); // <fallthrough>
}

void SetVRAMAddr_A(void) {
  ram[VRAM_Buffer_AddrCtrl] = x; // store offset into buffer control
  NextSubtask();
}

void NextSubtask(void) {
  IncSubtask(); // move onto next task
  // -------------------------------------------------------------------------------------
  // $00 - used as temp counter in GetPlayerColors
}

void GetBackgroundColor(void) {
  ldy_abs(BackgroundColorCtrl); // check background color control
  // if not set, increment task and fetch palette
  if (!zero_flag) {
    lda_absy(BGColorCtrl_Addr - 4); // put appropriate palette into vram
    ram[VRAM_Buffer_AddrCtrl] = a; // note that if set to 5-7, $0301 will not be read
  }
  // NoBGColor:
  inc_abs(ScreenRoutineTask); // increment to next subtask and plod on through
  GetPlayerColors(); // <fallthrough>
}

void GetPlayerColors(void) {
  ldx_abs(VRAM_Buffer1_Offset); // get current buffer offset
  ldy_imm(0x0);
  lda_abs(CurrentPlayer); // check which player is on the screen
  if (!zero_flag) {
    ldy_imm(0x4); // load offset for luigi
  }
  // ChkFiery:
  lda_abs(PlayerStatus); // check player status
  cmp_imm(0x2);
  // if fiery, load alternate offset for fiery player
  if (zero_flag) {
    ldy_imm(0x8);
  }
  // StartClrGet:
  lda_imm(0x3); // do four colors
  ram[0x0] = a;
  
ClrGetLoop:
  lda_absy(PlayerColors); // fetch player colors and store them
  ram[VRAM_Buffer1 + 3 + x] = a; // in the buffer
  iny();
  inx();
  dec_zp(0x0);
  if (!neg_flag) { goto ClrGetLoop; }
  ldx_abs(VRAM_Buffer1_Offset); // load original offset from before
  ldy_abs(BackgroundColorCtrl); // if this value is four or greater, it will be set
  // therefore use it as offset to background color
  if (zero_flag) {
    ldy_abs(AreaType); // otherwise use area type bits from area offset as offset
  }
  // SetBGColor:
  lda_absy(BackgroundColors); // to background color instead
  ram[VRAM_Buffer1 + 3 + x] = a;
  lda_imm(0x3f); // set for sprite palette address
  ram[VRAM_Buffer1 + x] = a; // save to buffer
  lda_imm(0x10);
  ram[VRAM_Buffer1 + 1 + x] = a;
  lda_imm(0x4); // write length byte to buffer
  ram[VRAM_Buffer1 + 2 + x] = a;
  lda_imm(0x0); // now the null terminator
  ram[VRAM_Buffer1 + 7 + x] = a;
  txa(); // move the buffer pointer ahead 7 bytes
  carry_flag = false; // in case we want to write anything else later
  adc_imm(0x7);
  SetVRAMOffset(); // <fallthrough>
}

void SetVRAMOffset(void) {
  ram[VRAM_Buffer1_Offset] = a; // store as new vram buffer offset
  // -------------------------------------------------------------------------------------
}

void GetAlternatePalette1(void) {
  lda_abs(AreaStyle); // check for mushroom level style
  cmp_imm(0x1);
  if (!zero_flag) {
    NoAltPal();
    return;
  }
  lda_imm(0xb); // if found, load appropriate palette
  SetVRAMAddr_B(); // <fallthrough>
}

void SetVRAMAddr_B(void) {
  ram[VRAM_Buffer_AddrCtrl] = a;
  NoAltPal();
}

void NoAltPal(void) {
  IncSubtask(); // now onto the next task
  // -------------------------------------------------------------------------------------
}

void WriteTopStatusLine(void) {
  lda_imm(0x0); // select main status bar
  WriteGameText(); // output it
  IncSubtask(); // onto the next task
  // -------------------------------------------------------------------------------------
}

void WriteBottomStatusLine(void) {
  GetSBNybbles(); // write player's score and coin tally to screen
  ldx_abs(VRAM_Buffer1_Offset);
  lda_imm(0x20); // write address for world-area number on screen
  ram[VRAM_Buffer1 + x] = a;
  lda_imm(0x73);
  ram[VRAM_Buffer1 + 1 + x] = a;
  lda_imm(0x3); // write length for it
  ram[VRAM_Buffer1 + 2 + x] = a;
  ldy_abs(WorldNumber); // first the world number
  iny();
  tya();
  ram[VRAM_Buffer1 + 3 + x] = a;
  lda_imm(0x28); // next the dash
  ram[VRAM_Buffer1 + 4 + x] = a;
  ldy_abs(LevelNumber); // next the level number
  iny(); // increment for proper number display
  tya();
  ram[VRAM_Buffer1 + 5 + x] = a;
  lda_imm(0x0); // put null terminator on
  ram[VRAM_Buffer1 + 6 + x] = a;
  txa(); // move the buffer offset up by 6 bytes
  carry_flag = false;
  adc_imm(0x6);
  ram[VRAM_Buffer1_Offset] = a;
  IncSubtask();
  // -------------------------------------------------------------------------------------
}

void DisplayTimeUp(void) {
  lda_abs(GameTimerExpiredFlag); // if game timer not expired, increment task
  // control 2 tasks forward, otherwise, stay here
  if (!zero_flag) {
    lda_imm(0x0);
    ram[GameTimerExpiredFlag] = a; // reset timer expiration flag
    lda_imm(0x2); // output time-up screen to buffer
    OutputInter();
    return;
  }
  // NoTimeUp:
  inc_abs(ScreenRoutineTask); // increment control task 2 tasks forward
  IncSubtask();
  // -------------------------------------------------------------------------------------
}

void DisplayIntermediate(void) {
  lda_abs(OperMode); // check primary mode of operation
  // if in title screen mode, skip this
  if (zero_flag) {
    NoInter();
    return;
  }
  cmp_imm(GameOverModeValue); // are we in game over mode?
  // if so, proceed to display game over screen
  if (zero_flag) {
    GameOverInter();
    return;
  }
  lda_abs(AltEntranceControl); // otherwise check for mode of alternate entry
  // and branch if found
  if (!zero_flag) {
    NoInter();
    return;
  }
  ldy_abs(AreaType); // check if we are on castle level
  cpy_imm(0x3); // and if so, branch (possibly residual)
  if (!zero_flag) {
    lda_abs(DisableIntermediate); // if this flag is set, skip intermediate lives display
    // and jump to specific task, otherwise
    if (!zero_flag) {
      NoInter();
      return;
    }
  }
  // PlayerInter:
  DrawPlayer_Intermediate(); // put player in appropriate place for
  lda_imm(0x1); // lives display, then output lives display to buffer
  OutputInter();
}

void GameOverInter(void) {
  lda_imm(0x12); // set screen timer
  ram[ScreenTimer] = a;
  lda_imm(0x3); // output game over screen to buffer
  WriteGameText();
  IncModeTask_B();
}

void NoInter(void) {
  lda_imm(0x8); // set for specific task and leave
  ram[ScreenRoutineTask] = a;
  // -------------------------------------------------------------------------------------
}

void AreaParserTaskControl(void) {
  inc_abs(DisableScreenFlag); // turn off screen
  
TaskLoop:
  AreaParserTaskHandler(); // render column set of current area
  lda_abs(AreaParserTaskNum); // check number of tasks
  if (!zero_flag) { goto TaskLoop; } // if tasks still not all done, do another one
  dec_abs(ColumnSets); // do we need to render more column sets?
  if (neg_flag) {
    inc_abs(ScreenRoutineTask); // if not, move on to the next task
  }
  // OutputCol:
  lda_imm(0x6); // set vram buffer to output rendered column set
  ram[VRAM_Buffer_AddrCtrl] = a; // on next NMI
  // -------------------------------------------------------------------------------------
  // $00 - vram buffer address table low
  // $01 - vram buffer address table high
}

void DrawTitleScreen(void) {
  lda_abs(OperMode); // are we in title screen mode?
  // if not, exit
  if (!zero_flag) {
    IncModeTask_B();
    return;
  }
  lda_imm(HIGH_BYTE(TitleScreenDataOffset)); // load address $1ec0 into
  ppu_write_address(a); // the vram address register
  lda_imm(LOW_BYTE(TitleScreenDataOffset));
  ppu_write_address(a);
  lda_imm(0x3); // put address $0300 into
  ram[0x1] = a; // the indirect at $00
  ldy_imm(0x0);
  ram[0x0] = y;
  lda_abs(PPU_DATA); // do one garbage read
  
OutputTScr:
  lda_abs(PPU_DATA); // get title screen from chr-rom
  dynamic_ram_write(read_word(0x0) + y, a); // store 256 bytes into buffer
  iny();
  // if not past 256 bytes, do not increment
  if (zero_flag) {
    inc_zp(0x1); // otherwise increment high byte of indirect
  }
  // ChkHiByte:
  lda_zp(0x1); // check high byte?
  cmp_imm(0x4); // at $0400?
  if (!zero_flag) { goto OutputTScr; } // if not, loop back and do another
  cpy_imm(0x3a); // check if offset points past end of data
  if (!carry_flag) { goto OutputTScr; } // if not, loop back and do another
  lda_imm(0x5); // set buffer transfer control to $0300,
  SetVRAMAddr_B(); // increment task and exit
  // -------------------------------------------------------------------------------------
}

void ClearBuffersDrawIcon(void) {
  lda_abs(OperMode); // check game mode
  // if not title screen mode, leave
  if (!zero_flag) {
    IncModeTask_B();
    return;
  }
  ldx_imm(0x0); // otherwise, clear buffer space
  
TScrClear:
  ram[VRAM_Buffer1 - 1 + x] = a;
  ram[VRAM_Buffer1 - 1 + 0x100 + x] = a;
  dex();
  if (!zero_flag) { goto TScrClear; }
  DrawMushroomIcon(); // draw player select icon
  IncSubtask(); // <fallthrough>
}

void IncSubtask(void) {
  inc_abs(ScreenRoutineTask); // move onto next task
  // -------------------------------------------------------------------------------------
}

void WriteTopScore(void) {
  lda_imm(0xfa); // run display routine to display top score on title
  UpdateNumber();
  IncModeTask_B();
}

void IncModeTask_B(void) {
  inc_abs(OperMode_Task); // move onto next mode
  // -------------------------------------------------------------------------------------
}

void ResetSpritesAndScreenTimer(void) {
  lda_abs(ScreenTimer); // check if screen timer has expired
  if (zero_flag) {
    MoveAllSpritesOffscreen(); // otherwise reset sprites now
    ResetScreenTimer();
  }
}

void InitializeGame(void) {
  ldy_imm(0x6f); // clear all memory as in initialization procedure,
  InitializeMemory(); // but this time, clear only as far as $076f
  ldy_imm(0x1f);
  
ClrSndLoop:
  ram[SoundMemory + y] = a; // clear out memory used
  dey(); // by the sound engines
  if (!neg_flag) { goto ClrSndLoop; }
  lda_imm(0x18); // set demo timer
  ram[DemoTimer] = a;
  LoadAreaPointer();
  InitializeArea(); // <fallthrough>
}

void InitializeArea(void) {
  ldy_imm(0x4b); // clear all memory again, only as far as $074b
  InitializeMemory(); // this is only necessary if branching from
  ldx_imm(0x21);
  lda_imm(0x0);
  
ClrTimersLoop:
  ram[Timers + x] = a; // clear out memory between
  dex(); // $0780 and $07a1
  if (!neg_flag) { goto ClrTimersLoop; }
  lda_abs(HalfwayPage);
  ldy_abs(AltEntranceControl); // if AltEntranceControl not set, use halfway page, if any found
  if (zero_flag) { goto StartPage; }
  lda_abs(EntrancePage); // otherwise use saved entry page number here
  
StartPage:
  ram[ScreenLeft_PageLoc] = a; // set as value here
  ram[CurrentPageLoc] = a; // also set as current page
  ram[BackloadingFlag] = a; // set flag here if halfway page or saved entry page number found
  GetScreenPosition(); // get pixel coordinates for screen borders
  ldy_imm(0x20); // if on odd numbered page, use $2480 as start of rendering
  and_imm(0b00000001); // otherwise use $2080, this address used later as name table
  if (zero_flag) { goto SetInitNTHigh; } // address for rendering of game area
  ldy_imm(0x24);
  
SetInitNTHigh:
  ram[CurrentNTAddr_High] = y; // store name table address
  ldy_imm(0x80);
  ram[CurrentNTAddr_Low] = y;
  asl_acc(); // store LSB of page number in high nybble
  asl_acc(); // of block buffer column position
  asl_acc();
  asl_acc();
  ram[BlockBufferColumnPos] = a;
  dec_abs(AreaObjectLength); // set area object lengths for all empty
  dec_abs(AreaObjectLength + 1);
  dec_abs(AreaObjectLength + 2);
  lda_imm(0xb); // set value for renderer to update 12 column sets
  ram[ColumnSets] = a; // 12 column sets = 24 metatile columns = 1 1/2 screens
  GetAreaDataAddrs(); // get enemy and level addresses and load header
  lda_abs(PrimaryHardMode); // check to see if primary hard mode has been activated
  if (!zero_flag) { goto SetSecHard; } // if so, activate the secondary no matter where we're at
  lda_abs(WorldNumber); // otherwise check world number
  cmp_imm(World5); // if less than 5, do not activate secondary
  if (!carry_flag) { goto CheckHalfway; }
  if (!zero_flag) { goto SetSecHard; } // if not equal to, then world > 5, thus activate
  lda_abs(LevelNumber); // otherwise, world 5, so check level number
  cmp_imm(Level3); // if 1 or 2, do not set secondary hard mode flag
  if (!carry_flag) { goto CheckHalfway; }
  
SetSecHard:
  inc_abs(SecondaryHardMode); // set secondary hard mode flag for areas 5-3 and beyond
  
CheckHalfway:
  lda_abs(HalfwayPage);
  if (zero_flag) { goto DoneInitArea; }
  lda_imm(0x2); // if halfway page set, overwrite start position from header
  ram[PlayerEntranceCtrl] = a;
  
DoneInitArea:
  lda_imm(Silence); // silence music
  ram[AreaMusicQueue] = a;
  lda_imm(0x1); // disable screen output
  ram[DisableScreenFlag] = a;
  inc_abs(OperMode_Task); // increment one of the modes
  // -------------------------------------------------------------------------------------
}

void PrimaryGameSetup(void) {
  lda_imm(0x1);
  ram[FetchNewGameTimerFlag] = a; // set flag to load game timer from header
  ram[PlayerSize] = a; // set player's size to small
  lda_imm(0x2);
  ram[NumberofLives] = a; // give each player three lives
  ram[OffScr_NumberofLives] = a;
  SecondaryGameSetup(); // <fallthrough>
}

void SecondaryGameSetup(void) {
  lda_imm(0x0);
  ram[DisableScreenFlag] = a; // enable screen output
  tay();
  
ClearVRLoop:
  ram[VRAM_Buffer1 - 1 + y] = a; // clear buffer at $0300-$03ff
  iny();
  if (!zero_flag) { goto ClearVRLoop; }
  ram[GameTimerExpiredFlag] = a; // clear game timer exp flag
  ram[DisableIntermediate] = a; // clear skip lives display flag
  ram[BackloadingFlag] = a; // clear value here
  lda_imm(0xff);
  ram[BalPlatformAlignment] = a; // initialize balance platform assignment flag
  lda_abs(ScreenLeft_PageLoc); // get left side page location
  lsr_abs(Mirror_PPU_CTRL_REG1); // shift LSB of ppu register #1 mirror out
  and_imm(0x1); // mask out all but LSB of page location
  ror_acc(); // rotate LSB of page location into carry then onto mirror
  rol_abs(Mirror_PPU_CTRL_REG1); // this is to set the proper PPU name table
  GetAreaMusic(); // load proper music into queue
  lda_imm(0x38); // load sprite shuffle amounts to be used later
  ram[SprShuffleAmt + 2] = a;
  lda_imm(0x48);
  ram[SprShuffleAmt + 1] = a;
  lda_imm(0x58);
  ram[SprShuffleAmt] = a;
  ldx_imm(0xe); // load default OAM offsets into $06e4-$06f2
  
ShufAmtLoop:
  lda_absx(DefaultSprOffsets);
  ram[SprDataOffset + x] = a;
  dex(); // do this until they're all set
  if (!neg_flag) { goto ShufAmtLoop; }
  ldy_imm(0x3); // set up sprite #0
  
ISpr0Loop:
  lda_absy(Sprite0Data);
  ram[Sprite_Data + y] = a;
  dey();
  if (!neg_flag) { goto ISpr0Loop; }
  DoNothing2(); // these jsrs doesn't do anything useful
  DoNothing1();
  inc_abs(Sprite0HitDetectFlag); // set sprite #0 check flag
  inc_abs(OperMode_Task); // increment to next task
  // -------------------------------------------------------------------------------------
  // $06 - RAM address low
  // $07 - RAM address high
}

void GameOverMode(void) {
  lda_abs(OperMode_Task);
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: SetupGameOver(); return;
    case 1: ScreenRoutines(); return;
    case 2: RunGameOver(); return;
  }
}

void SetupGameOver(void) {
  lda_imm(0x0); // reset screen routine task control for title screen, game,
  ram[ScreenRoutineTask] = a; // and game over modes
  ram[Sprite0HitDetectFlag] = a; // disable sprite 0 check
  lda_imm(GameOverMusic);
  ram[EventMusicQueue] = a; // put game over music in secondary queue
  inc_abs(DisableScreenFlag); // disable screen output
  inc_abs(OperMode_Task); // set secondary mode to 1
  // -------------------------------------------------------------------------------------
}

void RunGameOver(void) {
  lda_imm(0x0); // reenable screen
  ram[DisableScreenFlag] = a;
  lda_abs(SavedJoypad1Bits); // check controller for start pressed
  and_imm(Start_Button);
  if (!zero_flag) {
    TerminateGame();
    return;
  }
  lda_abs(ScreenTimer); // if not pressed, wait for
  if (zero_flag) {
    TerminateGame();
  }
}

void TerminateGame(void) {
  lda_imm(Silence); // silence music
  ram[EventMusicQueue] = a;
  TransposePlayers(); // check if other player can keep
  // going, and do so if possible
  if (!carry_flag) {
    ContinueGame();
    return;
  }
  lda_abs(WorldNumber); // otherwise put world number of current
  ram[ContinueWorld] = a; // player into secret continue function variable
  lda_imm(0x0);
  asl_acc(); // residual ASL instruction
  ram[OperMode_Task] = a; // reset all modes to title screen and
  ram[ScreenTimer] = a; // leave
  ram[OperMode] = a;
}

void ContinueGame(void) {
  LoadAreaPointer(); // update level pointer with
  lda_imm(0x1); // actual world and area numbers, then
  ram[PlayerSize] = a; // reset player's size, status, and
  inc_abs(FetchNewGameTimerFlag); // set game timer flag to reload
  lda_imm(0x0); // game timer from header
  ram[TimerControl] = a; // also set flag for timers to count again
  ram[PlayerStatus] = a;
  ram[GameEngineSubroutine] = a; // reset task for game core
  ram[OperMode_Task] = a; // set modes and leave
  lda_imm(0x1); // if in game over mode, switch back to
  ram[OperMode] = a; // game mode, because game is still on
}

void GameMode(void) {
  lda_abs(OperMode_Task);
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: InitializeArea(); return;
    case 1: ScreenRoutines(); return;
    case 2: SecondaryGameSetup(); return;
    case 3: GameCoreRoutine(); return;
  }
}

void GameCoreRoutine(void) {
  ldx_abs(CurrentPlayer); // get which player is on the screen
  lda_absx(SavedJoypadBits); // use appropriate player's controller bits
  ram[SavedJoypadBits] = a; // as the master controller bits
  GameRoutines(); // execute one of many possible subs
  lda_abs(OperMode_Task); // check major task of operating mode
  cmp_imm(0x3); // if we are supposed to be here,
  if (carry_flag) { goto GameEngine; } // branch to the game engine itself
  return;
  
GameEngine:
  ProcFireball_Bubble(); // process fireballs and air bubbles
  ldx_imm(0x0);
  
ProcELoop:
  ram[ObjectOffset] = x; // put incremented offset in X as enemy object offset
  EnemiesAndLoopsCore(); // process enemy objects
  FloateyNumbersRoutine(); // process floatey numbers
  inx();
  cpx_imm(0x6); // do these two subroutines until the whole buffer is done
  if (!zero_flag) { goto ProcELoop; }
  GetPlayerOffscreenBits(); // get offscreen bits for player object
  RelativePlayerPosition(); // get relative coordinates for player object
  PlayerGfxHandler(); // draw the player
  BlockObjMT_Updater(); // replace block objects with metatiles if necessary
  ldx_imm(0x1);
  ram[ObjectOffset] = x; // set offset for second
  BlockObjectsCore(); // process second block object
  dex();
  ram[ObjectOffset] = x; // set offset for first
  BlockObjectsCore(); // process first block object
  MiscObjectsCore(); // process misc objects (hammer, jumping coins)
  ProcessCannons(); // process bullet bill cannons
  ProcessWhirlpools(); // process whirlpools
  FlagpoleRoutine(); // process the flagpole
  RunGameTimer(); // count down the game timer
  ColorRotation(); // cycle one of the background colors
  lda_zp(Player_Y_HighPos);
  cmp_imm(0x2); // if player is below the screen, don't bother with the music
  if (!neg_flag) { goto NoChgMus; }
  lda_abs(StarInvincibleTimer); // if star mario invincibility timer at zero,
  if (zero_flag) { goto ClrPlrPal; } // skip this part
  cmp_imm(0x4);
  if (!zero_flag) { goto NoChgMus; } // if not yet at a certain point, continue
  lda_abs(IntervalTimerControl); // if interval timer not yet expired,
  if (!zero_flag) { goto NoChgMus; } // branch ahead, don't bother with the music
  GetAreaMusic(); // to re-attain appropriate level music
  
NoChgMus:
  ldy_abs(StarInvincibleTimer); // get invincibility timer
  lda_zp(FrameCounter); // get frame counter
  cpy_imm(0x8); // if timer still above certain point,
  if (carry_flag) { goto CycleTwo; } // branch to cycle player's palette quickly
  lsr_acc(); // otherwise, divide by 8 to cycle every eighth frame
  lsr_acc();
  
CycleTwo:
  lsr_acc(); // if branched here, divide by 2 to cycle every other frame
  CyclePlayerPalette(); // do sub to cycle the palette (note: shares fire flower code)
  goto SaveAB; // then skip this sub to finish up the game engine
  
ClrPlrPal:
  ResetPalStar(); // do sub to clear player's palette bits in attributes
  
SaveAB:
  lda_zp(A_B_Buttons); // save current A and B button
  ram[PreviousA_B_Buttons] = a; // into temp variable to be used on next frame
  lda_imm(0x0);
  ram[Left_Right_Buttons] = a; // nullify left and right buttons temp variable
  UpdScrollVar(); // <fallthrough>
}

void UpdScrollVar(void) {
  lda_abs(VRAM_Buffer_AddrCtrl);
  cmp_imm(0x6); // if vram address controller set to 6 (one of two $0341s)
  if (zero_flag) { return; } // then branch to leave
  lda_abs(AreaParserTaskNum); // otherwise check number of tasks
  if (!zero_flag) { goto RunParser; }
  lda_abs(ScrollThirtyTwo); // get horizontal scroll in 0-31 or $00-$20 range
  cmp_imm(0x20); // check to see if exceeded $21
  if (neg_flag) { return; } // branch to leave if not
  lda_abs(ScrollThirtyTwo);
  sbc_imm(0x20); // otherwise subtract $20 to set appropriately
  ram[ScrollThirtyTwo] = a; // and store
  lda_imm(0x0); // reset vram buffer offset used in conjunction with
  ram[VRAM_Buffer2_Offset] = a; // level graphics buffer at $0341-$035f
  
RunParser:
  AreaParserTaskHandler(); // update the name table with more level graphics
  // -------------------------------------------------------------------------------------
}

void PlayerGfxHandler(void) {
  lda_abs(InjuryTimer); // if player's injured invincibility timer
  if (zero_flag) { goto CntPl; } // not set, skip checkpoint and continue code
  lda_zp(FrameCounter);
  lsr_acc(); // otherwise check frame counter and branch
  if (carry_flag) { return; } // to leave on every other frame (when d0 is set)
  
CntPl:
  lda_zp(GameEngineSubroutine); // if executing specific game engine routine,
  cmp_imm(0xb); // branch ahead to some other part
  if (zero_flag) { PlayerKilled(); return; }
  lda_abs(PlayerChangeSizeFlag); // if grow/shrink flag set
  if (!zero_flag) { DoChangeSize(); return; } // then branch to some other code
  ldy_abs(SwimmingFlag); // if swimming flag set, branch to
  if (zero_flag) { FindPlayerAction(); return; } // different part, do not return
  lda_zp(Player_State);
  cmp_imm(0x0); // if player status normal,
  if (zero_flag) { FindPlayerAction(); return; } // branch and do not return
  FindPlayerAction(); // otherwise jump and return
  lda_zp(FrameCounter);
  and_imm(0b00000100); // check frame counter for d2 set (8 frames every
  if (!zero_flag) { return; } // eighth frame), and branch if set to leave
  tax(); // initialize X to zero
  ldy_abs(Player_SprDataOffset); // get player sprite data offset
  lda_zp(PlayerFacingDir); // get player's facing direction
  lsr_acc();
  if (carry_flag) { goto SwimKT; } // if player facing to the right, use current offset
  iny();
  iny(); // otherwise move to next OAM data
  iny();
  iny();
  
SwimKT:
  lda_abs(PlayerSize); // check player's size
  if (zero_flag) { goto BigKTS; } // if big, use first tile
  lda_absy(Sprite_Tilenumber + 24); // check tile number of seventh/eighth sprite
  cmp_abs(SwimTileRepOffset); // against tile number in player graphics table
  if (zero_flag) { return; } // if spr7/spr8 tile number = value, branch to leave
  inx(); // otherwise increment X for second tile
  
BigKTS:
  lda_absx(SwimKickTileNum); // overwrite tile number in sprite 7/8
  ram[Sprite_Tilenumber + 24 + y] = a; // to animate player's feet when swimming
}

void FindPlayerAction(void) {
  ProcessPlayerAction(); // find proper offset to graphics table by player's actions
  PlayerGfxProcessing(); // draw player, then process for fireball throwing
}

void DoChangeSize(void) {
  HandleChangeSize(); // find proper offset to graphics table for grow/shrink
  PlayerGfxProcessing(); // draw player, then process for fireball throwing
}

void PlayerKilled(void) {
  ldy_imm(0xe); // load offset for player killed
  lda_absy(PlayerGfxTblOffsets); // get offset to graphics table
  PlayerGfxProcessing(); // <fallthrough>
}

void PlayerGfxProcessing(void) {
  ram[PlayerGfxOffset] = a; // store offset to graphics table here
  lda_imm(0x4);
  RenderPlayerSub(); // draw player based on offset loaded
  ChkForPlayerAttrib(); // set horizontal flip bits as necessary
  lda_abs(FireballThrowingTimer);
  // if fireball throw timer not set, skip to the end
  if (!zero_flag) {
    ldy_imm(0x0); // set value to initialize by default
    lda_abs(PlayerAnimTimer); // get animation frame timer
    cmp_abs(FireballThrowingTimer); // compare to fireball throw timer
    ram[FireballThrowingTimer] = y; // initialize fireball throw timer
    // if animation frame timer => fireball throw timer skip to end
    if (!carry_flag) {
      ram[FireballThrowingTimer] = a; // otherwise store animation timer into fireball throw timer
      ldy_imm(0x7); // load offset for throwing
      lda_absy(PlayerGfxTblOffsets); // get offset to graphics table
      ram[PlayerGfxOffset] = a; // store it for use later
      ldy_imm(0x4); // set to update four sprite rows by default
      lda_zp(Player_X_Speed);
      ora_zp(Left_Right_Buttons); // check for horizontal speed or left/right button press
      // if no speed or button press, branch using set value in Y
      if (!zero_flag) {
        dey(); // otherwise set to update only three sprite rows
      }
      // SUpdR:
      tya(); // save in A for use
      RenderPlayerSub(); // in sub, draw player object again
    }
  }
  // PlayerOffscreenChk:
  lda_abs(Player_OffscreenBits); // get player's offscreen bits
  lsr_acc();
  lsr_acc(); // move vertical bits to low nybble
  lsr_acc();
  lsr_acc();
  ram[0x0] = a; // store here
  ldx_imm(0x3); // check all four rows of player sprites
  lda_abs(Player_SprDataOffset); // get player's sprite data offset
  carry_flag = false;
  adc_imm(0x18); // add 24 bytes to start at bottom row
  tay(); // set as offset here
  
PROfsLoop:
  lda_imm(0xf8); // load offscreen Y coordinate just in case
  lsr_zp(0x0); // shift bit into carry
  // if bit not set, skip, do not move sprites
  if (carry_flag) {
    DumpTwoSpr(); // otherwise dump offscreen Y coordinate into sprite data
  }
  // NPROffscr:
  tya();
  carry_flag = true; // subtract eight bytes to do
  sbc_imm(0x8); // next row up
  tay();
  dex(); // decrement row counter
  if (!neg_flag) { goto PROfsLoop; } // do this until all sprite rows are checked
  // -------------------------------------------------------------------------------------
}

void MoveAllSpritesOffscreen(void) {
  ldy_imm(0x0); // this routine moves all sprites off the screen
  //  in multiple places, the bit absolute ($2c) instruction opcode is used to skip the next instruction using only one byte
  MoveSpritesOffscreenSkip(); //  .db $2c ;BIT instruction opcode
}

void MoveSpritesOffscreen(void) {
  ldy_imm(0x4); // this routine moves all but sprite 0
  MoveSpritesOffscreenSkip();
}

void MoveSpritesOffscreenSkip(void) {
  lda_imm(0xf8); // off the screen
  
SprInitLoop:
  ram[Sprite_Y_Position + y] = a; // write 248 into OAM data's Y coordinate
  iny(); // which will move it off the screen
  iny();
  iny();
  iny();
  if (!zero_flag) { goto SprInitLoop; }
  // -------------------------------------------------------------------------------------
}

void GoContinue(void) {
  ram[WorldNumber] = a; // start both players at the first area
  ram[OffScr_WorldNumber] = a; // of the previously saved world number
  ldx_imm(0x0); // note that on power-up using this function
  ram[AreaNumber] = x; // will make no difference
  ram[OffScr_AreaNumber] = x;
  // -------------------------------------------------------------------------------------
}

void DrawMushroomIcon(void) {
  ldy_imm(0x7); // read eight bytes to be read by transfer routine
  
IconDataRead:
  lda_absy(MushroomIconData); // note that the default position is set for a
  ram[VRAM_Buffer1 - 1 + y] = a; // 1-player game
  dey();
  if (!neg_flag) { goto IconDataRead; }
  lda_abs(NumberOfPlayers); // check number of players
  if (!zero_flag) {
    lda_imm(0x24); // otherwise, load blank tile in 1-player position
    ram[VRAM_Buffer1 + 3] = a;
    lda_imm(0xce); // then load shroom icon tile in 2-player position
    ram[VRAM_Buffer1 + 5] = a;
    // -------------------------------------------------------------------------------------
  }
}

void DemoEngine(void) {
  ldx_abs(DemoAction); // load current demo action
  lda_abs(DemoActionTimer); // load current action timer
  if (!zero_flag) { goto DoAction; } // if timer still counting down, skip
  inx();
  inc_abs(DemoAction); // if expired, increment action, X, and
  carry_flag = true; // set carry by default for demo over
  lda_absx(DemoTimingData - 1); // get next timer
  ram[DemoActionTimer] = a; // store as current timer
  if (zero_flag) { return; } // if timer already at zero, skip
  
DoAction:
  lda_absx(DemoActionData - 1); // get and perform action (current or next)
  ram[SavedJoypad1Bits] = a;
  dec_abs(DemoActionTimer); // decrement action timer
  carry_flag = false; // clear carry if demo still going
  // -------------------------------------------------------------------------------------
}

void VictoryModeSubroutines(void) {
  lda_abs(OperMode_Task);
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: BridgeCollapse(); return;
    case 1: SetupVictoryMode(); return;
    case 2: PlayerVictoryWalk(); return;
    case 3: PrintVictoryMessages(); return;
    case 4: PlayerEndWorld(); return;
  }
}

void SetupVictoryMode(void) {
  ldx_abs(ScreenRight_PageLoc); // get page location of right side of screen
  inx(); // increment to next page
  ram[DestinationPageLoc] = x; // store here
  lda_imm(EndOfCastleMusic);
  ram[EventMusicQueue] = a; // play win castle music
  IncModeTask_B(); // jump to set next major task in victory mode
  // -------------------------------------------------------------------------------------
}

void PlayerVictoryWalk(void) {
  ldy_imm(0x0); // set value here to not walk player by default
  ram[VictoryWalkControl] = y;
  lda_zp(Player_PageLoc); // get player's page location
  cmp_zp(DestinationPageLoc); // compare with destination page location
  if (!zero_flag) { goto PerformWalk; } // if page locations don't match, branch
  lda_zp(Player_X_Position); // otherwise get player's horizontal position
  cmp_imm(0x60); // compare with preset horizontal position
  if (carry_flag) { goto DontWalk; } // if still on other page, branch ahead
  
PerformWalk:
  inc_zp(VictoryWalkControl); // otherwise increment value and Y
  iny(); // note Y will be used to walk the player
  
DontWalk:
  tya(); // put contents of Y in A and
  AutoControlPlayer(); // use A to move player to the right or not
  lda_abs(ScreenLeft_PageLoc); // check page location of left side of screen
  cmp_zp(DestinationPageLoc); // against set value here
  if (zero_flag) { goto ExitVWalk; } // branch if equal to change modes if necessary
  lda_abs(ScrollFractional);
  carry_flag = false; // do fixed point math on fractional part of scroll
  adc_imm(0x80);
  ram[ScrollFractional] = a; // save fractional movement amount
  lda_imm(0x1); // set 1 pixel per frame
  adc_imm(0x0); // add carry from previous addition
  tay(); // use as scroll amount
  ScrollScreen(); // do sub to scroll the screen
  UpdScrollVar(); // do another sub to update screen and scroll variables
  inc_zp(VictoryWalkControl); // increment value to stay in this routine
  
ExitVWalk:
  lda_zp(VictoryWalkControl); // load value set here
  if (zero_flag) { IncModeTask_A(); return; } // if zero, branch to change modes
  // -------------------------------------------------------------------------------------
}

void PrintVictoryMessages(void) {
  lda_abs(SecondaryMsgCounter); // load secondary message counter
  if (!zero_flag) { goto IncMsgCounter; } // if set, branch to increment message counters
  lda_abs(PrimaryMsgCounter); // otherwise load primary message counter
  if (zero_flag) { goto ThankPlayer; } // if set to zero, branch to print first message
  cmp_imm(0x9); // if at 9 or above, branch elsewhere (this comparison
  if (carry_flag) { goto IncMsgCounter; } // is residual code, counter never reaches 9)
  ldy_abs(WorldNumber); // check world number
  cpy_imm(World8);
  if (!zero_flag) { goto MRetainerMsg; } // if not at world 8, skip to next part
  cmp_imm(0x3); // check primary message counter again
  if (!carry_flag) { goto IncMsgCounter; } // if not at 3 yet (world 8 only), branch to increment
  sbc_imm(0x1); // otherwise subtract one
  goto ThankPlayer; // and skip to next part
  
MRetainerMsg:
  cmp_imm(0x2); // check primary message counter
  if (!carry_flag) { goto IncMsgCounter; } // if not at 2 yet (world 1-7 only), branch
  
ThankPlayer:
  tay(); // put primary message counter into Y
  if (!zero_flag) { goto SecondPartMsg; } // if counter nonzero, skip this part, do not print first message
  lda_abs(CurrentPlayer); // otherwise get player currently on the screen
  if (zero_flag) { goto EvalForMusic; } // if mario, branch
  iny(); // otherwise increment Y once for luigi and
  if (!zero_flag) { goto EvalForMusic; } // do an unconditional branch to the same place
  
SecondPartMsg:
  iny(); // increment Y to do world 8's message
  lda_abs(WorldNumber);
  cmp_imm(World8); // check world number
  if (zero_flag) { goto EvalForMusic; } // if at world 8, branch to next part
  dey(); // otherwise decrement Y for world 1-7's message
  cpy_imm(0x4); // if counter at 4 (world 1-7 only)
  if (carry_flag) { goto SetEndTimer; } // branch to set victory end timer
  cpy_imm(0x3); // if counter at 3 (world 1-7 only)
  if (carry_flag) { goto IncMsgCounter; } // branch to keep counting
  
EvalForMusic:
  cpy_imm(0x3); // if counter not yet at 3 (world 8 only), branch
  if (!zero_flag) { goto PrintMsg; } // to print message only (note world 1-7 will only
  lda_imm(VictoryMusic); // reach this code if counter = 0, and will always branch)
  ram[EventMusicQueue] = a; // otherwise load victory music first (world 8 only)
  
PrintMsg:
  tya(); // put primary message counter in A
  carry_flag = false; // add $0c or 12 to counter thus giving an appropriate value,
  adc_imm(0xc); // ($0c-$0d = first), ($0e = world 1-7's), ($0f-$12 = world 8's)
  ram[VRAM_Buffer_AddrCtrl] = a; // write message counter to vram address controller
  
IncMsgCounter:
  lda_abs(SecondaryMsgCounter);
  carry_flag = false;
  adc_imm(0x4); // add four to secondary message counter
  ram[SecondaryMsgCounter] = a;
  lda_abs(PrimaryMsgCounter);
  adc_imm(0x0); // add carry to primary message counter
  ram[PrimaryMsgCounter] = a;
  cmp_imm(0x7); // check primary counter one more time
  
SetEndTimer:
  if (!carry_flag) { return; } // if not reached value yet, branch to leave
  lda_imm(0x6);
  ram[WorldEndTimer] = a; // otherwise set world end timer
  IncModeTask_A();
}

void IncModeTask_A(void) {
  inc_abs(OperMode_Task); // move onto next task in mode
  // -------------------------------------------------------------------------------------
}

void PlayerEndWorld(void) {
  lda_abs(WorldEndTimer); // check to see if world end timer expired
  if (!zero_flag) { goto EndExitOne; } // branch to leave if not
  ldy_abs(WorldNumber); // check world number
  cpy_imm(World8); // if on world 8, player is done with game,
  if (carry_flag) { goto EndChkBButton; } // thus branch to read controller
  lda_imm(0x0);
  ram[AreaNumber] = a; // otherwise initialize area number used as offset
  ram[LevelNumber] = a; // and level number control to start at area 1
  ram[OperMode_Task] = a; // initialize secondary mode of operation
  inc_abs(WorldNumber); // increment world number to move onto the next world
  LoadAreaPointer(); // get area address offset for the next area
  inc_abs(FetchNewGameTimerFlag); // set flag to load game timer from header
  lda_imm(GameModeValue);
  ram[OperMode] = a; // set mode of operation to game mode
  
EndExitOne:
  return; // and leave
  
EndChkBButton:
  lda_abs(SavedJoypad1Bits);
  ora_abs(SavedJoypad2Bits); // check to see if B button was pressed on
  and_imm(B_Button); // either controller
  if (zero_flag) { return; } // branch to leave if not
  lda_imm(0x1); // otherwise set world selection flag
  ram[WorldSelectEnableFlag] = a;
  lda_imm(0xff); // remove onscreen player's lives
  ram[NumberofLives] = a;
  TerminateGame(); // do sub to continue other player or end game
  // -------------------------------------------------------------------------------------
  // data is used as tiles for numbers
  // that appear when you defeat enemies
}

void BridgeCollapse(void) {
  ldx_abs(BowserFront_Offset); // get enemy offset for bowser
  lda_zpx(Enemy_ID); // check enemy object identifier for bowser
  cmp_imm(Bowser); // if not found, branch ahead,
  // metatile removal not necessary
  if (zero_flag) {
    ram[ObjectOffset] = x; // store as enemy offset here
    lda_zpx(Enemy_State); // if bowser in normal state, skip all of this
    if (zero_flag) {
      RemoveBridge();
      return;
    }
    and_imm(0b01000000); // if bowser's state has d6 clear, skip to silence music
    if (!zero_flag) {
      lda_zpx(Enemy_Y_Position); // check bowser's vertical coordinate
      cmp_imm(0xe0); // if bowser not yet low enough, skip this part ahead
      if (!carry_flag) {
        MoveD_Bowser();
        return;
      }
    }
  }
  // SetM2:
  lda_imm(Silence); // silence music
  ram[EventMusicQueue] = a;
  inc_abs(OperMode_Task); // move onto next secondary mode in autoctrl mode
  KillAllEnemies(); // jump to empty all enemy slots and then leave
}

void MoveD_Bowser(void) {
  MoveEnemySlowVert(); // do a sub to move bowser downwards
  BowserGfxHandler(); // jump to draw bowser's front and rear, then leave
}

void RemoveBridge(void) {
  dec_abs(BowserFeetCounter); // decrement timer to control bowser's feet
  // if not expired, skip all of this
  if (zero_flag) {
    lda_imm(0x4);
    ram[BowserFeetCounter] = a; // otherwise, set timer now
    lda_abs(BowserBodyControls);
    eor_imm(0x1); // invert bit to control bowser's feet
    ram[BowserBodyControls] = a;
    lda_imm(0x22); // put high byte of name table address here for now
    ram[0x5] = a;
    ldy_abs(BridgeCollapseOffset); // get bridge collapse offset here
    lda_absy(BridgeCollapseData); // load low byte of name table address and store here
    ram[0x4] = a;
    ldy_abs(VRAM_Buffer1_Offset); // increment vram buffer offset
    iny();
    ldx_imm(0xc); // set offset for tile data for sub to draw blank metatile
    RemBridge(); // do sub here to remove bowser's bridge metatiles
    ldx_zp(ObjectOffset); // get enemy offset
    MoveVOffset(); // set new vram buffer offset
    lda_imm(Sfx_Blast); // load the fireworks/gunfire sound into the square 2 sfx
    ram[Square2SoundQueue] = a; // queue while at the same time loading the brick
    lda_imm(Sfx_BrickShatter); // shatter sound into the noise sfx queue thus
    ram[NoiseSoundQueue] = a; // producing the unique sound of the bridge collapsing
    inc_abs(BridgeCollapseOffset); // increment bridge collapse offset
    lda_abs(BridgeCollapseOffset);
    cmp_imm(0xf); // if bridge collapse offset has not yet reached
    // the end, go ahead and skip this part
    if (zero_flag) {
      InitVStf(); // initialize whatever vertical speed bowser has
      lda_imm(0b01000000);
      ram[Enemy_State + x] = a; // set bowser's state to one of defeated states (d6 set)
      lda_imm(Sfx_BowserFall);
      ram[Square2SoundQueue] = a; // play bowser defeat sound
    }
  }
  // NoBFall:
  BowserGfxHandler(); // jump to code that draws bowser
  // --------------------------------
}

void FloateyNumbersRoutine(void) {
  lda_absx(FloateyNum_Control); // load control for floatey number
  if (zero_flag) { return; } // if zero, branch to leave
  cmp_imm(0xb); // if less than $0b, branch
  if (!carry_flag) { goto ChkNumTimer; }
  lda_imm(0xb); // otherwise set to $0b, thus keeping
  ram[FloateyNum_Control + x] = a; // it in range
  
ChkNumTimer:
  tay(); // use as Y
  lda_absx(FloateyNum_Timer); // check value here
  if (!zero_flag) { goto DecNumTimer; } // if nonzero, branch ahead
  ram[FloateyNum_Control + x] = a; // initialize floatey number control and leave
  return;
  
DecNumTimer:
  dec_absx(FloateyNum_Timer); // decrement value here
  cmp_imm(0x2b); // if not reached a certain point, branch
  if (!zero_flag) { goto ChkTallEnemy; }
  cpy_imm(0xb); // check offset for $0b
  if (!zero_flag) { goto LoadNumTiles; } // branch ahead if not found
  inc_abs(NumberofLives); // give player one extra life (1-up)
  lda_imm(Sfx_ExtraLife);
  ram[Square2SoundQueue] = a; // and play the 1-up sound
  
LoadNumTiles:
  lda_absy(ScoreUpdateData); // load point value here
  lsr_acc(); // move high nybble to low
  lsr_acc();
  lsr_acc();
  lsr_acc();
  tax(); // use as X offset, essentially the digit
  lda_absy(ScoreUpdateData); // load again and this time
  and_imm(0b00001111); // mask out the high nybble
  ram[DigitModifier + x] = a; // store as amount to add to the digit
  AddToScore(); // update the score accordingly
  
ChkTallEnemy:
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset for enemy object
  lda_zpx(Enemy_ID); // get enemy object identifier
  cmp_imm(Spiny);
  if (zero_flag) { goto FloateyPart; } // branch if spiny
  cmp_imm(PiranhaPlant);
  if (zero_flag) { goto FloateyPart; } // branch if piranha plant
  cmp_imm(HammerBro);
  if (zero_flag) { goto GetAltOffset; } // branch elsewhere if hammer bro
  cmp_imm(GreyCheepCheep);
  if (zero_flag) { goto FloateyPart; } // branch if cheep-cheep of either color
  cmp_imm(RedCheepCheep);
  if (zero_flag) { goto FloateyPart; }
  cmp_imm(TallEnemy);
  if (carry_flag) { goto GetAltOffset; } // branch elsewhere if enemy object => $09
  lda_zpx(Enemy_State);
  cmp_imm(0x2); // if enemy state defeated or otherwise
  if (carry_flag) { goto FloateyPart; } // $02 or greater, branch beyond this part
  
GetAltOffset:
  ldx_abs(SprDataOffset_Ctrl); // load some kind of control bit
  ldy_absx(Alt_SprDataOffset); // get alternate OAM data offset
  ldx_zp(ObjectOffset); // get enemy object offset again
  
FloateyPart:
  lda_absx(FloateyNum_Y_Pos); // get vertical coordinate for
  cmp_imm(0x18); // floatey number, if coordinate in the
  if (!carry_flag) { goto SetupNumSpr; } // status bar, branch
  sbc_imm(0x1);
  ram[FloateyNum_Y_Pos + x] = a; // otherwise subtract one and store as new
  
SetupNumSpr:
  lda_absx(FloateyNum_Y_Pos); // get vertical coordinate
  sbc_imm(0x8); // subtract eight and dump into the
  DumpTwoSpr(); // left and right sprite's Y coordinates
  lda_absx(FloateyNum_X_Pos); // get horizontal coordinate
  ram[Sprite_X_Position + y] = a; // store into X coordinate of left sprite
  carry_flag = false;
  adc_imm(0x8); // add eight pixels and store into X
  ram[Sprite_X_Position + 4 + y] = a; // coordinate of right sprite
  lda_imm(0x2);
  ram[Sprite_Attributes + y] = a; // set palette control in attribute bytes
  ram[Sprite_Attributes + 4 + y] = a; // of left and right sprites
  lda_absx(FloateyNum_Control);
  asl_acc(); // multiply our floatey number control by 2
  tax(); // and use as offset for look-up table
  lda_absx(FloateyNumTileData);
  ram[Sprite_Tilenumber + y] = a; // display first half of number of points
  lda_absx(FloateyNumTileData + 1);
  ram[Sprite_Tilenumber + 4 + y] = a; // display the second half
  ldx_zp(ObjectOffset); // get enemy object offset and leave
  // -------------------------------------------------------------------------------------
}

void OutputInter(void) {
  WriteGameText();
  ResetScreenTimer();
  lda_imm(0x0);
  ram[DisableScreenFlag] = a; // reenable screen output
}

void WriteGameText(void) {
  pha(); // save text number to stack
  asl_acc();
  tay(); // multiply by 2 and use as offset
  cpy_imm(0x4); // if set to do top status bar or world/lives display,
  // branch to use current offset as-is
  if (carry_flag) {
    cpy_imm(0x8); // if set to do time-up or game over,
    // branch to check players
    if (carry_flag) {
      ldy_imm(0x8); // otherwise warp zone, therefore set offset
    }
    // Chk2Players:
    lda_abs(NumberOfPlayers); // check for number of players
    // if there are two, use current offset to also print name
    if (zero_flag) {
      iny(); // otherwise increment offset by one to not print name
    }
  }
  // LdGameText:
  ldx_absy(GameTextOffsets); // get offset to message we want to print
  ldy_imm(0x0);
  
GameTextLoop:
  lda_absx(GameText); // load message data
  cmp_imm(0xff); // check for terminator
  // branch to end text if found
  if (!zero_flag) {
    ram[VRAM_Buffer1 + y] = a; // otherwise write data to buffer
    inx(); // and increment increment
    iny();
    if (!zero_flag) { goto GameTextLoop; } // do this for 256 bytes if no terminator found
  }
  // EndGameText:
  lda_imm(0x0); // put null terminator at end
  ram[VRAM_Buffer1 + y] = a;
  pla(); // pull original text number from stack
  tax();
  cmp_imm(0x4); // are we printing warp zone?
  if (!carry_flag) {
    dex(); // are we printing the world/lives display?
    // if not, branch to check player's name
    if (zero_flag) {
      lda_abs(NumberofLives); // otherwise, check number of lives
      carry_flag = false; // and increment by one for display
      adc_imm(0x1);
      cmp_imm(10); // more than 9 lives?
      if (carry_flag) {
        sbc_imm(10); // if so, subtract 10 and put a crown tile
        ldy_imm(0x9f); // next to the difference...strange things happen if
        ram[VRAM_Buffer1 + 7] = y; // the number of lives exceeds 19
      }
      // PutLives:
      ram[VRAM_Buffer1 + 8] = a;
      ldy_abs(WorldNumber); // write world and level numbers (incremented for display)
      iny(); // to the buffer in the spaces surrounding the dash
      ram[VRAM_Buffer1 + 19] = y;
      ldy_abs(LevelNumber);
      iny();
      ram[VRAM_Buffer1 + 21] = y; // we're done here
      return;
    }
    // CheckPlayerName:
    lda_abs(NumberOfPlayers); // check number of players
    // if only 1 player, leave
    if (!zero_flag) {
      lda_abs(CurrentPlayer); // load current player
      dex(); // check to see if current message number is for time up
      if (zero_flag) {
        ldy_abs(OperMode); // check for game over mode
        cpy_imm(GameOverModeValue);
        if (!zero_flag) {
          eor_imm(0b00000001); // if not, must be time up, invert d0 to do other player
        }
      }
      // ChkLuigi:
      lsr_acc();
      // if mario is current player, do not change the name
      if (carry_flag) {
        ldy_imm(0x4);
        
NameLoop:
        lda_absy(LuigiName); // otherwise, replace "MARIO" with "LUIGI"
        ram[VRAM_Buffer1 + 3 + y] = a;
        dey();
        if (!neg_flag) { goto NameLoop; } // do this until each letter is replaced
      }
    }
    // ExitChkName:
    return;
  }
  // PrintWarpZoneNumbers:
  sbc_imm(0x4); // subtract 4 and then shift to the left
  asl_acc(); // twice to get proper warp zone number
  asl_acc(); // offset
  tax();
  ldy_imm(0x0);
  
WarpNumLoop:
  lda_absx(WarpZoneNumbers); // print warp zone numbers into the
  ram[VRAM_Buffer1 + 27 + y] = a; // placeholders from earlier
  inx();
  iny(); // put a number in every fourth space
  iny();
  iny();
  iny();
  cpy_imm(0xc);
  if (!carry_flag) { goto WarpNumLoop; }
  lda_imm(0x2c); // load new buffer pointer at end of message
  SetVRAMOffset();
  // -------------------------------------------------------------------------------------
}

void ResetScreenTimer(void) {
  lda_imm(0x7); // reset timer again
  ram[ScreenTimer] = a;
  inc_abs(ScreenRoutineTask); // move onto next task
  // -------------------------------------------------------------------------------------
  // $00 - temp vram buffer offset
  // $01 - temp metatile buffer offset
  // $02 - temp metatile graphics table offset
  // $03 - used to store attribute bits
  // $04 - used to determine attribute table row
  // $05 - used to determine attribute table column
  // $06 - metatile graphics table address low
  // $07 - metatile graphics table address high
}

void RenderAreaGraphics(void) {
  lda_abs(CurrentColumnPos); // store LSB of where we're at
  and_imm(0x1);
  ram[0x5] = a;
  ldy_abs(VRAM_Buffer2_Offset); // store vram buffer offset
  ram[0x0] = y;
  lda_abs(CurrentNTAddr_Low); // get current name table address we're supposed to render
  ram[VRAM_Buffer2 + 1 + y] = a;
  lda_abs(CurrentNTAddr_High);
  ram[VRAM_Buffer2 + y] = a;
  lda_imm(0x9a); // store length byte of 26 here with d7 set
  ram[VRAM_Buffer2 + 2 + y] = a; // to increment by 32 (in columns)
  lda_imm(0x0); // init attribute row
  ram[0x4] = a;
  tax();
  
DrawMTLoop:
  ram[0x1] = x; // store init value of 0 or incremented offset for buffer
  lda_absx(MetatileBuffer); // get first metatile number, and mask out all but 2 MSB
  and_imm(0b11000000);
  ram[0x3] = a; // store attribute table bits here
  asl_acc(); // note that metatile format is:
  rol_acc(); // %xx000000 - attribute table bits,
  rol_acc(); // %00xxxxxx - metatile number
  tay(); // rotate bits to d1-d0 and use as offset here
  lda_absy(MetatileGraphics_Low); // get address to graphics table from here
  ram[0x6] = a;
  lda_absy(MetatileGraphics_High);
  ram[0x7] = a;
  lda_absx(MetatileBuffer); // get metatile number again
  asl_acc(); // multiply by 4 and use as tile offset
  asl_acc();
  ram[0x2] = a;
  lda_abs(AreaParserTaskNum); // get current task number for level processing and
  and_imm(0b00000001); // mask out all but LSB, then invert LSB, multiply by 2
  eor_imm(0b00000001); // to get the correct column position in the metatile,
  asl_acc(); // then add to the tile offset so we can draw either side
  adc_zp(0x2); // of the metatiles
  tay();
  ldx_zp(0x0); // use vram buffer offset from before as X
  lda_indy(0x6);
  ram[VRAM_Buffer2 + 3 + x] = a; // get first tile number (top left or top right) and store
  iny();
  lda_indy(0x6); // now get the second (bottom left or bottom right) and store
  ram[VRAM_Buffer2 + 4 + x] = a;
  ldy_zp(0x4); // get current attribute row
  lda_zp(0x5); // get LSB of current column where we're at, and
  if (!zero_flag) { goto RightCheck; } // branch if set (clear = left attrib, set = right)
  lda_zp(0x1); // get current row we're rendering
  lsr_acc(); // branch if LSB set (clear = top left, set = bottom left)
  if (carry_flag) { goto LLeft; }
  rol_zp(0x3); // rotate attribute bits 3 to the left
  rol_zp(0x3); // thus in d1-d0, for upper left square
  rol_zp(0x3);
  goto SetAttrib;
  
RightCheck:
  lda_zp(0x1); // get LSB of current row we're rendering
  lsr_acc(); // branch if set (clear = top right, set = bottom right)
  if (carry_flag) { goto NextMTRow; }
  lsr_zp(0x3); // shift attribute bits 4 to the right
  lsr_zp(0x3); // thus in d3-d2, for upper right square
  lsr_zp(0x3);
  lsr_zp(0x3);
  goto SetAttrib;
  
LLeft:
  lsr_zp(0x3); // shift attribute bits 2 to the right
  lsr_zp(0x3); // thus in d5-d4 for lower left square
  
NextMTRow:
  inc_zp(0x4); // move onto next attribute row
  
SetAttrib:
  lda_absy(AttributeBuffer); // get previously saved bits from before
  ora_zp(0x3); // if any, and put new bits, if any, onto
  ram[AttributeBuffer + y] = a; // the old, and store
  inc_zp(0x0); // increment vram buffer offset by 2
  inc_zp(0x0);
  ldx_zp(0x1); // get current gfx buffer row, and check for
  inx(); // the bottom of the screen
  cpx_imm(0xd);
  if (!carry_flag) { goto DrawMTLoop; } // if not there yet, loop back
  ldy_zp(0x0); // get current vram buffer offset, increment by 3
  iny(); // (for name table address and length bytes)
  iny();
  iny();
  lda_imm(0x0);
  ram[VRAM_Buffer2 + y] = a; // put null terminator at end of data for name table
  ram[VRAM_Buffer2_Offset] = y; // store new buffer offset
  inc_abs(CurrentNTAddr_Low); // increment name table address low
  lda_abs(CurrentNTAddr_Low); // check current low byte
  and_imm(0b00011111); // if no wraparound, just skip this part
  if (!zero_flag) { goto ExitDrawM; }
  lda_imm(0x80); // if wraparound occurs, make sure low byte stays
  ram[CurrentNTAddr_Low] = a; // just under the status bar
  lda_abs(CurrentNTAddr_High); // and then invert d2 of the name table address high
  eor_imm(0b00000100); // to move onto the next appropriate name table
  ram[CurrentNTAddr_High] = a;
  
ExitDrawM:
  SetVRAMCtrl(); // jump to set buffer to $0341 and leave
  // -------------------------------------------------------------------------------------
  // $00 - temp attribute table address high (big endian order this time!)
  // $01 - temp attribute table address low
}

void RenderAttributeTables(void) {
  lda_abs(CurrentNTAddr_Low); // get low byte of next name table address
  and_imm(0b00011111); // to be written to, mask out all but 5 LSB,
  carry_flag = true; // subtract four
  sbc_imm(0x4);
  and_imm(0b00011111); // mask out bits again and store
  ram[0x1] = a;
  lda_abs(CurrentNTAddr_High); // get high byte and branch if borrow not set
  if (!carry_flag) {
    eor_imm(0b00000100); // otherwise invert d2
  }
  // SetATHigh:
  and_imm(0b00000100); // mask out all other bits
  ora_imm(0x23); // add $2300 to the high byte and store
  ram[0x0] = a;
  lda_zp(0x1); // get low byte - 4, divide by 4, add offset for
  lsr_acc(); // attribute table and store
  lsr_acc();
  adc_imm(0xc0); // we should now have the appropriate block of
  ram[0x1] = a; // attribute table in our temp address
  ldx_imm(0x0);
  ldy_abs(VRAM_Buffer2_Offset); // get buffer offset
  
AttribLoop:
  lda_zp(0x0);
  ram[VRAM_Buffer2 + y] = a; // store high byte of attribute table address
  lda_zp(0x1);
  carry_flag = false; // get low byte, add 8 because we want to start
  adc_imm(0x8); // below the status bar, and store
  ram[VRAM_Buffer2 + 1 + y] = a;
  ram[0x1] = a; // also store in temp again
  lda_absx(AttributeBuffer); // fetch current attribute table byte and store
  ram[VRAM_Buffer2 + 3 + y] = a; // in the buffer
  lda_imm(0x1);
  ram[VRAM_Buffer2 + 2 + y] = a; // store length of 1 in buffer
  lsr_acc();
  ram[AttributeBuffer + x] = a; // clear current byte in attribute buffer
  iny(); // increment buffer offset by 4 bytes
  iny();
  iny();
  iny();
  inx(); // increment attribute offset and check to see
  cpx_imm(0x7); // if we're at the end yet
  if (!carry_flag) { goto AttribLoop; }
  ram[VRAM_Buffer2 + y] = a; // put null terminator at the end
  ram[VRAM_Buffer2_Offset] = y; // store offset in case we want to do any more
  SetVRAMCtrl(); // <fallthrough>
}

void SetVRAMCtrl(void) {
  lda_imm(0x6);
  ram[VRAM_Buffer_AddrCtrl] = a; // set buffer to $0341 and leave
  // -------------------------------------------------------------------------------------
  // $00 - used as temporary counter in ColorRotation
}

void ColorRotation(void) {
  lda_zp(FrameCounter); // get frame counter
  and_imm(0x7); // mask out all but three LSB
  if (!zero_flag) { return; } // branch if not set to zero to do this every eighth frame
  ldx_abs(VRAM_Buffer1_Offset); // check vram buffer offset
  cpx_imm(0x31);
  if (carry_flag) { return; } // if offset over 48 bytes, branch to leave
  tay(); // otherwise use frame counter's 3 LSB as offset here
  
GetBlankPal:
  lda_absy(BlankPalette); // get blank palette for palette 3
  ram[VRAM_Buffer1 + x] = a; // store it in the vram buffer
  inx(); // increment offsets
  iny();
  cpy_imm(0x8);
  if (!carry_flag) { goto GetBlankPal; } // do this until all bytes are copied
  ldx_abs(VRAM_Buffer1_Offset); // get current vram buffer offset
  lda_imm(0x3);
  ram[0x0] = a; // set counter here
  lda_abs(AreaType); // get area type
  asl_acc(); // multiply by 4 to get proper offset
  asl_acc();
  tay(); // save as offset here
  
GetAreaPal:
  lda_absy(Palette3Data); // fetch palette to be written based on area type
  ram[VRAM_Buffer1 + 3 + x] = a; // store it to overwrite blank palette in vram buffer
  iny();
  inx();
  dec_zp(0x0); // decrement counter
  if (!neg_flag) { goto GetAreaPal; } // do this until the palette is all copied
  ldx_abs(VRAM_Buffer1_Offset); // get current vram buffer offset
  ldy_abs(ColorRotateOffset); // get color cycling offset
  lda_absy(ColorRotatePalette);
  ram[VRAM_Buffer1 + 4 + x] = a; // get and store current color in second slot of palette
  lda_abs(VRAM_Buffer1_Offset);
  carry_flag = false; // add seven bytes to vram buffer offset
  adc_imm(0x7);
  ram[VRAM_Buffer1_Offset] = a;
  inc_abs(ColorRotateOffset); // increment color cycling offset
  lda_abs(ColorRotateOffset);
  cmp_imm(0x6); // check to see if it's still in range
  if (!carry_flag) { return; } // if so, branch to leave
  lda_imm(0x0);
  ram[ColorRotateOffset] = a; // otherwise, init to keep it in range
  // -------------------------------------------------------------------------------------
  // $00 - temp store for offset control bit
  // $01 - temp vram buffer offset
  // $02 - temp store for vertical high nybble in block buffer routine
  // $03 - temp adder for high byte of name table address
  // $04, $05 - name table address low/high
  // $06, $07 - block buffer address low/high
}

void RemoveCoin_Axe(void) {
  ldy_imm(0x41); // set low byte so offset points to $0341
  lda_imm(0x3); // load offset for default blank metatile
  ldx_abs(AreaType); // check area type
  // if not water type, use offset
  if (zero_flag) {
    lda_imm(0x4); // otherwise load offset for blank metatile used in water
  }
  // WriteBlankMT:
  PutBlockMetatile(); // do a sub to write blank metatile to vram buffer
  lda_imm(0x6);
  ram[VRAM_Buffer_AddrCtrl] = a; // set vram address controller to $0341 and leave
}

void ReplaceBlockMetatile(void) {
  WriteBlockMetatile(); // write metatile to vram buffer to replace block object
  inc_abs(Block_ResidualCounter); // increment unused counter (residual code)
  dec_absx(Block_RepFlag); // decrement flag (residual code)
}

void DestroyBlockMetatile(void) {
  lda_imm(0x0); // force blank metatile if branched/jumped to this point
  WriteBlockMetatile();
}

void WriteBlockMetatile(void) {
  ldy_imm(0x3); // load offset for blank metatile
  cmp_imm(0x0); // check contents of A for blank metatile
  // branch if found (unconditional if branched from 8a6b)
  if (!zero_flag) {
    ldy_imm(0x0); // load offset for brick metatile w/ line
    cmp_imm(0x58);
    // use offset if metatile is brick with coins (w/ line)
    if (!zero_flag) {
      cmp_imm(0x51);
      // use offset if metatile is breakable brick w/ line
      if (!zero_flag) {
        iny(); // increment offset for brick metatile w/o line
        cmp_imm(0x5d);
        // use offset if metatile is brick with coins (w/o line)
        if (!zero_flag) {
          cmp_imm(0x52);
          // use offset if metatile is breakable brick w/o line
          if (!zero_flag) {
            iny(); // if any other metatile, increment offset for empty block
          }
        }
      }
    }
  }
  // UseBOffset:
  tya(); // put Y in A
  ldy_abs(VRAM_Buffer1_Offset); // get vram buffer offset
  iny(); // move onto next byte
  PutBlockMetatile(); // get appropriate block data and write to vram buffer
  MoveVOffset(); // <fallthrough>
}

void MoveVOffset(void) {
  dey(); // decrement vram buffer offset
  tya(); // add 10 bytes to it
  carry_flag = false;
  adc_imm(10);
  SetVRAMOffset(); // branch to store as new vram buffer offset
}

void PutBlockMetatile(void) {
  ram[0x0] = x; // store control bit from SprDataOffset_Ctrl
  ram[0x1] = y; // store vram buffer offset for next byte
  asl_acc();
  asl_acc(); // multiply A by four and use as X
  tax();
  ldy_imm(0x20); // load high byte for name table 0
  lda_zp(0x6); // get low byte of block buffer pointer
  cmp_imm(0xd0); // check to see if we're on odd-page block buffer
  // if not, use current high byte
  if (carry_flag) {
    ldy_imm(0x24); // otherwise load high byte for name table 1
  }
  // SaveHAdder:
  ram[0x3] = y; // save high byte here
  and_imm(0xf); // mask out high nybble of block buffer pointer
  asl_acc(); // multiply by 2 to get appropriate name table low byte
  ram[0x4] = a; // and then store it here
  lda_imm(0x0);
  ram[0x5] = a; // initialize temp high byte
  lda_zp(0x2); // get vertical high nybble offset used in block buffer routine
  carry_flag = false;
  adc_imm(0x20); // add 32 pixels for the status bar
  asl_acc();
  rol_zp(0x5); // shift and rotate d7 onto d0 and d6 into carry
  asl_acc();
  rol_zp(0x5); // shift and rotate d6 onto d0 and d5 into carry
  adc_zp(0x4); // add low byte of name table and carry to vertical high nybble
  ram[0x4] = a; // and store here
  lda_zp(0x5); // get whatever was in d7 and d6 of vertical high nybble
  adc_imm(0x0); // add carry
  carry_flag = false;
  adc_zp(0x3); // then add high byte of name table
  ram[0x5] = a; // store here
  ldy_zp(0x1); // get vram buffer offset to be used
  RemBridge(); // <fallthrough>
}

void RemBridge(void) {
  lda_absx(BlockGfxData); // write top left and top right
  ram[VRAM_Buffer1 + 2 + y] = a; // tile numbers into first spot
  lda_absx(BlockGfxData + 1);
  ram[VRAM_Buffer1 + 3 + y] = a;
  lda_absx(BlockGfxData + 2); // write bottom left and bottom
  ram[VRAM_Buffer1 + 7 + y] = a; // right tiles numbers into
  lda_absx(BlockGfxData + 3); // second spot
  ram[VRAM_Buffer1 + 8 + y] = a;
  lda_zp(0x4);
  ram[VRAM_Buffer1 + y] = a; // write low byte of name table
  carry_flag = false; // into first slot as read
  adc_imm(0x20); // add 32 bytes to value
  ram[VRAM_Buffer1 + 5 + y] = a; // write low byte of name table
  lda_zp(0x5); // plus 32 bytes into second slot
  ram[VRAM_Buffer1 - 1 + y] = a; // write high byte of name
  ram[VRAM_Buffer1 + 4 + y] = a; // table address to both slots
  lda_imm(0x2);
  ram[VRAM_Buffer1 + 1 + y] = a; // put length of 2 in
  ram[VRAM_Buffer1 + 6 + y] = a; // both slots
  lda_imm(0x0);
  ram[VRAM_Buffer1 + 9 + y] = a; // put null terminator at end
  ldx_zp(0x0); // get offset control bit here
  // -------------------------------------------------------------------------------------
  // METATILE GRAPHICS TABLE
}

void InitializeNameTables(void) {
  lda_abs(PPU_STATUS); // reset flip-flop
  lda_abs(Mirror_PPU_CTRL_REG1); // load mirror of ppu reg $2000
  ora_imm(0b00010000); // set sprites for first 4k and background for second 4k
  and_imm(0b11110000); // clear rest of lower nybble, leave higher alone
  WritePPUReg1();
  lda_imm(0x24); // set vram address to start of name table 1
  WriteNTAddr();
  lda_imm(0x20); // and then set it to name table 0
  WriteNTAddr();
}

void WriteNTAddr(void) {
  ppu_write_address(a);
  lda_imm(0x0);
  ppu_write_address(a);
  ldx_imm(0x4); // clear name table with blank tile #24
  ldy_imm(0xc0);
  lda_imm(0x24);
  
InitNTLoop:
  ppu_write_data(a); // count out exactly 768 tiles
  dey();
  if (!zero_flag) { goto InitNTLoop; }
  dex();
  if (!zero_flag) { goto InitNTLoop; }
  ldy_imm(64); // now to clear the attribute table (with zero this time)
  txa();
  ram[VRAM_Buffer1_Offset] = a; // init vram buffer 1 offset
  ram[VRAM_Buffer1] = a; // init vram buffer 1
  
InitATLoop:
  ppu_write_data(a);
  dey();
  if (!zero_flag) { goto InitATLoop; }
  ram[HorizontalScroll] = a; // reset scroll variables
  ram[VerticalScroll] = a;
  InitScroll(); // initialize scroll registers to zero
  // -------------------------------------------------------------------------------------
  // $00 - temp joypad bit
}

void ReadJoypads(void) {
  lda_imm(0x1); // reset and clear strobe of joypad ports
  write_joypad1(a);
  lsr_acc();
  tax(); // start with joypad 1's port
  write_joypad1(a);
  ReadPortBits();
  inx(); // increment for joypad 2's port
  ReadPortBits();
}

void ReadPortBits(void) {
  ldy_imm(0x8);
  
PortLoop:
  pha(); // push previous bit onto stack
  lda_absx(JOYPAD_PORT); // read current bit on joypad port
  ram[0x0] = a; // check d1 and d0 of port output
  lsr_acc(); // this is necessary on the old
  ora_zp(0x0); // famicom systems in japan
  lsr_acc();
  pla(); // read bits from stack
  rol_acc(); // rotate bit from carry flag
  dey();
  if (!zero_flag) { goto PortLoop; } // count down bits left
  ram[SavedJoypadBits + x] = a; // save controller status here always
  pha();
  and_imm(0b00110000); // check for select or start
  and_absx(JoypadBitMask); // if neither saved state nor current state
  // have any of these two set, branch
  if (!zero_flag) {
    pla();
    and_imm(0b11001111); // otherwise store without select
    ram[SavedJoypadBits + x] = a; // or start bits and leave
    return;
  }
  // Save8Bits:
  pla();
  ram[JoypadBitMask + x] = a; // save with all bits in another place and leave
  // -------------------------------------------------------------------------------------
  // $00 - vram buffer address table low
  // $01 - vram buffer address table high
}

void WriteBufferToScreen(void) {
  ppu_write_address(a); // store high byte of vram address
  iny();
  lda_indy(0x0); // load next byte (second)
  ppu_write_address(a); // store low byte of vram address
  iny();
  lda_indy(0x0); // load next byte (third)
  asl_acc(); // shift to left and save in stack
  pha();
  lda_abs(Mirror_PPU_CTRL_REG1); // load mirror of $2000,
  ora_imm(0b00000100); // set ppu to increment by 32 by default
  // if d7 of third byte was clear, ppu will
  if (!carry_flag) {
    and_imm(0b11111011); // only increment by 1
  }
  // SetupWrites:
  WritePPUReg1(); // write to register
  pla(); // pull from stack and shift to left again
  asl_acc();
  // if d6 of third byte was clear, do not repeat byte
  if (carry_flag) {
    ora_imm(0b00000010); // otherwise set d1 and increment Y
    iny();
  }
  // GetLength:
  lsr_acc(); // shift back to the right to get proper length
  lsr_acc(); // note that d1 will now be in carry
  tax();
  
OutputToVRAM:
  // if carry set, repeat loading the same byte
  if (!carry_flag) {
    iny(); // otherwise increment Y to load next byte
  }
  // RepeatByte:
  lda_indy(0x0); // load more data from buffer and write to vram
  ppu_write_data(a);
  dex(); // done writing?
  if (!zero_flag) { goto OutputToVRAM; }
  carry_flag = true;
  tya();
  adc_zp(0x0); // add end length plus one to the indirect at $00
  ram[0x0] = a; // to allow this routine to read another set of updates
  lda_imm(0x0);
  adc_zp(0x1);
  ram[0x1] = a;
  lda_imm(0x3f); // sets vram address to $3f00
  ppu_write_address(a);
  lda_imm(0x0);
  ppu_write_address(a);
  ppu_write_address(a); // then reinitializes it for some reason
  ppu_write_address(a);
  UpdateScreen(); // <fallthrough>
}

void UpdateScreen(void) {
  ldx_abs(PPU_STATUS); // reset flip-flop
  ldy_imm(0x0); // load first byte from indirect as a pointer
  lda_indy(0x0);
  // if byte is zero we have no further updates to make here
  if (!zero_flag) {
    WriteBufferToScreen();
    return;
  }
  InitScroll();
}

void InitScroll(void) {
  ppu_write_scroll(a); // store contents of A into scroll registers
  ppu_write_scroll(a); // and end whatever subroutine led us here
  // -------------------------------------------------------------------------------------
}

void WritePPUReg1(void) {
  ppu_ctrl = a; // write contents of A to PPU register 1
  ram[Mirror_PPU_CTRL_REG1] = a; // and its mirror
  // -------------------------------------------------------------------------------------
  // $00 - used to store status bar nybbles
  // $02 - used as temp vram offset
  // $03 - used to store length of status bar number
  // status bar name table offset and length data
}

void PrintStatusBarNumbers(void) {
  ram[0x0] = a; // store player-specific offset
  OutputNumbers(); // use first nybble to print the coin display
  lda_zp(0x0); // move high nybble to low
  lsr_acc(); // and print to score display
  lsr_acc();
  lsr_acc();
  lsr_acc();
  OutputNumbers();
}

void OutputNumbers(void) {
  carry_flag = false; // add 1 to low nybble
  adc_imm(0x1);
  and_imm(0b00001111); // mask out high nybble
  cmp_imm(0x6);
  if (!carry_flag) {
    pha(); // save incremented value to stack for now and
    asl_acc(); // shift to left and use as offset
    tay();
    ldx_abs(VRAM_Buffer1_Offset); // get current buffer pointer
    lda_imm(0x20); // put at top of screen by default
    cpy_imm(0x0); // are we writing top score on title screen?
    if (zero_flag) {
      lda_imm(0x22); // if so, put further down on the screen
    }
    // SetupNums:
    ram[VRAM_Buffer1 + x] = a;
    lda_absy(StatusBarData); // write low vram address and length of thing
    ram[VRAM_Buffer1 + 1 + x] = a; // we're printing to the buffer
    lda_absy(StatusBarData + 1);
    ram[VRAM_Buffer1 + 2 + x] = a;
    ram[0x3] = a; // save length byte in counter
    ram[0x2] = x; // and buffer pointer elsewhere for now
    pla(); // pull original incremented value from stack
    tax();
    lda_absx(StatusBarOffset); // load offset to value we want to write
    carry_flag = true;
    sbc_absy(StatusBarData + 1); // subtract from length byte we read before
    tay(); // use value as offset to display digits
    ldx_zp(0x2);
    
DigitPLoop:
    lda_absy(DisplayDigits); // write digits to the buffer
    ram[VRAM_Buffer1 + 3 + x] = a;
    inx();
    iny();
    dec_zp(0x3); // do this until all the digits are written
    if (!zero_flag) { goto DigitPLoop; }
    lda_imm(0x0); // put null terminator at end
    ram[VRAM_Buffer1 + 3 + x] = a;
    inx(); // increment buffer pointer by 3
    inx();
    inx();
    ram[VRAM_Buffer1_Offset] = x; // store it in case we want to use it again
    // -------------------------------------------------------------------------------------
  }
}

void DigitsMathRoutine(void) {
  lda_abs(OperMode); // check mode of operation
  cmp_imm(TitleScreenModeValue);
  if (zero_flag) { goto EraseDMods; } // if in title screen mode, branch to lock score
  ldx_imm(0x5);
  
AddModLoop:
  lda_absx(DigitModifier); // load digit amount to increment
  carry_flag = false;
  adc_absy(DisplayDigits); // add to current digit
  if (neg_flag) { goto BorrowOne; } // if result is a negative number, branch to subtract
  cmp_imm(10);
  if (carry_flag) { goto CarryOne; } // if digit greater than $09, branch to add
  
StoreNewD:
  ram[DisplayDigits + y] = a; // store as new score or game timer digit
  dey(); // move onto next digits in score or game timer
  dex(); // and digit amounts to increment
  if (!neg_flag) { goto AddModLoop; } // loop back if we're not done yet
  
EraseDMods:
  lda_imm(0x0); // store zero here
  ldx_imm(0x6); // start with the last digit
  
EraseMLoop:
  ram[DigitModifier - 1 + x] = a; // initialize the digit amounts to increment
  dex();
  if (!neg_flag) { goto EraseMLoop; } // do this until they're all reset, then leave
  return;
  
BorrowOne:
  dec_absx(DigitModifier - 1); // decrement the previous digit, then put $09 in
  lda_imm(0x9); // the game timer digit we're currently on to "borrow
  if (!zero_flag) { goto StoreNewD; } // the one", then do an unconditional branch back
  
CarryOne:
  carry_flag = true; // subtract ten from our digit to make it a
  sbc_imm(10); // proper BCD number, then increment the digit
  inc_absx(DigitModifier - 1); // preceding current digit to "carry the one" properly
  goto StoreNewD; // go back to just after we branched here
  // -------------------------------------------------------------------------------------
}

void UpdateTopScore(void) {
  ldx_imm(0x5); // start with mario's score
  TopScoreCheck();
  ldx_imm(0xb); // now do luigi's score
  TopScoreCheck();
}

void TopScoreCheck(void) {
  ldy_imm(0x5); // start with the lowest digit
  carry_flag = true;
  
GetScoreDiff:
  lda_absx(PlayerScoreDisplay); // subtract each player digit from each high score digit
  sbc_absy(TopScoreDisplay); // from lowest to highest, if any top score digit exceeds
  dex(); // any player digit, borrow will be set until a subsequent
  dey(); // subtraction clears it (player digit is higher than top)
  if (!neg_flag) { goto GetScoreDiff; }
  if (carry_flag) {
    inx(); // increment X and Y once to the start of the score
    iny();
    
CopyScore:
    lda_absx(PlayerScoreDisplay); // store player's score digits into high score memory area
    ram[TopScoreDisplay + y] = a;
    inx();
    iny();
    cpy_imm(0x6); // do this until we have stored them all
    if (!carry_flag) { goto CopyScore; }
    // -------------------------------------------------------------------------------------
  }
}

void InitializeMemory(void) {
  ldx_imm(0x7); // set initial high byte to $0700-$07ff
  lda_imm(0x0); // set initial low byte to start of page (at $00 of page)
  ram[0x6] = a;
  
InitPageLoop:
  ram[0x7] = x;
  
InitByteLoop:
  cpx_imm(0x1); // check to see if we're on the stack ($0100-$01ff)
  if (!zero_flag) { goto InitByte; } // if not, go ahead anyway
  cpy_imm(0x60); // otherwise, check to see if we're at $0160-$01ff
  if (carry_flag) { goto SkipByte; } // if so, skip write
  
InitByte:
  dynamic_ram_write(read_word(0x6) + y, a); // otherwise, initialize byte with current low byte in Y
  
SkipByte:
  dey();
  cpy_imm(0xff); // do this until all bytes in page have been erased
  if (!zero_flag) { goto InitByteLoop; }
  dex(); // go onto the next page
  if (!neg_flag) { goto InitPageLoop; } // do this until all pages of memory have been erased
  // -------------------------------------------------------------------------------------
}

void GetAreaMusic(void) {
  lda_abs(OperMode); // if in title screen mode, leave
  if (zero_flag) { return; }
  lda_abs(AltEntranceControl); // check for specific alternate mode of entry
  cmp_imm(0x2); // if found, branch without checking starting position
  if (zero_flag) { goto ChkAreaType; } // from area object data header
  ldy_imm(0x5); // select music for pipe intro scene by default
  lda_abs(PlayerEntranceCtrl); // check value from level header for certain values
  cmp_imm(0x6);
  if (zero_flag) { goto StoreMusic; } // load music for pipe intro scene if header
  cmp_imm(0x7); // start position either value $06 or $07
  if (zero_flag) { goto StoreMusic; }
  
ChkAreaType:
  ldy_abs(AreaType); // load area type as offset for music bit
  lda_abs(CloudTypeOverride);
  if (zero_flag) { goto StoreMusic; } // check for cloud type override
  ldy_imm(0x4); // select music for cloud type level if found
  
StoreMusic:
  lda_absy(MusicSelectData); // otherwise select appropriate music for level type
  ram[AreaMusicQueue] = a; // store in queue and leave
  // -------------------------------------------------------------------------------------
}

void Entrance_GameTimerSetup(void) {
  lda_abs(ScreenLeft_PageLoc); // set current page for area objects
  ram[Player_PageLoc] = a; // as page location for player
  lda_imm(0x28); // store value here
  ram[VerticalForceDown] = a; // for fractional movement downwards if necessary
  lda_imm(0x1); // set high byte of player position and
  ram[PlayerFacingDir] = a; // set facing direction so that player faces right
  ram[Player_Y_HighPos] = a;
  lda_imm(0x0); // set player state to on the ground by default
  ram[Player_State] = a;
  dec_abs(Player_CollisionBits); // initialize player's collision bits
  ldy_imm(0x0); // initialize halfway page
  ram[HalfwayPage] = y;
  lda_abs(AreaType); // check area type
  // if water type, set swimming flag, otherwise do not set
  if (zero_flag) {
    iny();
  }
  // ChkStPos:
  ram[SwimmingFlag] = y;
  ldx_abs(PlayerEntranceCtrl); // get starting position loaded from header
  ldy_abs(AltEntranceControl); // check alternate mode of entry flag for 0 or 1
  if (!zero_flag) {
    cpy_imm(0x1);
    if (!zero_flag) {
      ldx_absy(AltYPosOffset - 2); // if not 0 or 1, override $0710 with new offset in X
    }
  }
  // SetStPos:
  lda_absy(PlayerStarting_X_Pos); // load appropriate horizontal position
  ram[Player_X_Position] = a; // and vertical positions for the player, using
  lda_absx(PlayerStarting_Y_Pos); // AltEntranceControl as offset for horizontal and either $0710
  ram[Player_Y_Position] = a; // or value that overwrote $0710 as offset for vertical
  lda_absx(PlayerBGPriorityData);
  ram[Player_SprAttrib] = a; // set player sprite attributes using offset in X
  GetPlayerColors(); // get appropriate player palette
  ldy_abs(GameTimerSetting); // get timer control value from header
  // if set to zero, branch (do not use dummy byte for this)
  if (!zero_flag) {
    lda_abs(FetchNewGameTimerFlag); // do we need to set the game timer? if not, use
    // old game timer setting
    if (!zero_flag) {
      lda_absy(GameTimerData); // if game timer is set and game timer flag is also set,
      ram[GameTimerDisplay] = a; // use value of game timer control for first digit of game timer
      lda_imm(0x1);
      ram[GameTimerDisplay + 2] = a; // set last digit of game timer to 1
      lsr_acc();
      ram[GameTimerDisplay + 1] = a; // set second digit of game timer
      ram[FetchNewGameTimerFlag] = a; // clear flag for game timer reset
      ram[StarInvincibleTimer] = a; // clear star mario timer
    }
  }
  // ChkOverR:
  ldy_abs(JoypadOverride); // if controller bits not set, branch to skip this part
  if (!zero_flag) {
    lda_imm(0x3); // set player state to climbing
    ram[Player_State] = a;
    ldx_imm(0x0); // set offset for first slot, for block object
    InitBlock_XY_Pos();
    lda_imm(0xf0); // set vertical coordinate for block object
    ram[Block_Y_Position] = a;
    ldx_imm(0x5); // set offset in X for last enemy object buffer slot
    ldy_imm(0x0); // set offset in Y for object coordinates used earlier
    Setup_Vine(); // do a sub to grow vine
  }
  // ChkSwimE:
  ldy_abs(AreaType); // if level not water-type,
  // skip this subroutine
  if (zero_flag) {
    SetupBubble(); // otherwise, execute sub to set up air bubbles
  }
  // SetPESub:
  lda_imm(0x7); // set to run player entrance subroutine
  ram[GameEngineSubroutine] = a; // on the next frame of game engine
  // -------------------------------------------------------------------------------------
  // page numbers are in order from -1 to -4
}

void PlayerLoseLife(void) {
  inc_abs(DisableScreenFlag); // disable screen and sprite 0 check
  lda_imm(0x0);
  ram[Sprite0HitDetectFlag] = a;
  lda_imm(Silence); // silence music
  ram[EventMusicQueue] = a;
  dec_abs(NumberofLives); // take one life from player
  // if player still has lives, branch
  if (neg_flag) {
    lda_imm(0x0);
    ram[OperMode_Task] = a; // initialize mode task,
    lda_imm(GameOverModeValue); // switch to game over mode
    ram[OperMode] = a; // and leave
    return;
  }
  // StillInGame:
  lda_abs(WorldNumber); // multiply world number by 2 and use
  asl_acc(); // as offset
  tax();
  lda_abs(LevelNumber); // if in area -3 or -4, increment
  and_imm(0x2); // offset by one byte, otherwise
  // leave offset alone
  if (!zero_flag) {
    inx();
  }
  // GetHalfway:
  ldy_absx(HalfwayPageNybbles); // get halfway page number with offset
  lda_abs(LevelNumber); // check area number's LSB
  lsr_acc();
  tya(); // if in area -2 or -4, use lower nybble
  if (!carry_flag) {
    lsr_acc(); // move higher nybble to lower if area
    lsr_acc(); // number is -1 or -3
    lsr_acc();
    lsr_acc();
  }
  // MaskHPNyb:
  and_imm(0b00001111); // mask out all but lower nybble
  cmp_abs(ScreenLeft_PageLoc);
  // left side of screen must be at the halfway page,
  if (!zero_flag) {
    // otherwise player must start at the
    if (carry_flag) {
      lda_imm(0x0); // beginning of the level
    }
  }
  // SetHalfway:
  ram[HalfwayPage] = a; // store as halfway page for player
  TransposePlayers(); // switch players around if 2-player game
  ContinueGame(); // continue the game
  // -------------------------------------------------------------------------------------
}

void TransposePlayers(void) {
  carry_flag = true; // set carry flag by default to end game
  lda_abs(NumberOfPlayers); // if only a 1 player game, leave
  if (zero_flag) { return; }
  lda_abs(OffScr_NumberofLives); // does offscreen player have any lives left?
  if (neg_flag) { return; } // branch if not
  lda_abs(CurrentPlayer); // invert bit to update
  eor_imm(0b00000001); // which player is on the screen
  ram[CurrentPlayer] = a;
  ldx_imm(0x6);
  
TransLoop:
  lda_absx(OnscreenPlayerInfo); // transpose the information
  pha(); // of the onscreen player
  lda_absx(OffscreenPlayerInfo); // with that of the offscreen player
  ram[OnscreenPlayerInfo + x] = a;
  pla();
  ram[OffscreenPlayerInfo + x] = a;
  dex();
  if (!neg_flag) { goto TransLoop; }
  carry_flag = false; // clear carry flag to get game going
  // -------------------------------------------------------------------------------------
}

void DoNothing1(void) {
  lda_imm(0xff); // this is residual code, this value is
  ram[0x6c9] = a; // not used anywhere in the program
  DoNothing2(); // <fallthrough>
}

void DoNothing2(void) {
  // -------------------------------------------------------------------------------------
}

void AreaParserTaskHandler(void) {
  ldy_abs(AreaParserTaskNum); // check number of tasks here
  // if already set, go ahead
  if (zero_flag) {
    ldy_imm(0x8);
    ram[AreaParserTaskNum] = y; // otherwise, set eight by default
  }
  // DoAPTasks:
  dey();
  tya();
  AreaParserTasks();
  dec_abs(AreaParserTaskNum); // if all tasks not complete do not
  if (zero_flag) {
    RenderAttributeTables();
  }
}

void AreaParserTasks(void) {
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: IncrementColumnPos(); return;
    case 1: RenderAreaGraphics(); return;
    case 2: RenderAreaGraphics(); return;
    case 3: AreaParserCore(); return;
    case 4: IncrementColumnPos(); return;
    case 5: RenderAreaGraphics(); return;
    case 6: RenderAreaGraphics(); return;
    case 7: AreaParserCore(); return;
  }
}

void IncrementColumnPos(void) {
  inc_abs(CurrentColumnPos); // increment column where we're at
  lda_abs(CurrentColumnPos);
  and_imm(0b00001111); // mask out higher nybble
  if (zero_flag) {
    ram[CurrentColumnPos] = a; // if no bits left set, wrap back to zero (0-f)
    inc_abs(CurrentPageLoc); // and increment page number where we're at
  }
  // NoColWrap:
  inc_abs(BlockBufferColumnPos); // increment column offset where we're at
  lda_abs(BlockBufferColumnPos);
  and_imm(0b00011111); // mask out all but 5 LSB (0-1f)
  ram[BlockBufferColumnPos] = a; // and save
  // -------------------------------------------------------------------------------------
  // $00 - used as counter, store for low nybble for background, ceiling byte for terrain
  // $01 - used to store floor byte for terrain
  // $07 - used to store terrain metatile
  // $06-$07 - used to store block buffer address
}

void AreaParserCore(void) {
  lda_abs(BackloadingFlag); // check to see if we are starting right of start
  // if not, go ahead and render background, foreground and terrain
  if (!zero_flag) {
    ProcessAreaData(); // otherwise skip ahead and load level data
  }
  // RenderSceneryTerrain:
  ldx_imm(0xc);
  lda_imm(0x0);
  
ClrMTBuf:
  ram[MetatileBuffer + x] = a; // clear out metatile buffer
  dex();
  if (!neg_flag) { goto ClrMTBuf; }
  ldy_abs(BackgroundScenery); // do we need to render the background scenery?
  // if not, skip to check the foreground
  if (!zero_flag) {
    lda_abs(CurrentPageLoc); // otherwise check for every third page
    
ThirdP:
    cmp_imm(0x3);
    // if less than three we're there
    if (!neg_flag) {
      carry_flag = true;
      sbc_imm(0x3); // if 3 or more, subtract 3 and
      if (!neg_flag) { goto ThirdP; } // do an unconditional branch
    }
    // RendBack:
    asl_acc(); // move results to higher nybble
    asl_acc();
    asl_acc();
    asl_acc();
    adc_absy(BSceneDataOffsets - 1); // add to it offset loaded from here
    adc_abs(CurrentColumnPos); // add to the result our current column position
    tax();
    lda_absx(BackSceneryData); // load data from sum of offsets
    // if zero, no scenery for that part
    if (!zero_flag) {
      pha();
      and_imm(0xf); // save to stack and clear high nybble
      carry_flag = true;
      sbc_imm(0x1); // subtract one (because low nybble is $01-$0c)
      ram[0x0] = a; // save low nybble
      asl_acc(); // multiply by three (shift to left and add result to old one)
      adc_zp(0x0); // note that since d7 was nulled, the carry flag is always clear
      tax(); // save as offset for background scenery metatile data
      pla(); // get high nybble from stack, move low
      lsr_acc();
      lsr_acc();
      lsr_acc();
      lsr_acc();
      tay(); // use as second offset (used to determine height)
      lda_imm(0x3); // use previously saved memory location for counter
      ram[0x0] = a;
      
SceLoop1:
      lda_absx(BackSceneryMetatiles); // load metatile data from offset of (lsb - 1) * 3
      ram[MetatileBuffer + y] = a; // store into buffer from offset of (msb / 16)
      inx();
      iny();
      cpy_imm(0xb); // if at this location, leave loop
      if (!zero_flag) {
        dec_zp(0x0); // decrement until counter expires, barring exception
        if (!zero_flag) { goto SceLoop1; }
      }
    }
  }
  // RendFore:
  ldx_abs(ForegroundScenery); // check for foreground data needed or not
  // if not, skip this part
  if (!zero_flag) {
    ldy_absx(FSceneDataOffsets - 1); // load offset from location offset by header value, then
    ldx_imm(0x0); // reinit X
    
SceLoop2:
    lda_absy(ForeSceneryData); // load data until counter expires
    // do not store if zero found
    if (!zero_flag) {
      ram[MetatileBuffer + x] = a;
    }
    // NoFore:
    iny();
    inx();
    cpx_imm(0xd); // store up to end of metatile buffer
    if (!zero_flag) { goto SceLoop2; }
  }
  // RendTerr:
  ldy_abs(AreaType); // check world type for water level
  // if not water level, skip this part
  if (zero_flag) {
    lda_abs(WorldNumber); // check world number, if not world number eight
    cmp_imm(World8); // then skip this part
    if (zero_flag) {
      lda_imm(0x62); // if set as water level and world number eight,
      goto StoreMT; // use castle wall metatile as terrain type
    }
  }
  // TerMTile:
  lda_absy(TerrainMetatiles); // otherwise get appropriate metatile for area type
  ldy_abs(CloudTypeOverride); // check for cloud type override
  // if not set, keep value otherwise
  if (!zero_flag) {
    lda_imm(0x88); // use cloud block terrain
  }
  
StoreMT:
  ram[0x7] = a; // store value here
  ldx_imm(0x0); // initialize X, use as metatile buffer offset
  lda_abs(TerrainControl); // use yet another value from the header
  asl_acc(); // multiply by 2 and use as yet another offset
  tay();
  
TerrLoop:
  lda_absy(TerrainRenderBits); // get one of the terrain rendering bit data
  ram[0x0] = a;
  iny(); // increment Y and use as offset next time around
  ram[0x1] = y;
  lda_abs(CloudTypeOverride); // skip if value here is zero
  if (!zero_flag) {
    cpx_imm(0x0); // otherwise, check if we're doing the ceiling byte
    if (!zero_flag) {
      lda_zp(0x0); // if not, mask out all but d3
      and_imm(0b00001000);
      ram[0x0] = a;
    }
  }
  // NoCloud2:
  ldy_imm(0x0); // start at beginning of bitmasks
  
TerrBChk:
  lda_absy(Bitmasks); // load bitmask, then perform AND on contents of first byte
  bit_zp(0x0);
  // if not set, skip this part (do not write terrain to buffer)
  if (!zero_flag) {
    lda_zp(0x7);
    ram[MetatileBuffer + x] = a; // load terrain type metatile number and store into buffer here
  }
  // NextTBit:
  inx(); // continue until end of buffer
  cpx_imm(0xd);
  // if we're at the end, break out of this loop
  if (!zero_flag) {
    lda_abs(AreaType); // check world type for underground area
    cmp_imm(0x2);
    // if not underground, skip this part
    if (zero_flag) {
      cpx_imm(0xb);
      // if we're at the bottom of the screen, override
      if (zero_flag) {
        lda_imm(0x54); // old terrain type with ground level terrain type
        ram[0x7] = a;
      }
    }
    // EndUChk:
    iny(); // increment bitmasks offset in Y
    cpy_imm(0x8);
    if (!zero_flag) { goto TerrBChk; } // if not all bits checked, loop back
    ldy_zp(0x1);
    if (!zero_flag) { goto TerrLoop; } // unconditional branch, use Y to load next byte
  }
  // RendBBuf:
  ProcessAreaData(); // do the area data loading routine now
  lda_abs(BlockBufferColumnPos);
  GetBlockBufferAddr(); // get block buffer address from where we're at
  ldx_imm(0x0);
  ldy_imm(0x0); // init index regs and start at beginning of smaller buffer
  
ChkMTLow:
  ram[0x0] = y;
  lda_absx(MetatileBuffer); // load stored metatile number
  and_imm(0b11000000); // mask out all but 2 MSB
  asl_acc();
  rol_acc(); // make %xx000000 into %000000xx
  rol_acc();
  tay(); // use as offset in Y
  lda_absx(MetatileBuffer); // reload original unmasked value here
  cmp_absy(BlockBuffLowBounds); // check for certain values depending on bits set
  // if equal or greater, branch
  if (!carry_flag) {
    lda_imm(0x0); // if less, init value before storing
  }
  // StrBlock:
  ldy_zp(0x0); // get offset for block buffer
  dynamic_ram_write(read_word(0x6) + y, a); // store value into block buffer
  tya();
  carry_flag = false; // add 16 (move down one row) to offset
  adc_imm(0x10);
  tay();
  inx(); // increment column value
  cpx_imm(0xd);
  if (!carry_flag) { goto ChkMTLow; } // continue until we pass last row, then leave
  // numbers lower than these with the same attribute bits
  // will not be stored in the block buffer
}

void ProcessAreaData(void) {
  ldx_imm(0x2); // start at the end of area object buffer
  
ProcADLoop:
  ram[ObjectOffset] = x;
  lda_imm(0x0); // reset flag
  ram[BehindAreaParserFlag] = a;
  ldy_abs(AreaDataOffset); // get offset of area data pointer
  lda_indy(AreaData); // get first byte of area object
  cmp_imm(0xfd); // if end-of-area, skip all this crap
  if (zero_flag) { goto RdyDecode; }
  lda_absx(AreaObjectLength); // check area object buffer flag
  if (!neg_flag) { goto RdyDecode; } // if buffer not negative, branch, otherwise
  iny();
  lda_indy(AreaData); // get second byte of area object
  asl_acc(); // check for page select bit (d7), branch if not set
  if (!carry_flag) { goto Chk1Row13; }
  lda_abs(AreaObjectPageSel); // check page select
  if (!zero_flag) { goto Chk1Row13; }
  inc_abs(AreaObjectPageSel); // if not already set, set it now
  inc_abs(AreaObjectPageLoc); // and increment page location
  
Chk1Row13:
  dey();
  lda_indy(AreaData); // reread first byte of level object
  and_imm(0xf); // mask out high nybble
  cmp_imm(0xd); // row 13?
  if (!zero_flag) { goto Chk1Row14; }
  iny(); // if so, reread second byte of level object
  lda_indy(AreaData);
  dey(); // decrement to get ready to read first byte
  and_imm(0b01000000); // check for d6 set (if not, object is page control)
  if (!zero_flag) { goto CheckRear; }
  lda_abs(AreaObjectPageSel); // if page select is set, do not reread
  if (!zero_flag) { goto CheckRear; }
  iny(); // if d6 not set, reread second byte
  lda_indy(AreaData);
  and_imm(0b00011111); // mask out all but 5 LSB and store in page control
  ram[AreaObjectPageLoc] = a;
  inc_abs(AreaObjectPageSel); // increment page select
  goto NextAObj;
  
Chk1Row14:
  cmp_imm(0xe); // row 14?
  if (!zero_flag) { goto CheckRear; }
  lda_abs(BackloadingFlag); // check flag for saved page number and branch if set
  if (!zero_flag) { goto RdyDecode; } // to render the object (otherwise bg might not look right)
  
CheckRear:
  lda_abs(AreaObjectPageLoc); // check to see if current page of level object is
  cmp_abs(CurrentPageLoc); // behind current page of renderer
  if (!carry_flag) { goto SetBehind; } // if so branch
  
RdyDecode:
  DecodeAreaData(); // do sub and do not turn on flag
  goto ChkLength;
  
SetBehind:
  inc_abs(BehindAreaParserFlag); // turn on flag if object is behind renderer
  
NextAObj:
  IncAreaObjOffset(); // increment buffer offset and move on
  
ChkLength:
  ldx_zp(ObjectOffset); // get buffer offset
  lda_absx(AreaObjectLength); // check object length for anything stored here
  if (neg_flag) { goto ProcLoopb; } // if not, branch to handle loopback
  dec_absx(AreaObjectLength); // otherwise decrement length or get rid of it
  
ProcLoopb:
  dex(); // decrement buffer offset
  if (!neg_flag) { goto ProcADLoop; } // and loopback unless exceeded buffer
  lda_abs(BehindAreaParserFlag); // check for flag set if objects were behind renderer
  if (!zero_flag) { ProcessAreaData(); return; } // branch if true to load more level data, otherwise
  lda_abs(BackloadingFlag); // check for flag set if starting right of page $00
  if (!zero_flag) { ProcessAreaData(); return; } // branch if true to load more level data, otherwise leave
}

void IncAreaObjOffset(void) {
  inc_abs(AreaDataOffset); // increment offset of level pointer
  inc_abs(AreaDataOffset);
  lda_imm(0x0); // reset page select
  ram[AreaObjectPageSel] = a;
}

void DecodeAreaData(void) {
  lda_absx(AreaObjectLength); // check current buffer flag
  if (neg_flag) { goto Chk1stB; }
  ldy_absx(AreaObjOffsetBuffer); // if not, get offset from buffer
  
Chk1stB:
  ldx_imm(0x10); // load offset of 16 for special row 15
  lda_indy(AreaData); // get first byte of level object again
  cmp_imm(0xfd);
  if (zero_flag) { goto LeavePar; } // if end of level, leave this routine
  and_imm(0xf); // otherwise, mask out low nybble
  cmp_imm(0xf); // row 15?
  if (zero_flag) { goto ChkRow14; } // if so, keep the offset of 16
  ldx_imm(0x8); // otherwise load offset of 8 for special row 12
  cmp_imm(0xc); // row 12?
  if (zero_flag) { goto ChkRow14; } // if so, keep the offset value of 8
  ldx_imm(0x0); // otherwise nullify value by default
  
ChkRow14:
  ram[0x7] = x; // store whatever value we just loaded here
  ldx_zp(ObjectOffset); // get object offset again
  cmp_imm(0xe); // row 14?
  if (!zero_flag) { goto ChkRow13; }
  lda_imm(0x0); // if so, load offset with $00
  ram[0x7] = a;
  lda_imm(0x2e); // and load A with another value
  if (!zero_flag) { goto NormObj; } // unconditional branch
  
ChkRow13:
  cmp_imm(0xd); // row 13?
  if (!zero_flag) { goto ChkSRows; }
  lda_imm(0x22); // if so, load offset with 34
  ram[0x7] = a;
  iny(); // get next byte
  lda_indy(AreaData);
  and_imm(0b01000000); // mask out all but d6 (page control obj bit)
  if (zero_flag) { goto LeavePar; } // if d6 clear, branch to leave (we handled this earlier)
  lda_indy(AreaData); // otherwise, get byte again
  and_imm(0b01111111); // mask out d7
  cmp_imm(0x4b); // check for loop command in low nybble
  if (!zero_flag) { goto Mask2MSB; } // (plus d6 set for object other than page control)
  inc_abs(LoopCommand); // if loop command, set loop command flag
  
Mask2MSB:
  and_imm(0b00111111); // mask out d7 and d6
  goto NormObj; // and jump
  
ChkSRows:
  cmp_imm(0xc); // row 12-15?
  if (carry_flag) { goto SpecObj; }
  iny(); // if not, get second byte of level object
  lda_indy(AreaData);
  and_imm(0b01110000); // mask out all but d6-d4
  if (!zero_flag) { goto LrgObj; } // if any bits set, branch to handle large object
  lda_imm(0x16);
  ram[0x7] = a; // otherwise set offset of 24 for small object
  lda_indy(AreaData); // reload second byte of level object
  and_imm(0b00001111); // mask out higher nybble and jump
  goto NormObj;
  
LrgObj:
  ram[0x0] = a; // store value here (branch for large objects)
  cmp_imm(0x70); // check for vertical pipe object
  if (!zero_flag) { goto NotWPipe; }
  lda_indy(AreaData); // if not, reload second byte
  and_imm(0b00001000); // mask out all but d3 (usage control bit)
  if (zero_flag) { goto NotWPipe; } // if d3 clear, branch to get original value
  lda_imm(0x0); // otherwise, nullify value for warp pipe
  ram[0x0] = a;
  
NotWPipe:
  lda_zp(0x0); // get value and jump ahead
  goto MoveAOId;
  
SpecObj:
  iny(); // branch here for rows 12-15
  lda_indy(AreaData);
  and_imm(0b01110000); // get next byte and mask out all but d6-d4
  
MoveAOId:
  lsr_acc(); // move d6-d4 to lower nybble
  lsr_acc();
  lsr_acc();
  lsr_acc();
  
NormObj:
  ram[0x0] = a; // store value here (branch for small objects and rows 13 and 14)
  lda_absx(AreaObjectLength); // is there something stored here already?
  if (!neg_flag) { RunAObj(); return; } // if so, branch to do its particular sub
  lda_abs(AreaObjectPageLoc); // otherwise check to see if the object we've loaded is on the
  cmp_abs(CurrentPageLoc); // same page as the renderer, and if so, branch
  if (zero_flag) { goto InitRear; }
  ldy_abs(AreaDataOffset); // if not, get old offset of level pointer
  lda_indy(AreaData); // and reload first byte
  and_imm(0b00001111);
  cmp_imm(0xe); // row 14?
  if (!zero_flag) { goto LeavePar; }
  lda_abs(BackloadingFlag); // if so, check backloading flag
  if (!zero_flag) { StrAObj(); return; } // if set, branch to render object, else leave
  
LeavePar:
  return;
  
InitRear:
  lda_abs(BackloadingFlag); // check backloading flag to see if it's been initialized
  if (zero_flag) { BackColC(); return; } // branch to column-wise check
  lda_imm(0x0); // if not, initialize both backloading and
  ram[BackloadingFlag] = a; // behind-renderer flags and leave
  ram[BehindAreaParserFlag] = a;
  ram[ObjectOffset] = a;
  LoopCmdE(); // <fallthrough>
}

void LoopCmdE(void) {
}

void BackColC(void) {
  ldy_abs(AreaDataOffset); // get first byte again
  lda_indy(AreaData);
  and_imm(0b11110000); // mask out low nybble and move high to low
  lsr_acc();
  lsr_acc();
  lsr_acc();
  lsr_acc();
  cmp_abs(CurrentColumnPos); // is this where we're at?
  if (zero_flag) {
    StrAObj(); // <fallthrough>
  }
}

void StrAObj(void) {
  lda_abs(AreaDataOffset); // if so, load area obj offset and store in buffer
  ram[AreaObjOffsetBuffer + x] = a;
  IncAreaObjOffset(); // do sub to increment to next object data
  RunAObj(); // <fallthrough>
}

void RunAObj(void) {
  lda_zp(0x0); // get stored value and add offset to it
  carry_flag = false; // then use the jump engine with current contents of A
  adc_zp(0x7);
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: VerticalPipe(); return;
    case 1: AreaStyleObject(); return;
    case 2: RowOfBricks(); return;
    case 3: RowOfSolidBlocks(); return;
    case 4: RowOfCoins(); return;
    case 5: ColumnOfBricks(); return;
    case 6: ColumnOfSolidBlocks(); return;
    case 7: VerticalPipe(); return;
    case 8: Hole_Empty(); return;
    case 9: PulleyRopeObject(); return;
    case 10: Bridge_High(); return;
    case 11: Bridge_Middle(); return;
    case 12: Bridge_Low(); return;
    case 13: Hole_Water(); return;
    case 14: QuestionBlockRow_High(); return;
    case 15: QuestionBlockRow_Low(); return;
    case 16: EndlessRope(); return;
    case 17: BalancePlatRope(); return;
    case 18: CastleObject(); return;
    case 19: StaircaseObject(); return;
    case 20: ExitPipe(); return;
    case 21: FlagBalls_Residual(); return;
    case 22: QuestionBlock(); return;
    case 23: QuestionBlock(); return;
    case 24: QuestionBlock(); return;
    case 25: Hidden1UpBlock(); return;
    case 26: BrickWithItem(); return;
    case 27: BrickWithItem(); return;
    case 28: BrickWithItem(); return;
    case 29: BrickWithCoins(); return;
    case 30: BrickWithItem(); return;
    case 31: WaterPipe(); return;
    case 32: EmptyBlock(); return;
    case 33: Jumpspring(); return;
    case 34: IntroPipe(); return;
    case 35: FlagpoleObject(); return;
    case 36: AxeObj(); return;
    case 37: ChainObj(); return;
    case 38: CastleBridgeObj(); return;
    case 39: ScrollLockObject_Warp(); return;
    case 40: ScrollLockObject(); return;
    case 41: ScrollLockObject(); return;
    case 42: AreaFrenzy(); return;
    case 43: AreaFrenzy(); return;
    case 44: AreaFrenzy(); return;
    case 45: LoopCmdE(); return;
    case 46: AlterAreaAttributes(); return;
  }
}

void AlterAreaAttributes(void) {
  ldy_absx(AreaObjOffsetBuffer); // load offset for level object data saved in buffer
  iny(); // load second byte
  lda_indy(AreaData);
  pha(); // save in stack for now
  and_imm(0b01000000);
  // branch if d6 is set
  if (zero_flag) {
    pla();
    pha(); // pull and push offset to copy to A
    and_imm(0b00001111); // mask out high nybble and store as
    ram[TerrainControl] = a; // new terrain height type bits
    pla();
    and_imm(0b00110000); // pull and mask out all but d5 and d4
    lsr_acc(); // move bits to lower nybble and store
    lsr_acc(); // as new background scenery bits
    lsr_acc();
    lsr_acc();
    ram[BackgroundScenery] = a; // then leave
    return;
  }
  // Alter2:
  pla();
  and_imm(0b00000111); // mask out all but 3 LSB
  cmp_imm(0x4); // if four or greater, set color control bits
  // and nullify foreground scenery bits
  if (carry_flag) {
    ram[BackgroundColorCtrl] = a;
    lda_imm(0x0);
  }
  // SetFore:
  ram[ForegroundScenery] = a; // otherwise set new foreground scenery bits
  // --------------------------------
}

void ScrollLockObject_Warp(void) {
  ldx_imm(0x4); // load value of 4 for game text routine as default
  lda_abs(WorldNumber); // warp zone (4-3-2), then check world number
  if (!zero_flag) {
    inx(); // if world number > 1, increment for next warp zone (5)
    ldy_abs(AreaType); // check area type
    dey();
    // if ground area type, increment for last warp zone
    if (zero_flag) {
      inx(); // (8-7-6) and move on
    }
  }
  // WarpNum:
  txa();
  ram[WarpZoneControl] = a; // store number here to be used by warp zone routine
  WriteGameText(); // print text and warp zone numbers
  lda_imm(PiranhaPlant);
  KillEnemies(); // load identifier for piranha plants and do sub
  ScrollLockObject(); // <fallthrough>
}

void ScrollLockObject(void) {
  lda_abs(ScrollLock); // invert scroll lock to turn it on
  eor_imm(0b00000001);
  ram[ScrollLock] = a;
  // --------------------------------
  // $00 - used to store enemy identifier in KillEnemies
}

void AreaFrenzy(void) {
  ldx_zp(0x0); // use area object identifier bit as offset
  lda_absx(FrenzyIDData - 8); // note that it starts at 8, thus weird address here
  ldy_imm(0x5);
  
FreCompLoop:
  dey(); // check regular slots of enemy object buffer
  // if all slots checked and enemy object not found, branch to store
  if (!neg_flag) {
    cmp_zpy(Enemy_ID); // check for enemy object in buffer versus frenzy object
    if (!zero_flag) { goto FreCompLoop; }
    lda_imm(0x0); // if enemy object already present, nullify queue and leave
  }
  // ExitAFrenzy:
  ram[EnemyFrenzyQueue] = a; // store enemy into frenzy queue
  // --------------------------------
  // $06 - used by MushroomLedge to store length
}

void AreaStyleObject(void) {
  lda_abs(AreaStyle); // load level object style and jump to the right sub
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: TreeLedge(); return;
    case 1: MushroomLedge(); return;
    case 2: BulletBillCannon(); return;
  }
}

void TreeLedge(void) {
  GetLrgObjAttrib(); // get row and length of green ledge
  lda_absx(AreaObjectLength); // check length counter for expiration
  if (!zero_flag) {
    if (neg_flag) {
      tya();
      ram[AreaObjectLength + x] = a; // store lower nybble into buffer flag as length of ledge
      lda_abs(CurrentPageLoc);
      ora_abs(CurrentColumnPos); // are we at the start of the level?
      if (!zero_flag) {
        lda_imm(0x16); // render start of tree ledge
        NoUnder();
        return;
      }
    }
    // MidTreeL:
    ldx_zp(0x7);
    lda_imm(0x17); // render middle of tree ledge
    ram[MetatileBuffer + x] = a; // note that this is also used if ledge position is
    lda_imm(0x4c); // at the start of level for continuous effect
    AllUnder(); // now render the part underneath
    return;
  }
  // EndTreeL:
  lda_imm(0x18); // render end of tree ledge
  NoUnder();
}

void MushroomLedge(void) {
  ChkLrgObjLength(); // get shroom dimensions
  ram[0x6] = y; // store length here for now
  if (carry_flag) {
    lda_absx(AreaObjectLength); // divide length by 2 and store elsewhere
    lsr_acc();
    ram[MushroomLedgeHalfLen + x] = a;
    lda_imm(0x19); // render start of mushroom
    NoUnder();
    return;
  }
  // EndMushL:
  lda_imm(0x1b); // if at the end, render end of mushroom
  ldy_absx(AreaObjectLength);
  if (zero_flag) {
    NoUnder();
    return;
  }
  lda_absx(MushroomLedgeHalfLen); // get divided length and store where length
  ram[0x6] = a; // was stored originally
  ldx_zp(0x7);
  lda_imm(0x1a);
  ram[MetatileBuffer + x] = a; // render middle of mushroom
  cpy_zp(0x6); // are we smack dab in the center?
  if (zero_flag) {
    inx();
    lda_imm(0x4f);
    ram[MetatileBuffer + x] = a; // render stem top of mushroom underneath the middle
    lda_imm(0x50);
    AllUnder(); // <fallthrough>
  }
}

void AllUnder(void) {
  inx();
  ldy_imm(0xf); // set $0f to render all way down
  RenderUnderPart(); // now render the stem of mushroom
}

void NoUnder(void) {
  ldx_zp(0x7); // load row of ledge
  ldy_imm(0x0); // set 0 for no bottom on this part
  RenderUnderPart();
  // --------------------------------
  // tiles used by pulleys and rope object
}

void PulleyRopeObject(void) {
  ChkLrgObjLength(); // get length of pulley/rope object
  ldy_imm(0x0); // initialize metatile offset
  // if starting, render left pulley
  if (!carry_flag) {
    iny();
    lda_absx(AreaObjectLength); // if not at the end, render rope
    if (zero_flag) {
      iny(); // otherwise render right pulley
    }
  }
  // RenderPul:
  lda_absy(PulleyRopeMetatiles);
  ram[MetatileBuffer] = a; // render at the top of the screen
  // --------------------------------
  // $06 - used to store upper limit of rows for CastleObject
}

void CastleObject(void) {
  GetLrgObjAttrib(); // save lower nybble as starting row
  ram[0x7] = y; // if starting row is above $0a, game will crash!!!
  ldy_imm(0x4);
  ChkLrgObjFixedLength(); // load length of castle if not already loaded
  txa();
  pha(); // save obj buffer offset to stack
  ldy_absx(AreaObjectLength); // use current length as offset for castle data
  ldx_zp(0x7); // begin at starting row
  lda_imm(0xb);
  ram[0x6] = a; // load upper limit of number of rows to print
  
CRendLoop:
  lda_absy(CastleMetatiles); // load current byte using offset
  ram[MetatileBuffer + x] = a;
  inx(); // store in buffer and increment buffer offset
  lda_zp(0x6);
  if (zero_flag) { goto ChkCFloor; } // have we reached upper limit yet?
  iny(); // if not, increment column-wise
  iny(); // to byte in next row
  iny();
  iny();
  iny();
  dec_zp(0x6); // move closer to upper limit
  
ChkCFloor:
  cpx_imm(0xb); // have we reached the row just before floor?
  if (!zero_flag) { goto CRendLoop; } // if not, go back and do another row
  pla();
  tax(); // get obj buffer offset from before
  lda_abs(CurrentPageLoc);
  if (zero_flag) { return; } // if we're at page 0, we do not need to do anything else
  lda_absx(AreaObjectLength); // check length
  cmp_imm(0x1); // if length almost about to expire, put brick at floor
  if (zero_flag) { goto PlayerStop; }
  ldy_zp(0x7); // check starting row for tall castle ($00)
  if (!zero_flag) { goto NotTall; }
  cmp_imm(0x3); // if found, then check to see if we're at the second column
  if (zero_flag) { goto PlayerStop; }
  
NotTall:
  cmp_imm(0x2); // if not tall castle, check to see if we're at the third column
  if (!zero_flag) { return; } // if we aren't and the castle is tall, don't create flag yet
  GetAreaObjXPosition(); // otherwise, obtain and save horizontal pixel coordinate
  pha();
  FindEmptyEnemySlot(); // find an empty place on the enemy object buffer
  pla();
  ram[Enemy_X_Position + x] = a; // then write horizontal coordinate for star flag
  lda_abs(CurrentPageLoc);
  ram[Enemy_PageLoc + x] = a; // set page location for star flag
  lda_imm(0x1);
  ram[Enemy_Y_HighPos + x] = a; // set vertical high byte
  ram[Enemy_Flag + x] = a; // set flag for buffer
  lda_imm(0x90);
  ram[Enemy_Y_Position + x] = a; // set vertical coordinate
  lda_imm(StarFlagObject); // set star flag value in buffer itself
  ram[Enemy_ID + x] = a;
  return;
  
PlayerStop:
  ldy_imm(0x52); // put brick at floor to stop player at end of level
  ram[MetatileBuffer + 10] = y; // this is only done if we're on the second column
  // --------------------------------
}

void WaterPipe(void) {
  GetLrgObjAttrib(); // get row and lower nybble
  ldy_absx(AreaObjectLength); // get length (residual code, water pipe is 1 col thick)
  ldx_zp(0x7); // get row
  lda_imm(0x6b);
  ram[MetatileBuffer + x] = a; // draw something here and below it
  lda_imm(0x6c);
  ram[MetatileBuffer + 1 + x] = a;
  // --------------------------------
  // $05 - used to store length of vertical shaft in RenderSidewaysPipe
  // $06 - used to store leftover horizontal length in RenderSidewaysPipe
  //  and vertical length in VerticalPipe and GetPipeHeight
}

void IntroPipe(void) {
  ldy_imm(0x3); // check if length set, if not set, set it
  ChkLrgObjFixedLength();
  ldy_imm(0xa); // set fixed value and render the sideways part
  RenderSidewaysPipe();
  if (!carry_flag) {
    ldx_imm(0x6); // blank everything above the vertical pipe part
    
VPipeSectLoop:
    lda_imm(0x0); // all the way to the top of the screen
    ram[MetatileBuffer + x] = a; // because otherwise it will look like exit pipe
    dex();
    if (!neg_flag) { goto VPipeSectLoop; }
    lda_absy(VerticalPipeData); // draw the end of the vertical pipe part
    ram[MetatileBuffer + 7] = a;
  }
}

void ExitPipe(void) {
  ldy_imm(0x3); // check if length set, if not set, set it
  ChkLrgObjFixedLength();
  GetLrgObjAttrib(); // get vertical length, then plow on through RenderSidewaysPipe
  RenderSidewaysPipe(); // <fallthrough>
}

void RenderSidewaysPipe(void) {
  dey(); // decrement twice to make room for shaft at bottom
  dey(); // and store here for now as vertical length
  ram[0x5] = y;
  ldy_absx(AreaObjectLength); // get length left over and store here
  ram[0x6] = y;
  ldx_zp(0x5); // get vertical length plus one, use as buffer offset
  inx();
  lda_absy(SidePipeShaftData); // check for value $00 based on horizontal offset
  cmp_imm(0x0);
  // if found, do not draw the vertical pipe shaft
  if (!zero_flag) {
    ldx_imm(0x0);
    ldy_zp(0x5); // init buffer offset and get vertical length
    RenderUnderPart(); // and render vertical shaft using tile number in A
    carry_flag = false; // clear carry flag to be used by IntroPipe
  }
  // DrawSidePart:
  ldy_zp(0x6); // render side pipe part at the bottom
  lda_absy(SidePipeTopPart);
  ram[MetatileBuffer + x] = a; // note that the pipe parts are stored
  lda_absy(SidePipeBottomPart); // backwards horizontally
  ram[MetatileBuffer + 1 + x] = a;
}

void VerticalPipe(void) {
  GetPipeHeight();
  lda_zp(0x0); // check to see if value was nullified earlier
  // (if d3, the usage control bit of second byte, was set)
  if (!zero_flag) {
    iny();
    iny();
    iny();
    iny(); // add four if usage control bit was not set
  }
  // WarpPipe:
  tya(); // save value in stack
  pha();
  lda_abs(AreaNumber);
  ora_abs(WorldNumber); // if at world 1-1, do not add piranha plant ever
  if (!zero_flag) {
    ldy_absx(AreaObjectLength); // if on second column of pipe, branch
    // (because we only need to do this once)
    if (!zero_flag) {
      FindEmptyEnemySlot(); // check for an empty moving data buffer space
      // if not found, too many enemies, thus skip
      if (!carry_flag) {
        GetAreaObjXPosition(); // get horizontal pixel coordinate
        carry_flag = false;
        adc_imm(0x8); // add eight to put the piranha plant in the center
        ram[Enemy_X_Position + x] = a; // store as enemy's horizontal coordinate
        lda_abs(CurrentPageLoc); // add carry to current page number
        adc_imm(0x0);
        ram[Enemy_PageLoc + x] = a; // store as enemy's page coordinate
        lda_imm(0x1);
        ram[Enemy_Y_HighPos + x] = a;
        ram[Enemy_Flag + x] = a; // activate enemy flag
        GetAreaObjYPosition(); // get piranha plant's vertical coordinate and store here
        ram[Enemy_Y_Position + x] = a;
        lda_imm(PiranhaPlant); // write piranha plant's value into buffer
        ram[Enemy_ID + x] = a;
        InitPiranhaPlant();
      }
    }
  }
  // DrawPipe:
  pla(); // get value saved earlier and use as Y
  tay();
  ldx_zp(0x7); // get buffer offset
  lda_absy(VerticalPipeData); // draw the appropriate pipe with the Y we loaded earlier
  ram[MetatileBuffer + x] = a; // render the top of the pipe
  inx();
  lda_absy(VerticalPipeData + 2); // render the rest of the pipe
  ldy_zp(0x6); // subtract one from length and render the part underneath
  dey();
  RenderUnderPart();
}

void Hole_Water(void) {
  ChkLrgObjLength(); // get low nybble and save as length
  lda_imm(0x86); // render waves
  ram[MetatileBuffer + 10] = a;
  ldx_imm(0xb);
  ldy_imm(0x1); // now render the water underneath
  lda_imm(0x87);
  RenderUnderPart();
  // --------------------------------
}

void QuestionBlockRow_High(void) {
  lda_imm(0x3); // start on the fourth row
  QuestionBlockRow_LowSkip(); //  .db $2c ;BIT instruction opcode
}

void QuestionBlockRow_Low(void) {
  lda_imm(0x7); // start on the eighth row
  QuestionBlockRow_LowSkip(); // <fallthrough>
}

void QuestionBlockRow_LowSkip(void) {
  pha(); // save whatever row to the stack for now
  ChkLrgObjLength(); // get low nybble and save as length
  pla();
  tax(); // render question boxes with coins
  lda_imm(0xc0);
  ram[MetatileBuffer + x] = a;
  // --------------------------------
}

void Bridge_High(void) {
  lda_imm(0x6); // start on the seventh row from top of screen
  Bridge_LowSkip(); //  .db $2c ;BIT instruction opcode
}

void Bridge_Middle(void) {
  lda_imm(0x7); // start on the eighth row
  Bridge_LowSkip(); //  .db $2c ;BIT instruction opcode
}

void Bridge_Low(void) {
  lda_imm(0x9); // start on the tenth row
  Bridge_LowSkip(); // <fallthrough>
}

void Bridge_LowSkip(void) {
  pha(); // save whatever row to the stack for now
  ChkLrgObjLength(); // get low nybble and save as length
  pla();
  tax(); // render bridge railing
  lda_imm(0xb);
  ram[MetatileBuffer + x] = a;
  inx();
  ldy_imm(0x0); // now render the bridge itself
  lda_imm(0x63);
  RenderUnderPart();
  // --------------------------------
}

void FlagBalls_Residual(void) {
  GetLrgObjAttrib(); // get low nybble from object byte
  ldx_imm(0x2); // render flag balls on third row from top
  lda_imm(0x6d); // of screen downwards based on low nybble
  RenderUnderPart();
  // --------------------------------
}

void FlagpoleObject(void) {
  lda_imm(0x24); // render flagpole ball on top
  ram[MetatileBuffer] = a;
  ldx_imm(0x1); // now render the flagpole shaft
  ldy_imm(0x8);
  lda_imm(0x25);
  RenderUnderPart();
  lda_imm(0x61); // render solid block at the bottom
  ram[MetatileBuffer + 10] = a;
  GetAreaObjXPosition();
  carry_flag = true; // get pixel coordinate of where the flagpole is,
  sbc_imm(0x8); // subtract eight pixels and use as horizontal
  ram[Enemy_X_Position + 5] = a; // coordinate for the flag
  lda_abs(CurrentPageLoc);
  sbc_imm(0x0); // subtract borrow from page location and use as
  ram[Enemy_PageLoc + 5] = a; // page location for the flag
  lda_imm(0x30);
  ram[Enemy_Y_Position + 5] = a; // set vertical coordinate for flag
  lda_imm(0xb0);
  ram[FlagpoleFNum_Y_Pos] = a; // set initial vertical coordinate for flagpole's floatey number
  lda_imm(FlagpoleFlagObject);
  ram[Enemy_ID + 5] = a; // set flag identifier, note that identifier and coordinates
  inc_zp(Enemy_Flag + 5); // use last space in enemy object buffer
  // --------------------------------
}

void EndlessRope(void) {
  ldx_imm(0x0); // render rope from the top to the bottom of screen
  ldy_imm(0xf);
  DrawRope();
}

void BalancePlatRope(void) {
  txa(); // save object buffer offset for now
  pha();
  ldx_imm(0x1); // blank out all from second row to the bottom
  ldy_imm(0xf); // with blank used for balance platform rope
  lda_imm(0x44);
  RenderUnderPart();
  pla(); // get back object buffer offset
  tax();
  GetLrgObjAttrib(); // get vertical length from lower nybble
  ldx_imm(0x1);
  DrawRope(); // <fallthrough>
}

void DrawRope(void) {
  lda_imm(0x40); // render the actual rope
  RenderUnderPart();
  // --------------------------------
}

void RowOfCoins(void) {
  ldy_abs(AreaType); // get area type
  lda_absy(CoinMetatileData); // load appropriate coin metatile
  GetRow();
  // --------------------------------
}

void CastleBridgeObj(void) {
  ldy_imm(0xc); // load length of 13 columns
  ChkLrgObjFixedLength();
  ChainObj(); return;
}

void AxeObj(void) {
  lda_imm(0x8); // load bowser's palette into sprite portion of palette
  ram[VRAM_Buffer_AddrCtrl] = a;
  ChainObj(); // <fallthrough>
}

void ChainObj(void) {
  ldy_zp(0x0); // get value loaded earlier from decoder
  ldx_absy(C_ObjectRow - 2); // get appropriate row and metatile for object
  lda_absy(C_ObjectMetatile - 2);
  ColObj();
}

void EmptyBlock(void) {
  GetLrgObjAttrib(); // get row location
  ldx_zp(0x7);
  lda_imm(0xc4);
  ColObj(); // <fallthrough>
}

void ColObj(void) {
  ldy_imm(0x0); // column length of 1
  RenderUnderPart();
  // --------------------------------
}

void RowOfBricks(void) {
  ldy_abs(AreaType); // load area type obtained from area offset pointer
  lda_abs(CloudTypeOverride); // check for cloud type override
  if (!zero_flag) {
    ldy_imm(0x4); // if cloud type, override area type
  }
  // DrawBricks:
  lda_absy(BrickMetatiles); // get appropriate metatile
  GetRow(); // and go render it
}

void RowOfSolidBlocks(void) {
  ldy_abs(AreaType); // load area type obtained from area offset pointer
  lda_absy(SolidBlockMetatiles); // get metatile
  GetRow(); // <fallthrough>
}

void GetRow(void) {
  pha(); // store metatile here
  ChkLrgObjLength(); // get row number, load length
  DrawRow(); // <fallthrough>
}

void DrawRow(void) {
  ldx_zp(0x7);
  ldy_imm(0x0); // set vertical height of 1
  pla();
  RenderUnderPart(); // render object
}

void ColumnOfBricks(void) {
  ldy_abs(AreaType); // load area type obtained from area offset
  lda_absy(BrickMetatiles); // get metatile (no cloud override as for row)
  GetRow2();
}

void ColumnOfSolidBlocks(void) {
  ldy_abs(AreaType); // load area type obtained from area offset
  lda_absy(SolidBlockMetatiles); // get metatile
  GetRow2(); // <fallthrough>
}

void GetRow2(void) {
  pha(); // save metatile to stack for now
  GetLrgObjAttrib(); // get length and row
  pla(); // restore metatile
  ldx_zp(0x7); // get starting row
  RenderUnderPart(); // now render the column
  // --------------------------------
}

void BulletBillCannon(void) {
  GetLrgObjAttrib(); // get row and length of bullet bill cannon
  ldx_zp(0x7); // start at first row
  lda_imm(0x64); // render bullet bill cannon
  ram[MetatileBuffer + x] = a;
  inx();
  dey(); // done yet?
  if (!neg_flag) {
    lda_imm(0x65); // if not, render middle part
    ram[MetatileBuffer + x] = a;
    inx();
    dey(); // done yet?
    if (!neg_flag) {
      lda_imm(0x66); // if not, render bottom until length expires
      RenderUnderPart();
    }
  }
  // SetupCannon:
  ldx_abs(Cannon_Offset); // get offset for data used by cannons and whirlpools
  GetAreaObjYPosition(); // get proper vertical coordinate for cannon
  ram[Cannon_Y_Position + x] = a; // and store it here
  lda_abs(CurrentPageLoc);
  ram[Cannon_PageLoc + x] = a; // store page number for cannon here
  GetAreaObjXPosition(); // get proper horizontal coordinate for cannon
  ram[Cannon_X_Position + x] = a; // and store it here
  inx();
  cpx_imm(0x6); // increment and check offset
  // if not yet reached sixth cannon, branch to save offset
  if (carry_flag) {
    ldx_imm(0x0); // otherwise initialize it
  }
  // StrCOffset:
  ram[Cannon_Offset] = x; // save new offset and leave
  // --------------------------------
}

void StaircaseObject(void) {
  ChkLrgObjLength(); // check and load length
  // if length already loaded, skip init part
  if (carry_flag) {
    lda_imm(0x9); // start past the end for the bottom
    ram[StaircaseControl] = a; // of the staircase
  }
  // NextStair:
  dec_abs(StaircaseControl); // move onto next step (or first if starting)
  ldy_abs(StaircaseControl);
  ldx_absy(StaircaseRowData); // get starting row and height to render
  lda_absy(StaircaseHeightData);
  tay();
  lda_imm(0x61); // now render solid block staircase
  RenderUnderPart();
  // --------------------------------
}

void Jumpspring(void) {
  GetLrgObjAttrib();
  FindEmptyEnemySlot(); // find empty space in enemy object buffer
  GetAreaObjXPosition(); // get horizontal coordinate for jumpspring
  ram[Enemy_X_Position + x] = a; // and store
  lda_abs(CurrentPageLoc); // store page location of jumpspring
  ram[Enemy_PageLoc + x] = a;
  GetAreaObjYPosition(); // get vertical coordinate for jumpspring
  ram[Enemy_Y_Position + x] = a; // and store
  ram[Jumpspring_FixedYPos + x] = a; // store as permanent coordinate here
  lda_imm(JumpspringObject);
  ram[Enemy_ID + x] = a; // write jumpspring object to enemy object buffer
  ldy_imm(0x1);
  ram[Enemy_Y_HighPos + x] = y; // store vertical high byte
  inc_zpx(Enemy_Flag); // set flag for enemy object buffer
  ldx_zp(0x7);
  lda_imm(0x67); // draw metatiles in two rows where jumpspring is
  ram[MetatileBuffer + x] = a;
  lda_imm(0x68);
  ram[MetatileBuffer + 1 + x] = a;
  // --------------------------------
  // $07 - used to save ID of brick object
}

void Hidden1UpBlock(void) {
  lda_abs(Hidden1UpFlag); // if flag not set, do not render object
  if (!zero_flag) {
    lda_imm(0x0); // if set, init for the next one
    ram[Hidden1UpFlag] = a;
    BrickWithItem(); // jump to code shared with unbreakable bricks
  }
}

void QuestionBlock(void) {
  GetAreaObjectID(); // get value from level decoder routine
  DrawQBlk(); // go to render it
}

void BrickWithCoins(void) {
  lda_imm(0x0); // initialize multi-coin timer flag
  ram[BrickCoinTimerFlag] = a;
  BrickWithItem(); // <fallthrough>
}

void BrickWithItem(void) {
  GetAreaObjectID(); // save area object ID
  ram[0x7] = y;
  lda_imm(0x0); // load default adder for bricks with lines
  ldy_abs(AreaType); // check level type for ground level
  dey();
  // if ground type, do not start with 5
  if (!zero_flag) {
    lda_imm(0x5); // otherwise use adder for bricks without lines
  }
  // BWithL:
  carry_flag = false; // add object ID to adder
  adc_zp(0x7);
  tay(); // use as offset for metatile
  DrawQBlk(); // <fallthrough>
}

void DrawQBlk(void) {
  lda_absy(BrickQBlockMetatiles); // get appropriate metatile for brick (question block
  pha(); // if branched to here from question block routine)
  GetLrgObjAttrib(); // get row from location byte
  DrawRow(); // now render the object
}

void Hole_Empty(void) {
  ChkLrgObjLength(); // get lower nybble and save as length
  // skip this part if length already loaded
  if (carry_flag) {
    lda_abs(AreaType); // check for water type level
    // if not water type, skip this part
    if (zero_flag) {
      ldx_abs(Whirlpool_Offset); // get offset for data used by cannons and whirlpools
      GetAreaObjXPosition(); // get proper vertical coordinate of where we're at
      carry_flag = true;
      sbc_imm(0x10); // subtract 16 pixels
      ram[Whirlpool_LeftExtent + x] = a; // store as left extent of whirlpool
      lda_abs(CurrentPageLoc); // get page location of where we're at
      sbc_imm(0x0); // subtract borrow
      ram[Whirlpool_PageLoc + x] = a; // save as page location of whirlpool
      iny();
      iny(); // increment length by 2
      tya();
      asl_acc(); // multiply by 16 to get size of whirlpool
      asl_acc(); // note that whirlpool will always be
      asl_acc(); // two blocks bigger than actual size of hole
      asl_acc(); // and extend one block beyond each edge
      ram[Whirlpool_Length + x] = a; // save size of whirlpool here
      inx();
      cpx_imm(0x5); // increment and check offset
      // if not yet reached fifth whirlpool, branch to save offset
      if (carry_flag) {
        ldx_imm(0x0); // otherwise initialize it
      }
      // StrWOffset:
      ram[Whirlpool_Offset] = x; // save new offset here
    }
  }
  // NoWhirlP:
  ldx_abs(AreaType); // get appropriate metatile, then
  lda_absx(HoleMetatiles); // render the hole proper
  ldx_imm(0x8);
  ldy_imm(0xf); // start at ninth row and go to bottom, run RenderUnderPart
  // --------------------------------
  RenderUnderPart(); // <fallthrough>
}

void RenderUnderPart(void) {
  ram[AreaObjectHeight] = y; // store vertical length to render
  ldy_absx(MetatileBuffer); // check current spot to see if there's something
  if (zero_flag) { goto DrawThisRow; } // we need to keep, if nothing, go ahead
  cpy_imm(0x17);
  if (zero_flag) { goto WaitOneRow; } // if middle part (tree ledge), wait until next row
  cpy_imm(0x1a);
  if (zero_flag) { goto WaitOneRow; } // if middle part (mushroom ledge), wait until next row
  cpy_imm(0xc0);
  if (zero_flag) { goto DrawThisRow; } // if question block w/ coin, overwrite
  cpy_imm(0xc0);
  if (carry_flag) { goto WaitOneRow; } // if any other metatile with palette 3, wait until next row
  cpy_imm(0x54);
  if (!zero_flag) { goto DrawThisRow; } // if cracked rock terrain, overwrite
  cmp_imm(0x50);
  if (zero_flag) { goto WaitOneRow; } // if stem top of mushroom, wait until next row
  
DrawThisRow:
  ram[MetatileBuffer + x] = a; // render contents of A from routine that called this
  
WaitOneRow:
  inx();
  cpx_imm(0xd); // stop rendering if we're at the bottom of the screen
  if (carry_flag) { return; }
  ldy_abs(AreaObjectHeight); // decrement, and stop rendering if there is no more length
  dey();
  if (!neg_flag) { RenderUnderPart(); return; }
  // --------------------------------
}

void KillEnemies(void) {
  ram[0x0] = a; // store identifier here
  lda_imm(0x0);
  ldx_imm(0x4); // check for identifier in enemy object buffer
  
KillELoop:
  ldy_zpx(Enemy_ID);
  cpy_zp(0x0); // if not found, branch
  if (zero_flag) {
    ram[Enemy_Flag + x] = a; // if found, deactivate enemy object flag
  }
  // NoKillE:
  dex(); // do this until all slots are checked
  if (!neg_flag) { goto KillELoop; }
  // --------------------------------
}

void GetPipeHeight(void) {
  ldy_imm(0x1); // check for length loaded, if not, load
  ChkLrgObjFixedLength(); // pipe length of 2 (horizontal)
  GetLrgObjAttrib();
  tya(); // get saved lower nybble as height
  and_imm(0x7); // save only the three lower bits as
  ram[0x6] = a; // vertical length, then load Y with
  ldy_absx(AreaObjectLength); // length left over
}

void FindEmptyEnemySlot(void) {
  ldx_imm(0x0); // start at first enemy slot
  
EmptyChkLoop:
  carry_flag = false; // clear carry flag by default
  lda_zpx(Enemy_Flag); // check enemy buffer for nonzero
  if (!zero_flag) {
    inx();
    cpx_imm(0x5); // if nonzero, check next value
    if (!zero_flag) { goto EmptyChkLoop; }
    // --------------------------------
  }
}

void GetAreaObjectID(void) {
  lda_zp(0x0); // get value saved from area parser routine
  carry_flag = true;
  sbc_imm(0x0); // possibly residual code
  tay(); // save to Y
  // --------------------------------
}

void ChkLrgObjLength(void) {
  GetLrgObjAttrib(); // get row location and size (length if branched to from here)
  ChkLrgObjFixedLength(); // <fallthrough>
}

void ChkLrgObjFixedLength(void) {
  lda_absx(AreaObjectLength); // check for set length counter
  carry_flag = false; // clear carry flag for not just starting
  if (neg_flag) {
    tya(); // save length into length counter
    ram[AreaObjectLength + x] = a;
    carry_flag = true; // set carry flag if just starting
  }
}

void GetLrgObjAttrib(void) {
  ldy_absx(AreaObjOffsetBuffer); // get offset saved from area obj decoding routine
  lda_indy(AreaData); // get first byte of level object
  and_imm(0b00001111);
  ram[0x7] = a; // save row location
  iny();
  lda_indy(AreaData); // get next byte, save lower nybble (length or height)
  and_imm(0b00001111); // as Y, then leave
  tay();
  // --------------------------------
}

void GetAreaObjXPosition(void) {
  lda_abs(CurrentColumnPos); // multiply current offset where we're at by 16
  asl_acc(); // to obtain horizontal pixel coordinate
  asl_acc();
  asl_acc();
  asl_acc();
  // --------------------------------
}

void GetAreaObjYPosition(void) {
  lda_zp(0x7); // multiply value by 16
  asl_acc();
  asl_acc(); // this will give us the proper vertical pixel coordinate
  asl_acc();
  asl_acc();
  carry_flag = false;
  adc_imm(32); // add 32 pixels for the status bar
  // -------------------------------------------------------------------------------------
  // $06-$07 - used to store block buffer address used as indirect
}

void GetBlockBufferAddr(void) {
  pha(); // take value of A, save
  lsr_acc(); // move high nybble to low
  lsr_acc();
  lsr_acc();
  lsr_acc();
  tay(); // use nybble as pointer to high byte
  lda_absy(BlockBufferAddr + 2); // of indirect here
  ram[0x7] = a;
  pla();
  and_imm(0b00001111); // pull from stack, mask out high nybble
  carry_flag = false;
  adc_absy(BlockBufferAddr); // add to low byte
  ram[0x6] = a; // store here and leave
  // -------------------------------------------------------------------------------------
  // unused space
  //       .db $ff, $ff
  // -------------------------------------------------------------------------------------
}

void LoadAreaPointer(void) {
  FindAreaPointer(); // find it and store it here
  ram[AreaPointer] = a;
  GetAreaType(); // <fallthrough>
}

void GetAreaType(void) {
  and_imm(0b01100000); // mask out all but d6 and d5
  asl_acc();
  rol_acc();
  rol_acc();
  rol_acc(); // make %0xx00000 into %000000xx
  ram[AreaType] = a; // save 2 MSB as area type
}

void FindAreaPointer(void) {
  ldy_abs(WorldNumber); // load offset from world variable
  lda_absy(WorldAddrOffsets);
  carry_flag = false; // add area number used to find data
  adc_abs(AreaNumber);
  tay();
  lda_absy(AreaAddrOffsets); // from there we have our area pointer
}

void GetAreaDataAddrs(void) {
  lda_abs(AreaPointer); // use 2 MSB for Y
  GetAreaType();
  tay();
  lda_abs(AreaPointer); // mask out all but 5 LSB
  and_imm(0b00011111);
  ram[AreaAddrsLOffset] = a; // save as low offset
  lda_absy(EnemyAddrHOffsets); // load base value with 2 altered MSB,
  carry_flag = false; // then add base value to 5 LSB, result
  adc_abs(AreaAddrsLOffset); // becomes offset for level data
  tay();
  lda_absy(EnemyDataAddrLow); // use offset to load pointer
  ram[EnemyDataLow] = a;
  lda_absy(EnemyDataAddrHigh);
  ram[EnemyDataHigh] = a;
  ldy_abs(AreaType); // use area type as offset
  lda_absy(AreaDataHOffsets); // do the same thing but with different base value
  carry_flag = false;
  adc_abs(AreaAddrsLOffset);
  tay();
  lda_absy(AreaDataAddrLow); // use this offset to load another pointer
  ram[AreaDataLow] = a;
  lda_absy(AreaDataAddrHigh);
  ram[AreaDataHigh] = a;
  ldy_imm(0x0); // load first byte of header
  lda_indy(AreaData);
  pha(); // save it to the stack for now
  and_imm(0b00000111); // save 3 LSB for foreground scenery or bg color control
  cmp_imm(0x4);
  if (carry_flag) {
    ram[BackgroundColorCtrl] = a; // if 4 or greater, save value here as bg color control
    lda_imm(0x0);
  }
  // StoreFore:
  ram[ForegroundScenery] = a; // if less, save value here as foreground scenery
  pla(); // pull byte from stack and push it back
  pha();
  and_imm(0b00111000); // save player entrance control bits
  lsr_acc(); // shift bits over to LSBs
  lsr_acc();
  lsr_acc();
  ram[PlayerEntranceCtrl] = a; // save value here as player entrance control
  pla(); // pull byte again but do not push it back
  and_imm(0b11000000); // save 2 MSB for game timer setting
  carry_flag = false;
  rol_acc(); // rotate bits over to LSBs
  rol_acc();
  rol_acc();
  ram[GameTimerSetting] = a; // save value here as game timer setting
  iny();
  lda_indy(AreaData); // load second byte of header
  pha(); // save to stack
  and_imm(0b00001111); // mask out all but lower nybble
  ram[TerrainControl] = a;
  pla(); // pull and push byte to copy it to A
  pha();
  and_imm(0b00110000); // save 2 MSB for background scenery type
  lsr_acc();
  lsr_acc(); // shift bits to LSBs
  lsr_acc();
  lsr_acc();
  ram[BackgroundScenery] = a; // save as background scenery
  pla();
  and_imm(0b11000000);
  carry_flag = false;
  rol_acc(); // rotate bits over to LSBs
  rol_acc();
  rol_acc();
  cmp_imm(0b00000011); // if set to 3, store here
  // and nullify other value
  if (zero_flag) {
    ram[CloudTypeOverride] = a; // otherwise store value in other place
    lda_imm(0x0);
  }
  // StoreStyle:
  ram[AreaStyle] = a;
  lda_zp(AreaDataLow); // increment area data address by 2 bytes
  carry_flag = false;
  adc_imm(0x2);
  ram[AreaDataLow] = a;
  lda_zp(AreaDataHigh);
  adc_imm(0x0);
  ram[AreaDataHigh] = a;
  // -------------------------------------------------------------------------------------
  // GAME LEVELS DATA
}

void ScrollHandler(void) {
  lda_abs(Player_X_Scroll); // load value saved here
  carry_flag = false;
  adc_abs(Platform_X_Scroll); // add value used by left/right platforms
  ram[Player_X_Scroll] = a; // save as new value here to impose force on scroll
  lda_abs(ScrollLock); // check scroll lock flag
  // skip a bunch of code here if set
  if (!zero_flag) {
    InitScrlAmt();
    return;
  }
  lda_abs(Player_Pos_ForScroll);
  cmp_imm(0x50); // check player's horizontal screen position
  // if less than 80 pixels to the right, branch
  if (!carry_flag) {
    InitScrlAmt();
    return;
  }
  lda_abs(SideCollisionTimer); // if timer related to player's side collision
  // not expired, branch
  if (!zero_flag) {
    InitScrlAmt();
    return;
  }
  ldy_abs(Player_X_Scroll); // get value and decrement by one
  dey(); // if value originally set to zero or otherwise
  // negative for left movement, branch
  if (neg_flag) {
    InitScrlAmt();
    return;
  }
  iny();
  cpy_imm(0x2); // if value $01, branch and do not decrement
  if (carry_flag) {
    dey(); // otherwise decrement by one
  }
  // ChkNearMid:
  lda_abs(Player_Pos_ForScroll);
  cmp_imm(0x70); // check player's horizontal screen position
  // if less than 112 pixels to the right, branch
  if (!carry_flag) {
    ScrollScreen();
    return;
  }
  ldy_abs(Player_X_Scroll); // otherwise get original value undecremented
  ScrollScreen(); // <fallthrough>
}

void ScrollScreen(void) {
  tya();
  ram[ScrollAmount] = a; // save value here
  carry_flag = false;
  adc_abs(ScrollThirtyTwo); // add to value already set here
  ram[ScrollThirtyTwo] = a; // save as new value here
  tya();
  carry_flag = false;
  adc_abs(ScreenLeft_X_Pos); // add to left side coordinate
  ram[ScreenLeft_X_Pos] = a; // save as new left side coordinate
  ram[HorizontalScroll] = a; // save here also
  lda_abs(ScreenLeft_PageLoc);
  adc_imm(0x0); // add carry to page location for left
  ram[ScreenLeft_PageLoc] = a; // side of the screen
  and_imm(0x1); // get LSB of page location
  ram[0x0] = a; // save as temp variable for PPU register 1 mirror
  lda_abs(Mirror_PPU_CTRL_REG1); // get PPU register 1 mirror
  and_imm(0b11111110); // save all bits except d0
  ora_zp(0x0); // get saved bit here and save in PPU register 1
  ram[Mirror_PPU_CTRL_REG1] = a; // mirror to be used to set name table later
  GetScreenPosition(); // figure out where the right side is
  lda_imm(0x8);
  ram[ScrollIntervalTimer] = a; // set scroll timer (residual, not used elsewhere)
  ChkPOffscr(); // skip this part
}

void InitScrlAmt(void) {
  lda_imm(0x0);
  ram[ScrollAmount] = a; // initialize value here
  ChkPOffscr(); // <fallthrough>
}

void ChkPOffscr(void) {
  ldx_imm(0x0); // set X for player offset
  GetXOffscreenBits(); // get horizontal offscreen bits for player
  ram[0x0] = a; // save them here
  ldy_imm(0x0); // load default offset (left side)
  asl_acc(); // if d7 of offscreen bits are set,
  if (carry_flag) { goto KeepOnscr; } // branch with default offset
  iny(); // otherwise use different offset (right side)
  lda_zp(0x0);
  and_imm(0b00100000); // check offscreen bits for d5 set
  if (zero_flag) { goto InitPlatScrl; } // if not set, branch ahead of this part
  
KeepOnscr:
  lda_absy(ScreenEdge_X_Pos); // get left or right side coordinate based on offset
  carry_flag = true;
  sbc_absy(X_SubtracterData); // subtract amount based on offset
  ram[Player_X_Position] = a; // store as player position to prevent movement further
  lda_absy(ScreenEdge_PageLoc); // get left or right page location based on offset
  sbc_imm(0x0); // subtract borrow
  ram[Player_PageLoc] = a; // save as player's page location
  lda_zp(Left_Right_Buttons); // check saved controller bits
  cmp_absy(OffscrJoypadBitsData); // against bits based on offset
  if (zero_flag) { goto InitPlatScrl; } // if not equal, branch
  lda_imm(0x0);
  ram[Player_X_Speed] = a; // otherwise nullify horizontal speed of player
  
InitPlatScrl:
  lda_imm(0x0); // nullify platform force imposed on scroll
  ram[Platform_X_Scroll] = a;
}

void GetScreenPosition(void) {
  lda_abs(ScreenLeft_X_Pos); // get coordinate of screen's left boundary
  carry_flag = false;
  adc_imm(0xff); // add 255 pixels
  ram[ScreenRight_X_Pos] = a; // store as coordinate of screen's right boundary
  lda_abs(ScreenLeft_PageLoc); // get page number where left boundary is
  adc_imm(0x0); // add carry from before
  ram[ScreenRight_PageLoc] = a; // store as page number where right boundary is
  // -------------------------------------------------------------------------------------
}

void GameRoutines(void) {
  lda_zp(GameEngineSubroutine); // run routine based on number (a few of these routines are
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: Entrance_GameTimerSetup(); return;
    case 1: Vine_AutoClimb(); return;
    case 2: SideExitPipeEntry(); return;
    case 3: VerticalPipeEntry(); return;
    case 4: FlagpoleSlide(); return;
    case 5: PlayerEndLevel(); return;
    case 6: PlayerLoseLife(); return;
    case 7: PlayerEntrance(); return;
    case 8: PlayerCtrlRoutine(); return;
    case 9: PlayerChangeSize(); return;
    case 10: PlayerInjuryBlink(); return;
    case 11: PlayerDeath(); return;
    case 12: PlayerFireFlower(); return;
  }
}

void PlayerEntrance(void) {
  lda_abs(AltEntranceControl); // check for mode of alternate entry
  cmp_imm(0x2);
  if (zero_flag) { goto EntrMode2; } // if found, branch to enter from pipe or with vine
  lda_imm(0x0);
  ldy_zp(Player_Y_Position); // if vertical position above a certain
  cpy_imm(0x30); // point, nullify controller bits and continue
  if (!carry_flag) { AutoControlPlayer(); return; } // with player movement code, do not return
  lda_abs(PlayerEntranceCtrl); // check player entry bits from header
  cmp_imm(0x6);
  if (zero_flag) { goto ChkBehPipe; } // if set to 6 or 7, execute pipe intro code
  cmp_imm(0x7); // otherwise branch to normal entry
  if (!zero_flag) { goto PlayerRdy; }
  
ChkBehPipe:
  lda_abs(Player_SprAttrib); // check for sprite attributes
  if (!zero_flag) { goto IntroEntr; } // branch if found
  lda_imm(0x1);
  AutoControlPlayer(); return; // force player to walk to the right
  
IntroEntr:
  EnterSidePipe(); // execute sub to move player to the right
  dec_abs(ChangeAreaTimer); // decrement timer for change of area
  if (!zero_flag) { return; } // branch to exit if not yet expired
  inc_abs(DisableIntermediate); // set flag to skip world and lives display
  NextArea(); // jump to increment to next area and set modes
  return;
  
EntrMode2:
  lda_abs(JoypadOverride); // if controller override bits set here,
  if (!zero_flag) { goto VineEntr; } // branch to enter with vine
  lda_imm(0xff); // otherwise, set value here then execute sub
  MovePlayerYAxis(); // to move player upwards (note $ff = -1)
  lda_zp(Player_Y_Position); // check to see if player is at a specific coordinate
  cmp_imm(0x91); // if player risen to a certain point (this requires pipes
  if (!carry_flag) { goto PlayerRdy; } // to be at specific height to look/function right) branch
  return; // to the last part, otherwise leave
  
VineEntr:
  lda_abs(VineHeight);
  cmp_imm(0x60); // check vine height
  if (!zero_flag) { return; } // if vine not yet reached maximum height, branch to leave
  lda_zp(Player_Y_Position); // get player's vertical coordinate
  cmp_imm(0x99); // check player's vertical coordinate against preset value
  ldy_imm(0x0); // load default values to be written to
  lda_imm(0x1); // this value moves player to the right off the vine
  if (!carry_flag) { goto OffVine; } // if vertical coordinate < preset value, use defaults
  lda_imm(0x3);
  ram[Player_State] = a; // otherwise set player state to climbing
  iny(); // increment value in Y
  lda_imm(0x8); // set block in block buffer to cover hole, then
  ram[Block_Buffer_1 + 0xb4] = a; // use same value to force player to climb
  
OffVine:
  ram[DisableCollisionDet] = y; // set collision detection disable flag
  AutoControlPlayer(); // use contents of A to move player up or right, execute sub
  lda_zp(Player_X_Position);
  cmp_imm(0x48); // check player's horizontal position
  if (!carry_flag) { return; } // if not far enough to the right, branch to leave
  
PlayerRdy:
  lda_imm(0x8); // set routine to be executed by game engine next frame
  ram[GameEngineSubroutine] = a;
  lda_imm(0x1); // set to face player to the right
  ram[PlayerFacingDir] = a;
  lsr_acc(); // init A
  ram[AltEntranceControl] = a; // init mode of entry
  ram[DisableCollisionDet] = a; // init collision detection disable flag
  ram[JoypadOverride] = a; // nullify controller override bits
  // -------------------------------------------------------------------------------------
  // $07 - used to hold upper limit of high byte when player falls down hole
}

void AutoControlPlayer(void) {
  ram[SavedJoypadBits] = a; // override controller bits with contents of A if executing here
  PlayerCtrlRoutine(); // <fallthrough>
}

void PlayerCtrlRoutine(void) {
  lda_zp(GameEngineSubroutine); // check task here
  cmp_imm(0xb); // if certain value is set, branch to skip controller bit loading
  if (zero_flag) { goto SizeChk; }
  lda_abs(AreaType); // are we in a water type area?
  if (!zero_flag) { goto SaveJoyp; } // if not, branch
  ldy_zp(Player_Y_HighPos);
  dey(); // if not in vertical area between
  if (!zero_flag) { goto DisJoyp; } // status bar and bottom, branch
  lda_zp(Player_Y_Position);
  cmp_imm(0xd0); // if nearing the bottom of the screen or
  if (!carry_flag) { goto SaveJoyp; } // not in the vertical area between status bar or bottom,
  
DisJoyp:
  lda_imm(0x0); // disable controller bits
  ram[SavedJoypadBits] = a;
  
SaveJoyp:
  lda_abs(SavedJoypadBits); // otherwise store A and B buttons in $0a
  and_imm(0b11000000);
  ram[A_B_Buttons] = a;
  lda_abs(SavedJoypadBits); // store left and right buttons in $0c
  and_imm(0b00000011);
  ram[Left_Right_Buttons] = a;
  lda_abs(SavedJoypadBits); // store up and down buttons in $0b
  and_imm(0b00001100);
  ram[Up_Down_Buttons] = a;
  and_imm(0b00000100); // check for pressing down
  if (zero_flag) { goto SizeChk; } // if not, branch
  lda_zp(Player_State); // check player's state
  if (!zero_flag) { goto SizeChk; } // if not on the ground, branch
  ldy_zp(Left_Right_Buttons); // check left and right
  if (zero_flag) { goto SizeChk; } // if neither pressed, branch
  lda_imm(0x0);
  ram[Left_Right_Buttons] = a; // if pressing down while on the ground,
  ram[Up_Down_Buttons] = a; // nullify directional bits
  
SizeChk:
  PlayerMovementSubs(); // run movement subroutines
  ldy_imm(0x1); // is player small?
  lda_abs(PlayerSize);
  if (!zero_flag) { goto ChkMoveDir; }
  ldy_imm(0x0); // check for if crouching
  lda_abs(CrouchingFlag);
  if (zero_flag) { goto ChkMoveDir; } // if not, branch ahead
  ldy_imm(0x2); // if big and crouching, load y with 2
  
ChkMoveDir:
  ram[Player_BoundBoxCtrl] = y; // set contents of Y as player's bounding box size control
  lda_imm(0x1); // set moving direction to right by default
  ldy_zp(Player_X_Speed); // check player's horizontal speed
  if (zero_flag) { goto PlayerSubs; } // if not moving at all horizontally, skip this part
  if (!neg_flag) { goto SetMoveDir; } // if moving to the right, use default moving direction
  asl_acc(); // otherwise change to move to the left
  
SetMoveDir:
  ram[Player_MovingDir] = a; // set moving direction
  
PlayerSubs:
  ScrollHandler(); // move the screen if necessary
  GetPlayerOffscreenBits(); // get player's offscreen bits
  RelativePlayerPosition(); // get coordinates relative to the screen
  ldx_imm(0x0); // set offset for player object
  BoundingBoxCore(); // get player's bounding box coordinates
  PlayerBGCollision(); // do collision detection and process
  lda_zp(Player_Y_Position);
  cmp_imm(0x40); // check to see if player is higher than 64th pixel
  if (!carry_flag) { goto PlayerHole; } // if so, branch ahead
  lda_zp(GameEngineSubroutine);
  cmp_imm(0x5); // if running end-of-level routine, branch ahead
  if (zero_flag) { goto PlayerHole; }
  cmp_imm(0x7); // if running player entrance routine, branch ahead
  if (zero_flag) { goto PlayerHole; }
  cmp_imm(0x4); // if running routines $00-$03, branch ahead
  if (!carry_flag) { goto PlayerHole; }
  lda_abs(Player_SprAttrib);
  and_imm(0b11011111); // otherwise nullify player's
  ram[Player_SprAttrib] = a; // background priority flag
  
PlayerHole:
  lda_zp(Player_Y_HighPos); // check player's vertical high byte
  cmp_imm(0x2); // for below the screen
  if (neg_flag) { goto ExitCtrl; } // branch to leave if not that far down
  ldx_imm(0x1);
  ram[ScrollLock] = x; // set scroll lock
  ldy_imm(0x4);
  ram[0x7] = y; // set value here
  ldx_imm(0x0); // use X as flag, and clear for cloud level
  ldy_abs(GameTimerExpiredFlag); // check game timer expiration flag
  if (!zero_flag) { goto HoleDie; } // if set, branch
  ldy_abs(CloudTypeOverride); // check for cloud type override
  if (!zero_flag) { goto ChkHoleX; } // skip to last part if found
  
HoleDie:
  inx(); // set flag in X for player death
  ldy_zp(GameEngineSubroutine);
  cpy_imm(0xb); // check for some other routine running
  if (zero_flag) { goto ChkHoleX; } // if so, branch ahead
  ldy_abs(DeathMusicLoaded); // check value here
  if (!zero_flag) { goto HoleBottom; } // if already set, branch to next part
  iny();
  ram[EventMusicQueue] = y; // otherwise play death music
  ram[DeathMusicLoaded] = y; // and set value here
  
HoleBottom:
  ldy_imm(0x6);
  ram[0x7] = y; // change value here
  
ChkHoleX:
  cmp_zp(0x7); // compare vertical high byte with value set here
  if (neg_flag) { goto ExitCtrl; } // if less, branch to leave
  dex(); // otherwise decrement flag in X
  if (neg_flag) { goto CloudExit; } // if flag was clear, branch to set modes and other values
  ldy_abs(EventMusicBuffer); // check to see if music is still playing
  if (!zero_flag) { goto ExitCtrl; } // branch to leave if so
  lda_imm(0x6); // otherwise set to run lose life routine
  ram[GameEngineSubroutine] = a; // on next frame
  
ExitCtrl:
  return; // leave
  
CloudExit:
  lda_imm(0x0);
  ram[JoypadOverride] = a; // clear controller override bits if any are set
  SetEntr(); // do sub to set secondary mode
  inc_abs(AltEntranceControl); // set mode of entry to 3
  // -------------------------------------------------------------------------------------
}

void Vine_AutoClimb(void) {
  lda_zp(Player_Y_HighPos); // check to see whether player reached position
  // above the status bar yet and if so, set modes
  if (zero_flag) {
    lda_zp(Player_Y_Position);
    cmp_imm(0xe4);
    if (!carry_flag) {
      SetEntr();
      return;
    }
  }
  // AutoClimb:
  lda_imm(0b00001000); // set controller bits override to up
  ram[JoypadOverride] = a;
  ldy_imm(0x3); // set player state to climbing
  ram[Player_State] = y;
  AutoControlPlayer(); return;
}

void SetEntr(void) {
  lda_imm(0x2); // set starting position to override
  ram[AltEntranceControl] = a;
  ChgAreaMode(); return; // set modes
  // -------------------------------------------------------------------------------------
}

void VerticalPipeEntry(void) {
  lda_imm(0x1); // set 1 as movement amount
  MovePlayerYAxis(); // do sub to move player downwards
  ScrollHandler(); // do sub to scroll screen with saved force if necessary
  ldy_imm(0x0); // load default mode of entry
  lda_abs(WarpZoneControl); // check warp zone control variable/flag
  // if set, branch to use mode 0
  if (!zero_flag) {
    ChgAreaPipe();
    return;
  }
  iny();
  lda_abs(AreaType); // check for castle level type
  cmp_imm(0x3);
  // if not castle type level, use mode 1
  if (!zero_flag) {
    ChgAreaPipe();
    return;
  }
  iny();
  ChgAreaPipe(); // otherwise use mode 2
}

void SideExitPipeEntry(void) {
  EnterSidePipe(); // execute sub to move player to the right
  ldy_imm(0x2);
  ChgAreaPipe(); // <fallthrough>
}

void ChgAreaPipe(void) {
  dec_abs(ChangeAreaTimer); // decrement timer for change of area
  if (zero_flag) {
    ram[AltEntranceControl] = y; // when timer expires set mode of alternate entry
    ChgAreaMode(); // <fallthrough>
  }
}

void ChgAreaMode(void) {
  inc_abs(DisableScreenFlag); // set flag to disable screen output
  lda_imm(0x0);
  ram[OperMode_Task] = a; // set secondary mode of operation
  ram[Sprite0HitDetectFlag] = a; // disable sprite 0 check
}

void PlayerChangeSize(void) {
  lda_abs(TimerControl); // check master timer control
  cmp_imm(0xf8); // for specific moment in time
  // branch if before or after that point
  if (zero_flag) {
    InitChangeSize(); // otherwise run code to get growing/shrinking going
    return;
  }
  // EndChgSize:
  cmp_imm(0xc4); // check again for another specific moment
  if (zero_flag) {
    DonePlayerTask(); // otherwise do sub to init timer control and set routine
    // -------------------------------------------------------------------------------------
  }
}

void PlayerInjuryBlink(void) {
  lda_abs(TimerControl); // check master timer control
  cmp_imm(0xf0); // for specific moment in time
  // branch if before that point
  if (!carry_flag) {
    cmp_imm(0xc8); // check again for another specific point
    // branch if at that point, and not before or after
    if (zero_flag) {
      DonePlayerTask();
      return;
    }
    PlayerCtrlRoutine(); return; // otherwise run player control routine
  }
  // ExitBlink:
  if (zero_flag) {
    InitChangeSize(); // <fallthrough>
  }
}

void InitChangeSize(void) {
  ldy_abs(PlayerChangeSizeFlag); // if growing/shrinking flag already set
  if (zero_flag) {
    ram[PlayerAnimCtrl] = y; // otherwise initialize player's animation frame control
    inc_abs(PlayerChangeSizeFlag); // set growing/shrinking flag
    lda_abs(PlayerSize);
    eor_imm(0x1); // invert player's size
    ram[PlayerSize] = a;
    // -------------------------------------------------------------------------------------
    // $00 - used in CyclePlayerPalette to store current palette to cycle
  }
}

void PlayerDeath(void) {
  lda_abs(TimerControl); // check master timer control
  cmp_imm(0xf0); // for specific moment in time
  if (!carry_flag) {
    PlayerCtrlRoutine(); return; // otherwise run player control routine
  }
}

void DonePlayerTask(void) {
  lda_imm(0x0);
  ram[TimerControl] = a; // initialize master timer control to continue timers
  lda_imm(0x8);
  ram[GameEngineSubroutine] = a; // set player control routine to run next frame
}

void PlayerFireFlower(void) {
  lda_abs(TimerControl); // check master timer control
  cmp_imm(0xc0); // for specific moment in time
  // branch if at moment, not before or after
  if (zero_flag) {
    ResetPalFireFlower();
    return;
  }
  lda_zp(FrameCounter); // get frame counter
  lsr_acc();
  lsr_acc(); // divide by four to change every four frames
  CyclePlayerPalette(); // <fallthrough>
}

void CyclePlayerPalette(void) {
  and_imm(0x3); // mask out all but d1-d0 (previously d3-d2)
  ram[0x0] = a; // store result here to use as palette bits
  lda_abs(Player_SprAttrib); // get player attributes
  and_imm(0b11111100); // save any other bits but palette bits
  ora_zp(0x0); // add palette bits
  ram[Player_SprAttrib] = a; // store as new player attributes
}

void ResetPalFireFlower(void) {
  DonePlayerTask(); // do sub to init timer control and run player control routine
  ResetPalStar(); // <fallthrough>
}

void ResetPalStar(void) {
  lda_abs(Player_SprAttrib); // get player attributes
  and_imm(0b11111100); // mask out palette bits to force palette 0
  ram[Player_SprAttrib] = a; // store as new player attributes
  // -------------------------------------------------------------------------------------
}

void FlagpoleSlide(void) {
  lda_zp(Enemy_ID + 5); // check special use enemy slot
  cmp_imm(FlagpoleFlagObject); // for flagpole flag object
  // if not found, branch to something residual
  if (zero_flag) {
    lda_abs(FlagpoleSoundQueue); // load flagpole sound
    ram[Square1SoundQueue] = a; // into square 1's sfx queue
    lda_imm(0x0);
    ram[FlagpoleSoundQueue] = a; // init flagpole sound queue
    ldy_zp(Player_Y_Position);
    cpy_imm(0x9e); // check to see if player has slid down
    // far enough, and if so, branch with no controller bits set
    if (!carry_flag) {
      lda_imm(0x4); // otherwise force player to climb down (to slide)
    }
    // SlidePlayer:
    AutoControlPlayer(); return; // jump to player control routine
  }
  // NoFPObj:
  inc_zp(GameEngineSubroutine); // increment to next routine (this may
  // -------------------------------------------------------------------------------------
}

void PlayerEndLevel(void) {
  lda_imm(0x1); // force player to walk to the right
  AutoControlPlayer();
  lda_zp(Player_Y_Position); // check player's vertical position
  cmp_imm(0xae);
  // if player is not yet off the flagpole, skip this part
  if (carry_flag) {
    lda_abs(ScrollLock); // if scroll lock not set, branch ahead to next part
    // because we only need to do this part once
    if (!zero_flag) {
      lda_imm(EndOfLevelMusic);
      ram[EventMusicQueue] = a; // load win level music in event music queue
      lda_imm(0x0);
      ram[ScrollLock] = a; // turn off scroll lock to skip this part later
    }
  }
  // ChkStop:
  lda_abs(Player_CollisionBits); // get player collision bits
  lsr_acc(); // check for d0 set
  // if d0 set, skip to next part
  if (!carry_flag) {
    lda_abs(StarFlagTaskControl); // if star flag task control already set,
    // go ahead with the rest of the code
    if (zero_flag) {
      inc_abs(StarFlagTaskControl); // otherwise set task control now (this gets ball rolling!)
    }
    // InCastle:
    lda_imm(0b00100000); // set player's background priority bit to
    ram[Player_SprAttrib] = a; // give illusion of being inside the castle
  }
  // RdyNextA:
  lda_abs(StarFlagTaskControl);
  cmp_imm(0x5); // if star flag task control not yet set
  if (zero_flag) {
    inc_abs(LevelNumber); // increment level number used for game logic
    lda_abs(LevelNumber);
    cmp_imm(0x3); // check to see if we have yet reached level -4
    // and skip this last part here if not
    if (!zero_flag) {
      NextArea();
      return;
    }
    ldy_abs(WorldNumber); // get world number as offset
    lda_abs(CoinTallyFor1Ups); // check third area coin tally for bonus 1-ups
    cmp_absy(Hidden1UpCoinAmts); // against minimum value, if player has not collected
    // at least this number of coins, leave flag clear
    if (!carry_flag) {
      NextArea();
      return;
    }
    inc_abs(Hidden1UpFlag); // otherwise set hidden 1-up box control flag
    NextArea(); // <fallthrough>
  }
}

void NextArea(void) {
  inc_abs(AreaNumber); // increment area number used for address loader
  LoadAreaPointer(); // get new level pointer
  inc_abs(FetchNewGameTimerFlag); // set flag to load new game timer
  ChgAreaMode(); // do sub to set secondary mode, disable screen and sprite 0
  ram[HalfwayPage] = a; // reset halfway page to 0 (beginning)
  lda_imm(Silence);
  ram[EventMusicQueue] = a; // silence music and leave
  // -------------------------------------------------------------------------------------
}

void MovePlayerYAxis(void) {
  carry_flag = false;
  adc_zp(Player_Y_Position); // add contents of A to player position
  ram[Player_Y_Position] = a;
  // -------------------------------------------------------------------------------------
}

void EnterSidePipe(void) {
  lda_imm(0x8); // set player's horizontal speed
  ram[Player_X_Speed] = a;
  ldy_imm(0x1); // set controller right button by default
  lda_zp(Player_X_Position); // mask out higher nybble of player's
  and_imm(0b00001111); // horizontal position
  if (zero_flag) {
    ram[Player_X_Speed] = a; // if lower nybble = 0, set as horizontal speed
    tay(); // and nullify controller bit override here
  }
  // RightPipe:
  tya(); // use contents of Y to
  AutoControlPlayer(); // execute player control routine with ctrl bits nulled
  // -------------------------------------------------------------------------------------
}

void PlayerMovementSubs(void) {
  lda_imm(0x0); // set A to init crouch flag by default
  ldy_abs(PlayerSize); // is player small?
  if (!zero_flag) { goto SetCrouch; } // if so, branch
  lda_zp(Player_State); // check state of player
  if (!zero_flag) { goto ProcMove; } // if not on the ground, branch
  lda_zp(Up_Down_Buttons); // load controller bits for up and down
  and_imm(0b00000100); // single out bit for down button
  
SetCrouch:
  ram[CrouchingFlag] = a; // store value in crouch flag
  
ProcMove:
  PlayerPhysicsSub(); // run sub related to jumping and swimming
  lda_abs(PlayerChangeSizeFlag); // if growing/shrinking flag set,
  if (!zero_flag) { return; } // branch to leave
  lda_zp(Player_State);
  cmp_imm(0x3); // get player state
  if (zero_flag) { goto MoveSubs; } // if climbing, branch ahead, leave timer unset
  ldy_imm(0x18);
  ram[ClimbSideTimer] = y; // otherwise reset timer now
  
MoveSubs:
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: OnGroundStateSub(); return;
    case 1: JumpSwimSub(); return;
    case 2: FallingSub(); return;
    case 3: ClimbingSub(); return;
  }
}

void OnGroundStateSub(void) {
  GetPlayerAnimSpeed(); // do a sub to set animation frame timing
  lda_zp(Left_Right_Buttons);
  // if left/right controller bits not set, skip instruction
  if (!zero_flag) {
    ram[PlayerFacingDir] = a; // otherwise set new facing direction
  }
  // GndMove:
  ImposeFriction(); // do a sub to impose friction on player's walk/run
  MovePlayerHorizontally(); // do another sub to move player horizontally
  ram[Player_X_Scroll] = a; // set returned value as player's movement speed for scroll
  // --------------------------------
}

void FallingSub(void) {
  lda_abs(VerticalForceDown);
  ram[VerticalForce] = a; // dump vertical movement force for falling into main one
  LRAir(); // movement force, then skip ahead to process left/right movement
  // --------------------------------
}

void JumpSwimSub(void) {
  ldy_zp(Player_Y_Speed); // if player's vertical speed zero
  if (!neg_flag) { goto DumpFall; } // or moving downwards, branch to falling
  lda_zp(A_B_Buttons);
  and_imm(A_Button); // check to see if A button is being pressed
  and_zp(PreviousA_B_Buttons); // and was pressed in previous frame
  if (!zero_flag) { goto ProcSwim; } // if so, branch elsewhere
  lda_abs(JumpOrigin_Y_Position); // get vertical position player jumped from
  carry_flag = true;
  sbc_zp(Player_Y_Position); // subtract current from original vertical coordinate
  cmp_abs(DiffToHaltJump); // compare to value set here to see if player is in mid-jump
  if (!carry_flag) { goto ProcSwim; } // or just starting to jump, if just starting, skip ahead
  
DumpFall:
  lda_abs(VerticalForceDown); // otherwise dump falling into main fractional
  ram[VerticalForce] = a;
  
ProcSwim:
  lda_abs(SwimmingFlag); // if swimming flag not set,
  if (zero_flag) { LRAir(); return; } // branch ahead to last part
  GetPlayerAnimSpeed(); // do a sub to get animation frame timing
  lda_zp(Player_Y_Position);
  cmp_imm(0x14); // check vertical position against preset value
  if (carry_flag) { goto LRWater; } // if not yet reached a certain position, branch ahead
  lda_imm(0x18);
  ram[VerticalForce] = a; // otherwise set fractional
  
LRWater:
  lda_zp(Left_Right_Buttons); // check left/right controller bits (check for swimming)
  if (zero_flag) { LRAir(); return; } // if not pressing any, skip
  ram[PlayerFacingDir] = a; // otherwise set facing direction accordingly
  LRAir(); // <fallthrough>
}

void LRAir(void) {
  lda_zp(Left_Right_Buttons); // check left/right controller bits (check for jumping/falling)
  // if not pressing any, skip
  if (!zero_flag) {
    ImposeFriction(); // otherwise process horizontal movement
  }
  // JSMove:
  MovePlayerHorizontally(); // do a sub to move player horizontally
  ram[Player_X_Scroll] = a; // set player's speed here, to be used for scroll later
  lda_zp(GameEngineSubroutine);
  cmp_imm(0xb); // check for specific routine selected
  // branch if not set to run
  if (zero_flag) {
    lda_imm(0x28);
    ram[VerticalForce] = a; // otherwise set fractional
  }
  // ExitMov1:
  MovePlayerVertically(); // jump to move player vertically, then leave
  // --------------------------------
}

void ClimbingSub(void) {
  lda_abs(Player_YMF_Dummy);
  carry_flag = false; // add movement force to dummy variable
  adc_abs(Player_Y_MoveForce); // save with carry
  ram[Player_YMF_Dummy] = a;
  ldy_imm(0x0); // set default adder here
  lda_zp(Player_Y_Speed); // get player's vertical speed
  // if not moving upwards, branch
  if (neg_flag) {
    dey(); // otherwise set adder to $ff
  }
  // MoveOnVine:
  ram[0x0] = y; // store adder here
  adc_zp(Player_Y_Position); // add carry to player's vertical position
  ram[Player_Y_Position] = a; // and store to move player up or down
  lda_zp(Player_Y_HighPos);
  adc_zp(0x0); // add carry to player's page location
  ram[Player_Y_HighPos] = a; // and store
  lda_zp(Left_Right_Buttons); // compare left/right controller bits
  and_abs(Player_CollisionBits); // to collision flag
  // if not set, skip to end
  if (!zero_flag) {
    ldy_abs(ClimbSideTimer); // otherwise check timer
    // if timer not expired, branch to leave
    if (zero_flag) {
      ldy_imm(0x18);
      ram[ClimbSideTimer] = y; // otherwise set timer now
      ldx_imm(0x0); // set default offset here
      ldy_zp(PlayerFacingDir); // get facing direction
      lsr_acc(); // move right button controller bit to carry
      // if controller right pressed, branch ahead
      if (!carry_flag) {
        inx();
        inx(); // otherwise increment offset by 2 bytes
      }
      // ClimbFD:
      dey(); // check to see if facing right
      // if so, branch, do not increment
      if (!zero_flag) {
        inx(); // otherwise increment by 1 byte
      }
      // CSetFDir:
      lda_zp(Player_X_Position);
      carry_flag = false; // add or subtract from player's horizontal position
      adc_absx(ClimbAdderLow); // using value here as adder and X as offset
      ram[Player_X_Position] = a;
      lda_zp(Player_PageLoc); // add or subtract carry or borrow using value here
      adc_absx(ClimbAdderHigh); // from the player's page location
      ram[Player_PageLoc] = a;
      lda_zp(Left_Right_Buttons); // get left/right controller bits again
      eor_imm(0b00000011); // invert them and store them while player
      ram[PlayerFacingDir] = a; // is on vine to face player in opposite direction
    }
    // ExitCSub:
    return; // then leave
  }
  // InitCSTimer:
  ram[ClimbSideTimer] = a; // initialize timer here
  // -------------------------------------------------------------------------------------
  // $00 - used to store offset to friction data
}

void PlayerPhysicsSub(void) {
  lda_zp(Player_State); // check player state
  cmp_imm(0x3);
  if (!zero_flag) { goto CheckForJumping; } // if not climbing, branch
  ldy_imm(0x0);
  lda_zp(Up_Down_Buttons); // get controller bits for up/down
  and_abs(Player_CollisionBits); // check against player's collision detection bits
  if (zero_flag) { goto ProcClimb; } // if not pressing up or down, branch
  iny();
  and_imm(0b00001000); // check for pressing up
  if (!zero_flag) { goto ProcClimb; }
  iny();
  
ProcClimb:
  ldx_absy(Climb_Y_MForceData); // load value here
  ram[Player_Y_MoveForce] = x; // store as vertical movement force
  lda_imm(0x8); // load default animation timing
  ldx_absy(Climb_Y_SpeedData); // load some other value here
  ram[Player_Y_Speed] = x; // store as vertical speed
  if (neg_flag) { goto SetCAnim; } // if climbing down, use default animation timing value
  lsr_acc(); // otherwise divide timer setting by 2
  
SetCAnim:
  ram[PlayerAnimTimerSet] = a; // store animation timer setting and leave
  return;
  
CheckForJumping:
  lda_abs(JumpspringAnimCtrl); // if jumpspring animating,
  if (!zero_flag) { goto NoJump; } // skip ahead to something else
  lda_zp(A_B_Buttons); // check for A button press
  and_imm(A_Button);
  if (zero_flag) { goto NoJump; } // if not, branch to something else
  and_zp(PreviousA_B_Buttons); // if button not pressed in previous frame, branch
  if (zero_flag) { goto ProcJumping; }
  
NoJump:
  goto X_Physics; // otherwise, jump to something else
  
ProcJumping:
  lda_zp(Player_State); // check player state
  if (zero_flag) { goto InitJS; } // if on the ground, branch
  lda_abs(SwimmingFlag); // if swimming flag not set, jump to do something else
  if (zero_flag) { goto NoJump; } // to prevent midair jumping, otherwise continue
  lda_abs(JumpSwimTimer); // if jump/swim timer nonzero, branch
  if (!zero_flag) { goto InitJS; }
  lda_zp(Player_Y_Speed); // check player's vertical speed
  if (!neg_flag) { goto InitJS; } // if player's vertical speed motionless or down, branch
  goto X_Physics; // if timer at zero and player still rising, do not swim
  
InitJS:
  lda_imm(0x20); // set jump/swim timer
  ram[JumpSwimTimer] = a;
  ldy_imm(0x0); // initialize vertical force and dummy variable
  ram[Player_YMF_Dummy] = y;
  ram[Player_Y_MoveForce] = y;
  lda_zp(Player_Y_HighPos); // get vertical high and low bytes of jump origin
  ram[JumpOrigin_Y_HighPos] = a; // and store them next to each other here
  lda_zp(Player_Y_Position);
  ram[JumpOrigin_Y_Position] = a;
  lda_imm(0x1); // set player state to jumping/swimming
  ram[Player_State] = a;
  lda_abs(Player_XSpeedAbsolute); // check value related to walking/running speed
  cmp_imm(0x9);
  if (!carry_flag) { goto ChkWtr; } // branch if below certain values, increment Y
  iny(); // for each amount equal or exceeded
  cmp_imm(0x10);
  if (!carry_flag) { goto ChkWtr; }
  iny();
  cmp_imm(0x19);
  if (!carry_flag) { goto ChkWtr; }
  iny();
  cmp_imm(0x1c);
  if (!carry_flag) { goto ChkWtr; } // note that for jumping, range is 0-4 for Y
  iny();
  
ChkWtr:
  lda_imm(0x1); // set value here (apparently always set to 1)
  ram[DiffToHaltJump] = a;
  lda_abs(SwimmingFlag); // if swimming flag disabled, branch
  if (zero_flag) { goto GetYPhy; }
  ldy_imm(0x5); // otherwise set Y to 5, range is 5-6
  lda_abs(Whirlpool_Flag); // if whirlpool flag not set, branch
  if (zero_flag) { goto GetYPhy; }
  iny(); // otherwise increment to 6
  
GetYPhy:
  lda_absy(JumpMForceData); // store appropriate jump/swim
  ram[VerticalForce] = a; // data here
  lda_absy(FallMForceData);
  ram[VerticalForceDown] = a;
  lda_absy(InitMForceData);
  ram[Player_Y_MoveForce] = a;
  lda_absy(PlayerYSpdData);
  ram[Player_Y_Speed] = a;
  lda_abs(SwimmingFlag); // if swimming flag disabled, branch
  if (zero_flag) { goto PJumpSnd; }
  lda_imm(Sfx_EnemyStomp); // load swim/goomba stomp sound into
  ram[Square1SoundQueue] = a; // square 1's sfx queue
  lda_zp(Player_Y_Position);
  cmp_imm(0x14); // check vertical low byte of player position
  if (carry_flag) { goto X_Physics; } // if below a certain point, branch
  lda_imm(0x0); // otherwise reset player's vertical speed
  ram[Player_Y_Speed] = a; // and jump to something else to keep player
  goto X_Physics; // from swimming above water level
  
PJumpSnd:
  lda_imm(Sfx_BigJump); // load big mario's jump sound by default
  ldy_abs(PlayerSize); // is mario big?
  if (zero_flag) { goto SJumpSnd; }
  lda_imm(Sfx_SmallJump); // if not, load small mario's jump sound
  
SJumpSnd:
  ram[Square1SoundQueue] = a; // store appropriate jump sound in square 1 sfx queue
  
X_Physics:
  ldy_imm(0x0);
  ram[0x0] = y; // init value here
  lda_zp(Player_State); // if mario is on the ground, branch
  if (zero_flag) { goto ProcPRun; }
  lda_abs(Player_XSpeedAbsolute); // check something that seems to be related
  cmp_imm(0x19); // to mario's speed
  if (carry_flag) { goto GetXPhy; } // if =>$19 branch here
  if (!carry_flag) { goto ChkRFast; } // if not branch elsewhere
  
ProcPRun:
  iny(); // if mario on the ground, increment Y
  lda_abs(AreaType); // check area type
  if (zero_flag) { goto ChkRFast; } // if water type, branch
  dey(); // decrement Y by default for non-water type area
  lda_zp(Left_Right_Buttons); // get left/right controller bits
  cmp_zp(Player_MovingDir); // check against moving direction
  if (!zero_flag) { goto ChkRFast; } // if controller bits <> moving direction, skip this part
  lda_zp(A_B_Buttons); // check for b button pressed
  and_imm(B_Button);
  if (!zero_flag) { goto SetRTmr; } // if pressed, skip ahead to set timer
  lda_abs(RunningTimer); // check for running timer set
  if (!zero_flag) { goto GetXPhy; } // if set, branch
  
ChkRFast:
  iny(); // if running timer not set or level type is water,
  inc_zp(0x0); // increment Y again and temp variable in memory
  lda_abs(RunningSpeed);
  if (!zero_flag) { goto FastXSp; } // if running speed set here, branch
  lda_abs(Player_XSpeedAbsolute);
  cmp_imm(0x21); // otherwise check player's walking/running speed
  if (!carry_flag) { goto GetXPhy; } // if less than a certain amount, branch ahead
  
FastXSp:
  inc_zp(0x0); // if running speed set or speed => $21 increment $00
  goto GetXPhy; // and jump ahead
  
SetRTmr:
  lda_imm(0xa); // if b button pressed, set running timer
  ram[RunningTimer] = a;
  
GetXPhy:
  lda_absy(MaxLeftXSpdData); // get maximum speed to the left
  ram[MaximumLeftSpeed] = a;
  lda_zp(GameEngineSubroutine); // check for specific routine running
  cmp_imm(0x7); // (player entrance)
  if (!zero_flag) { goto GetXPhy2; } // if not running, skip and use old value of Y
  ldy_imm(0x3); // otherwise set Y to 3
  
GetXPhy2:
  lda_absy(MaxRightXSpdData); // get maximum speed to the right
  ram[MaximumRightSpeed] = a;
  ldy_zp(0x0); // get other value in memory
  lda_absy(FrictionData); // get value using value in memory as offset
  ram[FrictionAdderLow] = a;
  lda_imm(0x0);
  ram[FrictionAdderHigh] = a; // init something here
  lda_zp(PlayerFacingDir);
  cmp_zp(Player_MovingDir); // check facing direction against moving direction
  if (zero_flag) { return; } // if the same, branch to leave
  asl_abs(FrictionAdderLow); // otherwise shift d7 of friction adder low into carry
  rol_abs(FrictionAdderHigh); // then rotate carry onto d0 of friction adder high
  // -------------------------------------------------------------------------------------
}

void GetPlayerAnimSpeed(void) {
  ldy_imm(0x0); // initialize offset in Y
  lda_abs(Player_XSpeedAbsolute); // check player's walking/running speed
  cmp_imm(0x1c); // against preset amount
  if (carry_flag) { goto SetRunSpd; } // if greater than a certain amount, branch ahead
  iny(); // otherwise increment Y
  cmp_imm(0xe); // compare against lower amount
  if (carry_flag) { goto ChkSkid; } // if greater than this but not greater than first, skip increment
  iny(); // otherwise increment Y again
  
ChkSkid:
  lda_abs(SavedJoypadBits); // get controller bits
  and_imm(0b01111111); // mask out A button
  if (zero_flag) { goto SetAnimSpd; } // if no other buttons pressed, branch ahead of all this
  and_imm(0x3); // mask out all others except left and right
  cmp_zp(Player_MovingDir); // check against moving direction
  if (!zero_flag) { goto ProcSkid; } // if left/right controller bits <> moving direction, branch
  lda_imm(0x0); // otherwise set zero value here
  
SetRunSpd:
  ram[RunningSpeed] = a; // store zero or running speed here
  goto SetAnimSpd;
  
ProcSkid:
  lda_abs(Player_XSpeedAbsolute); // check player's walking/running speed
  cmp_imm(0xb); // against one last amount
  if (carry_flag) { goto SetAnimSpd; } // if greater than this amount, branch
  lda_zp(PlayerFacingDir);
  ram[Player_MovingDir] = a; // otherwise use facing direction to set moving direction
  lda_imm(0x0);
  ram[Player_X_Speed] = a; // nullify player's horizontal speed
  ram[Player_X_MoveForce] = a; // and dummy variable for player
  
SetAnimSpd:
  lda_absy(PlayerAnimTmrData); // get animation timer setting using Y as offset
  ram[PlayerAnimTimerSet] = a;
  // -------------------------------------------------------------------------------------
}

void ImposeFriction(void) {
  and_abs(Player_CollisionBits); // perform AND between left/right controller bits and collision flag
  cmp_imm(0x0); // then compare to zero (this instruction is redundant)
  if (!zero_flag) { goto JoypFrict; } // if any bits set, branch to next part
  lda_zp(Player_X_Speed);
  if (zero_flag) { goto SetAbsSpd; } // if player has no horizontal speed, branch ahead to last part
  if (!neg_flag) { goto RghtFrict; } // if player moving to the right, branch to slow
  if (neg_flag) { goto LeftFrict; } // otherwise logic dictates player moving left, branch to slow
  
JoypFrict:
  lsr_acc(); // put right controller bit into carry
  if (!carry_flag) { goto RghtFrict; } // if left button pressed, carry = 0, thus branch
  
LeftFrict:
  lda_abs(Player_X_MoveForce); // load value set here
  carry_flag = false;
  adc_abs(FrictionAdderLow); // add to it another value set here
  ram[Player_X_MoveForce] = a; // store here
  lda_zp(Player_X_Speed);
  adc_abs(FrictionAdderHigh); // add value plus carry to horizontal speed
  ram[Player_X_Speed] = a; // set as new horizontal speed
  cmp_abs(MaximumRightSpeed); // compare against maximum value for right movement
  if (neg_flag) { goto XSpdSign; } // if horizontal speed greater negatively, branch
  lda_abs(MaximumRightSpeed); // otherwise set preset value as horizontal speed
  ram[Player_X_Speed] = a; // thus slowing the player's left movement down
  goto SetAbsSpd; // skip to the end
  
RghtFrict:
  lda_abs(Player_X_MoveForce); // load value set here
  carry_flag = true;
  sbc_abs(FrictionAdderLow); // subtract from it another value set here
  ram[Player_X_MoveForce] = a; // store here
  lda_zp(Player_X_Speed);
  sbc_abs(FrictionAdderHigh); // subtract value plus borrow from horizontal speed
  ram[Player_X_Speed] = a; // set as new horizontal speed
  cmp_abs(MaximumLeftSpeed); // compare against maximum value for left movement
  if (!neg_flag) { goto XSpdSign; } // if horizontal speed greater positively, branch
  lda_abs(MaximumLeftSpeed); // otherwise set preset value as horizontal speed
  ram[Player_X_Speed] = a; // thus slowing the player's right movement down
  
XSpdSign:
  cmp_imm(0x0); // if player not moving or moving to the right,
  if (!neg_flag) { goto SetAbsSpd; } // branch and leave horizontal speed value unmodified
  eor_imm(0xff);
  carry_flag = false; // otherwise get two's compliment to get absolute
  adc_imm(0x1); // unsigned walking/running speed
  
SetAbsSpd:
  ram[Player_XSpeedAbsolute] = a; // store walking/running speed here and leave
  // -------------------------------------------------------------------------------------
  // $00 - used to store downward movement force in FireballObjCore
  // $02 - used to store maximum vertical speed in FireballObjCore
  // $07 - used to store pseudorandom bit in BubbleCheck
}

void ProcFireball_Bubble(void) {
  lda_abs(PlayerStatus); // check player's status
  cmp_imm(0x2);
  // if not fiery, branch
  if (carry_flag) {
    lda_zp(A_B_Buttons);
    and_imm(B_Button); // check for b button pressed
    // branch if not pressed
    if (!zero_flag) {
      and_zp(PreviousA_B_Buttons);
      // if button pressed in previous frame, branch
      if (zero_flag) {
        lda_abs(FireballCounter); // load fireball counter
        and_imm(0b00000001); // get LSB and use as offset for buffer
        tax();
        lda_zpx(Fireball_State); // load fireball state
        // if not inactive, branch
        if (zero_flag) {
          ldy_zp(Player_Y_HighPos); // if player too high or too low, branch
          dey();
          if (zero_flag) {
            lda_abs(CrouchingFlag); // if player crouching, branch
            if (zero_flag) {
              lda_zp(Player_State); // if player's state = climbing, branch
              cmp_imm(0x3);
              if (!zero_flag) {
                lda_imm(Sfx_Fireball); // play fireball sound effect
                ram[Square1SoundQueue] = a;
                lda_imm(0x2); // load state
                ram[Fireball_State + x] = a;
                ldy_abs(PlayerAnimTimerSet); // copy animation frame timer setting
                ram[FireballThrowingTimer] = y; // into fireball throwing timer
                dey();
                ram[PlayerAnimTimer] = y; // decrement and store in player's animation timer
                inc_abs(FireballCounter); // increment fireball counter
              }
            }
          }
        }
      }
    }
    // ProcFireballs:
    ldx_imm(0x0);
    FireballObjCore(); // process first fireball object
    ldx_imm(0x1);
    FireballObjCore(); // process second fireball object, then do air bubbles
  }
  // ProcAirBubbles:
  lda_abs(AreaType); // if not water type level, skip the rest of this
  if (zero_flag) {
    ldx_imm(0x2); // otherwise load counter and use as offset
    
BublLoop:
    ram[ObjectOffset] = x; // store offset
    BubbleCheck(); // check timers and coordinates, create air bubble
    RelativeBubblePosition(); // get relative coordinates
    GetBubbleOffscreenBits(); // get offscreen information
    DrawBubble(); // draw the air bubble
    dex();
    if (!neg_flag) { goto BublLoop; } // do this until all three are handled
  }
}

void FireballObjCore(void) {
  ram[ObjectOffset] = x; // store offset as current object
  lda_zpx(Fireball_State); // check for d7 = 1
  asl_acc();
  // if so, branch to get relative coordinates and draw explosion
  if (!carry_flag) {
    ldy_zpx(Fireball_State); // if fireball inactive, branch to leave
    if (!zero_flag) {
      dey(); // if fireball state set to 1, skip this part and just run it
      if (!zero_flag) {
        lda_zp(Player_X_Position); // get player's horizontal position
        adc_imm(0x4); // add four pixels and store as fireball's horizontal position
        ram[Fireball_X_Position + x] = a;
        lda_zp(Player_PageLoc); // get player's page location
        adc_imm(0x0); // add carry and store as fireball's page location
        ram[Fireball_PageLoc + x] = a;
        lda_zp(Player_Y_Position); // get player's vertical position and store
        ram[Fireball_Y_Position + x] = a;
        lda_imm(0x1); // set high byte of vertical position
        ram[Fireball_Y_HighPos + x] = a;
        ldy_zp(PlayerFacingDir); // get player's facing direction
        dey(); // decrement to use as offset here
        lda_absy(FireballXSpdData); // set horizontal speed of fireball accordingly
        ram[Fireball_X_Speed + x] = a;
        lda_imm(0x4); // set vertical speed of fireball
        ram[Fireball_Y_Speed + x] = a;
        lda_imm(0x7);
        ram[Fireball_BoundBoxCtrl + x] = a; // set bounding box size control for fireball
        dec_zpx(Fireball_State); // decrement state to 1 to skip this part from now on
      }
      // RunFB:
      txa(); // add 7 to offset to use
      carry_flag = false; // as fireball offset for next routines
      adc_imm(0x7);
      tax();
      lda_imm(0x50); // set downward movement force here
      ram[0x0] = a;
      lda_imm(0x3); // set maximum speed here
      ram[0x2] = a;
      lda_imm(0x0);
      ImposeGravity(); // do sub here to impose gravity on fireball and move vertically
      MoveObjectHorizontally(); // do another sub to move it horizontally
      ldx_zp(ObjectOffset); // return fireball offset to X
      RelativeFireballPosition(); // get relative coordinates
      GetFireballOffscreenBits(); // get offscreen information
      GetFireballBoundBox(); // get bounding box coordinates
      FireballBGCollision(); // do fireball to background collision detection
      lda_abs(FBall_OffscreenBits); // get fireball offscreen bits
      and_imm(0b11001100); // mask out certain bits
      // if any bits still set, branch to kill fireball
      if (zero_flag) {
        FireballEnemyCollision(); // do fireball to enemy collision detection and deal with collisions
        DrawFireball(); // draw fireball appropriately and leave
        return;
      }
      // EraseFB:
      lda_imm(0x0); // erase fireball state
      ram[Fireball_State + x] = a;
    }
    // NoFBall:
    return; // leave
  }
  // FireballExplosion:
  RelativeFireballPosition();
  DrawExplosion_Fireball();
}

void BubbleCheck(void) {
  lda_absx(PseudoRandomBitReg + 1); // get part of LSFR
  and_imm(0x1);
  ram[0x7] = a; // store pseudorandom bit here
  lda_zpx(Bubble_Y_Position); // get vertical coordinate for air bubble
  cmp_imm(0xf8); // if offscreen coordinate not set,
  // branch to move air bubble
  if (!zero_flag) {
    MoveBubl();
    return;
  }
  lda_abs(AirBubbleTimer); // if air bubble timer not expired,
  if (zero_flag) {
    SetupBubble(); // <fallthrough>
  }
}

void SetupBubble(void) {
  ldy_imm(0x0); // load default value here
  lda_zp(PlayerFacingDir); // get player's facing direction
  lsr_acc(); // move d0 to carry
  // branch to use default value if facing left
  if (carry_flag) {
    ldy_imm(0x8); // otherwise load alternate value here
  }
  // PosBubl:
  tya(); // use value loaded as adder
  adc_zp(Player_X_Position); // add to player's horizontal position
  ram[Bubble_X_Position + x] = a; // save as horizontal position for airbubble
  lda_zp(Player_PageLoc);
  adc_imm(0x0); // add carry to player's page location
  ram[Bubble_PageLoc + x] = a; // save as page location for airbubble
  lda_zp(Player_Y_Position);
  carry_flag = false; // add eight pixels to player's vertical position
  adc_imm(0x8);
  ram[Bubble_Y_Position + x] = a; // save as vertical position for air bubble
  lda_imm(0x1);
  ram[Bubble_Y_HighPos + x] = a; // set vertical high byte for air bubble
  ldy_zp(0x7); // get pseudorandom bit, use as offset
  lda_absy(BubbleTimerData); // get data for air bubble timer
  ram[AirBubbleTimer] = a; // set air bubble timer
  MoveBubl(); // <fallthrough>
}

void MoveBubl(void) {
  ldy_zp(0x7); // get pseudorandom bit again, use as offset
  lda_absx(Bubble_YMF_Dummy);
  carry_flag = true; // subtract pseudorandom amount from dummy variable
  sbc_absy(Bubble_MForceData);
  ram[Bubble_YMF_Dummy + x] = a; // save dummy variable
  lda_zpx(Bubble_Y_Position);
  sbc_imm(0x0); // subtract borrow from airbubble's vertical coordinate
  cmp_imm(0x20); // if below the status bar,
  // branch to go ahead and use to move air bubble upwards
  if (!carry_flag) {
    lda_imm(0xf8); // otherwise set offscreen coordinate
  }
  // Y_Bubl:
  ram[Bubble_Y_Position + x] = a; // store as new vertical coordinate for air bubble
}

void RunGameTimer(void) {
  lda_abs(OperMode); // get primary mode of operation
  if (zero_flag) { return; } // branch to leave if in title screen mode
  lda_zp(GameEngineSubroutine);
  cmp_imm(0x8); // if routine number less than eight running,
  if (!carry_flag) { return; } // branch to leave
  cmp_imm(0xb); // if running death routine,
  if (zero_flag) { return; } // branch to leave
  lda_zp(Player_Y_HighPos);
  cmp_imm(0x2); // if player below the screen,
  if (carry_flag) { return; } // branch to leave regardless of level type
  lda_abs(GameTimerCtrlTimer); // if game timer control not yet expired,
  if (!zero_flag) { return; } // branch to leave
  lda_abs(GameTimerDisplay);
  ora_abs(GameTimerDisplay + 1); // otherwise check game timer digits
  ora_abs(GameTimerDisplay + 2);
  if (zero_flag) { goto TimeUpOn; } // if game timer digits at 000, branch to time-up code
  ldy_abs(GameTimerDisplay); // otherwise check first digit
  dey(); // if first digit not on 1,
  if (!zero_flag) { goto ResGTCtrl; } // branch to reset game timer control
  lda_abs(GameTimerDisplay + 1); // otherwise check second and third digits
  ora_abs(GameTimerDisplay + 2);
  if (!zero_flag) { goto ResGTCtrl; } // if timer not at 100, branch to reset game timer control
  lda_imm(TimeRunningOutMusic);
  ram[EventMusicQueue] = a; // otherwise load time running out music
  
ResGTCtrl:
  lda_imm(0x18); // reset game timer control
  ram[GameTimerCtrlTimer] = a;
  ldy_imm(0x23); // set offset for last digit
  lda_imm(0xff); // set value to decrement game timer digit
  ram[DigitModifier + 5] = a;
  DigitsMathRoutine(); // do sub to decrement game timer slowly
  lda_imm(0xa4); // set status nybbles to update game timer display
  PrintStatusBarNumbers(); return; // do sub to update the display
  
TimeUpOn:
  ram[PlayerStatus] = a; // init player status (note A will always be zero here)
  ForceInjury(); // do sub to kill the player (note player is small here)
  inc_abs(GameTimerExpiredFlag); // set game timer expiration flag
  // -------------------------------------------------------------------------------------
}

void WarpZoneObject(void) {
  lda_abs(ScrollLock); // check for scroll lock flag
  if (zero_flag) { return; } // branch if not set to leave
  lda_zp(Player_Y_Position); // check to see if player's vertical coordinate has
  and_zp(Player_Y_HighPos); // same bits set as in vertical high byte (why?)
  if (!zero_flag) { return; } // if so, branch to leave
  ram[ScrollLock] = a; // otherwise nullify scroll lock flag
  inc_abs(WarpZoneControl); // increment warp zone flag to make warp pipes for warp zone
  EraseEnemyObject(); // kill this object
  // -------------------------------------------------------------------------------------
  // $00 - used in WhirlpoolActivate to store whirlpool length / 2, page location of center of whirlpool
  // and also to store movement force exerted on player
  // $01 - used in ProcessWhirlpools to store page location of right extent of whirlpool
  // and in WhirlpoolActivate to store center of whirlpool
  // $02 - used in ProcessWhirlpools to store right extent of whirlpool and in
  // WhirlpoolActivate to store maximum vertical speed
}

void ProcessWhirlpools(void) {
  lda_abs(AreaType); // check for water type level
  if (!zero_flag) { return; } // branch to leave if not found
  ram[Whirlpool_Flag] = a; // otherwise initialize whirlpool flag
  lda_abs(TimerControl); // if master timer control set,
  if (!zero_flag) { return; } // branch to leave
  ldy_imm(0x4); // otherwise start with last whirlpool data
  
WhLoop:
  lda_absy(Whirlpool_LeftExtent); // get left extent of whirlpool
  carry_flag = false;
  adc_absy(Whirlpool_Length); // add length of whirlpool
  ram[0x2] = a; // store result as right extent here
  lda_absy(Whirlpool_PageLoc); // get page location
  if (zero_flag) { goto NextWh; } // if none or page 0, branch to get next data
  adc_imm(0x0); // add carry
  ram[0x1] = a; // store result as page location of right extent here
  lda_zp(Player_X_Position); // get player's horizontal position
  carry_flag = true;
  sbc_absy(Whirlpool_LeftExtent); // subtract left extent
  lda_zp(Player_PageLoc); // get player's page location
  sbc_absy(Whirlpool_PageLoc); // subtract borrow
  if (neg_flag) { goto NextWh; } // if player too far left, branch to get next data
  lda_zp(0x2); // otherwise get right extent
  carry_flag = true;
  sbc_zp(Player_X_Position); // subtract player's horizontal coordinate
  lda_zp(0x1); // get right extent's page location
  sbc_zp(Player_PageLoc); // subtract borrow
  if (!neg_flag) { goto WhirlpoolActivate; } // if player within right extent, branch to whirlpool code
  
NextWh:
  dey(); // move onto next whirlpool data
  if (!neg_flag) { goto WhLoop; } // do this until all whirlpools are checked
  return; // leave
  
WhirlpoolActivate:
  lda_absy(Whirlpool_Length); // get length of whirlpool
  lsr_acc(); // divide by 2
  ram[0x0] = a; // save here
  lda_absy(Whirlpool_LeftExtent); // get left extent of whirlpool
  carry_flag = false;
  adc_zp(0x0); // add length divided by 2
  ram[0x1] = a; // save as center of whirlpool
  lda_absy(Whirlpool_PageLoc); // get page location
  adc_imm(0x0); // add carry
  ram[0x0] = a; // save as page location of whirlpool center
  lda_zp(FrameCounter); // get frame counter
  lsr_acc(); // shift d0 into carry (to run on every other frame)
  if (!carry_flag) { goto WhPull; } // if d0 not set, branch to last part of code
  lda_zp(0x1); // get center
  carry_flag = true;
  sbc_zp(Player_X_Position); // subtract player's horizontal coordinate
  lda_zp(0x0); // get page location of center
  sbc_zp(Player_PageLoc); // subtract borrow
  if (!neg_flag) { goto LeftWh; } // if player to the left of center, branch
  lda_zp(Player_X_Position); // otherwise slowly pull player left, towards the center
  carry_flag = true;
  sbc_imm(0x1); // subtract one pixel
  ram[Player_X_Position] = a; // set player's new horizontal coordinate
  lda_zp(Player_PageLoc);
  sbc_imm(0x0); // subtract borrow
  goto SetPWh; // jump to set player's new page location
  
LeftWh:
  lda_abs(Player_CollisionBits); // get player's collision bits
  lsr_acc(); // shift d0 into carry
  if (!carry_flag) { goto WhPull; } // if d0 not set, branch
  lda_zp(Player_X_Position); // otherwise slowly pull player right, towards the center
  carry_flag = false;
  adc_imm(0x1); // add one pixel
  ram[Player_X_Position] = a; // set player's new horizontal coordinate
  lda_zp(Player_PageLoc);
  adc_imm(0x0); // add carry
  
SetPWh:
  ram[Player_PageLoc] = a; // set player's new page location
  
WhPull:
  lda_imm(0x10);
  ram[0x0] = a; // set vertical movement force
  lda_imm(0x1);
  ram[Whirlpool_Flag] = a; // set whirlpool flag to be used later
  ram[0x2] = a; // also set maximum vertical speed
  lsr_acc();
  tax(); // set X for player offset
  ImposeGravity(); return; // jump to put whirlpool effect on player vertically, do not return
  // -------------------------------------------------------------------------------------
}

void ImposeGravity(void) {
  pha(); // push value to stack
  lda_absx(SprObject_YMF_Dummy);
  carry_flag = false; // add value in movement force to contents of dummy variable
  adc_absx(SprObject_Y_MoveForce);
  ram[SprObject_YMF_Dummy + x] = a;
  ldy_imm(0x0); // set Y to zero by default
  lda_zpx(SprObject_Y_Speed); // get current vertical speed
  if (!neg_flag) { goto AlterYP; } // if currently moving downwards, do not decrement Y
  dey(); // otherwise decrement Y
  
AlterYP:
  ram[0x7] = y; // store Y here
  adc_zpx(SprObject_Y_Position); // add vertical position to vertical speed plus carry
  ram[SprObject_Y_Position + x] = a; // store as new vertical position
  lda_zpx(SprObject_Y_HighPos);
  adc_zp(0x7); // add carry plus contents of $07 to vertical high byte
  ram[SprObject_Y_HighPos + x] = a; // store as new vertical high byte
  lda_absx(SprObject_Y_MoveForce);
  carry_flag = false;
  adc_zp(0x0); // add downward movement amount to contents of $0433
  ram[SprObject_Y_MoveForce + x] = a;
  lda_zpx(SprObject_Y_Speed); // add carry to vertical speed and store
  adc_imm(0x0);
  ram[SprObject_Y_Speed + x] = a;
  cmp_zp(0x2); // compare to maximum speed
  if (neg_flag) { goto ChkUpM; } // if less than preset value, skip this part
  lda_absx(SprObject_Y_MoveForce);
  cmp_imm(0x80); // if less positively than preset maximum, skip this part
  if (!carry_flag) { goto ChkUpM; }
  lda_zp(0x2);
  ram[SprObject_Y_Speed + x] = a; // keep vertical speed within maximum value
  lda_imm(0x0);
  ram[SprObject_Y_MoveForce + x] = a; // clear fractional
  
ChkUpM:
  pla(); // get value from stack
  if (zero_flag) { return; } // if set to zero, branch to leave
  lda_zp(0x2);
  eor_imm(0b11111111); // otherwise get two's compliment of maximum speed
  tay();
  iny();
  ram[0x7] = y; // store two's compliment here
  lda_absx(SprObject_Y_MoveForce);
  carry_flag = true; // subtract upward movement amount from contents
  sbc_zp(0x1); // of movement force, note that $01 is twice as large as $00,
  ram[SprObject_Y_MoveForce + x] = a; // thus it effectively undoes add we did earlier
  lda_zpx(SprObject_Y_Speed);
  sbc_imm(0x0); // subtract borrow from vertical speed and store
  ram[SprObject_Y_Speed + x] = a;
  cmp_zp(0x7); // compare vertical speed to two's compliment
  if (!neg_flag) { return; } // if less negatively than preset maximum, skip this part
  lda_absx(SprObject_Y_MoveForce);
  cmp_imm(0x80); // check if fractional part is above certain amount,
  if (carry_flag) { return; } // and if so, branch to leave
  lda_zp(0x7);
  ram[SprObject_Y_Speed + x] = a; // keep vertical speed within maximum value
  lda_imm(0xff);
  ram[SprObject_Y_MoveForce + x] = a; // clear fractional
  // -------------------------------------------------------------------------------------
}

void FlagpoleRoutine(void) {
  ldx_imm(0x5); // set enemy object offset
  ram[ObjectOffset] = x; // to special use slot
  lda_zpx(Enemy_ID);
  cmp_imm(FlagpoleFlagObject); // if flagpole flag not found,
  if (!zero_flag) { return; } // branch to leave
  lda_zp(GameEngineSubroutine);
  cmp_imm(0x4); // if flagpole slide routine not running,
  if (!zero_flag) { goto SkipScore; } // branch to near the end of code
  lda_zp(Player_State);
  cmp_imm(0x3); // if player state not climbing,
  if (!zero_flag) { goto SkipScore; } // branch to near the end of code
  lda_zpx(Enemy_Y_Position); // check flagpole flag's vertical coordinate
  cmp_imm(0xaa); // if flagpole flag down to a certain point,
  if (carry_flag) { goto GiveFPScr; } // branch to end the level
  lda_zp(Player_Y_Position); // check player's vertical coordinate
  cmp_imm(0xa2); // if player down to a certain point,
  if (carry_flag) { goto GiveFPScr; } // branch to end the level
  lda_absx(Enemy_YMF_Dummy);
  adc_imm(0xff); // add movement amount to dummy variable
  ram[Enemy_YMF_Dummy + x] = a; // save dummy variable
  lda_zpx(Enemy_Y_Position); // get flag's vertical coordinate
  adc_imm(0x1); // add 1 plus carry to move flag, and
  ram[Enemy_Y_Position + x] = a; // store vertical coordinate
  lda_abs(FlagpoleFNum_YMFDummy);
  carry_flag = true; // subtract movement amount from dummy variable
  sbc_imm(0xff);
  ram[FlagpoleFNum_YMFDummy] = a; // save dummy variable
  lda_abs(FlagpoleFNum_Y_Pos);
  sbc_imm(0x1); // subtract one plus borrow to move floatey number,
  ram[FlagpoleFNum_Y_Pos] = a; // and store vertical coordinate here
  
SkipScore:
  goto FPGfx; // jump to skip ahead and draw flag and floatey number
  
GiveFPScr:
  ldy_abs(FlagpoleScore); // get score offset from earlier (when player touched flagpole)
  lda_absy(FlagpoleScoreMods); // get amount to award player points
  ldx_absy(FlagpoleScoreDigits); // get digit with which to award points
  ram[DigitModifier + x] = a; // store in digit modifier
  AddToScore(); // do sub to award player points depending on height of collision
  lda_imm(0x5);
  ram[GameEngineSubroutine] = a; // set to run end-of-level subroutine on next frame
  
FPGfx:
  GetEnemyOffscreenBits(); // get offscreen information
  RelativeEnemyPosition(); // get relative coordinates
  FlagpoleGfxHandler(); // draw flagpole flag and floatey number
  // -------------------------------------------------------------------------------------
}

void JumpspringHandler(void) {
  GetEnemyOffscreenBits(); // get offscreen information
  lda_abs(TimerControl); // check master timer control
  if (!zero_flag) { goto DrawJSpr; } // branch to last section if set
  lda_abs(JumpspringAnimCtrl); // check jumpspring frame control
  if (zero_flag) { goto DrawJSpr; } // branch to last section if not set
  tay();
  dey(); // subtract one from frame control,
  tya(); // the only way a poor nmos 6502 can
  and_imm(0b00000010); // mask out all but d1, original value still in Y
  if (!zero_flag) { goto DownJSpr; } // if set, branch to move player up
  inc_zp(Player_Y_Position);
  inc_zp(Player_Y_Position); // move player's vertical position down two pixels
  goto PosJSpr; // skip to next part
  
DownJSpr:
  dec_zp(Player_Y_Position); // move player's vertical position up two pixels
  dec_zp(Player_Y_Position);
  
PosJSpr:
  lda_zpx(Jumpspring_FixedYPos); // get permanent vertical position
  carry_flag = false;
  adc_absy(Jumpspring_Y_PosData); // add value using frame control as offset
  ram[Enemy_Y_Position + x] = a; // store as new vertical position
  cpy_imm(0x1); // check frame control offset (second frame is $00)
  if (!carry_flag) { goto BounceJS; } // if offset not yet at third frame ($01), skip to next part
  lda_zp(A_B_Buttons);
  and_imm(A_Button); // check saved controller bits for A button press
  if (zero_flag) { goto BounceJS; } // skip to next part if A not pressed
  and_zp(PreviousA_B_Buttons); // check for A button pressed in previous frame
  if (!zero_flag) { goto BounceJS; } // skip to next part if so
  lda_imm(0xf4);
  ram[JumpspringForce] = a; // otherwise write new jumpspring force here
  
BounceJS:
  cpy_imm(0x3); // check frame control offset again
  if (!zero_flag) { goto DrawJSpr; } // skip to last part if not yet at fifth frame ($03)
  lda_abs(JumpspringForce);
  ram[Player_Y_Speed] = a; // store jumpspring force as player's new vertical speed
  lda_imm(0x0);
  ram[JumpspringAnimCtrl] = a; // initialize jumpspring frame control
  
DrawJSpr:
  RelativeEnemyPosition(); // get jumpspring's relative coordinates
  EnemyGfxHandler(); // draw jumpspring
  OffscreenBoundsCheck(); // check to see if we need to kill it
  lda_abs(JumpspringAnimCtrl); // if frame control at zero, don't bother
  if (zero_flag) { return; } // trying to animate it, just leave
  lda_abs(JumpspringTimer);
  if (!zero_flag) { return; } // if jumpspring timer not expired yet, leave
  lda_imm(0x4);
  ram[JumpspringTimer] = a; // otherwise initialize jumpspring timer
  inc_abs(JumpspringAnimCtrl); // increment frame control to animate jumpspring
  // -------------------------------------------------------------------------------------
}

void Setup_Vine(void) {
  lda_imm(VineObject); // load identifier for vine object
  ram[Enemy_ID + x] = a; // store in buffer
  lda_imm(0x1);
  ram[Enemy_Flag + x] = a; // set flag for enemy object buffer
  lda_zpy(Block_PageLoc);
  ram[Enemy_PageLoc + x] = a; // copy page location from previous object
  lda_zpy(Block_X_Position);
  ram[Enemy_X_Position + x] = a; // copy horizontal coordinate from previous object
  lda_zpy(Block_Y_Position);
  ram[Enemy_Y_Position + x] = a; // copy vertical coordinate from previous object
  ldy_abs(VineFlagOffset); // load vine flag/offset to next available vine slot
  // if set at all, don't bother to store vertical
  if (zero_flag) {
    ram[VineStart_Y_Position] = a; // otherwise store vertical coordinate here
  }
  // NextVO:
  txa(); // store object offset to next available vine slot
  ram[VineObjOffset + y] = a; // using vine flag as offset
  inc_abs(VineFlagOffset); // increment vine flag offset
  lda_imm(Sfx_GrowVine);
  ram[Square2SoundQueue] = a; // load vine grow sound
  // -------------------------------------------------------------------------------------
  // $06-$07 - used as address to block buffer data
  // $02 - used as vertical high nybble of block buffer offset
}

void VineObjectHandler(void) {
  cpx_imm(0x5); // check enemy offset for special use slot
  // if not in last slot, branch to leave
  if (zero_flag) {
    ldy_abs(VineFlagOffset);
    dey(); // decrement vine flag in Y, use as offset
    lda_abs(VineHeight);
    cmp_absy(VineHeightData); // if vine has reached certain height,
    // branch ahead to skip this part
    if (!zero_flag) {
      lda_zp(FrameCounter); // get frame counter
      lsr_acc(); // shift d1 into carry
      lsr_acc();
      // if d1 not set (2 frames every 4) skip this part
      if (carry_flag) {
        lda_zp(Enemy_Y_Position + 5);
        sbc_imm(0x1); // subtract vertical position of vine
        ram[Enemy_Y_Position + 5] = a; // one pixel every frame it's time
        inc_abs(VineHeight); // increment vine height
      }
    }
    // RunVSubs:
    lda_abs(VineHeight); // if vine still very small,
    cmp_imm(0x8); // branch to leave
    if (carry_flag) {
      RelativeEnemyPosition(); // get relative coordinates of vine,
      GetEnemyOffscreenBits(); // and any offscreen bits
      ldy_imm(0x0); // initialize offset used in draw vine sub
      
VDrawLoop:
      DrawVine(); // draw vine
      iny(); // increment offset
      cpy_abs(VineFlagOffset); // if offset in Y and offset here
      if (!zero_flag) { goto VDrawLoop; } // do not yet match, loop back to draw more vine
      lda_abs(Enemy_OffscreenBits);
      and_imm(0b00001100); // mask offscreen bits
      // if none of the saved offscreen bits set, skip ahead
      if (!zero_flag) {
        dey(); // otherwise decrement Y to get proper offset again
        
KillVine:
        ldx_absy(VineObjOffset); // get enemy object offset for this vine object
        EraseEnemyObject(); // kill this vine object
        dey(); // decrement Y
        if (!neg_flag) { goto KillVine; } // if any vine objects left, loop back to kill it
        ram[VineFlagOffset] = a; // initialize vine flag/offset
        ram[VineHeight] = a; // initialize vine height
      }
      // WrCMTile:
      lda_abs(VineHeight); // check vine height
      cmp_imm(0x20); // if vine small (less than 32 pixels tall)
      // then branch ahead to leave
      if (carry_flag) {
        ldx_imm(0x6); // set offset in X to last enemy slot
        lda_imm(0x1); // set A to obtain horizontal in $04, but we don't care
        ldy_imm(0x1b); // set Y to offset to get block at ($04, $10) of coordinates
        BlockBufferCollision(); // do a sub to get block buffer address set, return contents
        ldy_zp(0x2);
        cpy_imm(0xd0); // if vertical high nybble offset beyond extent of
        // current block buffer, branch to leave, do not write
        if (!carry_flag) {
          lda_indy(0x6); // otherwise check contents of block buffer at
          // current offset, if not empty, branch to leave
          if (zero_flag) {
            lda_imm(0x26);
            dynamic_ram_write(read_word(0x6) + y, a); // otherwise, write climbing metatile to block buffer
          }
        }
      }
    }
  }
  // ExitVH:
  ldx_zp(ObjectOffset); // get enemy object offset and leave
  // -------------------------------------------------------------------------------------
}

void ProcessCannons(void) {
  lda_abs(AreaType); // get area type
  if (!zero_flag) {
    ldx_imm(0x2);
    
ThreeSChk:
    ram[ObjectOffset] = x; // start at third enemy slot
    lda_zpx(Enemy_Flag); // check enemy buffer flag
    // if set, branch to check enemy
    if (zero_flag) {
      lda_absx(PseudoRandomBitReg + 1); // otherwise get part of LSFR
      ldy_abs(SecondaryHardMode); // get secondary hard mode flag, use as offset
      and_absy(CannonBitmasks); // mask out bits of LSFR as decided by flag
      cmp_imm(0x6); // check to see if lower nybble is above certain value
      // if so, branch to check enemy
      if (!carry_flag) {
        tay(); // transfer masked contents of LSFR to Y as pseudorandom offset
        lda_absy(Cannon_PageLoc); // get page location
        // if not set or on page 0, branch to check enemy
        if (!zero_flag) {
          lda_absy(Cannon_Timer); // get cannon timer
          // if expired, branch to fire cannon
          if (!zero_flag) {
            sbc_imm(0x0); // otherwise subtract borrow (note carry will always be clear here)
            ram[Cannon_Timer + y] = a; // to count timer down
            goto Chk_BB; // then jump ahead to check enemy
          }
          // FireCannon:
          lda_abs(TimerControl); // if master timer control set,
          // branch to check enemy
          if (zero_flag) {
            lda_imm(0xe); // otherwise we start creating one
            ram[Cannon_Timer + y] = a; // first, reset cannon timer
            lda_absy(Cannon_PageLoc); // get page location of cannon
            ram[Enemy_PageLoc + x] = a; // save as page location of bullet bill
            lda_absy(Cannon_X_Position); // get horizontal coordinate of cannon
            ram[Enemy_X_Position + x] = a; // save as horizontal coordinate of bullet bill
            lda_absy(Cannon_Y_Position); // get vertical coordinate of cannon
            carry_flag = true;
            sbc_imm(0x8); // subtract eight pixels (because enemies are 24 pixels tall)
            ram[Enemy_Y_Position + x] = a; // save as vertical coordinate of bullet bill
            lda_imm(0x1);
            ram[Enemy_Y_HighPos + x] = a; // set vertical high byte of bullet bill
            ram[Enemy_Flag + x] = a; // set buffer flag
            lsr_acc(); // shift right once to init A
            ram[Enemy_State + x] = a; // then initialize enemy's state
            lda_imm(0x9);
            ram[Enemy_BoundBoxCtrl + x] = a; // set bounding box size control for bullet bill
            lda_imm(BulletBill_CannonVar);
            ram[Enemy_ID + x] = a; // load identifier for bullet bill (cannon variant)
            goto Next3Slt; // move onto next slot
          }
        }
      }
    }
    
Chk_BB:
    lda_zpx(Enemy_ID); // check enemy identifier for bullet bill (cannon variant)
    cmp_imm(BulletBill_CannonVar);
    // if not found, branch to get next slot
    if (zero_flag) {
      OffscreenBoundsCheck(); // otherwise, check to see if it went offscreen
      lda_zpx(Enemy_Flag); // check enemy buffer flag
      // if not set, branch to get next slot
      if (!zero_flag) {
        GetEnemyOffscreenBits(); // otherwise, get offscreen information
        BulletBillHandler(); // then do sub to handle bullet bill
      }
    }
    
Next3Slt:
    dex(); // move onto next slot
    if (!neg_flag) { goto ThreeSChk; } // do this until first three slots are checked
    // --------------------------------
  }
}

void BulletBillHandler(void) {
  lda_abs(TimerControl); // if master timer control set,
  if (!zero_flag) { goto RunBBSubs; } // branch to run subroutines except movement sub
  lda_zpx(Enemy_State);
  if (!zero_flag) { goto ChkDSte; } // if bullet bill's state set, branch to check defeated state
  lda_abs(Enemy_OffscreenBits); // otherwise load offscreen bits
  and_imm(0b00001100); // mask out bits
  cmp_imm(0b00001100); // check to see if all bits are set
  if (zero_flag) { goto KillBB; } // if so, branch to kill this object
  ldy_imm(0x1); // set to move right by default
  PlayerEnemyDiff(); // get horizontal difference between player and bullet bill
  if (neg_flag) { goto SetupBB; } // if enemy to the left of player, branch
  iny(); // otherwise increment to move left
  
SetupBB:
  ram[Enemy_MovingDir + x] = y; // set bullet bill's moving direction
  dey(); // decrement to use as offset
  lda_absy(BulletBillXSpdData); // get horizontal speed based on moving direction
  ram[Enemy_X_Speed + x] = a; // and store it
  lda_zp(0x0); // get horizontal difference
  adc_imm(0x28); // add 40 pixels
  cmp_imm(0x50); // if less than a certain amount, player is too close
  if (!carry_flag) { goto KillBB; } // to cannon either on left or right side, thus branch
  lda_imm(0x1);
  ram[Enemy_State + x] = a; // otherwise set bullet bill's state
  lda_imm(0xa);
  ram[EnemyFrameTimer + x] = a; // set enemy frame timer
  lda_imm(Sfx_Blast);
  ram[Square2SoundQueue] = a; // play fireworks/gunfire sound
  
ChkDSte:
  lda_zpx(Enemy_State); // check enemy state for d5 set
  and_imm(0b00100000);
  if (zero_flag) { goto BBFly; } // if not set, skip to move horizontally
  MoveD_EnemyVertically(); // otherwise do sub to move bullet bill vertically
  
BBFly:
  MoveEnemyHorizontally(); // do sub to move bullet bill horizontally
  
RunBBSubs:
  GetEnemyOffscreenBits(); // get offscreen information
  RelativeEnemyPosition(); // get relative coordinates
  GetEnemyBoundBox(); // get bounding box coordinates
  PlayerEnemyCollision(); // handle player to enemy collisions
  EnemyGfxHandler(); return; // draw the bullet bill and leave
  
KillBB:
  EraseEnemyObject(); // kill bullet bill and leave
  // -------------------------------------------------------------------------------------
}

void EnemyGfxHandler(void) {
  lda_zpx(Enemy_Y_Position); // get enemy object vertical position
  ram[0x2] = a;
  lda_abs(Enemy_Rel_XPos); // get enemy object horizontal position
  ram[0x5] = a; // relative to screen
  ldy_absx(Enemy_SprDataOffset);
  ram[0xeb] = y; // get sprite data offset
  lda_imm(0x0);
  ram[VerticalFlipFlag] = a; // initialize vertical flip flag by default
  lda_zpx(Enemy_MovingDir);
  ram[0x3] = a; // get enemy object moving direction
  lda_absx(Enemy_SprAttrib);
  ram[0x4] = a; // get enemy object sprite attributes
  lda_zpx(Enemy_ID);
  cmp_imm(PiranhaPlant); // is enemy object piranha plant?
  if (!zero_flag) { goto CheckForRetainerObj; } // if not, branch
  ldy_zpx(PiranhaPlant_Y_Speed);
  if (neg_flag) { goto CheckForRetainerObj; } // if piranha plant moving upwards, branch
  ldy_absx(EnemyFrameTimer);
  if (zero_flag) { goto CheckForRetainerObj; } // if timer for movement expired, branch
  return; // if all conditions fail, leave
  
CheckForRetainerObj:
  lda_zpx(Enemy_State); // store enemy state
  ram[0xed] = a;
  and_imm(0b00011111); // nullify all but 5 LSB and use as Y
  tay();
  lda_zpx(Enemy_ID); // check for mushroom retainer/princess object
  cmp_imm(RetainerObject);
  if (!zero_flag) { goto CheckForBulletBillCV; } // if not found, branch
  ldy_imm(0x0); // if found, nullify saved state in Y
  lda_imm(0x1); // set value that will not be used
  ram[0x3] = a;
  lda_imm(0x15); // set value $15 as code for mushroom retainer/princess object
  
CheckForBulletBillCV:
  cmp_imm(BulletBill_CannonVar); // otherwise check for bullet bill object
  if (!zero_flag) { goto CheckForJumpspring; } // if not found, branch again
  dec_zp(0x2); // decrement saved vertical position
  lda_imm(0x3);
  ldy_absx(EnemyFrameTimer); // get timer for enemy object
  if (zero_flag) { goto SBBAt; } // if expired, do not set priority bit
  ora_imm(0b00100000); // otherwise do so
  
SBBAt:
  ram[0x4] = a; // set new sprite attributes
  ldy_imm(0x0); // nullify saved enemy state both in Y and in
  ram[0xed] = y; // memory location here
  lda_imm(0x8); // set specific value to unconditionally branch once
  
CheckForJumpspring:
  cmp_imm(JumpspringObject); // check for jumpspring object
  if (!zero_flag) { goto CheckForPodoboo; }
  ldy_imm(0x3); // set enemy state -2 MSB here for jumpspring object
  ldx_abs(JumpspringAnimCtrl); // get current frame number for jumpspring object
  lda_absx(JumpspringFrameOffsets); // load data using frame number as offset
  
CheckForPodoboo:
  ram[0xef] = a; // store saved enemy object value here
  ram[0xec] = y; // and Y here (enemy state -2 MSB if not changed)
  ldx_zp(ObjectOffset); // get enemy object offset
  cmp_imm(0xc); // check for podoboo object
  if (!zero_flag) { goto CheckBowserGfxFlag; } // branch if not found
  lda_zpx(Enemy_Y_Speed); // if moving upwards, branch
  if (neg_flag) { goto CheckBowserGfxFlag; }
  inc_abs(VerticalFlipFlag); // otherwise, set flag for vertical flip
  
CheckBowserGfxFlag:
  lda_abs(BowserGfxFlag); // if not drawing bowser at all, skip to something else
  if (zero_flag) { goto CheckForGoomba; }
  ldy_imm(0x16); // if set to 1, draw bowser's front
  cmp_imm(0x1);
  if (zero_flag) { goto SBwsrGfxOfs; }
  iny(); // otherwise draw bowser's rear
  
SBwsrGfxOfs:
  ram[0xef] = y;
  
CheckForGoomba:
  ldy_zp(0xef); // check value for goomba object
  cpy_imm(Goomba);
  if (!zero_flag) { goto CheckBowserFront; } // branch if not found
  lda_zpx(Enemy_State);
  cmp_imm(0x2); // check for defeated state
  if (!carry_flag) { goto GmbaAnim; } // if not defeated, go ahead and animate
  ldx_imm(0x4); // if defeated, write new value here
  ram[0xec] = x;
  
GmbaAnim:
  and_imm(0b00100000); // check for d5 set in enemy object state
  ora_abs(TimerControl); // or timer disable flag set
  if (!zero_flag) { goto CheckBowserFront; } // if either condition true, do not animate goomba
  lda_zp(FrameCounter);
  and_imm(0b00001000); // check for every eighth frame
  if (!zero_flag) { goto CheckBowserFront; }
  lda_zp(0x3);
  eor_imm(0b00000011); // invert bits to flip horizontally every eight frames
  ram[0x3] = a; // leave alone otherwise
  
CheckBowserFront:
  lda_absy(EnemyAttributeData); // load sprite attribute using enemy object
  ora_zp(0x4); // as offset, and add to bits already loaded
  ram[0x4] = a;
  lda_absy(EnemyGfxTableOffsets); // load value based on enemy object as offset
  tax(); // save as X
  ldy_zp(0xec); // get previously saved value
  lda_abs(BowserGfxFlag);
  if (zero_flag) { goto CheckForSpiny; } // if not drawing bowser object at all, skip all of this
  cmp_imm(0x1);
  if (!zero_flag) { goto CheckBowserRear; } // if not drawing front part, branch to draw the rear part
  lda_abs(BowserBodyControls); // check bowser's body control bits
  if (!neg_flag) { goto ChkFrontSte; } // branch if d7 not set (control's bowser's mouth)
  ldx_imm(0xde); // otherwise load offset for second frame
  
ChkFrontSte:
  lda_zp(0xed); // check saved enemy state
  and_imm(0b00100000); // if bowser not defeated, do not set flag
  if (zero_flag) { goto DrawBowser; }
  
FlipBowserOver:
  ram[VerticalFlipFlag] = x; // set vertical flip flag to nonzero
  
DrawBowser:
  goto DrawEnemyObject; // draw bowser's graphics now
  
CheckBowserRear:
  lda_abs(BowserBodyControls); // check bowser's body control bits
  and_imm(0x1);
  if (zero_flag) { goto ChkRearSte; } // branch if d0 not set (control's bowser's feet)
  ldx_imm(0xe4); // otherwise load offset for second frame
  
ChkRearSte:
  lda_zp(0xed); // check saved enemy state
  and_imm(0b00100000); // if bowser not defeated, do not set flag
  if (zero_flag) { goto DrawBowser; }
  lda_zp(0x2); // subtract 16 pixels from
  carry_flag = true; // saved vertical coordinate
  sbc_imm(0x10);
  ram[0x2] = a;
  goto FlipBowserOver; // jump to set vertical flip flag
  
CheckForSpiny:
  cpx_imm(0x24); // check if value loaded is for spiny
  if (!zero_flag) { goto CheckForLakitu; } // if not found, branch
  cpy_imm(0x5); // if enemy state set to $05, do this,
  if (!zero_flag) { goto NotEgg; } // otherwise branch
  ldx_imm(0x30); // set to spiny egg offset
  lda_imm(0x2);
  ram[0x3] = a; // set enemy direction to reverse sprites horizontally
  lda_imm(0x5);
  ram[0xec] = a; // set enemy state
  
NotEgg:
  goto CheckForHammerBro; // skip a big chunk of this if we found spiny but not in egg
  
CheckForLakitu:
  cpx_imm(0x90); // check value for lakitu's offset loaded
  if (!zero_flag) { goto CheckUpsideDownShell; } // branch if not loaded
  lda_zp(0xed);
  and_imm(0b00100000); // check for d5 set in enemy state
  if (!zero_flag) { goto NoLAFr; } // branch if set
  lda_abs(FrenzyEnemyTimer);
  cmp_imm(0x10); // check timer to see if we've reached a certain range
  if (carry_flag) { goto NoLAFr; } // branch if not
  ldx_imm(0x96); // if d6 not set and timer in range, load alt frame for lakitu
  
NoLAFr:
  goto CheckDefeatedState; // skip this next part if we found lakitu but alt frame not needed
  
CheckUpsideDownShell:
  lda_zp(0xef); // check for enemy object => $04
  cmp_imm(0x4);
  if (carry_flag) { goto CheckRightSideUpShell; } // branch if true
  cpy_imm(0x2);
  if (!carry_flag) { goto CheckRightSideUpShell; } // branch if enemy state < $02
  ldx_imm(0x5a); // set for upside-down koopa shell by default
  ldy_zp(0xef);
  cpy_imm(BuzzyBeetle); // check for buzzy beetle object
  if (!zero_flag) { goto CheckRightSideUpShell; }
  ldx_imm(0x7e); // set for upside-down buzzy beetle shell if found
  inc_zp(0x2); // increment vertical position by one pixel
  
CheckRightSideUpShell:
  lda_zp(0xec); // check for value set here
  cmp_imm(0x4); // if enemy state < $02, do not change to shell, if
  if (!zero_flag) { goto CheckForHammerBro; } // enemy state => $02 but not = $04, leave shell upside-down
  ldx_imm(0x72); // set right-side up buzzy beetle shell by default
  inc_zp(0x2); // increment saved vertical position by one pixel
  ldy_zp(0xef);
  cpy_imm(BuzzyBeetle); // check for buzzy beetle object
  if (zero_flag) { goto CheckForDefdGoomba; } // branch if found
  ldx_imm(0x66); // change to right-side up koopa shell if not found
  inc_zp(0x2); // and increment saved vertical position again
  
CheckForDefdGoomba:
  cpy_imm(Goomba); // check for goomba object (necessary if previously
  if (!zero_flag) { goto CheckForHammerBro; } // failed buzzy beetle object test)
  ldx_imm(0x54); // load for regular goomba
  lda_zp(0xed); // note that this only gets performed if enemy state => $02
  and_imm(0b00100000); // check saved enemy state for d5 set
  if (!zero_flag) { goto CheckForHammerBro; } // branch if set
  ldx_imm(0x8a); // load offset for defeated goomba
  dec_zp(0x2); // set different value and decrement saved vertical position
  
CheckForHammerBro:
  ldy_zp(ObjectOffset);
  lda_zp(0xef); // check for hammer bro object
  cmp_imm(HammerBro);
  if (!zero_flag) { goto CheckForBloober; } // branch if not found
  lda_zp(0xed);
  if (zero_flag) { goto CheckToAnimateEnemy; } // branch if not in normal enemy state
  and_imm(0b00001000);
  if (zero_flag) { goto CheckDefeatedState; } // if d3 not set, branch further away
  ldx_imm(0xb4); // otherwise load offset for different frame
  if (!zero_flag) { goto CheckToAnimateEnemy; } // unconditional branch
  
CheckForBloober:
  cpx_imm(0x48); // check for cheep-cheep offset loaded
  if (zero_flag) { goto CheckToAnimateEnemy; } // branch if found
  lda_absy(EnemyIntervalTimer);
  cmp_imm(0x5);
  if (carry_flag) { goto CheckDefeatedState; } // branch if some timer is above a certain point
  cpx_imm(0x3c); // check for bloober offset loaded
  if (!zero_flag) { goto CheckToAnimateEnemy; } // branch if not found this time
  cmp_imm(0x1);
  if (zero_flag) { goto CheckDefeatedState; } // branch if timer is set to certain point
  inc_zp(0x2); // increment saved vertical coordinate three pixels
  inc_zp(0x2);
  inc_zp(0x2);
  goto CheckAnimationStop; // and do something else
  
CheckToAnimateEnemy:
  lda_zp(0xef); // check for specific enemy objects
  cmp_imm(Goomba);
  if (zero_flag) { goto CheckDefeatedState; } // branch if goomba
  cmp_imm(0x8);
  if (zero_flag) { goto CheckDefeatedState; } // branch if bullet bill (note both variants use $08 here)
  cmp_imm(Podoboo);
  if (zero_flag) { goto CheckDefeatedState; } // branch if podoboo
  cmp_imm(0x18); // branch if => $18
  if (carry_flag) { goto CheckDefeatedState; }
  ldy_imm(0x0);
  cmp_imm(0x15); // check for mushroom retainer/princess object
  if (!zero_flag) { goto CheckForSecondFrame; } // which uses different code here, branch if not found
  iny(); // residual instruction
  lda_abs(WorldNumber); // are we on world 8?
  cmp_imm(World8);
  if (carry_flag) { goto CheckDefeatedState; } // if so, leave the offset alone (use princess)
  ldx_imm(0xa2); // otherwise, set for mushroom retainer object instead
  lda_imm(0x3); // set alternate state here
  ram[0xec] = a;
  if (!zero_flag) { goto CheckDefeatedState; } // unconditional branch
  
CheckForSecondFrame:
  lda_zp(FrameCounter); // load frame counter
  and_absy(EnemyAnimTimingBMask); // mask it (partly residual, one byte not ever used)
  if (!zero_flag) { goto CheckDefeatedState; } // branch if timing is off
  
CheckAnimationStop:
  lda_zp(0xed); // check saved enemy state
  and_imm(0b10100000); // for d7 or d5, or check for timers stopped
  ora_abs(TimerControl);
  if (!zero_flag) { goto CheckDefeatedState; } // if either condition true, branch
  txa();
  carry_flag = false;
  adc_imm(0x6); // add $06 to current enemy offset
  tax(); // to animate various enemy objects
  
CheckDefeatedState:
  lda_zp(0xed); // check saved enemy state
  and_imm(0b00100000); // for d5 set
  if (zero_flag) { goto DrawEnemyObject; } // branch if not set
  lda_zp(0xef);
  cmp_imm(0x4); // check for saved enemy object => $04
  if (!carry_flag) { goto DrawEnemyObject; } // branch if less
  ldy_imm(0x1);
  ram[VerticalFlipFlag] = y; // set vertical flip flag
  dey();
  ram[0xec] = y; // init saved value here
  
DrawEnemyObject:
  ldy_zp(0xeb); // load sprite data offset
  DrawEnemyObjRow(); // draw six tiles of data
  DrawEnemyObjRow(); // into sprite data
  DrawEnemyObjRow();
  ldx_zp(ObjectOffset); // get enemy object offset
  ldy_absx(Enemy_SprDataOffset); // get sprite data offset
  lda_zp(0xef);
  cmp_imm(0x8); // get saved enemy object and check
  if (!zero_flag) { goto CheckForVerticalFlip; } // for bullet bill, branch if not found
  
SkipToOffScrChk:
  SprObjectOffscrChk(); // jump if found
  return;
  
CheckForVerticalFlip:
  lda_abs(VerticalFlipFlag); // check if vertical flip flag is set here
  if (zero_flag) { goto CheckForESymmetry; } // branch if not
  lda_absy(Sprite_Attributes); // get attributes of first sprite we dealt with
  ora_imm(0b10000000); // set bit for vertical flip
  iny();
  iny(); // increment two bytes so that we store the vertical flip
  DumpSixSpr(); // in attribute bytes of enemy obj sprite data
  dey();
  dey(); // now go back to the Y coordinate offset
  tya();
  tax(); // give offset to X
  lda_zp(0xef);
  cmp_imm(HammerBro); // check saved enemy object for hammer bro
  if (zero_flag) { goto FlipEnemyVertically; }
  cmp_imm(Lakitu); // check saved enemy object for lakitu
  if (zero_flag) { goto FlipEnemyVertically; } // branch for hammer bro or lakitu
  cmp_imm(0x15);
  if (carry_flag) { goto FlipEnemyVertically; } // also branch if enemy object => $15
  txa();
  carry_flag = false;
  adc_imm(0x8); // if not selected objects or => $15, set
  tax(); // offset in X for next row
  
FlipEnemyVertically:
  lda_absx(Sprite_Tilenumber); // load first or second row tiles
  pha(); // and save tiles to the stack
  lda_absx(Sprite_Tilenumber + 4);
  pha();
  lda_absy(Sprite_Tilenumber + 16); // exchange third row tiles
  ram[Sprite_Tilenumber + x] = a; // with first or second row tiles
  lda_absy(Sprite_Tilenumber + 20);
  ram[Sprite_Tilenumber + 4 + x] = a;
  pla(); // pull first or second row tiles from stack
  ram[Sprite_Tilenumber + 20 + y] = a; // and save in third row
  pla();
  ram[Sprite_Tilenumber + 16 + y] = a;
  
CheckForESymmetry:
  lda_abs(BowserGfxFlag); // are we drawing bowser at all?
  if (!zero_flag) { goto SkipToOffScrChk; } // branch if so
  lda_zp(0xef);
  ldx_zp(0xec); // get alternate enemy state
  cmp_imm(0x5); // check for hammer bro object
  if (!zero_flag) { goto ContES; }
  SprObjectOffscrChk(); // jump if found
  return;
  
ContES:
  cmp_imm(Bloober); // check for bloober object
  if (zero_flag) { goto MirrorEnemyGfx; }
  cmp_imm(PiranhaPlant); // check for piranha plant object
  if (zero_flag) { goto MirrorEnemyGfx; }
  cmp_imm(Podoboo); // check for podoboo object
  if (zero_flag) { goto MirrorEnemyGfx; } // branch if either of three are found
  cmp_imm(Spiny); // check for spiny object
  if (!zero_flag) { goto ESRtnr; } // branch closer if not found
  cpx_imm(0x5); // check spiny's state
  if (!zero_flag) { goto CheckToMirrorLakitu; } // branch if not an egg, otherwise
  
ESRtnr:
  cmp_imm(0x15); // check for princess/mushroom retainer object
  if (!zero_flag) { goto SpnySC; }
  lda_imm(0x42); // set horizontal flip on bottom right sprite
  ram[Sprite_Attributes + 20 + y] = a; // note that palette bits were already set earlier
  
SpnySC:
  cpx_imm(0x2); // if alternate enemy state set to 1 or 0, branch
  if (!carry_flag) { goto CheckToMirrorLakitu; }
  
MirrorEnemyGfx:
  lda_abs(BowserGfxFlag); // if enemy object is bowser, skip all of this
  if (!zero_flag) { goto CheckToMirrorLakitu; }
  lda_absy(Sprite_Attributes); // load attribute bits of first sprite
  and_imm(0b10100011);
  ram[Sprite_Attributes + y] = a; // save vertical flip, priority, and palette bits
  ram[Sprite_Attributes + 8 + y] = a; // in left sprite column of enemy object OAM data
  ram[Sprite_Attributes + 16 + y] = a;
  ora_imm(0b01000000); // set horizontal flip
  cpx_imm(0x5); // check for state used by spiny's egg
  if (!zero_flag) { goto EggExc; } // if alternate state not set to $05, branch
  ora_imm(0b10000000); // otherwise set vertical flip
  
EggExc:
  ram[Sprite_Attributes + 4 + y] = a; // set bits of right sprite column
  ram[Sprite_Attributes + 12 + y] = a; // of enemy object sprite data
  ram[Sprite_Attributes + 20 + y] = a;
  cpx_imm(0x4); // check alternate enemy state
  if (!zero_flag) { goto CheckToMirrorLakitu; } // branch if not $04
  lda_absy(Sprite_Attributes + 8); // get second row left sprite attributes
  ora_imm(0b10000000);
  ram[Sprite_Attributes + 8 + y] = a; // store bits with vertical flip in
  ram[Sprite_Attributes + 16 + y] = a; // second and third row left sprites
  ora_imm(0b01000000);
  ram[Sprite_Attributes + 12 + y] = a; // store with horizontal and vertical flip in
  ram[Sprite_Attributes + 20 + y] = a; // second and third row right sprites
  
CheckToMirrorLakitu:
  lda_zp(0xef); // check for lakitu enemy object
  cmp_imm(Lakitu);
  if (!zero_flag) { goto CheckToMirrorJSpring; } // branch if not found
  lda_abs(VerticalFlipFlag);
  if (!zero_flag) { goto NVFLak; } // branch if vertical flip flag not set
  lda_absy(Sprite_Attributes + 16); // save vertical flip and palette bits
  and_imm(0b10000001); // in third row left sprite
  ram[Sprite_Attributes + 16 + y] = a;
  lda_absy(Sprite_Attributes + 20); // set horizontal flip and palette bits
  ora_imm(0b01000001); // in third row right sprite
  ram[Sprite_Attributes + 20 + y] = a;
  ldx_abs(FrenzyEnemyTimer); // check timer
  cpx_imm(0x10);
  if (carry_flag) { SprObjectOffscrChk(); return; } // branch if timer has not reached a certain range
  ram[Sprite_Attributes + 12 + y] = a; // otherwise set same for second row right sprite
  and_imm(0b10000001);
  ram[Sprite_Attributes + 8 + y] = a; // preserve vertical flip and palette bits for left sprite
  if (!carry_flag) { SprObjectOffscrChk(); return; } // unconditional branch
  
NVFLak:
  lda_absy(Sprite_Attributes); // get first row left sprite attributes
  and_imm(0b10000001);
  ram[Sprite_Attributes + y] = a; // save vertical flip and palette bits
  lda_absy(Sprite_Attributes + 4); // get first row right sprite attributes
  ora_imm(0b01000001); // set horizontal flip and palette bits
  ram[Sprite_Attributes + 4 + y] = a; // note that vertical flip is left as-is
  
CheckToMirrorJSpring:
  lda_zp(0xef); // check for jumpspring object (any frame)
  cmp_imm(0x18);
  if (!carry_flag) { SprObjectOffscrChk(); return; } // branch if not jumpspring object at all
  lda_imm(0x82);
  ram[Sprite_Attributes + 8 + y] = a; // set vertical flip and palette bits of
  ram[Sprite_Attributes + 16 + y] = a; // second and third row left sprites
  ora_imm(0b01000000);
  ram[Sprite_Attributes + 12 + y] = a; // set, in addition to those, horizontal flip
  ram[Sprite_Attributes + 20 + y] = a; // for second and third row right sprites
  SprObjectOffscrChk(); // <fallthrough>
}

void SprObjectOffscrChk(void) {
  ldx_zp(ObjectOffset); // get enemy buffer offset
  lda_abs(Enemy_OffscreenBits); // check offscreen information
  lsr_acc();
  lsr_acc(); // shift three times to the right
  lsr_acc(); // which puts d2 into carry
  pha(); // save to stack
  if (!carry_flag) { goto LcChk; } // branch if not set
  lda_imm(0x4); // set for right column sprites
  MoveESprColOffscreen(); // and move them offscreen
  
LcChk:
  pla(); // get from stack
  lsr_acc(); // move d3 to carry
  pha(); // save to stack
  if (!carry_flag) { goto Row3C; } // branch if not set
  lda_imm(0x0); // set for left column sprites,
  MoveESprColOffscreen(); // move them offscreen
  
Row3C:
  pla(); // get from stack again
  lsr_acc(); // move d5 to carry this time
  lsr_acc();
  pha(); // save to stack again
  if (!carry_flag) { goto Row23C; } // branch if carry not set
  lda_imm(0x10); // set for third row of sprites
  MoveESprRowOffscreen(); // and move them offscreen
  
Row23C:
  pla(); // get from stack
  lsr_acc(); // move d6 into carry
  pha(); // save to stack
  if (!carry_flag) { goto AllRowC; }
  lda_imm(0x8); // set for second and third rows
  MoveESprRowOffscreen(); // move them offscreen
  
AllRowC:
  pla(); // get from stack once more
  lsr_acc(); // move d7 into carry
  if (!carry_flag) { return; }
  MoveESprRowOffscreen(); // move all sprites offscreen (A should be 0 by now)
  lda_zpx(Enemy_ID);
  cmp_imm(Podoboo); // check enemy identifier for podoboo
  if (zero_flag) { return; } // skip this part if found, we do not want to erase podoboo!
  lda_zpx(Enemy_Y_HighPos); // check high byte of vertical position
  cmp_imm(0x2); // if not yet past the bottom of the screen, branch
  if (!zero_flag) { return; }
  EraseEnemyObject(); // what it says
}

void SpawnHammerObj(void) {
  lda_abs(PseudoRandomBitReg + 1); // get pseudorandom bits from
  and_imm(0b00000111); // second part of LSFR
  // if any bits are set, branch and use as offset
  if (zero_flag) {
    lda_abs(PseudoRandomBitReg + 1);
    and_imm(0b00001000); // get d3 from same part of LSFR
  }
  // SetMOfs:
  tay(); // use either d3 or d2-d0 for offset here
  lda_zpy(Misc_State); // if any values loaded in
  // $2a-$32 where offset is then leave with carry clear
  if (zero_flag) {
    ldx_absy(HammerEnemyOfsData); // get offset of enemy slot to check using Y as offset
    lda_zpx(Enemy_Flag); // check enemy buffer flag at offset
    // if buffer flag set, branch to leave with carry clear
    if (zero_flag) {
      ldx_zp(ObjectOffset); // get original enemy object offset
      txa();
      ram[HammerEnemyOffset + y] = a; // save here
      lda_imm(0x90);
      ram[Misc_State + y] = a; // save hammer's state here
      lda_imm(0x7);
      ram[Misc_BoundBoxCtrl + y] = a; // set something else entirely, here
      carry_flag = true; // return with carry set
      return;
    }
  }
  // NoHammer:
  ldx_zp(ObjectOffset); // get original enemy object offset
  carry_flag = false; // return with carry clear
  // --------------------------------
  // $00 - used to set downward force
  // $01 - used to set upward force (residual)
  // $02 - used to set maximum speed
}

void ProcHammerObj(void) {
  lda_abs(TimerControl); // if master timer control set
  if (!zero_flag) { goto RunHSubs; } // skip all of this code and go to last subs at the end
  lda_zpx(Misc_State); // otherwise get hammer's state
  and_imm(0b01111111); // mask out d7
  ldy_absx(HammerEnemyOffset); // get enemy object offset that spawned this hammer
  cmp_imm(0x2); // check hammer's state
  if (zero_flag) { goto SetHSpd; } // if currently at 2, branch
  if (carry_flag) { goto SetHPos; } // if greater than 2, branch elsewhere
  txa();
  carry_flag = false; // add 13 bytes to use
  adc_imm(0xd); // proper misc object
  tax(); // return offset to X
  lda_imm(0x10);
  ram[0x0] = a; // set downward movement force
  lda_imm(0xf);
  ram[0x1] = a; // set upward movement force (not used)
  lda_imm(0x4);
  ram[0x2] = a; // set maximum vertical speed
  lda_imm(0x0); // set A to impose gravity on hammer
  ImposeGravity(); // do sub to impose gravity on hammer and move vertically
  MoveObjectHorizontally(); // do sub to move it horizontally
  ldx_zp(ObjectOffset); // get original misc object offset
  goto RunAllH; // branch to essential subroutines
  
SetHSpd:
  lda_imm(0xfe);
  ram[Misc_Y_Speed + x] = a; // set hammer's vertical speed
  lda_zpy(Enemy_State); // get enemy object state
  and_imm(0b11110111); // mask out d3
  ram[Enemy_State + y] = a; // store new state
  ldx_zpy(Enemy_MovingDir); // get enemy's moving direction
  dex(); // decrement to use as offset
  lda_absx(HammerXSpdData); // get proper speed to use based on moving direction
  ldx_zp(ObjectOffset); // reobtain hammer's buffer offset
  ram[Misc_X_Speed + x] = a; // set hammer's horizontal speed
  
SetHPos:
  dec_zpx(Misc_State); // decrement hammer's state
  lda_zpy(Enemy_X_Position); // get enemy's horizontal position
  carry_flag = false;
  adc_imm(0x2); // set position 2 pixels to the right
  ram[Misc_X_Position + x] = a; // store as hammer's horizontal position
  lda_zpy(Enemy_PageLoc); // get enemy's page location
  adc_imm(0x0); // add carry
  ram[Misc_PageLoc + x] = a; // store as hammer's page location
  lda_zpy(Enemy_Y_Position); // get enemy's vertical position
  carry_flag = true;
  sbc_imm(0xa); // move position 10 pixels upward
  ram[Misc_Y_Position + x] = a; // store as hammer's vertical position
  lda_imm(0x1);
  ram[Misc_Y_HighPos + x] = a; // set hammer's vertical high byte
  if (!zero_flag) { goto RunHSubs; } // unconditional branch to skip first routine
  
RunAllH:
  PlayerHammerCollision(); // handle collisions
  
RunHSubs:
  GetMiscOffscreenBits(); // get offscreen information
  RelativeMiscPosition(); // get relative coordinates
  GetMiscBoundBox(); // get bounding box coordinates
  DrawHammer(); // draw the hammer
  // -------------------------------------------------------------------------------------
  // $02 - used to store vertical high nybble offset from block buffer routine
  // $06 - used to store low byte of block buffer address
}

void CoinBlock(void) {
  FindEmptyMiscSlot(); // set offset for empty or last misc object buffer slot
  lda_zpx(Block_PageLoc); // get page location of block object
  ram[Misc_PageLoc + y] = a; // store as page location of misc object
  lda_zpx(Block_X_Position); // get horizontal coordinate of block object
  ora_imm(0x5); // add 5 pixels
  ram[Misc_X_Position + y] = a; // store as horizontal coordinate of misc object
  lda_zpx(Block_Y_Position); // get vertical coordinate of block object
  sbc_imm(0x10); // subtract 16 pixels
  ram[Misc_Y_Position + y] = a; // store as vertical coordinate of misc object
  JCoinC(); // jump to rest of code as applies to this misc object
}

void SetupJumpCoin(void) {
  FindEmptyMiscSlot(); // set offset for empty or last misc object buffer slot
  lda_absx(Block_PageLoc2); // get page location saved earlier
  ram[Misc_PageLoc + y] = a; // and save as page location for misc object
  lda_zp(0x6); // get low byte of block buffer offset
  asl_acc();
  asl_acc(); // multiply by 16 to use lower nybble
  asl_acc();
  asl_acc();
  ora_imm(0x5); // add five pixels
  ram[Misc_X_Position + y] = a; // save as horizontal coordinate for misc object
  lda_zp(0x2); // get vertical high nybble offset from earlier
  adc_imm(0x20); // add 32 pixels for the status bar
  ram[Misc_Y_Position + y] = a; // store as vertical coordinate
  JCoinC(); // <fallthrough>
}

void JCoinC(void) {
  lda_imm(0xfb);
  ram[Misc_Y_Speed + y] = a; // set vertical speed
  lda_imm(0x1);
  ram[Misc_Y_HighPos + y] = a; // set vertical high byte
  ram[Misc_State + y] = a; // set state for misc object
  ram[Square2SoundQueue] = a; // load coin grab sound
  ram[ObjectOffset] = x; // store current control bit as misc object offset
  GiveOneCoin(); // update coin tally on the screen and coin amount variable
  inc_abs(CoinTallyFor1Ups); // increment coin tally used to activate 1-up block flag
}

void FindEmptyMiscSlot(void) {
  ldy_imm(0x8); // start at end of misc objects buffer
  
FMiscLoop:
  lda_zpy(Misc_State); // get misc object state
  // branch if none found to use current offset
  if (!zero_flag) {
    dey(); // decrement offset
    cpy_imm(0x5); // do this for three slots
    if (!zero_flag) { goto FMiscLoop; } // do this until all slots are checked
    ldy_imm(0x8); // if no empty slots found, use last slot
  }
  // UseMiscS:
  ram[JumpCoinMiscOffset] = y; // store offset of misc object buffer here (residual)
  // -------------------------------------------------------------------------------------
}

void MiscObjectsCore(void) {
  ldx_imm(0x8); // set at end of misc object buffer
  
MiscLoop:
  ram[ObjectOffset] = x; // store misc object offset here
  lda_zpx(Misc_State); // check misc object state
  if (zero_flag) { goto MiscLoopBack; } // branch to check next slot
  asl_acc(); // otherwise shift d7 into carry
  if (!carry_flag) { goto ProcJumpCoin; } // if d7 not set, jumping coin, thus skip to rest of code here
  ProcHammerObj(); // otherwise go to process hammer,
  goto MiscLoopBack; // then check next slot
  // --------------------------------
  // $00 - used to set downward force
  // $01 - used to set upward force (residual)
  // $02 - used to set maximum speed
  
ProcJumpCoin:
  ldy_zpx(Misc_State); // check misc object state
  dey(); // decrement to see if it's set to 1
  if (zero_flag) { goto JCoinRun; } // if so, branch to handle jumping coin
  inc_zpx(Misc_State); // otherwise increment state to either start off or as timer
  lda_zpx(Misc_X_Position); // get horizontal coordinate for misc object
  carry_flag = false; // whether its jumping coin (state 0 only) or floatey number
  adc_abs(ScrollAmount); // add current scroll speed
  ram[Misc_X_Position + x] = a; // store as new horizontal coordinate
  lda_zpx(Misc_PageLoc); // get page location
  adc_imm(0x0); // add carry
  ram[Misc_PageLoc + x] = a; // store as new page location
  lda_zpx(Misc_State);
  cmp_imm(0x30); // check state of object for preset value
  if (!zero_flag) { goto RunJCSubs; } // if not yet reached, branch to subroutines
  lda_imm(0x0);
  ram[Misc_State + x] = a; // otherwise nullify object state
  goto MiscLoopBack; // and move onto next slot
  
JCoinRun:
  txa();
  carry_flag = false; // add 13 bytes to offset for next subroutine
  adc_imm(0xd);
  tax();
  lda_imm(0x50); // set downward movement amount
  ram[0x0] = a;
  lda_imm(0x6); // set maximum vertical speed
  ram[0x2] = a;
  lsr_acc(); // divide by 2 and set
  ram[0x1] = a; // as upward movement amount (apparently residual)
  lda_imm(0x0); // set A to impose gravity on jumping coin
  ImposeGravity(); // do sub to move coin vertically and impose gravity on it
  ldx_zp(ObjectOffset); // get original misc object offset
  lda_zpx(Misc_Y_Speed); // check vertical speed
  cmp_imm(0x5);
  if (!zero_flag) { goto RunJCSubs; } // if not moving downward fast enough, keep state as-is
  inc_zpx(Misc_State); // otherwise increment state to change to floatey number
  
RunJCSubs:
  RelativeMiscPosition(); // get relative coordinates
  GetMiscOffscreenBits(); // get offscreen information
  GetMiscBoundBox(); // get bounding box coordinates (why?)
  JCoinGfxHandler(); // draw the coin or floatey number
  
MiscLoopBack:
  dex(); // decrement misc object offset
  if (!neg_flag) { goto MiscLoop; } // loop back until all misc objects handled
  // -------------------------------------------------------------------------------------
}

void GiveOneCoin(void) {
  lda_imm(0x1); // set digit modifier to add 1 coin
  ram[DigitModifier + 5] = a; // to the current player's coin tally
  ldx_abs(CurrentPlayer); // get current player on the screen
  ldy_absx(CoinTallyOffsets); // get offset for player's coin tally
  DigitsMathRoutine(); // update the coin tally
  inc_abs(CoinTally); // increment onscreen player's coin amount
  lda_abs(CoinTally);
  cmp_imm(100); // does player have 100 coins yet?
  // if not, skip all of this
  if (zero_flag) {
    lda_imm(0x0);
    ram[CoinTally] = a; // otherwise, reinitialize coin amount
    inc_abs(NumberofLives); // give the player an extra life
    lda_imm(Sfx_ExtraLife);
    ram[Square2SoundQueue] = a; // play 1-up sound
  }
  // CoinPoints:
  lda_imm(0x2); // set digit modifier to award
  ram[DigitModifier + 4] = a; // 200 points to the player
  AddToScore(); // <fallthrough>
}

void AddToScore(void) {
  ldx_abs(CurrentPlayer); // get current player
  ldy_absx(ScoreOffsets); // get offset for player's score
  DigitsMathRoutine(); // update the score internally with value in digit modifier
  GetSBNybbles(); // <fallthrough>
}

void GetSBNybbles(void) {
  ldy_abs(CurrentPlayer); // get current player
  lda_absy(StatusBarNybbles); // get nybbles based on player, use to update score and coins
  UpdateNumber(); // <fallthrough>
}

void UpdateNumber(void) {
  PrintStatusBarNumbers(); // print status bar numbers based on nybbles, whatever they be
  ldy_abs(VRAM_Buffer1_Offset);
  lda_absy(VRAM_Buffer1 - 6); // check highest digit of score
  // if zero, overwrite with space tile for zero suppression
  if (zero_flag) {
    lda_imm(0x24);
    ram[VRAM_Buffer1 - 6 + y] = a;
  }
  // NoZSup:
  ldx_zp(ObjectOffset); // get enemy object buffer offset
  // -------------------------------------------------------------------------------------
}

void SetupPowerUp(void) {
  lda_imm(PowerUpObject); // load power-up identifier into
  ram[Enemy_ID + 5] = a; // special use slot of enemy object buffer
  lda_zpx(Block_PageLoc); // store page location of block object
  ram[Enemy_PageLoc + 5] = a; // as page location of power-up object
  lda_zpx(Block_X_Position); // store horizontal coordinate of block object
  ram[Enemy_X_Position + 5] = a; // as horizontal coordinate of power-up object
  lda_imm(0x1);
  ram[Enemy_Y_HighPos + 5] = a; // set vertical high byte of power-up object
  lda_zpx(Block_Y_Position); // get vertical coordinate of block object
  carry_flag = true;
  sbc_imm(0x8); // subtract 8 pixels
  ram[Enemy_Y_Position + 5] = a; // and use as vertical coordinate of power-up object
  PwrUpJmp(); // <fallthrough>
}

void PwrUpJmp(void) {
  lda_imm(0x1); // this is a residual jump point in enemy object jump table
  ram[Enemy_State + 5] = a; // set power-up object's state
  ram[Enemy_Flag + 5] = a; // set buffer flag
  lda_imm(0x3);
  ram[Enemy_BoundBoxCtrl + 5] = a; // set bounding box size control for power-up object
  lda_zp(PowerUpType);
  cmp_imm(0x2); // check currently loaded power-up type
  // if star or 1-up, branch ahead
  if (!carry_flag) {
    lda_abs(PlayerStatus); // otherwise check player's current status
    cmp_imm(0x2);
    // if player not fiery, use status as power-up type
    if (carry_flag) {
      lsr_acc(); // otherwise shift right to force fire flower type
    }
    // StrType:
    ram[PowerUpType] = a; // store type here
  }
  // PutBehind:
  lda_imm(0b00100000);
  ram[Enemy_SprAttrib + 5] = a; // set background priority bit
  lda_imm(Sfx_GrowPowerUp);
  ram[Square2SoundQueue] = a; // load power-up reveal sound and leave
  // -------------------------------------------------------------------------------------
}

void PowerUpObjHandler(void) {
  ldx_imm(0x5); // set object offset for last slot in enemy object buffer
  ram[ObjectOffset] = x;
  lda_zp(Enemy_State + 5); // check power-up object's state
  if (zero_flag) { return; } // if not set, branch to leave
  asl_acc(); // shift to check if d7 was set in object state
  if (!carry_flag) { goto GrowThePowerUp; } // if not set, branch ahead to skip this part
  lda_abs(TimerControl); // if master timer control set,
  if (!zero_flag) { goto RunPUSubs; } // branch ahead to enemy object routines
  lda_zp(PowerUpType); // check power-up type
  if (zero_flag) { goto ShroomM; } // if normal mushroom, branch ahead to move it
  cmp_imm(0x3);
  if (zero_flag) { goto ShroomM; } // if 1-up mushroom, branch ahead to move it
  cmp_imm(0x2);
  if (!zero_flag) { goto RunPUSubs; } // if not star, branch elsewhere to skip movement
  MoveJumpingEnemy(); // otherwise impose gravity on star power-up and make it jump
  EnemyJump(); // note that green paratroopa shares the same code here
  goto RunPUSubs; // then jump to other power-up subroutines
  
ShroomM:
  MoveNormalEnemy(); // do sub to make mushrooms move
  EnemyToBGCollisionDet(); // deal with collisions
  goto RunPUSubs; // run the other subroutines
  
GrowThePowerUp:
  lda_zp(FrameCounter); // get frame counter
  and_imm(0x3); // mask out all but 2 LSB
  if (!zero_flag) { goto ChkPUSte; } // if any bits set here, branch
  dec_zp(Enemy_Y_Position + 5); // otherwise decrement vertical coordinate slowly
  lda_zp(Enemy_State + 5); // load power-up object state
  inc_zp(Enemy_State + 5); // increment state for next frame (to make power-up rise)
  cmp_imm(0x11); // if power-up object state not yet past 16th pixel,
  if (!carry_flag) { goto ChkPUSte; } // branch ahead to last part here
  lda_imm(0x10);
  ram[Enemy_X_Speed + x] = a; // otherwise set horizontal speed
  lda_imm(0b10000000);
  ram[Enemy_State + 5] = a; // and then set d7 in power-up object's state
  asl_acc(); // shift once to init A
  ram[Enemy_SprAttrib + 5] = a; // initialize background priority bit set here
  rol_acc(); // rotate A to set right moving direction
  ram[Enemy_MovingDir + x] = a; // set moving direction
  
ChkPUSte:
  lda_zp(Enemy_State + 5); // check power-up object's state
  cmp_imm(0x6); // for if power-up has risen enough
  if (!carry_flag) { return; } // if not, don't even bother running these routines
  
RunPUSubs:
  RelativeEnemyPosition(); // get coordinates relative to screen
  GetEnemyOffscreenBits(); // get offscreen bits
  GetEnemyBoundBox(); // get bounding box coordinates
  DrawPowerUp(); // draw the power-up object
  PlayerEnemyCollision(); // check for collision with player
  OffscreenBoundsCheck(); // check to see if it went offscreen
  // -------------------------------------------------------------------------------------
  // These apply to all routines in this section unless otherwise noted:
  // $00 - used to store metatile from block buffer routine
  // $02 - used to store vertical high nybble offset from block buffer routine
  // $05 - used to store metatile stored in A at beginning of PlayerHeadCollision
  // $06-$07 - used as block buffer address indirect
}

void PlayerHeadCollision(void) {
  pha(); // store metatile number to stack
  lda_imm(0x11); // load unbreakable block object state by default
  ldx_abs(SprDataOffset_Ctrl); // load offset control bit here
  ldy_abs(PlayerSize); // check player's size
  if (!zero_flag) { goto DBlockSte; } // if small, branch
  lda_imm(0x12); // otherwise load breakable block object state
  
DBlockSte:
  ram[Block_State + x] = a; // store into block object buffer
  DestroyBlockMetatile(); // store blank metatile in vram buffer to write to name table
  ldx_abs(SprDataOffset_Ctrl); // load offset control bit
  lda_zp(0x2); // get vertical high nybble offset used in block buffer routine
  ram[Block_Orig_YPos + x] = a; // set as vertical coordinate for block object
  tay();
  lda_zp(0x6); // get low byte of block buffer address used in same routine
  ram[Block_BBuf_Low + x] = a; // save as offset here to be used later
  lda_indy(0x6); // get contents of block buffer at old address at $06, $07
  BlockBumpedChk(); // do a sub to check which block player bumped head on
  ram[0x0] = a; // store metatile here
  ldy_abs(PlayerSize); // check player's size
  if (!zero_flag) { goto ChkBrick; } // if small, use metatile itself as contents of A
  tya(); // otherwise init A (note: big = 0)
  
ChkBrick:
  if (!carry_flag) { goto PutMTileB; } // if no match was found in previous sub, skip ahead
  ldy_imm(0x11); // otherwise load unbreakable state into block object buffer
  ram[Block_State + x] = y; // note this applies to both player sizes
  lda_imm(0xc4); // load empty block metatile into A for now
  ldy_zp(0x0); // get metatile from before
  cpy_imm(0x58); // is it brick with coins (with line)?
  if (zero_flag) { goto StartBTmr; } // if so, branch
  cpy_imm(0x5d); // is it brick with coins (without line)?
  if (!zero_flag) { goto PutMTileB; } // if not, branch ahead to store empty block metatile
  
StartBTmr:
  lda_abs(BrickCoinTimerFlag); // check brick coin timer flag
  if (!zero_flag) { goto ContBTmr; } // if set, timer expired or counting down, thus branch
  lda_imm(0xb);
  ram[BrickCoinTimer] = a; // if not set, set brick coin timer
  inc_abs(BrickCoinTimerFlag); // and set flag linked to it
  
ContBTmr:
  lda_abs(BrickCoinTimer); // check brick coin timer
  if (!zero_flag) { goto PutOldMT; } // if not yet expired, branch to use current metatile
  ldy_imm(0xc4); // otherwise use empty block metatile
  
PutOldMT:
  tya(); // put metatile into A
  
PutMTileB:
  ram[Block_Metatile + x] = a; // store whatever metatile be appropriate here
  InitBlock_XY_Pos(); // get block object horizontal coordinates saved
  ldy_zp(0x2); // get vertical high nybble offset
  lda_imm(0x23);
  dynamic_ram_write(read_word(0x6) + y, a); // write blank metatile $23 to block buffer
  lda_imm(0x10);
  ram[BlockBounceTimer] = a; // set block bounce timer
  pla(); // pull original metatile from stack
  ram[0x5] = a; // and save here
  ldy_imm(0x0); // set default offset
  lda_abs(CrouchingFlag); // is player crouching?
  if (!zero_flag) { goto SmallBP; } // if so, branch to increment offset
  lda_abs(PlayerSize); // is player big?
  if (zero_flag) { goto BigBP; } // if so, branch to use default offset
  
SmallBP:
  iny(); // increment for small or big and crouching
  
BigBP:
  lda_zp(Player_Y_Position); // get player's vertical coordinate
  carry_flag = false;
  adc_absy(BlockYPosAdderData); // add value determined by size
  and_imm(0xf0); // mask out low nybble to get 16-pixel correspondence
  ram[Block_Y_Position + x] = a; // save as vertical coordinate for block object
  ldy_zpx(Block_State); // get block object state
  cpy_imm(0x11);
  if (zero_flag) { goto Unbreak; } // if set to value loaded for unbreakable, branch
  BrickShatter(); // execute code for breakable brick
  goto InvOBit; // skip subroutine to do last part of code here
  
Unbreak:
  BumpBlock(); // execute code for unbreakable brick or question block
  
InvOBit:
  lda_abs(SprDataOffset_Ctrl); // invert control bit used by block objects
  eor_imm(0x1); // and floatey numbers
  ram[SprDataOffset_Ctrl] = a;
  // --------------------------------
}

void InitBlock_XY_Pos(void) {
  lda_zp(Player_X_Position); // get player's horizontal coordinate
  carry_flag = false;
  adc_imm(0x8); // add eight pixels
  and_imm(0xf0); // mask out low nybble to give 16-pixel correspondence
  ram[Block_X_Position + x] = a; // save as horizontal coordinate for block object
  lda_zp(Player_PageLoc);
  adc_imm(0x0); // add carry to page location of player
  ram[Block_PageLoc + x] = a; // save as page location of block object
  ram[Block_PageLoc2 + x] = a; // save elsewhere to be used later
  lda_zp(Player_Y_HighPos);
  ram[Block_Y_HighPos + x] = a; // save vertical high byte of player into
  // --------------------------------
}

void BumpBlock(void) {
  CheckTopOfBlock(); // check to see if there's a coin directly above this block
  lda_imm(Sfx_Bump);
  ram[Square1SoundQueue] = a; // play bump sound
  lda_imm(0x0);
  ram[Block_X_Speed + x] = a; // initialize horizontal speed for block object
  ram[Block_Y_MoveForce + x] = a; // init fractional movement force
  ram[Player_Y_Speed] = a; // init player's vertical speed
  lda_imm(0xfe);
  ram[Block_Y_Speed + x] = a; // set vertical speed for block object
  lda_zp(0x5); // get original metatile from stack
  BlockBumpedChk(); // do a sub to check which block player bumped head on
  if (carry_flag) {
    tya(); // move block number to A
    cmp_imm(0x9); // if block number was within 0-8 range,
    // branch to use current number
    if (carry_flag) {
      sbc_imm(0x5); // otherwise subtract 5 for second set to get proper number
    }
    // BlockCode:
    carry_flag = (a & 0x80u) != 0;
    switch (a) {
      case 0: MushFlowerBlock(); return;
      case 1: CoinBlock(); return;
      case 2: CoinBlock(); return;
      case 3: ExtraLifeMushBlock(); return;
      case 4: MushFlowerBlock(); return;
      case 5: VineBlock(); return;
      case 6: StarBlock(); return;
      case 7: CoinBlock(); return;
      case 8: ExtraLifeMushBlock(); return;
    }
  }
}

void MushFlowerBlock(void) {
  lda_imm(0x0); // load mushroom/fire flower into power-up type
  ExtraLifeMushBlockSkip(); //  .db $2c ;BIT instruction opcode
}

void StarBlock(void) {
  lda_imm(0x2); // load star into power-up type
  ExtraLifeMushBlockSkip(); //  .db $2c ;BIT instruction opcode
}

void ExtraLifeMushBlock(void) {
  lda_imm(0x3); // load 1-up mushroom into power-up type
  ExtraLifeMushBlockSkip(); // <fallthrough>
}

void ExtraLifeMushBlockSkip(void) {
  ram[0x39] = a; // store correct power-up type
  SetupPowerUp();
}

void VineBlock(void) {
  ldx_imm(0x5); // load last slot for enemy object buffer
  ldy_abs(SprDataOffset_Ctrl); // get control bit
  Setup_Vine(); // set up vine object
  // --------------------------------
}

void BlockBumpedChk(void) {
  ldy_imm(0xd); // start at end of metatile data
  
BumpChkLoop:
  cmp_absy(BrickQBlockMetatiles); // check to see if current metatile matches
  if (!zero_flag) {
    dey(); // otherwise move onto next metatile
    if (!neg_flag) { goto BumpChkLoop; } // do this until all metatiles are checked
    carry_flag = false; // if none match, return with carry clear
    // --------------------------------
  }
}

void BrickShatter(void) {
  CheckTopOfBlock(); // check to see if there's a coin directly above this block
  lda_imm(Sfx_BrickShatter);
  ram[Block_RepFlag + x] = a; // set flag for block object to immediately replace metatile
  ram[NoiseSoundQueue] = a; // load brick shatter sound
  SpawnBrickChunks(); // create brick chunk objects
  lda_imm(0xfe);
  ram[Player_Y_Speed] = a; // set vertical speed for player
  lda_imm(0x5);
  ram[DigitModifier + 5] = a; // set digit modifier to give player 50 points
  AddToScore(); // do sub to update the score
  ldx_abs(SprDataOffset_Ctrl); // load control bit and leave
  // --------------------------------
}

void CheckTopOfBlock(void) {
  ldx_abs(SprDataOffset_Ctrl); // load control bit
  ldy_zp(0x2); // get vertical high nybble offset used in block buffer
  if (zero_flag) { return; } // branch to leave if set to zero, because we're at the top
  tya(); // otherwise set to A
  carry_flag = true;
  sbc_imm(0x10); // subtract $10 to move up one row in the block buffer
  ram[0x2] = a; // store as new vertical high nybble offset
  tay();
  lda_indy(0x6); // get contents of block buffer in same column, one row up
  cmp_imm(0xc2); // is it a coin? (not underwater)
  if (!zero_flag) { return; } // if not, branch to leave
  lda_imm(0x0);
  dynamic_ram_write(read_word(0x6) + y, a); // otherwise put blank metatile where coin was
  RemoveCoin_Axe(); // write blank metatile to vram buffer
  ldx_abs(SprDataOffset_Ctrl); // get control bit
  SetupJumpCoin(); // create jumping coin object and update coin variables
  // --------------------------------
}

void SpawnBrickChunks(void) {
  lda_zpx(Block_X_Position); // set horizontal coordinate of block object
  ram[Block_Orig_XPos + x] = a; // as original horizontal coordinate here
  lda_imm(0xf0);
  ram[Block_X_Speed + x] = a; // set horizontal speed for brick chunk objects
  ram[Block_X_Speed + 2 + x] = a;
  lda_imm(0xfa);
  ram[Block_Y_Speed + x] = a; // set vertical speed for one
  lda_imm(0xfc);
  ram[Block_Y_Speed + 2 + x] = a; // set lower vertical speed for the other
  lda_imm(0x0);
  ram[Block_Y_MoveForce + x] = a; // init fractional movement force for both
  ram[Block_Y_MoveForce + 2 + x] = a;
  lda_zpx(Block_PageLoc);
  ram[Block_PageLoc + 2 + x] = a; // copy page location
  lda_zpx(Block_X_Position);
  ram[Block_X_Position + 2 + x] = a; // copy horizontal coordinate
  lda_zpx(Block_Y_Position);
  carry_flag = false; // add 8 pixels to vertical coordinate
  adc_imm(0x8); // and save as vertical coordinate for one of them
  ram[Block_Y_Position + 2 + x] = a;
  lda_imm(0xfa);
  ram[Block_Y_Speed + x] = a; // set vertical speed...again??? (redundant)
  // -------------------------------------------------------------------------------------
}

void BlockObjectsCore(void) {
  lda_zpx(Block_State); // get state of block object
  if (zero_flag) { goto UpdSte; } // if not set, branch to leave
  and_imm(0xf); // mask out high nybble
  pha(); // push to stack
  tay(); // put in Y for now
  txa();
  carry_flag = false;
  adc_imm(0x9); // add 9 bytes to offset (note two block objects are created
  tax(); // when using brick chunks, but only one offset for both)
  dey(); // decrement Y to check for solid block state
  if (zero_flag) { goto BouncingBlockHandler; } // branch if found, otherwise continue for brick chunks
  ImposeGravityBlock(); // do sub to impose gravity on one block object object
  MoveObjectHorizontally(); // do another sub to move horizontally
  txa();
  carry_flag = false; // move onto next block object
  adc_imm(0x2);
  tax();
  ImposeGravityBlock(); // do sub to impose gravity on other block object
  MoveObjectHorizontally(); // do another sub to move horizontally
  ldx_zp(ObjectOffset); // get block object offset used for both
  RelativeBlockPosition(); // get relative coordinates
  GetBlockOffscreenBits(); // get offscreen information
  DrawBrickChunks(); // draw the brick chunks
  pla(); // get lower nybble of saved state
  ldy_zpx(Block_Y_HighPos); // check vertical high byte of block object
  if (zero_flag) { goto UpdSte; } // if above the screen, branch to kill it
  pha(); // otherwise save state back into stack
  lda_imm(0xf0);
  cmp_zpx(Block_Y_Position + 2); // check to see if bottom block object went
  if (carry_flag) { goto ChkTop; } // to the bottom of the screen, and branch if not
  ram[Block_Y_Position + 2 + x] = a; // otherwise set offscreen coordinate
  
ChkTop:
  lda_zpx(Block_Y_Position); // get top block object's vertical coordinate
  cmp_imm(0xf0); // see if it went to the bottom of the screen
  pla(); // pull block object state from stack
  if (!carry_flag) { goto UpdSte; } // if not, branch to save state
  if (carry_flag) { goto KillBlock; } // otherwise do unconditional branch to kill it
  
BouncingBlockHandler:
  ImposeGravityBlock(); // do sub to impose gravity on block object
  ldx_zp(ObjectOffset); // get block object offset
  RelativeBlockPosition(); // get relative coordinates
  GetBlockOffscreenBits(); // get offscreen information
  DrawBlock(); // draw the block
  lda_zpx(Block_Y_Position); // get vertical coordinate
  and_imm(0xf); // mask out high nybble
  cmp_imm(0x5); // check to see if low nybble wrapped around
  pla(); // pull state from stack
  if (carry_flag) { goto UpdSte; } // if still above amount, not time to kill block yet, thus branch
  lda_imm(0x1);
  ram[Block_RepFlag + x] = a; // otherwise set flag to replace metatile
  
KillBlock:
  lda_imm(0x0); // if branched here, nullify object state
  
UpdSte:
  ram[Block_State + x] = a; // store contents of A in block object state
  // -------------------------------------------------------------------------------------
  // $02 - used to store offset to block buffer
  // $06-$07 - used to store block buffer address
}

void BlockObjMT_Updater(void) {
  ldx_imm(0x1); // set offset to start with second block object
  
UpdateLoop:
  ram[ObjectOffset] = x; // set offset here
  lda_abs(VRAM_Buffer1); // if vram buffer already being used here,
  // branch to move onto next block object
  if (zero_flag) {
    lda_absx(Block_RepFlag); // if flag for block object already clear,
    // branch to move onto next block object
    if (!zero_flag) {
      lda_absx(Block_BBuf_Low); // get low byte of block buffer
      ram[0x6] = a; // store into block buffer address
      lda_imm(0x5);
      ram[0x7] = a; // set high byte of block buffer address
      lda_absx(Block_Orig_YPos); // get original vertical coordinate of block object
      ram[0x2] = a; // store here and use as offset to block buffer
      tay();
      lda_absx(Block_Metatile); // get metatile to be written
      dynamic_ram_write(read_word(0x6) + y, a); // write it to the block buffer
      ReplaceBlockMetatile(); // do sub to replace metatile where block object is
      lda_imm(0x0);
      ram[Block_RepFlag + x] = a; // clear block object flag
    }
  }
  // NextBUpd:
  dex(); // decrement block object offset
  if (!neg_flag) { goto UpdateLoop; } // do this until both block objects are dealt with
  // -------------------------------------------------------------------------------------
  // $00 - used to store high nybble of horizontal speed as adder
  // $01 - used to store low nybble of horizontal speed
  // $02 - used to store adder to page location
}

void MoveEnemyHorizontally(void) {
  inx(); // increment offset for enemy offset
  MoveObjectHorizontally(); // position object horizontally according to
  ldx_zp(ObjectOffset); // counters, return with saved value in A,
}

void MovePlayerHorizontally(void) {
  lda_abs(JumpspringAnimCtrl); // if jumpspring currently animating,
  if (zero_flag) {
    tax(); // otherwise set zero for offset to use player's stuff
    MoveObjectHorizontally(); return;
  }
}

void MoveObjectHorizontally(void) {
  lda_zpx(SprObject_X_Speed); // get currently saved value (horizontal
  asl_acc(); // speed, secondary counter, whatever)
  asl_acc(); // and move low nybble to high
  asl_acc();
  asl_acc();
  ram[0x1] = a; // store result here
  lda_zpx(SprObject_X_Speed); // get saved value again
  lsr_acc(); // move high nybble to low
  lsr_acc();
  lsr_acc();
  lsr_acc();
  cmp_imm(0x8); // if < 8, branch, do not change
  if (carry_flag) {
    ora_imm(0b11110000); // otherwise alter high nybble
  }
  // SaveXSpd:
  ram[0x0] = a; // save result here
  ldy_imm(0x0); // load default Y value here
  cmp_imm(0x0); // if result positive, leave Y alone
  if (neg_flag) {
    dey(); // otherwise decrement Y
  }
  // UseAdder:
  ram[0x2] = y; // save Y here
  lda_absx(SprObject_X_MoveForce); // get whatever number's here
  carry_flag = false;
  adc_zp(0x1); // add low nybble moved to high
  ram[SprObject_X_MoveForce + x] = a; // store result here
  lda_imm(0x0); // init A
  rol_acc(); // rotate carry into d0
  pha(); // push onto stack
  ror_acc(); // rotate d0 back onto carry
  lda_zpx(SprObject_X_Position);
  adc_zp(0x0); // add carry plus saved value (high nybble moved to low
  ram[SprObject_X_Position + x] = a; // plus $f0 if necessary) to object's horizontal position
  lda_zpx(SprObject_PageLoc);
  adc_zp(0x2); // add carry plus other saved value to the
  ram[SprObject_PageLoc + x] = a; // object's page location and save
  pla();
  carry_flag = false; // pull old carry from stack and add
  adc_zp(0x0); // to high nybble moved to low
  // -------------------------------------------------------------------------------------
  // $00 - used for downward force
  // $01 - used for upward force
  // $02 - used for maximum vertical speed
}

void MovePlayerVertically(void) {
  ldx_imm(0x0); // set X for player offset
  lda_abs(TimerControl);
  if (!zero_flag) { goto NoJSChk; } // if master timer control set, branch ahead
  lda_abs(JumpspringAnimCtrl); // otherwise check to see if jumpspring is animating
  if (!zero_flag) { return; } // branch to leave if so
  
NoJSChk:
  lda_abs(VerticalForce); // dump vertical force
  ram[0x0] = a;
  lda_imm(0x4); // set maximum vertical speed here
  ImposeGravitySprObj(); // then jump to move player vertically
  // --------------------------------
}

void MoveD_EnemyVertically(void) {
  ldy_imm(0x3d); // set quick movement amount downwards
  lda_zpx(Enemy_State); // then check enemy state
  cmp_imm(0x5); // if not set to unique state for spiny's egg, go ahead
  // and use, otherwise set different movement amount, continue on
  if (!zero_flag) {
    ContVMove();
    return;
  }
  MoveFallingPlatform(); // <fallthrough>
}

void MoveFallingPlatform(void) {
  ldy_imm(0x20); // set movement amount
  ContVMove(); // <fallthrough>
}

void ContVMove(void) {
  SetHiMax(); // jump to skip the rest of this
  // --------------------------------
}

void MoveRedPTroopaDown(void) {
  ldy_imm(0x0); // set Y to move downwards
  MoveRedPTroopa(); // skip to movement routine
}

void MoveRedPTroopaUp(void) {
  ldy_imm(0x1); // set Y to move upwards
  MoveRedPTroopa(); // <fallthrough>
}

void MoveRedPTroopa(void) {
  inx(); // increment X for enemy offset
  lda_imm(0x3);
  ram[0x0] = a; // set downward movement amount here
  lda_imm(0x6);
  ram[0x1] = a; // set upward movement amount here
  lda_imm(0x2);
  ram[0x2] = a; // set maximum speed here
  tya(); // set movement direction in A, and
  RedPTroopaGrav(); // jump to move this thing
  // --------------------------------
}

void MoveDropPlatform(void) {
  ldy_imm(0x7f); // set movement amount for drop platform
  // skip ahead of other value set here
  if (!zero_flag) {
    SetMdMax();
    return;
  }
  MoveEnemySlowVert(); // <fallthrough>
}

void MoveEnemySlowVert(void) {
  ldy_imm(0xf); // set movement amount for bowser/other objects
  SetMdMax(); // <fallthrough>
}

void SetMdMax(void) {
  lda_imm(0x2); // set maximum speed in A
  SetXMoveAmt(); // unconditional branch
  // --------------------------------
}

void MoveJ_EnemyVertically(void) {
  ldy_imm(0x1c); // set movement amount for podoboo/other objects
  SetHiMax(); // <fallthrough>
}

void SetHiMax(void) {
  lda_imm(0x3); // set maximum speed in A
  SetXMoveAmt(); // <fallthrough>
}

void SetXMoveAmt(void) {
  ram[0x0] = y; // set movement amount here
  inx(); // increment X for enemy offset
  ImposeGravitySprObj(); // do a sub to move enemy object downwards
  ldx_zp(ObjectOffset); // get enemy object buffer offset and leave
  // --------------------------------
}

void ImposeGravityBlock(void) {
  ldy_imm(0x1); // set offset for maximum speed
  lda_imm(0x50); // set movement amount here
  ram[0x0] = a;
  lda_absy(MaxSpdBlockData); // get maximum speed
  ImposeGravitySprObj(); // <fallthrough>
}

void ImposeGravitySprObj(void) {
  ram[0x2] = a; // set maximum speed here
  lda_imm(0x0); // set value to move downwards
  ImposeGravity(); // jump to the code that actually moves it
  // --------------------------------
}

void MovePlatformDown(void) {
  lda_imm(0x0); // save value to stack (if branching here, execute next
  MovePlatformUpSkip(); //  .db $2c     ;part as BIT instruction)
}

void MovePlatformUp(void) {
  lda_imm(0x1); // save value to stack
  MovePlatformUpSkip(); // <fallthrough>
}

void MovePlatformUpSkip(void) {
  pha();
  ldy_zpx(Enemy_ID); // get enemy object identifier
  inx(); // increment offset for enemy object
  lda_imm(0x5); // load default value here
  cpy_imm(0x29); // residual comparison, object #29 never executes
  // this code, thus unconditional branch here
  if (zero_flag) {
    lda_imm(0x9); // residual code
  }
  // SetDplSpd:
  ram[0x0] = a; // save downward movement amount here
  lda_imm(0xa); // save upward movement amount here
  ram[0x1] = a;
  lda_imm(0x3); // save maximum vertical speed here
  ram[0x2] = a;
  pla(); // get value from stack
  tay(); // use as Y, then move onto code shared by red koopa
  RedPTroopaGrav(); // <fallthrough>
}

void RedPTroopaGrav(void) {
  ImposeGravity(); // do a sub to move object gradually
  ldx_zp(ObjectOffset); // get enemy object offset and leave
  // -------------------------------------------------------------------------------------
  // $00 - used for downward force
  // $01 - used for upward force
  // $07 - used as adder for vertical position
}

void EnemiesAndLoopsCore(void) {
  lda_zpx(Enemy_Flag); // check data here for MSB set
  pha(); // save in stack
  asl_acc();
  if (carry_flag) { goto ChkBowserF; } // if MSB set in enemy flag, branch ahead of jumps
  pla(); // get from stack
  if (zero_flag) { goto ChkAreaTsk; } // if data zero, branch
  RunEnemyObjectsCore(); // otherwise, jump to run enemy subroutines
  return;
  
ChkAreaTsk:
  lda_abs(AreaParserTaskNum); // check number of tasks to perform
  and_imm(0x7);
  cmp_imm(0x7); // if at a specific task, jump and leave
  if (zero_flag) { return; }
  ProcLoopCommand(); // otherwise, jump to process loop command/load enemies
  return;
  
ChkBowserF:
  pla(); // get data from stack
  and_imm(0b00001111); // mask out high nybble
  tay();
  lda_zpy(Enemy_Flag); // use as pointer and load same place with different offset
  if (!zero_flag) { return; }
  ram[Enemy_Flag + x] = a; // if second enemy flag not set, also clear first one
  // --------------------------------
  // loop command data
}

void ExecGameLoopback(void) {
  lda_zp(Player_PageLoc); // send player back four pages
  carry_flag = true;
  sbc_imm(0x4);
  ram[Player_PageLoc] = a;
  lda_abs(CurrentPageLoc); // send current page back four pages
  carry_flag = true;
  sbc_imm(0x4);
  ram[CurrentPageLoc] = a;
  lda_abs(ScreenLeft_PageLoc); // subtract four from page location
  carry_flag = true; // of screen's left border
  sbc_imm(0x4);
  ram[ScreenLeft_PageLoc] = a;
  lda_abs(ScreenRight_PageLoc); // do the same for the page location
  carry_flag = true; // of screen's right border
  sbc_imm(0x4);
  ram[ScreenRight_PageLoc] = a;
  lda_abs(AreaObjectPageLoc); // subtract four from page control
  carry_flag = true; // for area objects
  sbc_imm(0x4);
  ram[AreaObjectPageLoc] = a;
  lda_imm(0x0); // initialize page select for both
  ram[EnemyObjectPageSel] = a; // area and enemy objects
  ram[AreaObjectPageSel] = a;
  ram[EnemyDataOffset] = a; // initialize enemy object data offset
  ram[EnemyObjectPageLoc] = a; // and enemy object page control
  lda_absy(AreaDataOfsLoopback); // adjust area object offset based on
  ram[AreaDataOffset] = a; // which loop command we encountered
}

void ProcLoopCommand(void) {
  lda_abs(LoopCommand); // check if loop command was found
  if (zero_flag) { goto ChkEnemyFrenzy; }
  lda_abs(CurrentColumnPos); // check to see if we're still on the first page
  if (!zero_flag) { goto ChkEnemyFrenzy; } // if not, do not loop yet
  ldy_imm(0xb); // start at the end of each set of loop data
  
FindLoop:
  dey();
  if (neg_flag) { goto ChkEnemyFrenzy; } // if all data is checked and not match, do not loop
  lda_abs(WorldNumber); // check to see if one of the world numbers
  cmp_absy(LoopCmdWorldNumber); // matches our current world number
  if (!zero_flag) { goto FindLoop; }
  lda_abs(CurrentPageLoc); // check to see if one of the page numbers
  cmp_absy(LoopCmdPageNumber); // matches the page we're currently on
  if (!zero_flag) { goto FindLoop; }
  lda_zp(Player_Y_Position); // check to see if the player is at the correct position
  cmp_absy(LoopCmdYPosition); // if not, branch to check for world 7
  if (!zero_flag) { goto WrongChk; }
  lda_zp(Player_State); // check to see if the player is
  cmp_imm(0x0); // on solid ground (i.e. not jumping or falling)
  if (!zero_flag) { goto WrongChk; } // if not, player fails to pass loop, and loopback
  lda_abs(WorldNumber); // are we in world 7? (check performed on correct
  cmp_imm(World7); // vertical position and on solid ground)
  if (!zero_flag) { goto InitMLp; } // if not, initialize flags used there, otherwise
  inc_abs(MultiLoopCorrectCntr); // increment counter for correct progression
  
IncMLoop:
  inc_abs(MultiLoopPassCntr); // increment master multi-part counter
  lda_abs(MultiLoopPassCntr); // have we done all three parts?
  cmp_imm(0x3);
  if (!zero_flag) { goto InitLCmd; } // if not, skip this part
  lda_abs(MultiLoopCorrectCntr); // if so, have we done them all correctly?
  cmp_imm(0x3);
  if (zero_flag) { goto InitMLp; } // if so, branch past unnecessary check here
  if (!zero_flag) { goto DoLpBack; } // unconditional branch if previous branch fails
  
WrongChk:
  lda_abs(WorldNumber); // are we in world 7? (check performed on
  cmp_imm(World7); // incorrect vertical position or not on solid ground)
  if (zero_flag) { goto IncMLoop; }
  
DoLpBack:
  ExecGameLoopback(); // if player is not in right place, loop back
  KillAllEnemies();
  
InitMLp:
  lda_imm(0x0); // initialize counters used for multi-part loop commands
  ram[MultiLoopPassCntr] = a;
  ram[MultiLoopCorrectCntr] = a;
  
InitLCmd:
  lda_imm(0x0); // initialize loop command flag
  ram[LoopCommand] = a;
  // --------------------------------
  
ChkEnemyFrenzy:
  lda_abs(EnemyFrenzyQueue); // check for enemy object in frenzy queue
  if (zero_flag) { goto ProcessEnemyData; } // if not, skip this part
  ram[Enemy_ID + x] = a; // store as enemy object identifier here
  lda_imm(0x1);
  ram[Enemy_Flag + x] = a; // activate enemy object flag
  lda_imm(0x0);
  ram[Enemy_State + x] = a; // initialize state and frenzy queue
  ram[EnemyFrenzyQueue] = a;
  InitEnemyObject(); return; // and then jump to deal with this enemy
  // --------------------------------
  // $06 - used to hold page location of extended right boundary
  // $07 - used to hold high nybble of position of extended right boundary
  
ProcessEnemyData:
  ldy_abs(EnemyDataOffset); // get offset of enemy object data
  lda_indy(EnemyData); // load first byte
  cmp_imm(0xff); // check for EOD terminator
  if (!zero_flag) { goto CheckEndofBuffer; }
  goto CheckFrenzyBuffer; // if found, jump to check frenzy buffer, otherwise
  
CheckEndofBuffer:
  and_imm(0b00001111); // check for special row $0e
  cmp_imm(0xe);
  if (zero_flag) { goto CheckRightBounds; } // if found, branch, otherwise
  cpx_imm(0x5); // check for end of buffer
  if (!carry_flag) { goto CheckRightBounds; } // if not at end of buffer, branch
  iny();
  lda_indy(EnemyData); // check for specific value here
  and_imm(0b00111111); // not sure what this was intended for, exactly
  cmp_imm(0x2e); // this part is quite possibly residual code
  if (zero_flag) { goto CheckRightBounds; } // but it has the effect of keeping enemies out of
  return; // the sixth slot
  
CheckRightBounds:
  lda_abs(ScreenRight_X_Pos); // add 48 to pixel coordinate of right boundary
  carry_flag = false;
  adc_imm(0x30);
  and_imm(0b11110000); // store high nybble
  ram[0x7] = a;
  lda_abs(ScreenRight_PageLoc); // add carry to page location of right boundary
  adc_imm(0x0);
  ram[0x6] = a; // store page location + carry
  ldy_abs(EnemyDataOffset);
  iny();
  lda_indy(EnemyData); // if MSB of enemy object is clear, branch to check for row $0f
  asl_acc();
  if (!carry_flag) { goto CheckPageCtrlRow; }
  lda_abs(EnemyObjectPageSel); // if page select already set, do not set again
  if (!zero_flag) { goto CheckPageCtrlRow; }
  inc_abs(EnemyObjectPageSel); // otherwise, if MSB is set, set page select
  inc_abs(EnemyObjectPageLoc); // and increment page control
  
CheckPageCtrlRow:
  dey();
  lda_indy(EnemyData); // reread first byte
  and_imm(0xf);
  cmp_imm(0xf); // check for special row $0f
  if (!zero_flag) { goto PositionEnemyObj; } // if not found, branch to position enemy object
  lda_abs(EnemyObjectPageSel); // if page select set,
  if (!zero_flag) { goto PositionEnemyObj; } // branch without reading second byte
  iny();
  lda_indy(EnemyData); // otherwise, get second byte, mask out 2 MSB
  and_imm(0b00111111);
  ram[EnemyObjectPageLoc] = a; // store as page control for enemy object data
  inc_abs(EnemyDataOffset); // increment enemy object data offset 2 bytes
  inc_abs(EnemyDataOffset);
  inc_abs(EnemyObjectPageSel); // set page select for enemy object data and
  ProcLoopCommand(); // jump back to process loop commands again
  return;
  
PositionEnemyObj:
  lda_abs(EnemyObjectPageLoc); // store page control as page location
  ram[Enemy_PageLoc + x] = a; // for enemy object
  lda_indy(EnemyData); // get first byte of enemy object
  and_imm(0b11110000);
  ram[Enemy_X_Position + x] = a; // store column position
  cmp_abs(ScreenRight_X_Pos); // check column position against right boundary
  lda_zpx(Enemy_PageLoc); // without subtracting, then subtract borrow
  sbc_abs(ScreenRight_PageLoc); // from page location
  if (carry_flag) { goto CheckRightExtBounds; } // if enemy object beyond or at boundary, branch
  lda_indy(EnemyData);
  and_imm(0b00001111); // check for special row $0e
  cmp_imm(0xe); // if found, jump elsewhere
  if (zero_flag) { ParseRow0e(); return; }
  CheckThreeBytes(); // if not found, unconditional jump
  return;
  
CheckRightExtBounds:
  lda_zp(0x7); // check right boundary + 48 against
  cmp_zpx(Enemy_X_Position); // column position without subtracting,
  lda_zp(0x6); // then subtract borrow from page control temp
  sbc_zpx(Enemy_PageLoc); // plus carry
  if (!carry_flag) { goto CheckFrenzyBuffer; } // if enemy object beyond extended boundary, branch
  lda_imm(0x1); // store value in vertical high byte
  ram[Enemy_Y_HighPos + x] = a;
  lda_indy(EnemyData); // get first byte again
  asl_acc(); // multiply by four to get the vertical
  asl_acc(); // coordinate
  asl_acc();
  asl_acc();
  ram[Enemy_Y_Position + x] = a;
  cmp_imm(0xe0); // do one last check for special row $0e
  if (zero_flag) { ParseRow0e(); return; } // (necessary if branched to $c1cb)
  iny();
  lda_indy(EnemyData); // get second byte of object
  and_imm(0b01000000); // check to see if hard mode bit is set
  if (zero_flag) { goto CheckForEnemyGroup; } // if not, branch to check for group enemy objects
  lda_abs(SecondaryHardMode); // if set, check to see if secondary hard mode flag
  if (zero_flag) { Inc2B(); return; } // is on, and if not, branch to skip this object completely
  
CheckForEnemyGroup:
  lda_indy(EnemyData); // get second byte and mask out 2 MSB
  and_imm(0b00111111);
  cmp_imm(0x37); // check for value below $37
  if (!carry_flag) { goto BuzzyBeetleMutate; }
  cmp_imm(0x3f); // if $37 or greater, check for value
  if (!carry_flag) { DoGroup(); return; } // below $3f, branch if below $3f
  
BuzzyBeetleMutate:
  cmp_imm(Goomba); // if below $37, check for goomba
  if (!zero_flag) { goto StrID; } // value ($3f or more always fails)
  ldy_abs(PrimaryHardMode); // check if primary hard mode flag is set
  if (zero_flag) { goto StrID; } // and if so, change goomba to buzzy beetle
  lda_imm(BuzzyBeetle);
  
StrID:
  ram[Enemy_ID + x] = a; // store enemy object number into buffer
  lda_imm(0x1);
  ram[Enemy_Flag + x] = a; // set flag for enemy in buffer
  InitEnemyObject();
  lda_zpx(Enemy_Flag); // check to see if flag is set
  if (!zero_flag) { Inc2B(); return; } // if not, leave, otherwise branch
  return;
  
CheckFrenzyBuffer:
  lda_abs(EnemyFrenzyBuffer); // if enemy object stored in frenzy buffer
  if (!zero_flag) { goto StrFre; } // then branch ahead to store in enemy object buffer
  lda_abs(VineFlagOffset); // otherwise check vine flag offset
  cmp_imm(0x1);
  if (!zero_flag) { return; } // if other value <> 1, leave
  lda_imm(VineObject); // otherwise put vine in enemy identifier
  
StrFre:
  ram[Enemy_ID + x] = a; // store contents of frenzy buffer into enemy identifier value
  InitEnemyObject(); // <fallthrough>
}

void InitEnemyObject(void) {
  lda_imm(0x0); // initialize enemy state
  ram[Enemy_State + x] = a;
  CheckpointEnemyID(); // jump ahead to run jump engine and subroutines
}

void DoGroup(void) {
  HandleGroupEnemies(); // handle enemy group objects
}

void ParseRow0e(void) {
  iny(); // increment Y to load third byte of object
  iny();
  lda_indy(EnemyData);
  lsr_acc(); // move 3 MSB to the bottom, effectively
  lsr_acc(); // making %xxx00000 into %00000xxx
  lsr_acc();
  lsr_acc();
  lsr_acc();
  cmp_abs(WorldNumber); // is it the same world number as we're on?
  // if not, do not use (this allows multiple uses
  if (zero_flag) {
    dey(); // of the same area, like the underground bonus areas)
    lda_indy(EnemyData); // otherwise, get second byte and use as offset
    ram[AreaPointer] = a; // to addresses for level and enemy object data
    iny();
    lda_indy(EnemyData); // get third byte again, and this time mask out
    and_imm(0b00011111); // the 3 MSB from before, save as page number to be
    ram[EntrancePage] = a; // used upon entry to area, if area is entered
  }
  // NotUse:
  Inc3B();
}

void Inc2B(void) {
  inc_abs(EnemyDataOffset); // otherwise increment two bytes
  inc_abs(EnemyDataOffset);
  lda_imm(0x0); // init page select for enemy objects
  ram[EnemyObjectPageSel] = a;
  ldx_zp(ObjectOffset); // reload current offset in enemy buffers
}

void CheckThreeBytes(void) {
  ldy_abs(EnemyDataOffset); // load current offset for enemy object data
  lda_indy(EnemyData); // get first byte
  and_imm(0b00001111); // check for special row $0e
  cmp_imm(0xe);
  if (!zero_flag) {
    Inc2B();
    return;
  }
  Inc3B(); // <fallthrough>
}

void Inc3B(void) {
  inc_abs(EnemyDataOffset); // if row = $0e, increment three bytes
  Inc2B(); // <fallthrough>
}

void CheckpointEnemyID(void) {
  lda_zpx(Enemy_ID);
  cmp_imm(0x15); // check enemy object identifier for $15 or greater
  // and branch straight to the jump engine if found
  if (!carry_flag) {
    tay(); // save identifier in Y register for now
    lda_zpx(Enemy_Y_Position);
    adc_imm(0x8); // add eight pixels to what will eventually be the
    ram[Enemy_Y_Position + x] = a; // enemy object's vertical coordinate ($00-$14 only)
    lda_imm(0x1);
    ram[EnemyOffscrBitsMasked + x] = a; // set offscreen masked bit
    tya(); // get identifier back and use as offset for jump engine
  }
  // InitEnemyRoutines:
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: InitNormalEnemy(); return;
    case 1: InitNormalEnemy(); return;
    case 2: InitNormalEnemy(); return;
    case 3: InitRedKoopa(); return;
    case 4: NoInitCode(); return;
    case 5: InitHammerBro(); return;
    case 6: InitGoomba(); return;
    case 7: InitBloober(); return;
    case 8: InitBulletBill(); return;
    case 9: NoInitCode(); return;
    case 10: InitCheepCheep(); return;
    case 11: InitCheepCheep(); return;
    case 12: InitPodoboo(); return;
    case 13: InitPiranhaPlant(); return;
    case 14: InitJumpGPTroopa(); return;
    case 15: InitRedPTroopa(); return;
    case 16: InitHorizFlySwimEnemy(); return;
    case 17: InitLakitu(); return;
    case 18: InitEnemyFrenzy(); return;
    case 19: NoInitCode(); return;
    case 20: InitEnemyFrenzy(); return;
    case 21: InitEnemyFrenzy(); return;
    case 22: InitEnemyFrenzy(); return;
    case 23: InitEnemyFrenzy(); return;
    case 24: EndFrenzy(); return;
    case 25: NoInitCode(); return;
    case 26: NoInitCode(); return;
    case 27: InitShortFirebar(); return;
    case 28: InitShortFirebar(); return;
    case 29: InitShortFirebar(); return;
    case 30: InitShortFirebar(); return;
    case 31: InitLongFirebar(); return;
    case 32: NoInitCode(); return;
    case 33: NoInitCode(); return;
    case 34: NoInitCode(); return;
    case 35: NoInitCode(); return;
    case 36: InitBalPlatform(); return;
    case 37: InitVertPlatform(); return;
    case 38: LargeLiftUp(); return;
    case 39: LargeLiftDown(); return;
    case 40: InitHoriPlatform(); return;
    case 41: InitDropPlatform(); return;
    case 42: InitHoriPlatform(); return;
    case 43: PlatLiftUp(); return;
    case 44: PlatLiftDown(); return;
    case 45: InitBowser(); return;
    case 46: PwrUpJmp(); return;
    case 47: Setup_Vine(); return;
    case 48: NoInitCode(); return;
    case 49: NoInitCode(); return;
    case 50: NoInitCode(); return;
    case 51: NoInitCode(); return;
    case 52: NoInitCode(); return;
    case 53: InitRetainerObj(); return;
    case 54: EndOfEnemyInitCode(); return;
  }
}

void NoInitCode(void) {
  // --------------------------------
}

void InitGoomba(void) {
  InitNormalEnemy(); // set appropriate horizontal speed
  SmallBBox(); return; // set $09 as bounding box control, set other values
  // --------------------------------
}

void InitPodoboo(void) {
  lda_imm(0x2); // set enemy position to below
  ram[Enemy_Y_HighPos + x] = a; // the bottom of the screen
  ram[Enemy_Y_Position + x] = a;
  lsr_acc();
  ram[EnemyIntervalTimer + x] = a; // set timer for enemy
  lsr_acc();
  ram[Enemy_State + x] = a; // initialize enemy state, then jump to use
  SmallBBox(); return; // $09 as bounding box size and set other things
  // --------------------------------
}

void InitRetainerObj(void) {
  lda_imm(0xb8); // set fixed vertical position for
  ram[Enemy_Y_Position + x] = a; // princess/mushroom retainer object
  // --------------------------------
}

void InitNormalEnemy(void) {
  ldy_imm(0x1); // load offset of 1 by default
  lda_abs(PrimaryHardMode); // check for primary hard mode flag set
  if (zero_flag) {
    dey(); // if not set, decrement offset
  }
  // GetESpd:
  lda_absy(NormalXSpdData); // get appropriate horizontal speed
  SetESpd(); // <fallthrough>
}

void SetESpd(void) {
  ram[Enemy_X_Speed + x] = a; // store as speed for enemy object
  TallBBox(); // branch to set bounding box control and other data
  // --------------------------------
}

void InitRedKoopa(void) {
  InitNormalEnemy(); // load appropriate horizontal speed
  lda_imm(0x1); // set enemy state for red koopa troopa $03
  ram[Enemy_State + x] = a;
  // --------------------------------
}

void InitHammerBro(void) {
  lda_imm(0x0); // init horizontal speed and timer used by hammer bro
  ram[HammerThrowingTimer + x] = a; // apparently to time hammer throwing
  ram[Enemy_X_Speed + x] = a;
  ldy_abs(SecondaryHardMode); // get secondary hard mode flag
  lda_absy(HBroWalkingTimerData);
  ram[EnemyIntervalTimer + x] = a; // set value as delay for hammer bro to walk left
  lda_imm(0xb); // set specific value for bounding box size control
  SetBBox();
  // --------------------------------
}

void InitHorizFlySwimEnemy(void) {
  lda_imm(0x0); // initialize horizontal speed
  SetESpd();
  // --------------------------------
}

void InitBloober(void) {
  lda_imm(0x0); // initialize horizontal speed
  ram[BlooperMoveSpeed + x] = a;
  SmallBBox(); // <fallthrough>
}

void SmallBBox(void) {
  lda_imm(0x9); // set specific bounding box size control
  // unconditional branch
  if (!zero_flag) {
    SetBBox();
    return;
  }
  // --------------------------------
  InitRedPTroopa(); // <fallthrough>
}

void InitRedPTroopa(void) {
  ldy_imm(0x30); // load central position adder for 48 pixels down
  lda_zpx(Enemy_Y_Position); // set vertical coordinate into location to
  ram[RedPTroopaOrigXPos + x] = a; // be used as original vertical coordinate
  // if vertical coordinate < $80
  if (neg_flag) {
    ldy_imm(0xe0); // if => $80, load position adder for 32 pixels up
  }
  // GetCent:
  tya(); // send central position adder to A
  adc_zpx(Enemy_Y_Position); // add to current vertical coordinate
  ram[RedPTroopaCenterYPos + x] = a; // store as central vertical coordinate
  TallBBox(); // <fallthrough>
}

void TallBBox(void) {
  lda_imm(0x3); // set specific bounding box size control
  SetBBox(); // <fallthrough>
}

void SetBBox(void) {
  ram[Enemy_BoundBoxCtrl + x] = a; // set bounding box control here
  lda_imm(0x2); // set moving direction for left
  ram[Enemy_MovingDir + x] = a;
  InitVStf(); // <fallthrough>
}

void InitVStf(void) {
  lda_imm(0x0); // initialize vertical speed
  ram[Enemy_Y_Speed + x] = a; // and movement force
  ram[Enemy_Y_MoveForce + x] = a;
  // --------------------------------
}

void InitBulletBill(void) {
  lda_imm(0x2); // set moving direction for left
  ram[Enemy_MovingDir + x] = a;
  lda_imm(0x9); // set bounding box control for $09
  ram[Enemy_BoundBoxCtrl + x] = a;
  // --------------------------------
}

void InitCheepCheep(void) {
  SmallBBox(); // set vertical bounding box, speed, init others
  lda_absx(PseudoRandomBitReg); // check one portion of LSFR
  and_imm(0b00010000); // get d4 from it
  ram[CheepCheepMoveMFlag + x] = a; // save as movement flag of some sort
  lda_zpx(Enemy_Y_Position);
  ram[CheepCheepOrigYPos + x] = a; // save original vertical coordinate here
  // --------------------------------
}

void InitLakitu(void) {
  lda_abs(EnemyFrenzyBuffer); // check to see if an enemy is already in
  // the frenzy buffer, and branch to kill lakitu if so
  if (!zero_flag) {
    KillLakitu();
    return;
  }
  SetupLakitu(); // <fallthrough>
}

void SetupLakitu(void) {
  lda_imm(0x0); // erase counter for lakitu's reappearance
  ram[LakituReappearTimer] = a;
  InitHorizFlySwimEnemy(); // set $03 as bounding box, set other attributes
  TallBBox2(); // set $03 as bounding box again (not necessary) and leave
}

void KillLakitu(void) {
  EraseEnemyObject();
  // --------------------------------
  // $01-$03 - used to hold pseudorandom difference adjusters
}

void LakituAndSpinyHandler(void) {
  lda_abs(FrenzyEnemyTimer); // if timer here not expired, leave
  if (!zero_flag) { goto ExLSHand; }
  cpx_imm(0x5); // if we are on the special use slot, leave
  if (carry_flag) { goto ExLSHand; }
  lda_imm(0x80); // set timer
  ram[FrenzyEnemyTimer] = a;
  ldy_imm(0x4); // start with the last enemy slot
  
ChkLak:
  lda_zpy(Enemy_ID); // check all enemy slots to see
  cmp_imm(Lakitu); // if lakitu is on one of them
  if (zero_flag) { goto CreateSpiny; } // if so, branch out of this loop
  dey(); // otherwise check another slot
  if (!neg_flag) { goto ChkLak; } // loop until all slots are checked
  inc_abs(LakituReappearTimer); // increment reappearance timer
  lda_abs(LakituReappearTimer);
  cmp_imm(0x7); // check to see if we're up to a certain value yet
  if (!carry_flag) { goto ExLSHand; } // if not, leave
  ldx_imm(0x4); // start with the last enemy slot again
  
ChkNoEn:
  lda_zpx(Enemy_Flag); // check enemy buffer flag for non-active enemy slot
  if (zero_flag) { goto CreateL; } // branch out of loop if found
  dex(); // otherwise check next slot
  if (!neg_flag) { goto ChkNoEn; } // branch until all slots are checked
  if (neg_flag) { goto RetEOfs; } // if no empty slots were found, branch to leave
  
CreateL:
  lda_imm(0x0); // initialize enemy state
  ram[Enemy_State + x] = a;
  lda_imm(Lakitu); // create lakitu enemy object
  ram[Enemy_ID + x] = a;
  SetupLakitu(); // do a sub to set up lakitu
  lda_imm(0x20);
  PutAtRightExtent(); // finish setting up lakitu
  
RetEOfs:
  ldx_zp(ObjectOffset); // get enemy object buffer offset again and leave
  
ExLSHand:
  return;
  // --------------------------------
  
CreateSpiny:
  lda_zp(Player_Y_Position); // if player above a certain point, branch to leave
  cmp_imm(0x2c);
  if (!carry_flag) { goto ExLSHand; }
  lda_zpy(Enemy_State); // if lakitu is not in normal state, branch to leave
  if (!zero_flag) { goto ExLSHand; }
  lda_zpy(Enemy_PageLoc); // store horizontal coordinates (high and low) of lakitu
  ram[Enemy_PageLoc + x] = a; // into the coordinates of the spiny we're going to create
  lda_zpy(Enemy_X_Position);
  ram[Enemy_X_Position + x] = a;
  lda_imm(0x1); // put spiny within vertical screen unit
  ram[Enemy_Y_HighPos + x] = a;
  lda_zpy(Enemy_Y_Position); // put spiny eight pixels above where lakitu is
  carry_flag = true;
  sbc_imm(0x8);
  ram[Enemy_Y_Position + x] = a;
  lda_absx(PseudoRandomBitReg); // get 2 LSB of LSFR and save to Y
  and_imm(0b00000011);
  tay();
  ldx_imm(0x2);
  
DifLoop:
  lda_absy(PRDiffAdjustData); // get three values and save them
  ram[0x1 + x] = a; // to $01-$03
  iny();
  iny(); // increment Y four bytes for each value
  iny();
  iny();
  dex(); // decrement X for each one
  if (!neg_flag) { goto DifLoop; } // loop until all three are written
  ldx_zp(ObjectOffset); // get enemy object buffer offset
  PlayerLakituDiff(); // move enemy, change direction, get value - difference
  ldy_zp(Player_X_Speed); // check player's horizontal speed
  cpy_imm(0x8);
  if (carry_flag) { goto SetSpSpd; } // if moving faster than a certain amount, branch elsewhere
  tay(); // otherwise save value in A to Y for now
  lda_absx(PseudoRandomBitReg + 1);
  and_imm(0b00000011); // get one of the LSFR parts and save the 2 LSB
  if (zero_flag) { goto UsePosv; } // branch if neither bits are set
  tya();
  eor_imm(0b11111111); // otherwise get two's compliment of Y
  tay();
  iny();
  
UsePosv:
  tya(); // put value from A in Y back to A (they will be lost anyway)
  
SetSpSpd:
  SmallBBox(); // set bounding box control, init attributes, lose contents of A
  ldy_imm(0x2);
  ram[Enemy_X_Speed + x] = a; // set horizontal speed to zero because previous contents
  cmp_imm(0x0); // of A were lost...branch here will never be taken for
  if (neg_flag) { goto SpinyRte; } // the same reason
  dey();
  
SpinyRte:
  ram[Enemy_MovingDir + x] = y; // set moving direction to the right
  lda_imm(0xfd);
  ram[Enemy_Y_Speed + x] = a; // set vertical speed to move upwards
  lda_imm(0x1);
  ram[Enemy_Flag + x] = a; // enable enemy object by setting flag
  lda_imm(0x5);
  ram[Enemy_State + x] = a; // put spiny in egg state and leave
  // --------------------------------
}

void InitLongFirebar(void) {
  DuplicateEnemyObj(); // create enemy object for long firebar
  InitShortFirebar(); // <fallthrough>
}

void InitShortFirebar(void) {
  lda_imm(0x0); // initialize low byte of spin state
  ram[FirebarSpinState_Low + x] = a;
  lda_zpx(Enemy_ID); // subtract $1b from enemy identifier
  carry_flag = true; // to get proper offset for firebar data
  sbc_imm(0x1b);
  tay();
  lda_absy(FirebarSpinSpdData); // get spinning speed of firebar
  ram[FirebarSpinSpeed + x] = a;
  lda_absy(FirebarSpinDirData); // get spinning direction of firebar
  ram[FirebarSpinDirection + x] = a;
  lda_zpx(Enemy_Y_Position);
  carry_flag = false; // add four pixels to vertical coordinate
  adc_imm(0x4);
  ram[Enemy_Y_Position + x] = a;
  lda_zpx(Enemy_X_Position);
  carry_flag = false; // add four pixels to horizontal coordinate
  adc_imm(0x4);
  ram[Enemy_X_Position + x] = a;
  lda_zpx(Enemy_PageLoc);
  adc_imm(0x0); // add carry to page location
  ram[Enemy_PageLoc + x] = a;
  TallBBox2(); // set bounding box control (not used) and leave
  // --------------------------------
  // $00-$01 - used to hold pseudorandom bits
}

void InitFlyingCheepCheep(void) {
  lda_abs(FrenzyEnemyTimer); // if timer here not expired yet, branch to leave
  if (!zero_flag) { return; }
  SmallBBox(); // jump to set bounding box size $09 and init other values
  lda_absx(PseudoRandomBitReg + 1);
  and_imm(0b00000011); // set pseudorandom offset here
  tay();
  lda_absy(FlyCCTimerData); // load timer with pseudorandom offset
  ram[FrenzyEnemyTimer] = a;
  ldy_imm(0x3); // load Y with default value
  lda_abs(SecondaryHardMode);
  if (zero_flag) { goto MaxCC; } // if secondary hard mode flag not set, do not increment Y
  iny(); // otherwise, increment Y to allow as many as four onscreen
  
MaxCC:
  ram[0x0] = y; // store whatever pseudorandom bits are in Y
  cpx_zp(0x0); // compare enemy object buffer offset with Y
  if (carry_flag) { return; } // if X => Y, branch to leave
  lda_absx(PseudoRandomBitReg);
  and_imm(0b00000011); // get last two bits of LSFR, first part
  ram[0x0] = a; // and store in two places
  ram[0x1] = a;
  lda_imm(0xfb); // set vertical speed for cheep-cheep
  ram[Enemy_Y_Speed + x] = a;
  lda_imm(0x0); // load default value
  ldy_zp(Player_X_Speed); // check player's horizontal speed
  if (zero_flag) { goto GSeed; } // if player not moving left or right, skip this part
  lda_imm(0x4);
  cpy_imm(0x19); // if moving to the right but not very quickly,
  if (!carry_flag) { goto GSeed; } // do not change A
  asl_acc(); // otherwise, multiply A by 2
  
GSeed:
  pha(); // save to stack
  carry_flag = false;
  adc_zp(0x0); // add to last two bits of LSFR we saved earlier
  ram[0x0] = a; // save it there
  lda_absx(PseudoRandomBitReg + 1);
  and_imm(0b00000011); // if neither of the last two bits of second LSFR set,
  if (zero_flag) { goto RSeed; } // skip this part and save contents of $00
  lda_absx(PseudoRandomBitReg + 2);
  and_imm(0b00001111); // otherwise overwrite with lower nybble of
  ram[0x0] = a; // third LSFR part
  
RSeed:
  pla(); // get value from stack we saved earlier
  carry_flag = false;
  adc_zp(0x1); // add to last two bits of LSFR we saved in other place
  tay(); // use as pseudorandom offset here
  lda_absy(FlyCCXSpeedData); // get horizontal speed using pseudorandom offset
  ram[Enemy_X_Speed + x] = a;
  lda_imm(0x1); // set to move towards the right
  ram[Enemy_MovingDir + x] = a;
  lda_zp(Player_X_Speed); // if player moving left or right, branch ahead of this part
  if (!zero_flag) { goto D2XPos1; }
  ldy_zp(0x0); // get first LSFR or third LSFR lower nybble
  tya(); // and check for d1 set
  and_imm(0b00000010);
  if (zero_flag) { goto D2XPos1; } // if d1 not set, branch
  lda_zpx(Enemy_X_Speed);
  eor_imm(0xff); // if d1 set, change horizontal speed
  carry_flag = false; // into two's compliment, thus moving in the opposite
  adc_imm(0x1); // direction
  ram[Enemy_X_Speed + x] = a;
  inc_zpx(Enemy_MovingDir); // increment to move towards the left
  
D2XPos1:
  tya(); // get first LSFR or third LSFR lower nybble again
  and_imm(0b00000010);
  if (zero_flag) { goto D2XPos2; } // check for d1 set again, branch again if not set
  lda_zp(Player_X_Position); // get player's horizontal position
  carry_flag = false;
  adc_absy(FlyCCXPositionData); // if d1 set, add value obtained from pseudorandom offset
  ram[Enemy_X_Position + x] = a; // and save as enemy's horizontal position
  lda_zp(Player_PageLoc); // get player's page location
  adc_imm(0x0); // add carry and jump past this part
  goto FinCCSt;
  
D2XPos2:
  lda_zp(Player_X_Position); // get player's horizontal position
  carry_flag = true;
  sbc_absy(FlyCCXPositionData); // if d1 not set, subtract value obtained from pseudorandom
  ram[Enemy_X_Position + x] = a; // offset and save as enemy's horizontal position
  lda_zp(Player_PageLoc); // get player's page location
  sbc_imm(0x0); // subtract borrow
  
FinCCSt:
  ram[Enemy_PageLoc + x] = a; // save as enemy's page location
  lda_imm(0x1);
  ram[Enemy_Flag + x] = a; // set enemy's buffer flag
  ram[Enemy_Y_HighPos + x] = a; // set enemy's high vertical byte
  lda_imm(0xf8);
  ram[Enemy_Y_Position + x] = a; // put enemy below the screen, and we are done
  // --------------------------------
}

void InitBowser(void) {
  DuplicateEnemyObj(); // jump to create another bowser object
  ram[BowserFront_Offset] = x; // save offset of first here
  lda_imm(0x0);
  ram[BowserBodyControls] = a; // initialize bowser's body controls
  ram[BridgeCollapseOffset] = a; // and bridge collapse offset
  lda_zpx(Enemy_X_Position);
  ram[BowserOrigXPos] = a; // store original horizontal position here
  lda_imm(0xdf);
  ram[BowserFireBreathTimer] = a; // store something here
  ram[Enemy_MovingDir + x] = a; // and in moving direction
  lda_imm(0x20);
  ram[BowserFeetCounter] = a; // set bowser's feet timer and in enemy timer
  ram[EnemyFrameTimer + x] = a;
  lda_imm(0x5);
  ram[BowserHitPoints] = a; // give bowser 5 hit points
  lsr_acc();
  ram[BowserMovementSpeed] = a; // set default movement speed here
  // --------------------------------
}

void InitBowserFlame(void) {
  lda_abs(FrenzyEnemyTimer); // if timer not expired yet, branch to leave
  if (zero_flag) {
    ram[Enemy_Y_MoveForce + x] = a; // reset something here
    lda_zp(NoiseSoundQueue);
    ora_imm(Sfx_BowserFlame); // load bowser's flame sound into queue
    ram[NoiseSoundQueue] = a;
    ldy_abs(BowserFront_Offset); // get bowser's buffer offset
    lda_zpy(Enemy_ID); // check for bowser
    cmp_imm(Bowser);
    // branch if found
    if (zero_flag) {
      SpawnFromMouth();
      return;
    }
    SetFlameTimer(); // get timer data based on flame counter
    carry_flag = false;
    adc_imm(0x20); // add 32 frames by default
    ldy_abs(SecondaryHardMode);
    // if secondary mode flag not set, use as timer setting
    if (!zero_flag) {
      carry_flag = true;
      sbc_imm(0x10); // otherwise subtract 16 frames for secondary hard mode
    }
    // SetFrT:
    ram[FrenzyEnemyTimer] = a; // set timer accordingly
    lda_absx(PseudoRandomBitReg);
    and_imm(0b00000011); // get 2 LSB from first part of LSFR
    ram[BowserFlamePRandomOfs + x] = a; // set here
    tay(); // use as offset
    lda_absy(FlameYPosData); // load vertical position based on pseudorandom offset
    PutAtRightExtent(); // <fallthrough>
  }
}

void PutAtRightExtent(void) {
  ram[Enemy_Y_Position + x] = a; // set vertical position
  lda_abs(ScreenRight_X_Pos);
  carry_flag = false;
  adc_imm(0x20); // place enemy 32 pixels beyond right side of screen
  ram[Enemy_X_Position + x] = a;
  lda_abs(ScreenRight_PageLoc);
  adc_imm(0x0); // add carry
  ram[Enemy_PageLoc + x] = a;
  FinishFlame(); // skip this part to finish setting values
}

void SpawnFromMouth(void) {
  lda_zpy(Enemy_X_Position); // get bowser's horizontal position
  carry_flag = true;
  sbc_imm(0xe); // subtract 14 pixels
  ram[Enemy_X_Position + x] = a; // save as flame's horizontal position
  lda_zpy(Enemy_PageLoc);
  ram[Enemy_PageLoc + x] = a; // copy page location from bowser to flame
  lda_zpy(Enemy_Y_Position);
  carry_flag = false; // add 8 pixels to bowser's vertical position
  adc_imm(0x8);
  ram[Enemy_Y_Position + x] = a; // save as flame's vertical position
  lda_absx(PseudoRandomBitReg);
  and_imm(0b00000011); // get 2 LSB from first part of LSFR
  ram[Enemy_YMF_Dummy + x] = a; // save here
  tay(); // use as offset
  lda_absy(FlameYPosData); // get value here using bits as offset
  ldy_imm(0x0); // load default offset
  cmp_zpx(Enemy_Y_Position); // compare value to flame's current vertical position
  // if less, do not increment offset
  if (carry_flag) {
    iny(); // otherwise increment now
  }
  // SetMF:
  lda_absy(FlameYMFAdderData); // get value here and save
  ram[Enemy_Y_MoveForce + x] = a; // to vertical movement force
  lda_imm(0x0);
  ram[EnemyFrenzyBuffer] = a; // clear enemy frenzy buffer
  FinishFlame(); // <fallthrough>
}

void FinishFlame(void) {
  lda_imm(0x8); // set $08 for bounding box control
  ram[Enemy_BoundBoxCtrl + x] = a;
  lda_imm(0x1); // set high byte of vertical and
  ram[Enemy_Y_HighPos + x] = a; // enemy buffer flag
  ram[Enemy_Flag + x] = a;
  lsr_acc();
  ram[Enemy_X_MoveForce + x] = a; // initialize horizontal movement force, and
  ram[Enemy_State + x] = a; // enemy state
  // --------------------------------
}

void InitFireworks(void) {
  lda_abs(FrenzyEnemyTimer); // if timer not expired yet, branch to leave
  if (zero_flag) {
    lda_imm(0x20); // otherwise reset timer
    ram[FrenzyEnemyTimer] = a;
    dec_abs(FireworksCounter); // decrement for each explosion
    ldy_imm(0x6); // start at last slot
    
StarFChk:
    dey();
    lda_zpy(Enemy_ID); // check for presence of star flag object
    cmp_imm(StarFlagObject); // if there isn't a star flag object,
    if (!zero_flag) { goto StarFChk; } // routine goes into infinite loop = crash
    lda_zpy(Enemy_X_Position);
    carry_flag = true; // get horizontal coordinate of star flag object, then
    sbc_imm(0x30); // subtract 48 pixels from it and save to
    pha(); // the stack
    lda_zpy(Enemy_PageLoc);
    sbc_imm(0x0); // subtract the carry from the page location
    ram[0x0] = a; // of the star flag object
    lda_abs(FireworksCounter); // get fireworks counter
    carry_flag = false;
    adc_zpy(Enemy_State); // add state of star flag object (possibly not necessary)
    tay(); // use as offset
    pla(); // get saved horizontal coordinate of star flag - 48 pixels
    carry_flag = false;
    adc_absy(FireworksXPosData); // add number based on offset of fireworks counter
    ram[Enemy_X_Position + x] = a; // store as the fireworks object horizontal coordinate
    lda_zp(0x0);
    adc_imm(0x0); // add carry and store as page location for
    ram[Enemy_PageLoc + x] = a; // the fireworks object
    lda_absy(FireworksYPosData); // get vertical position using same offset
    ram[Enemy_Y_Position + x] = a; // and store as vertical coordinate for fireworks object
    lda_imm(0x1);
    ram[Enemy_Y_HighPos + x] = a; // store in vertical high byte
    ram[Enemy_Flag + x] = a; // and activate enemy buffer flag
    lsr_acc();
    ram[ExplosionGfxCounter + x] = a; // initialize explosion counter
    lda_imm(0x8);
    ram[ExplosionTimerCounter + x] = a; // set explosion timing counter
    // --------------------------------
  }
}

void BulletBillCheepCheep(void) {
  lda_abs(FrenzyEnemyTimer); // if timer not expired yet, branch to leave
  if (!zero_flag) { goto ExF17; }
  lda_abs(AreaType); // are we in a water-type level?
  if (!zero_flag) { goto DoBulletBills; } // if not, branch elsewhere
  cpx_imm(0x3); // are we past third enemy slot?
  if (carry_flag) { goto ExF17; } // if so, branch to leave
  ldy_imm(0x0); // load default offset
  lda_absx(PseudoRandomBitReg);
  cmp_imm(0xaa); // check first part of LSFR against preset value
  if (!carry_flag) { goto ChkW2; } // if less than preset, do not increment offset
  iny(); // otherwise increment
  
ChkW2:
  lda_abs(WorldNumber); // check world number
  cmp_imm(World2);
  if (zero_flag) { goto Get17ID; } // if we're on world 2, do not increment offset
  iny(); // otherwise increment
  
Get17ID:
  tya();
  and_imm(0b00000001); // mask out all but last bit of offset
  tay();
  lda_absy(SwimCC_IDData); // load identifier for cheep-cheeps
  
Set17ID:
  ram[Enemy_ID + x] = a; // store whatever's in A as enemy identifier
  lda_abs(BitMFilter);
  cmp_imm(0xff); // if not all bits set, skip init part and compare bits
  if (!zero_flag) { goto GetRBit; }
  lda_imm(0x0); // initialize vertical position filter
  ram[BitMFilter] = a;
  
GetRBit:
  lda_absx(PseudoRandomBitReg); // get first part of LSFR
  and_imm(0b00000111); // mask out all but 3 LSB
  
ChkRBit:
  tay(); // use as offset
  lda_absy(Bitmasks); // load bitmask
  bit_abs(BitMFilter); // perform AND on filter without changing it
  if (zero_flag) { goto AddFBit; }
  iny(); // increment offset
  tya();
  and_imm(0b00000111); // mask out all but 3 LSB thus keeping it 0-7
  goto ChkRBit; // do another check
  
AddFBit:
  ora_abs(BitMFilter); // add bit to already set bits in filter
  ram[BitMFilter] = a; // and store
  lda_absy(Enemy17YPosData); // load vertical position using offset
  PutAtRightExtent(); // set vertical position and other values
  ram[Enemy_YMF_Dummy + x] = a; // initialize dummy variable
  lda_imm(0x20); // set timer
  ram[FrenzyEnemyTimer] = a;
  CheckpointEnemyID(); return; // process our new enemy object
  
DoBulletBills:
  ldy_imm(0xff); // start at beginning of enemy slots
  
BB_SLoop:
  iny(); // move onto the next slot
  cpy_imm(0x5); // branch to play sound if we've done all slots
  if (carry_flag) { goto FireBulletBill; }
  lda_zpy(Enemy_Flag); // if enemy buffer flag not set,
  if (zero_flag) { goto BB_SLoop; } // loop back and check another slot
  lda_zpy(Enemy_ID);
  cmp_imm(BulletBill_FrenzyVar); // check enemy identifier for
  if (!zero_flag) { goto BB_SLoop; } // bullet bill object (frenzy variant)
  
ExF17:
  return; // if found, leave
  
FireBulletBill:
  lda_zp(Square2SoundQueue);
  ora_imm(Sfx_Blast); // play fireworks/gunfire sound
  ram[Square2SoundQueue] = a;
  lda_imm(BulletBill_FrenzyVar); // load identifier for bullet bill object
  if (!zero_flag) { goto Set17ID; } // unconditional branch
  // --------------------------------
  // $00 - used to store Y position of group enemies
  // $01 - used to store enemy ID
  // $02 - used to store page location of right side of screen
  // $03 - used to store X position of right side of screen
  HandleGroupEnemies(); // <fallthrough>
}

void HandleGroupEnemies(void) {
  ldy_imm(0x0); // load value for green koopa troopa
  carry_flag = true;
  sbc_imm(0x37); // subtract $37 from second byte read
  pha(); // save result in stack for now
  cmp_imm(0x4); // was byte in $3b-$3e range?
  // if so, branch
  if (!carry_flag) {
    pha(); // save another copy to stack
    ldy_imm(Goomba); // load value for goomba enemy
    lda_abs(PrimaryHardMode); // if primary hard mode flag not set,
    // branch, otherwise change to value
    if (!zero_flag) {
      ldy_imm(BuzzyBeetle); // for buzzy beetle
    }
    // PullID:
    pla(); // get second copy from stack
  }
  // SnglID:
  ram[0x1] = y; // save enemy id here
  ldy_imm(0xb0); // load default y coordinate
  and_imm(0x2); // check to see if d1 was set
  // if so, move y coordinate up,
  if (!zero_flag) {
    ldy_imm(0x70); // otherwise branch and use default
  }
  // SetYGp:
  ram[0x0] = y; // save y coordinate here
  lda_abs(ScreenRight_PageLoc); // get page number of right edge of screen
  ram[0x2] = a; // save here
  lda_abs(ScreenRight_X_Pos); // get pixel coordinate of right edge
  ram[0x3] = a; // save here
  ldy_imm(0x2); // load two enemies by default
  pla(); // get first copy from stack
  lsr_acc(); // check to see if d0 was set
  // if not, use default value
  if (carry_flag) {
    iny(); // otherwise increment to three enemies
  }
  // CntGrp:
  ram[NumberofGroupEnemies] = y; // save number of enemies here
  
GrLoop:
  ldx_imm(0xff); // start at beginning of enemy buffers
  
GSltLp:
  inx(); // increment and branch if past
  cpx_imm(0x5); // end of buffers
  if (!carry_flag) {
    lda_zpx(Enemy_Flag); // check to see if enemy is already
    if (!zero_flag) { goto GSltLp; } // stored in buffer, and branch if so
    lda_zp(0x1);
    ram[Enemy_ID + x] = a; // store enemy object identifier
    lda_zp(0x2);
    ram[Enemy_PageLoc + x] = a; // store page location for enemy object
    lda_zp(0x3);
    ram[Enemy_X_Position + x] = a; // store x coordinate for enemy object
    carry_flag = false;
    adc_imm(0x18); // add 24 pixels for next enemy
    ram[0x3] = a;
    lda_zp(0x2); // add carry to page location for
    adc_imm(0x0); // next enemy
    ram[0x2] = a;
    lda_zp(0x0); // store y coordinate for enemy object
    ram[Enemy_Y_Position + x] = a;
    lda_imm(0x1); // activate flag for buffer, and
    ram[Enemy_Y_HighPos + x] = a; // put enemy within the screen vertically
    ram[Enemy_Flag + x] = a;
    CheckpointEnemyID(); // process each enemy object separately
    dec_abs(NumberofGroupEnemies); // do this until we run out of enemy objects
    if (!zero_flag) { goto GrLoop; }
  }
  // NextED:
  Inc2B(); // jump to increment data offset and leave
  // --------------------------------
}

void InitPiranhaPlant(void) {
  lda_imm(0x1); // set initial speed
  ram[PiranhaPlant_Y_Speed + x] = a;
  lsr_acc();
  ram[Enemy_State + x] = a; // initialize enemy state and what would normally
  ram[PiranhaPlant_MoveFlag + x] = a; // be used as vertical speed, but not in this case
  lda_zpx(Enemy_Y_Position);
  ram[PiranhaPlantDownYPos + x] = a; // save original vertical coordinate here
  carry_flag = true;
  sbc_imm(0x18);
  ram[PiranhaPlantUpYPos + x] = a; // save original vertical coordinate - 24 pixels here
  lda_imm(0x9);
  SetBBox2(); // set specific value for bounding box control
  // --------------------------------
}

void InitEnemyFrenzy(void) {
  lda_zpx(Enemy_ID); // load enemy identifier
  ram[EnemyFrenzyBuffer] = a; // save in enemy frenzy buffer
  carry_flag = true;
  sbc_imm(0x12); // subtract 12 and use as offset for jump engine
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: LakituAndSpinyHandler(); return;
    case 1: NoFrenzyCode(); return;
    case 2: InitFlyingCheepCheep(); return;
    case 3: InitBowserFlame(); return;
    case 4: InitFireworks(); return;
    case 5: BulletBillCheepCheep(); return;
  }
}

void NoFrenzyCode(void) {
  // --------------------------------
}

void EndFrenzy(void) {
  ldy_imm(0x5); // start at last slot
  
LakituChk:
  lda_zpy(Enemy_ID); // check enemy identifiers
  cmp_imm(Lakitu); // for lakitu
  if (zero_flag) {
    lda_imm(0x1); // if found, set state
    ram[Enemy_State + y] = a;
  }
  // NextFSlot:
  dey(); // move onto the next slot
  if (!neg_flag) { goto LakituChk; } // do this until all slots are checked
  lda_imm(0x0);
  ram[EnemyFrenzyBuffer] = a; // empty enemy frenzy buffer
  ram[Enemy_Flag + x] = a; // disable enemy buffer flag for this object
  // --------------------------------
}

void InitJumpGPTroopa(void) {
  lda_imm(0x2); // set for movement to the left
  ram[Enemy_MovingDir + x] = a;
  lda_imm(0xf8); // set horizontal speed
  ram[Enemy_X_Speed + x] = a;
  TallBBox2(); // <fallthrough>
}

void TallBBox2(void) {
  lda_imm(0x3); // set specific value for bounding box control
  SetBBox2(); // <fallthrough>
}

void SetBBox2(void) {
  ram[Enemy_BoundBoxCtrl + x] = a; // set bounding box control then leave
  // --------------------------------
}

void InitBalPlatform(void) {
  dec_zpx(Enemy_Y_Position); // raise vertical position by two pixels
  dec_zpx(Enemy_Y_Position);
  ldy_abs(SecondaryHardMode); // if secondary hard mode flag not set,
  // branch ahead
  if (zero_flag) {
    ldy_imm(0x2); // otherwise set value here
    PosPlatform(); // do a sub to add or subtract pixels
  }
  // AlignP:
  ldy_imm(0xff); // set default value here for now
  lda_abs(BalPlatformAlignment); // get current balance platform alignment
  ram[Enemy_State + x] = a; // set platform alignment to object state here
  // if old alignment $ff, put $ff as alignment for negative
  if (neg_flag) {
    txa(); // if old contents already $ff, put
    tay(); // object offset as alignment to make next positive
  }
  // SetBPA:
  ram[BalPlatformAlignment] = y; // store whatever value's in Y here
  lda_imm(0x0);
  ram[Enemy_MovingDir + x] = a; // init moving direction
  tay(); // init Y
  PosPlatform(); // do a sub to add 8 pixels, then run shared code here
  // --------------------------------
  InitDropPlatform(); // <fallthrough>
}

void InitDropPlatform(void) {
  lda_imm(0xff);
  ram[PlatformCollisionFlag + x] = a; // set some value here
  CommonPlatCode(); // then jump ahead to execute more code
  // --------------------------------
}

void InitHoriPlatform(void) {
  lda_imm(0x0);
  ram[XMoveSecondaryCounter + x] = a; // init one of the moving counters
  CommonPlatCode(); // jump ahead to execute more code
  // --------------------------------
}

void InitVertPlatform(void) {
  ldy_imm(0x40); // set default value here
  lda_zpx(Enemy_Y_Position); // check vertical position
  // if above a certain point, skip this part
  if (neg_flag) {
    eor_imm(0xff);
    carry_flag = false; // otherwise get two's compliment
    adc_imm(0x1);
    ldy_imm(0xc0); // get alternate value to add to vertical position
  }
  // SetYO:
  ram[YPlatformTopYPos + x] = a; // save as top vertical position
  tya();
  carry_flag = false; // load value from earlier, add number of pixels
  adc_zpx(Enemy_Y_Position); // to vertical position
  ram[YPlatformCenterYPos + x] = a; // save result as central vertical position
  // --------------------------------
  CommonPlatCode(); // <fallthrough>
}

void CommonPlatCode(void) {
  InitVStf(); // do a sub to init certain other values
  SPBBox(); // <fallthrough>
}

void SPBBox(void) {
  lda_imm(0x5); // set default bounding box size control
  ldy_abs(AreaType);
  cpy_imm(0x3); // check for castle-type level
  // use default value if found
  if (!zero_flag) {
    ldy_abs(SecondaryHardMode); // otherwise check for secondary hard mode flag
    // if set, use default value
    if (zero_flag) {
      lda_imm(0x6); // use alternate value if not castle or secondary not set
    }
  }
  // CasPBB:
  ram[Enemy_BoundBoxCtrl + x] = a; // set bounding box size control here and leave
  // --------------------------------
}

void LargeLiftUp(void) {
  PlatLiftUp(); // execute code for platforms going up
  LargeLiftBBox(); // overwrite bounding box for large platforms
}

void LargeLiftDown(void) {
  PlatLiftDown(); // execute code for platforms going down
  LargeLiftBBox(); // <fallthrough>
}

void LargeLiftBBox(void) {
  SPBBox(); // jump to overwrite bounding box size control
  // --------------------------------
}

void PlatLiftUp(void) {
  lda_imm(0x10); // set movement amount here
  ram[Enemy_Y_MoveForce + x] = a;
  lda_imm(0xff); // set moving speed for platforms going up
  ram[Enemy_Y_Speed + x] = a;
  CommonSmallLift(); // skip ahead to part we should be executing
  // --------------------------------
}

void PlatLiftDown(void) {
  lda_imm(0xf0); // set movement amount here
  ram[Enemy_Y_MoveForce + x] = a;
  lda_imm(0x0); // set moving speed for platforms going down
  ram[Enemy_Y_Speed + x] = a;
  // --------------------------------
  CommonSmallLift(); // <fallthrough>
}

void CommonSmallLift(void) {
  ldy_imm(0x1);
  PosPlatform(); // do a sub to add 12 pixels due to preset value
  lda_imm(0x4);
  ram[Enemy_BoundBoxCtrl + x] = a; // set bounding box control for small platforms
  // --------------------------------
}

void EndOfEnemyInitCode(void) {
  // -------------------------------------------------------------------------------------
}

void DuplicateEnemyObj(void) {
  ldy_imm(0xff); // start at beginning of enemy slots
  
FSLoop:
  iny(); // increment one slot
  lda_zpy(Enemy_Flag); // check enemy buffer flag for empty slot
  if (!zero_flag) { goto FSLoop; } // if set, branch and keep checking
  ram[DuplicateObj_Offset] = y; // otherwise set offset here
  txa(); // transfer original enemy buffer offset
  ora_imm(0b10000000); // store with d7 set as flag in new enemy
  ram[Enemy_Flag + y] = a; // slot as well as enemy offset
  lda_zpx(Enemy_PageLoc);
  ram[Enemy_PageLoc + y] = a; // copy page location and horizontal coordinates
  lda_zpx(Enemy_X_Position); // from original enemy to new enemy
  ram[Enemy_X_Position + y] = a;
  lda_imm(0x1);
  ram[Enemy_Flag + x] = a; // set flag as normal for original enemy
  ram[Enemy_Y_HighPos + y] = a; // set high vertical byte for new enemy
  lda_zpx(Enemy_Y_Position);
  ram[Enemy_Y_Position + y] = a; // copy vertical coordinate from original to new
  // --------------------------------
}

void PosPlatform(void) {
  lda_zpx(Enemy_X_Position); // get horizontal coordinate
  carry_flag = false;
  adc_absy(PlatPosDataLow); // add or subtract pixels depending on offset
  ram[Enemy_X_Position + x] = a; // store as new horizontal coordinate
  lda_zpx(Enemy_PageLoc);
  adc_absy(PlatPosDataHigh); // add or subtract page location depending on offset
  ram[Enemy_PageLoc + x] = a; // store as new page location
  // --------------------------------
}

void RunEnemyObjectsCore(void) {
  ldx_zp(ObjectOffset); // get offset for enemy object buffer
  lda_imm(0x0); // load value 0 for jump engine by default
  ldy_zpx(Enemy_ID);
  cpy_imm(0x15); // if enemy object < $15, use default value
  if (carry_flag) {
    tya(); // otherwise subtract $14 from the value and use
    sbc_imm(0x14); // as value for jump engine
  }
  // JmpEO:
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: RunNormalEnemies(); return;
    case 1: RunBowserFlame(); return;
    case 2: RunFireworks(); return;
    case 3: NoRunCode(); return;
    case 4: NoRunCode(); return;
    case 5: NoRunCode(); return;
    case 6: NoRunCode(); return;
    case 7: RunFirebarObj(); return;
    case 8: RunFirebarObj(); return;
    case 9: RunFirebarObj(); return;
    case 10: RunFirebarObj(); return;
    case 11: RunFirebarObj(); return;
    case 12: RunFirebarObj(); return;
    case 13: RunFirebarObj(); return;
    case 14: RunFirebarObj(); return;
    case 15: NoRunCode(); return;
    case 16: RunLargePlatform(); return;
    case 17: RunLargePlatform(); return;
    case 18: RunLargePlatform(); return;
    case 19: RunLargePlatform(); return;
    case 20: RunLargePlatform(); return;
    case 21: RunLargePlatform(); return;
    case 22: RunLargePlatform(); return;
    case 23: RunSmallPlatform(); return;
    case 24: RunSmallPlatform(); return;
    case 25: RunBowser(); return;
    case 26: PowerUpObjHandler(); return;
    case 27: VineObjectHandler(); return;
    case 28: NoRunCode(); return;
    case 29: RunStarFlagObj(); return;
    case 30: JumpspringHandler(); return;
    case 31: NoRunCode(); return;
    case 32: WarpZoneObject(); return;
    case 33: RunRetainerObj(); return;
  }
}

void NoRunCode(void) {
  // --------------------------------
}

void RunRetainerObj(void) {
  GetEnemyOffscreenBits();
  RelativeEnemyPosition();
  EnemyGfxHandler(); return;
  // --------------------------------
}

void RunNormalEnemies(void) {
  lda_imm(0x0); // init sprite attributes
  ram[Enemy_SprAttrib + x] = a;
  GetEnemyOffscreenBits();
  RelativeEnemyPosition();
  EnemyGfxHandler();
  GetEnemyBoundBox();
  EnemyToBGCollisionDet();
  EnemiesCollision();
  PlayerEnemyCollision();
  ldy_abs(TimerControl); // if master timer control set, skip to last routine
  if (zero_flag) {
    EnemyMovementSubs();
  }
  // SkipMove:
  OffscreenBoundsCheck(); return;
}

void RunBowserFlame(void) {
  ProcBowserFlame();
  GetEnemyOffscreenBits();
  RelativeEnemyPosition();
  GetEnemyBoundBox();
  PlayerEnemyCollision();
  OffscreenBoundsCheck(); return;
  // --------------------------------
}

void RunFirebarObj(void) {
  ProcFirebar();
  OffscreenBoundsCheck(); return;
  // --------------------------------
}

void RunSmallPlatform(void) {
  GetEnemyOffscreenBits();
  RelativeEnemyPosition();
  SmallPlatformBoundBox();
  SmallPlatformCollision();
  RelativeEnemyPosition();
  DrawSmallPlatform();
  MoveSmallPlatform();
  OffscreenBoundsCheck(); return;
  // --------------------------------
}

void RunLargePlatform(void) {
  GetEnemyOffscreenBits();
  RelativeEnemyPosition();
  LargePlatformBoundBox();
  LargePlatformCollision();
  lda_abs(TimerControl); // if master timer control set,
  // skip subroutine tree
  if (zero_flag) {
    LargePlatformSubroutines();
  }
  // SkipPT:
  RelativeEnemyPosition();
  DrawLargePlatform();
  OffscreenBoundsCheck(); return;
  // --------------------------------
}

void RunBowser(void) {
  lda_zpx(Enemy_State); // if d5 in enemy state is not set
  and_imm(0b00100000); // then branch elsewhere to run bowser
  if (zero_flag) {
    BowserControl();
    return;
  }
  lda_zpx(Enemy_Y_Position); // otherwise check vertical position
  cmp_imm(0xe0); // if above a certain point, branch to move defeated bowser
  // otherwise proceed to KillAllEnemies
  if (!carry_flag) {
    MoveD_Bowser();
    return;
  }
  KillAllEnemies(); // <fallthrough>
}

void KillAllEnemies(void) {
  ldx_imm(0x4); // start with last enemy slot
  
KillLoop:
  EraseEnemyObject(); // branch to kill enemy objects
  dex(); // move onto next enemy slot
  if (!neg_flag) { goto KillLoop; } // do this until all slots are emptied
  ram[EnemyFrenzyBuffer] = a; // empty frenzy buffer
  ldx_zp(ObjectOffset); // get enemy object offset and leave
}

void BowserControl(void) {
  lda_imm(0x0);
  ram[EnemyFrenzyBuffer] = a; // empty frenzy buffer
  lda_abs(TimerControl); // if master timer control not set,
  if (zero_flag) { goto ChkMouth; } // skip jump and execute code here
  goto SkipToFB; // otherwise, jump over a bunch of code
  
ChkMouth:
  lda_abs(BowserBodyControls); // check bowser's mouth
  if (!neg_flag) { goto FeetTmr; } // if bit clear, go ahead with code here
  goto HammerChk; // otherwise skip a whole section starting here
  
FeetTmr:
  dec_abs(BowserFeetCounter); // decrement timer to control bowser's feet
  if (!zero_flag) { goto ResetMDr; } // if not expired, skip this part
  lda_imm(0x20); // otherwise, reset timer
  ram[BowserFeetCounter] = a;
  lda_abs(BowserBodyControls); // and invert bit used
  eor_imm(0b00000001); // to control bowser's feet
  ram[BowserBodyControls] = a;
  
ResetMDr:
  lda_zp(FrameCounter); // check frame counter
  and_imm(0b00001111); // if not on every sixteenth frame, skip
  if (!zero_flag) { goto B_FaceP; } // ahead to continue code
  lda_imm(0x2); // otherwise reset moving/facing direction every
  ram[Enemy_MovingDir + x] = a; // sixteen frames
  
B_FaceP:
  lda_absx(EnemyFrameTimer); // if timer set here expired,
  if (zero_flag) { goto GetPRCmp; } // branch to next section
  PlayerEnemyDiff(); // get horizontal difference between player and bowser,
  if (!neg_flag) { goto GetPRCmp; } // and branch if bowser to the right of the player
  lda_imm(0x1);
  ram[Enemy_MovingDir + x] = a; // set bowser to move and face to the right
  lda_imm(0x2);
  ram[BowserMovementSpeed] = a; // set movement speed
  lda_imm(0x20);
  ram[EnemyFrameTimer + x] = a; // set timer here
  ram[BowserFireBreathTimer] = a; // set timer used for bowser's flame
  lda_zpx(Enemy_X_Position);
  cmp_imm(0xc8); // if bowser to the right past a certain point,
  if (carry_flag) { goto HammerChk; } // skip ahead to some other section
  
GetPRCmp:
  lda_zp(FrameCounter); // get frame counter
  and_imm(0b00000011);
  if (!zero_flag) { goto HammerChk; } // execute this code every fourth frame, otherwise branch
  lda_zpx(Enemy_X_Position);
  cmp_abs(BowserOrigXPos); // if bowser not at original horizontal position,
  if (!zero_flag) { goto GetDToO; } // branch to skip this part
  lda_absx(PseudoRandomBitReg);
  and_imm(0b00000011); // get pseudorandom offset
  tay();
  lda_absy(PRandomRange); // load value using pseudorandom offset
  ram[MaxRangeFromOrigin] = a; // and store here
  
GetDToO:
  lda_zpx(Enemy_X_Position);
  carry_flag = false; // add movement speed to bowser's horizontal
  adc_abs(BowserMovementSpeed); // coordinate and save as new horizontal position
  ram[Enemy_X_Position + x] = a;
  ldy_zpx(Enemy_MovingDir);
  cpy_imm(0x1); // if bowser moving and facing to the right, skip ahead
  if (zero_flag) { goto HammerChk; }
  ldy_imm(0xff); // set default movement speed here (move left)
  carry_flag = true; // get difference of current vs. original
  sbc_abs(BowserOrigXPos); // horizontal position
  if (!neg_flag) { goto CompDToO; } // if current position to the right of original, skip ahead
  eor_imm(0xff);
  carry_flag = false; // get two's compliment
  adc_imm(0x1);
  ldy_imm(0x1); // set alternate movement speed here (move right)
  
CompDToO:
  cmp_abs(MaxRangeFromOrigin); // compare difference with pseudorandom value
  if (!carry_flag) { goto HammerChk; } // if difference < pseudorandom value, leave speed alone
  ram[BowserMovementSpeed] = y; // otherwise change bowser's movement speed
  
HammerChk:
  lda_absx(EnemyFrameTimer); // if timer set here not expired yet, skip ahead to
  if (!zero_flag) { goto MakeBJump; } // some other section of code
  MoveEnemySlowVert(); // otherwise start by moving bowser downwards
  lda_abs(WorldNumber); // check world number
  cmp_imm(World6);
  if (!carry_flag) { goto SetHmrTmr; } // if world 1-5, skip this part (not time to throw hammers yet)
  lda_zp(FrameCounter);
  and_imm(0b00000011); // check to see if it's time to execute sub
  if (!zero_flag) { goto SetHmrTmr; } // if not, skip sub, otherwise
  SpawnHammerObj(); // execute sub on every fourth frame to spawn misc object (hammer)
  
SetHmrTmr:
  lda_zpx(Enemy_Y_Position); // get current vertical position
  cmp_imm(0x80); // if still above a certain point
  if (!carry_flag) { goto ChkFireB; } // then skip to world number check for flames
  lda_absx(PseudoRandomBitReg);
  and_imm(0b00000011); // get pseudorandom offset
  tay();
  lda_absy(PRandomRange); // get value using pseudorandom offset
  ram[EnemyFrameTimer + x] = a; // set for timer here
  
SkipToFB:
  goto ChkFireB; // jump to execute flames code
  
MakeBJump:
  cmp_imm(0x1); // if timer not yet about to expire,
  if (!zero_flag) { goto ChkFireB; } // skip ahead to next part
  dec_zpx(Enemy_Y_Position); // otherwise decrement vertical coordinate
  InitVStf(); // initialize movement amount
  lda_imm(0xfe);
  ram[Enemy_Y_Speed + x] = a; // set vertical speed to move bowser upwards
  
ChkFireB:
  lda_abs(WorldNumber); // check world number here
  cmp_imm(World8); // world 8?
  if (zero_flag) { goto SpawnFBr; } // if so, execute this part here
  cmp_imm(World6); // world 6-7?
  if (carry_flag) { BowserGfxHandler(); return; } // if so, skip this part here
  
SpawnFBr:
  lda_abs(BowserFireBreathTimer); // check timer here
  if (!zero_flag) { BowserGfxHandler(); return; } // if not expired yet, skip all of this
  lda_imm(0x20);
  ram[BowserFireBreathTimer] = a; // set timer here
  lda_abs(BowserBodyControls);
  eor_imm(0b10000000); // invert bowser's mouth bit to open
  ram[BowserBodyControls] = a; // and close bowser's mouth
  if (neg_flag) { goto ChkFireB; } // if bowser's mouth open, loop back
  SetFlameTimer(); // get timing for bowser's flame
  ldy_abs(SecondaryHardMode);
  if (zero_flag) { goto SetFBTmr; } // if secondary hard mode flag not set, skip this
  carry_flag = true;
  sbc_imm(0x10); // otherwise subtract from value in A
  
SetFBTmr:
  ram[BowserFireBreathTimer] = a; // set value as timer here
  lda_imm(BowserFlame); // put bowser's flame identifier
  ram[EnemyFrenzyBuffer] = a; // in enemy frenzy buffer
  // --------------------------------
  BowserGfxHandler(); // <fallthrough>
}

void BowserGfxHandler(void) {
  ProcessBowserHalf(); // do a sub here to process bowser's front
  ldy_imm(0x10); // load default value here to position bowser's rear
  lda_zpx(Enemy_MovingDir); // check moving direction
  lsr_acc();
  // if moving left, use default
  if (carry_flag) {
    ldy_imm(0xf0); // otherwise load alternate positioning value here
  }
  // CopyFToR:
  tya(); // move bowser's rear object position value to A
  carry_flag = false;
  adc_zpx(Enemy_X_Position); // add to bowser's front object horizontal coordinate
  ldy_abs(DuplicateObj_Offset); // get bowser's rear object offset
  ram[Enemy_X_Position + y] = a; // store A as bowser's rear horizontal coordinate
  lda_zpx(Enemy_Y_Position);
  carry_flag = false; // add eight pixels to bowser's front object
  adc_imm(0x8); // vertical coordinate and store as vertical coordinate
  ram[Enemy_Y_Position + y] = a; // for bowser's rear
  lda_zpx(Enemy_State);
  ram[Enemy_State + y] = a; // copy enemy state directly from front to rear
  lda_zpx(Enemy_MovingDir);
  ram[Enemy_MovingDir + y] = a; // copy moving direction also
  lda_zp(ObjectOffset); // save enemy object offset of front to stack
  pha();
  ldx_abs(DuplicateObj_Offset); // put enemy object offset of rear as current
  ram[ObjectOffset] = x;
  lda_imm(Bowser); // set bowser's enemy identifier
  ram[Enemy_ID + x] = a; // store in bowser's rear object
  ProcessBowserHalf(); // do a sub here to process bowser's rear
  pla();
  ram[ObjectOffset] = a; // get original enemy object offset
  tax();
  lda_imm(0x0); // nullify bowser's front/rear graphics flag
  ram[BowserGfxFlag] = a;
}

void RunFireworks(void) {
  dec_zpx(ExplosionTimerCounter); // decrement explosion timing counter here
  if (!zero_flag) { goto SetupExpl; } // if not expired, skip this part
  lda_imm(0x8);
  ram[ExplosionTimerCounter + x] = a; // reset counter
  inc_zpx(ExplosionGfxCounter); // increment explosion graphics counter
  lda_zpx(ExplosionGfxCounter);
  cmp_imm(0x3); // check explosion graphics counter
  if (carry_flag) { goto FireworksSoundScore; } // if at a certain point, branch to kill this object
  
SetupExpl:
  RelativeEnemyPosition(); // get relative coordinates of explosion
  lda_abs(Enemy_Rel_YPos); // copy relative coordinates
  ram[Fireball_Rel_YPos] = a; // from the enemy object to the fireball object
  lda_abs(Enemy_Rel_XPos); // first vertical, then horizontal
  ram[Fireball_Rel_XPos] = a;
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  lda_zpx(ExplosionGfxCounter); // get explosion graphics counter
  DrawExplosion_Fireworks(); // do a sub to draw the explosion then leave
  return;
  
FireworksSoundScore:
  lda_imm(0x0); // disable enemy buffer flag
  ram[Enemy_Flag + x] = a;
  lda_imm(Sfx_Blast); // play fireworks/gunfire sound
  ram[Square2SoundQueue] = a;
  lda_imm(0x5); // set part of score modifier for 500 points
  ram[DigitModifier + 4] = a;
  EndAreaPoints(); // jump to award points accordingly then leave
  // --------------------------------
}

void RunStarFlagObj(void) {
  lda_imm(0x0); // initialize enemy frenzy buffer
  ram[EnemyFrenzyBuffer] = a;
  lda_abs(StarFlagTaskControl); // check star flag object task number here
  cmp_imm(0x5); // if greater than 5, branch to exit
  if (carry_flag) {
    StarFlagExit();
    return;
  }
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: StarFlagExit(); return;
    case 1: GameTimerFireworks(); return;
    case 2: AwardGameTimerPoints(); return;
    case 3: RaiseFlagSetoffFWorks(); return;
    case 4: DelayToAreaEnd(); return;
  }
}

void GameTimerFireworks(void) {
  ldy_imm(0x5); // set default state for star flag object
  lda_abs(GameTimerDisplay + 2); // get game timer's last digit
  cmp_imm(0x1);
  // if last digit of game timer set to 1, skip ahead
  if (!zero_flag) {
    ldy_imm(0x3); // otherwise load new value for state
    cmp_imm(0x3);
    // if last digit of game timer set to 3, skip ahead
    if (!zero_flag) {
      ldy_imm(0x0); // otherwise load one more potential value for state
      cmp_imm(0x6);
      // if last digit of game timer set to 6, skip ahead
      if (!zero_flag) {
        lda_imm(0xff); // otherwise set value for no fireworks
      }
    }
  }
  // SetFWC:
  ram[FireworksCounter] = a; // set fireworks counter here
  ram[Enemy_State + x] = y; // set whatever state we have in star flag object
  IncrementSFTask1(); // <fallthrough>
}

void IncrementSFTask1(void) {
  inc_abs(StarFlagTaskControl); // increment star flag object task number
  StarFlagExit(); // <fallthrough>
}

void StarFlagExit(void) {
}

void AwardGameTimerPoints(void) {
  lda_abs(GameTimerDisplay); // check all game timer digits for any intervals left
  ora_abs(GameTimerDisplay + 1);
  ora_abs(GameTimerDisplay + 2);
  // if no time left on game timer at all, branch to next task
  if (zero_flag) {
    IncrementSFTask1();
    return;
  }
  lda_zp(FrameCounter);
  and_imm(0b00000100); // check frame counter for d2 set (skip ahead
  // for four frames every four frames) branch if not set
  if (!zero_flag) {
    lda_imm(Sfx_TimerTick);
    ram[Square2SoundQueue] = a; // load timer tick sound
  }
  // NoTTick:
  ldy_imm(0x23); // set offset here to subtract from game timer's last digit
  lda_imm(0xff); // set adder here to $ff, or -1, to subtract one
  ram[DigitModifier + 5] = a; // from the last digit of the game timer
  DigitsMathRoutine(); // subtract digit
  lda_imm(0x5); // set now to add 50 points
  ram[DigitModifier + 5] = a; // per game timer interval subtracted
  EndAreaPoints(); // <fallthrough>
}

void EndAreaPoints(void) {
  ldy_imm(0xb); // load offset for mario's score by default
  lda_abs(CurrentPlayer); // check player on the screen
  // if mario, do not change
  if (!zero_flag) {
    ldy_imm(0x11); // otherwise load offset for luigi's score
  }
  // ELPGive:
  DigitsMathRoutine(); // award 50 points per game timer interval
  lda_abs(CurrentPlayer); // get player on the screen (or 500 points per
  asl_acc(); // fireworks explosion if branched here from there)
  asl_acc(); // shift to high nybble
  asl_acc();
  asl_acc();
  ora_imm(0b00000100); // add four to set nybble for game timer
  UpdateNumber(); return; // jump to print the new score and game timer
}

void RaiseFlagSetoffFWorks(void) {
  lda_zpx(Enemy_Y_Position); // check star flag's vertical position
  cmp_imm(0x72); // against preset value
  // if star flag higher vertically, branch to other code
  if (carry_flag) {
    dec_zpx(Enemy_Y_Position); // otherwise, raise star flag by one pixel
    DrawStarFlag(); return; // and skip this part here
  }
  // SetoffF:
  lda_abs(FireworksCounter); // check fireworks counter
  // if no fireworks left to go off, skip this part
  if (zero_flag) {
    DrawFlagSetTimer();
    return;
  }
  // if no fireworks set to go off, skip this part
  if (neg_flag) {
    DrawFlagSetTimer();
    return;
  }
  lda_imm(Fireworks);
  ram[EnemyFrenzyBuffer] = a; // otherwise set fireworks object in frenzy queue
  DrawStarFlag(); // <fallthrough>
}

void DrawStarFlag(void) {
  RelativeEnemyPosition(); // get relative coordinates of star flag
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  ldx_imm(0x3); // do four sprites
  
DSFLoop:
  lda_abs(Enemy_Rel_YPos); // get relative vertical coordinate
  carry_flag = false;
  adc_absx(StarFlagYPosAdder); // add Y coordinate adder data
  ram[Sprite_Y_Position + y] = a; // store as Y coordinate
  lda_absx(StarFlagTileData); // get tile number
  ram[Sprite_Tilenumber + y] = a; // store as tile number
  lda_imm(0x22); // set palette and background priority bits
  ram[Sprite_Attributes + y] = a; // store as attributes
  lda_abs(Enemy_Rel_XPos); // get relative horizontal coordinate
  carry_flag = false;
  adc_absx(StarFlagXPosAdder); // add X coordinate adder data
  ram[Sprite_X_Position + y] = a; // store as X coordinate
  iny();
  iny(); // increment OAM data offset four bytes
  iny(); // for next sprite
  iny();
  dex(); // move onto next sprite
  if (!neg_flag) { goto DSFLoop; } // do this until all sprites are done
  ldx_zp(ObjectOffset); // get enemy object offset and leave
}

void DrawFlagSetTimer(void) {
  DrawStarFlag(); // do sub to draw star flag
  lda_imm(0x6);
  ram[EnemyIntervalTimer + x] = a; // set interval timer here
  IncrementSFTask2(); // <fallthrough>
}

void IncrementSFTask2(void) {
  inc_abs(StarFlagTaskControl); // move onto next task
}

void DelayToAreaEnd(void) {
  DrawStarFlag(); // do sub to draw star flag
  lda_absx(EnemyIntervalTimer); // if interval timer set in previous task
  if (zero_flag) {
    lda_abs(EventMusicBuffer); // if event music buffer empty,
    // branch to increment task
    if (zero_flag) {
      IncrementSFTask2();
      return;
    }
    // --------------------------------
    // $00 - used to store horizontal difference between player and piranha plant
  }
}

void OffscreenBoundsCheck(void) {
  lda_zpx(Enemy_ID); // check for cheep-cheep object
  cmp_imm(FlyingCheepCheep); // branch to leave if found
  if (zero_flag) { return; }
  lda_abs(ScreenLeft_X_Pos); // get horizontal coordinate for left side of screen
  ldy_zpx(Enemy_ID);
  cpy_imm(HammerBro); // check for hammer bro object
  if (zero_flag) { goto LimitB; }
  cpy_imm(PiranhaPlant); // check for piranha plant object
  if (!zero_flag) { goto ExtendLB; } // these two will be erased sooner than others if too far left
  
LimitB:
  adc_imm(0x38); // add 56 pixels to coordinate if hammer bro or piranha plant
  
ExtendLB:
  sbc_imm(0x48); // subtract 72 pixels regardless of enemy object
  ram[0x1] = a; // store result here
  lda_abs(ScreenLeft_PageLoc);
  sbc_imm(0x0); // subtract borrow from page location of left side
  ram[0x0] = a; // store result here
  lda_abs(ScreenRight_X_Pos); // add 72 pixels to the right side horizontal coordinate
  adc_imm(0x48);
  ram[0x3] = a; // store result here
  lda_abs(ScreenRight_PageLoc);
  adc_imm(0x0); // then add the carry to the page location
  ram[0x2] = a; // and store result here
  lda_zpx(Enemy_X_Position); // compare horizontal coordinate of the enemy object
  cmp_zp(0x1); // to modified horizontal left edge coordinate to get carry
  lda_zpx(Enemy_PageLoc);
  sbc_zp(0x0); // then subtract it from the page coordinate of the enemy object
  if (neg_flag) { goto TooFar; } // if enemy object is too far left, branch to erase it
  lda_zpx(Enemy_X_Position); // compare horizontal coordinate of the enemy object
  cmp_zp(0x3); // to modified horizontal right edge coordinate to get carry
  lda_zpx(Enemy_PageLoc);
  sbc_zp(0x2); // then subtract it from the page coordinate of the enemy object
  if (neg_flag) { return; } // if enemy object is on the screen, leave, do not erase enemy
  lda_zpx(Enemy_State); // if at this point, enemy is offscreen to the right, so check
  cmp_imm(HammerBro); // if in state used by spiny's egg, do not erase
  if (zero_flag) { return; }
  cpy_imm(PiranhaPlant); // if piranha plant, do not erase
  if (zero_flag) { return; }
  cpy_imm(FlagpoleFlagObject); // if flagpole flag, do not erase
  if (zero_flag) { return; }
  cpy_imm(StarFlagObject); // if star flag, do not erase
  if (zero_flag) { return; }
  cpy_imm(JumpspringObject); // if jumpspring, do not erase
  if (zero_flag) { return; } // erase all others too far to the right
  
TooFar:
  EraseEnemyObject(); // erase object if necessary
  // -------------------------------------------------------------------------------------
  // some unused space
  //       .db $ff, $ff, $ff
  // -------------------------------------------------------------------------------------
  // $01 - enemy buffer offset
}

void EnemyMovementSubs(void) {
  lda_zpx(Enemy_ID);
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: MoveNormalEnemy(); return;
    case 1: MoveNormalEnemy(); return;
    case 2: MoveNormalEnemy(); return;
    case 3: MoveNormalEnemy(); return;
    case 4: MoveNormalEnemy(); return;
    case 5: ProcHammerBro(); return;
    case 6: MoveNormalEnemy(); return;
    case 7: MoveBloober(); return;
    case 8: MoveBulletBill(); return;
    case 9: NoMoveCode(); return;
    case 10: MoveSwimmingCheepCheep(); return;
    case 11: MoveSwimmingCheepCheep(); return;
    case 12: MovePodoboo(); return;
    case 13: MovePiranhaPlant(); return;
    case 14: MoveJumpingEnemy(); return;
    case 15: ProcMoveRedPTroopa(); return;
    case 16: MoveFlyGreenPTroopa(); return;
    case 17: MoveLakitu(); return;
    case 18: MoveNormalEnemy(); return;
    case 19: NoMoveCode(); return;
    case 20: MoveFlyingCheepCheep(); return;
  }
}

void NoMoveCode(void) {
  // --------------------------------
}

void MovePodoboo(void) {
  lda_absx(EnemyIntervalTimer); // check enemy timer
  // branch to move enemy if not expired
  if (zero_flag) {
    InitPodoboo(); // otherwise set up podoboo again
    lda_absx(PseudoRandomBitReg + 1); // get part of LSFR
    ora_imm(0b10000000); // set d7
    ram[Enemy_Y_MoveForce + x] = a; // store as movement force
    and_imm(0b00001111); // mask out high nybble
    ora_imm(0x6); // set for at least six intervals
    ram[EnemyIntervalTimer + x] = a; // store as new enemy timer
    lda_imm(0xf9);
    ram[Enemy_Y_Speed + x] = a; // set vertical speed to move podoboo upwards
  }
  // PdbM:
  MoveJ_EnemyVertically(); // branch to impose gravity on podoboo
  // --------------------------------
  // $00 - used in HammerBroJumpCode as bitmask
}

void ProcHammerBro(void) {
  lda_zpx(Enemy_State); // check hammer bro's enemy state for d5 set
  and_imm(0b00100000);
  // if not set, go ahead with code
  if (!zero_flag) {
    MoveDefeatedEnemy(); // otherwise jump to something else
    return;
  }
  // ChkJH:
  lda_zpx(HammerBroJumpTimer); // check jump timer
  // if expired, branch to jump
  if (!zero_flag) {
    dec_zpx(HammerBroJumpTimer); // otherwise decrement jump timer
    lda_abs(Enemy_OffscreenBits);
    and_imm(0b00001100); // check offscreen bits
    // if hammer bro a little offscreen, skip to movement code
    if (!zero_flag) {
      MoveHammerBroXDir();
      return;
    }
    lda_absx(HammerThrowingTimer); // check hammer throwing timer
    // if not expired, skip ahead, do not throw hammer
    if (zero_flag) {
      ldy_abs(SecondaryHardMode); // otherwise get secondary hard mode flag
      lda_absy(HammerThrowTmrData); // get timer data using flag as offset
      ram[HammerThrowingTimer + x] = a; // set as new timer
      SpawnHammerObj(); // do a sub here to spawn hammer object
      // if carry clear, hammer not spawned, skip to decrement timer
      if (carry_flag) {
        lda_zpx(Enemy_State);
        ora_imm(0b00001000); // set d3 in enemy state for hammer throw
        ram[Enemy_State + x] = a;
        MoveHammerBroXDir(); // jump to move hammer bro
        return;
      }
    }
    // DecHT:
    dec_absx(HammerThrowingTimer); // decrement timer
    MoveHammerBroXDir(); // jump to move hammer bro
    return;
  }
  // HammerBroJumpCode:
  lda_zpx(Enemy_State); // get hammer bro's enemy state
  and_imm(0b00000111); // mask out all but 3 LSB
  cmp_imm(0x1); // check for d0 set (for jumping)
  // if set, branch ahead to moving code
  if (zero_flag) {
    MoveHammerBroXDir();
    return;
  }
  lda_imm(0x0); // load default value here
  ram[0x0] = a; // save into temp variable for now
  ldy_imm(0xfa); // set default vertical speed
  lda_zpx(Enemy_Y_Position); // check hammer bro's vertical coordinate
  // if on the bottom half of the screen, use current speed
  if (neg_flag) {
    SetHJ();
    return;
  }
  ldy_imm(0xfd); // otherwise set alternate vertical speed
  cmp_imm(0x70); // check to see if hammer bro is above the middle of screen
  inc_zp(0x0); // increment preset value to $01
  // if above the middle of the screen, use current speed and $01
  if (!carry_flag) {
    SetHJ();
    return;
  }
  dec_zp(0x0); // otherwise return value to $00
  lda_absx(PseudoRandomBitReg + 1); // get part of LSFR, mask out all but LSB
  and_imm(0x1);
  // if d0 of LSFR set, branch and use current speed and $00
  if (!zero_flag) {
    SetHJ();
    return;
  }
  ldy_imm(0xfa); // otherwise reset to default vertical speed
  SetHJ();
}

void SetHJ(void) {
  ram[Enemy_Y_Speed + x] = y; // set vertical speed for jumping
  lda_zpx(Enemy_State); // set d0 in enemy state for jumping
  ora_imm(0x1);
  ram[Enemy_State + x] = a;
  lda_zp(0x0); // load preset value here to use as bitmask
  and_absx(PseudoRandomBitReg + 2); // and do bit-wise comparison with part of LSFR
  tay(); // then use as offset
  lda_abs(SecondaryHardMode); // check secondary hard mode flag
  if (zero_flag) {
    tay(); // if secondary hard mode flag clear, set offset to 0
  }
  // HJump:
  lda_absy(HammerBroJumpLData); // get jump length timer data using offset from before
  ram[EnemyFrameTimer + x] = a; // save in enemy timer
  lda_absx(PseudoRandomBitReg + 1);
  ora_imm(0b11000000); // get contents of part of LSFR, set d7 and d6, then
  ram[HammerBroJumpTimer + x] = a; // store in jump timer
  MoveHammerBroXDir(); // <fallthrough>
}

void MoveHammerBroXDir(void) {
  ldy_imm(0xfc); // move hammer bro a little to the left
  lda_zp(FrameCounter);
  and_imm(0b01000000); // change hammer bro's direction every 64 frames
  if (zero_flag) {
    ldy_imm(0x4); // if d6 set in counter, move him a little to the right
  }
  // Shimmy:
  ram[Enemy_X_Speed + x] = y; // store horizontal speed
  ldy_imm(0x1); // set to face right by default
  PlayerEnemyDiff(); // get horizontal difference between player and hammer bro
  // if enemy to the left of player, skip this part
  if (!neg_flag) {
    iny(); // set to face left
    lda_absx(EnemyIntervalTimer); // check walking timer
    // if not yet expired, skip to set moving direction
    if (zero_flag) {
      lda_imm(0xf8);
      ram[Enemy_X_Speed + x] = a; // otherwise, make the hammer bro walk left towards player
    }
  }
  // SetShim:
  ram[Enemy_MovingDir + x] = y; // set moving direction
  MoveNormalEnemy(); // <fallthrough>
}

void MoveNormalEnemy(void) {
  ldy_imm(0x0); // init Y to leave horizontal movement as-is
  lda_zpx(Enemy_State);
  and_imm(0b01000000); // check enemy state for d6 set, if set skip
  if (!zero_flag) { goto FallE; } // to move enemy vertically, then horizontally if necessary
  lda_zpx(Enemy_State);
  asl_acc(); // check enemy state for d7 set
  if (carry_flag) { goto SteadM; } // if set, branch to move enemy horizontally
  lda_zpx(Enemy_State);
  and_imm(0b00100000); // check enemy state for d5 set
  if (!zero_flag) { MoveDefeatedEnemy(); return; } // if set, branch to move defeated enemy object
  lda_zpx(Enemy_State);
  and_imm(0b00000111); // check d2-d0 of enemy state for any set bits
  if (zero_flag) { goto SteadM; } // if enemy in normal state, branch to move enemy horizontally
  cmp_imm(0x5);
  if (zero_flag) { goto FallE; } // if enemy in state used by spiny's egg, go ahead here
  cmp_imm(0x3);
  if (carry_flag) { goto ReviveStunned; } // if enemy in states $03 or $04, skip ahead to yet another part
  
FallE:
  MoveD_EnemyVertically(); // do a sub here to move enemy downwards
  ldy_imm(0x0);
  lda_zpx(Enemy_State); // check for enemy state $02
  cmp_imm(0x2);
  if (zero_flag) { goto MEHor; } // if found, branch to move enemy horizontally
  and_imm(0b01000000); // check for d6 set
  if (zero_flag) { goto SteadM; } // if not set, branch to something else
  lda_zpx(Enemy_ID);
  cmp_imm(PowerUpObject); // check for power-up object
  if (zero_flag) { goto SteadM; }
  if (!zero_flag) { goto SlowM; } // if any other object where d6 set, jump to set Y
  
MEHor:
  MoveEnemyHorizontally(); // jump here to move enemy horizontally for <> $2e and d6 set
  return;
  
SlowM:
  ldy_imm(0x1); // if branched here, increment Y to slow horizontal movement
  
SteadM:
  lda_zpx(Enemy_X_Speed); // get current horizontal speed
  pha(); // save to stack
  if (!neg_flag) { goto AddHS; } // if not moving or moving right, skip, leave Y alone
  iny();
  iny(); // otherwise increment Y to next data
  
AddHS:
  carry_flag = false;
  adc_absy(XSpeedAdderData); // add value here to slow enemy down if necessary
  ram[Enemy_X_Speed + x] = a; // save as horizontal speed temporarily
  MoveEnemyHorizontally(); // then do a sub to move horizontally
  pla();
  ram[Enemy_X_Speed + x] = a; // get old horizontal speed from stack and return to
  return; // original memory location, then leave
  
ReviveStunned:
  lda_absx(EnemyIntervalTimer); // if enemy timer not expired yet,
  if (!zero_flag) { ChkKillGoomba(); return; } // skip ahead to something else
  ram[Enemy_State + x] = a; // otherwise initialize enemy state to normal
  lda_zp(FrameCounter);
  and_imm(0x1); // get d0 of frame counter
  tay(); // use as Y and increment for movement direction
  iny();
  ram[Enemy_MovingDir + x] = y; // store as pseudorandom movement direction
  dey(); // decrement for use as pointer
  lda_abs(PrimaryHardMode); // check primary hard mode flag
  if (zero_flag) { goto SetRSpd; } // if not set, use pointer as-is
  iny();
  iny(); // otherwise increment 2 bytes to next data
  
SetRSpd:
  lda_absy(RevivedXSpeed); // load and store new horizontal speed
  ram[Enemy_X_Speed + x] = a; // and leave
}

void MoveDefeatedEnemy(void) {
  MoveD_EnemyVertically(); // execute sub to move defeated enemy downwards
  MoveEnemyHorizontally(); // now move defeated enemy horizontally
}

void ChkKillGoomba(void) {
  cmp_imm(0xe); // check to see if enemy timer has reached
  if (!zero_flag) { return; } // a certain point, and branch to leave if not
  lda_zpx(Enemy_ID);
  cmp_imm(Goomba); // check for goomba object
  if (!zero_flag) { return; } // branch if not found
  EraseEnemyObject(); // otherwise, kill this goomba object
  // --------------------------------
}

void MoveJumpingEnemy(void) {
  MoveJ_EnemyVertically(); // do a sub to impose gravity on green paratroopa
  MoveEnemyHorizontally(); // jump to move enemy horizontally
  // --------------------------------
}

void ProcMoveRedPTroopa(void) {
  lda_zpx(Enemy_Y_Speed);
  ora_absx(Enemy_Y_MoveForce); // check for any vertical force or speed
  if (!zero_flag) { goto MoveRedPTUpOrDown; } // branch if any found
  ram[Enemy_YMF_Dummy + x] = a; // initialize something here
  lda_zpx(Enemy_Y_Position); // check current vs. original vertical coordinate
  cmp_absx(RedPTroopaOrigXPos);
  if (carry_flag) { goto MoveRedPTUpOrDown; } // if current => original, skip ahead to more code
  lda_zp(FrameCounter); // get frame counter
  and_imm(0b00000111); // mask out all but 3 LSB
  if (!zero_flag) { return; } // if any bits set, branch to leave
  inc_zpx(Enemy_Y_Position); // otherwise increment red paratroopa's vertical position
  return; // leave
  
MoveRedPTUpOrDown:
  lda_zpx(Enemy_Y_Position); // check current vs. central vertical coordinate
  cmp_zpx(RedPTroopaCenterYPos);
  if (!carry_flag) { goto MovPTDwn; } // if current < central, jump to move downwards
  MoveRedPTroopaUp(); // otherwise jump to move upwards
  return;
  
MovPTDwn:
  MoveRedPTroopaDown(); // move downwards
  // --------------------------------
  // $00 - used to store adder for movement, also used as adder for platform
  // $01 - used to store maximum value for secondary counter
}

void MoveFlyGreenPTroopa(void) {
  XMoveCntr_GreenPTroopa(); // do sub to increment primary and secondary counters
  MoveWithXMCntrs(); // do sub to move green paratroopa accordingly, and horizontally
  ldy_imm(0x1); // set Y to move green paratroopa down
  lda_zp(FrameCounter);
  and_imm(0b00000011); // check frame counter 2 LSB for any bits set
  if (zero_flag) {
    lda_zp(FrameCounter);
    and_imm(0b01000000); // check frame counter for d6 set
    // branch to move green paratroopa down if set
    if (zero_flag) {
      ldy_imm(0xff); // otherwise set Y to move green paratroopa up
    }
    // YSway:
    ram[0x0] = y; // store adder here
    lda_zpx(Enemy_Y_Position);
    carry_flag = false; // add or subtract from vertical position
    adc_zp(0x0); // to give green paratroopa a wavy flight
    ram[Enemy_Y_Position + x] = a;
  }
}

void MoveBloober(void) {
  lda_zpx(Enemy_State);
  and_imm(0b00100000); // check enemy state for d5 set
  if (!zero_flag) { goto MoveDefeatedBloober; } // branch if set to move defeated bloober
  ldy_abs(SecondaryHardMode); // use secondary hard mode flag as offset
  lda_absx(PseudoRandomBitReg + 1); // get LSFR
  and_absy(BlooberBitmasks); // mask out bits in LSFR using bitmask loaded with offset
  if (!zero_flag) { goto BlooberSwim; } // if any bits set, skip ahead to make swim
  txa();
  lsr_acc(); // check to see if on second or fourth slot (1 or 3)
  if (!carry_flag) { goto FBLeft; } // if not, branch to figure out moving direction
  ldy_zp(Player_MovingDir); // otherwise, load player's moving direction and
  if (carry_flag) { goto SBMDir; } // do an unconditional branch to set
  
FBLeft:
  ldy_imm(0x2); // set left moving direction by default
  PlayerEnemyDiff(); // get horizontal difference between player and bloober
  if (!neg_flag) { goto SBMDir; } // if enemy to the right of player, keep left
  dey(); // otherwise decrement to set right moving direction
  
SBMDir:
  ram[Enemy_MovingDir + x] = y; // set moving direction of bloober, then continue on here
  
BlooberSwim:
  ProcSwimmingB(); // execute sub to make bloober swim characteristically
  lda_zpx(Enemy_Y_Position); // get vertical coordinate
  carry_flag = true;
  sbc_absx(Enemy_Y_MoveForce); // subtract movement force
  cmp_imm(0x20); // check to see if position is above edge of status bar
  if (!carry_flag) { goto SwimX; } // if so, don't do it
  ram[Enemy_Y_Position + x] = a; // otherwise, set new vertical position, make bloober swim
  
SwimX:
  ldy_zpx(Enemy_MovingDir); // check moving direction
  dey();
  if (!zero_flag) { goto LeftSwim; } // if moving to the left, branch to second part
  lda_zpx(Enemy_X_Position);
  carry_flag = false; // add movement speed to horizontal coordinate
  adc_zpx(BlooperMoveSpeed);
  ram[Enemy_X_Position + x] = a; // store result as new horizontal coordinate
  lda_zpx(Enemy_PageLoc);
  adc_imm(0x0); // add carry to page location
  ram[Enemy_PageLoc + x] = a; // store as new page location and leave
  return;
  
LeftSwim:
  lda_zpx(Enemy_X_Position);
  carry_flag = true; // subtract movement speed from horizontal coordinate
  sbc_zpx(BlooperMoveSpeed);
  ram[Enemy_X_Position + x] = a; // store result as new horizontal coordinate
  lda_zpx(Enemy_PageLoc);
  sbc_imm(0x0); // subtract borrow from page location
  ram[Enemy_PageLoc + x] = a; // store as new page location and leave
  return;
  
MoveDefeatedBloober:
  MoveEnemySlowVert(); return; // jump to move defeated bloober downwards
}

void MoveBulletBill(void) {
  lda_zpx(Enemy_State); // check bullet bill's enemy object state for d5 set
  and_imm(0b00100000);
  // if not set, continue with movement code
  if (!zero_flag) {
    MoveJ_EnemyVertically(); // otherwise jump to move defeated bullet bill downwards
    return;
  }
  // NotDefB:
  lda_imm(0xe8); // set bullet bill's horizontal speed
  ram[Enemy_X_Speed + x] = a; // and move it accordingly (note: this bullet bill
  MoveEnemyHorizontally(); // object occurs in frenzy object $17, not from cannons)
  // --------------------------------
  // $02 - used to hold preset values
  // $03 - used to hold enemy state
}

void MoveSwimmingCheepCheep(void) {
  lda_zpx(Enemy_State); // check cheep-cheep's enemy object state
  and_imm(0b00100000); // for d5 set
  if (zero_flag) { goto CCSwim; } // if not set, continue with movement code
  MoveEnemySlowVert(); return; // otherwise jump to move defeated cheep-cheep downwards
  
CCSwim:
  ram[0x3] = a; // save enemy state in $03
  lda_zpx(Enemy_ID); // get enemy identifier
  carry_flag = true;
  sbc_imm(0xa); // subtract ten for cheep-cheep identifiers
  tay(); // use as offset
  lda_absy(SwimCCXMoveData); // load value here
  ram[0x2] = a;
  lda_absx(Enemy_X_MoveForce); // load horizontal force
  carry_flag = true;
  sbc_zp(0x2); // subtract preset value from horizontal force
  ram[Enemy_X_MoveForce + x] = a; // store as new horizontal force
  lda_zpx(Enemy_X_Position); // get horizontal coordinate
  sbc_imm(0x0); // subtract borrow (thus moving it slowly)
  ram[Enemy_X_Position + x] = a; // and save as new horizontal coordinate
  lda_zpx(Enemy_PageLoc);
  sbc_imm(0x0); // subtract borrow again, this time from the
  ram[Enemy_PageLoc + x] = a; // page location, then save
  lda_imm(0x20);
  ram[0x2] = a; // save new value here
  cpx_imm(0x2); // check enemy object offset
  if (!carry_flag) { return; } // if in first or second slot, branch to leave
  lda_zpx(CheepCheepMoveMFlag); // check movement flag
  cmp_imm(0x10); // if movement speed set to $00,
  if (!carry_flag) { goto CCSwimUpwards; } // branch to move upwards
  lda_absx(Enemy_YMF_Dummy);
  carry_flag = false;
  adc_zp(0x2); // add preset value to dummy variable to get carry
  ram[Enemy_YMF_Dummy + x] = a; // and save dummy
  lda_zpx(Enemy_Y_Position); // get vertical coordinate
  adc_zp(0x3); // add carry to it plus enemy state to slowly move it downwards
  ram[Enemy_Y_Position + x] = a; // save as new vertical coordinate
  lda_zpx(Enemy_Y_HighPos);
  adc_imm(0x0); // add carry to page location and
  goto ChkSwimYPos; // jump to end of movement code
  
CCSwimUpwards:
  lda_absx(Enemy_YMF_Dummy);
  carry_flag = true;
  sbc_zp(0x2); // subtract preset value to dummy variable to get borrow
  ram[Enemy_YMF_Dummy + x] = a; // and save dummy
  lda_zpx(Enemy_Y_Position); // get vertical coordinate
  sbc_zp(0x3); // subtract borrow to it plus enemy state to slowly move it upwards
  ram[Enemy_Y_Position + x] = a; // save as new vertical coordinate
  lda_zpx(Enemy_Y_HighPos);
  sbc_imm(0x0); // subtract borrow from page location
  
ChkSwimYPos:
  ram[Enemy_Y_HighPos + x] = a; // save new page location here
  ldy_imm(0x0); // load movement speed to upwards by default
  lda_zpx(Enemy_Y_Position); // get vertical coordinate
  carry_flag = true;
  sbc_absx(CheepCheepOrigYPos); // subtract original coordinate from current
  if (!neg_flag) { goto YPDiff; } // if result positive, skip to next part
  ldy_imm(0x10); // otherwise load movement speed to downwards
  eor_imm(0xff);
  carry_flag = false; // get two's compliment of result
  adc_imm(0x1); // to obtain total difference of original vs. current
  
YPDiff:
  cmp_imm(0xf); // if difference between original vs. current vertical
  if (!carry_flag) { return; } // coordinates < 15 pixels, leave movement speed alone
  tya();
  ram[CheepCheepMoveMFlag + x] = a; // otherwise change movement speed
  // --------------------------------
  // $00 - used as counter for firebar parts
  // $01 - used for oscillated high byte of spin state or to hold horizontal adder
  // $02 - used for oscillated high byte of spin state or to hold vertical adder
  // $03 - used for mirror data
  // $04 - used to store player's sprite 1 X coordinate
  // $05 - used to evaluate mirror data
  // $06 - used to store either screen X coordinate or sprite data offset
  // $07 - used to store screen Y coordinate
  // $ed - used to hold maximum length of firebar
  // $ef - used to hold high byte of spinstate
  // horizontal adder is at first byte + high byte of spinstate,
  // vertical adder is same + 8 bytes, two's compliment
  // if greater than $08 for proper oscillation
}

void MoveFlyingCheepCheep(void) {
  lda_zpx(Enemy_State); // check cheep-cheep's enemy state
  and_imm(0b00100000); // for d5 set
  // branch to continue code if not set
  if (!zero_flag) {
    lda_imm(0x0);
    ram[Enemy_SprAttrib + x] = a; // otherwise clear sprite attributes
    MoveJ_EnemyVertically(); // and jump to move defeated cheep-cheep downwards
    return;
  }
  // FlyCC:
  MoveEnemyHorizontally(); // move cheep-cheep horizontally based on speed and force
  ldy_imm(0xd); // set vertical movement amount
  lda_imm(0x5); // set maximum speed
  SetXMoveAmt(); // branch to impose gravity on flying cheep-cheep
  lda_absx(Enemy_Y_MoveForce);
  lsr_acc(); // get vertical movement force and
  lsr_acc(); // move high nybble to low
  lsr_acc();
  lsr_acc();
  tay(); // save as offset (note this tends to go into reach of code)
  lda_zpx(Enemy_Y_Position); // get vertical position
  carry_flag = true; // subtract pseudorandom value based on offset from position
  sbc_absy(PRandomSubtracter);
  // if result within top half of screen, skip this part
  if (neg_flag) {
    eor_imm(0xff);
    carry_flag = false; // otherwise get two's compliment
    adc_imm(0x1);
  }
  // AddCCF:
  cmp_imm(0x8); // if result or two's compliment greater than eight,
  // skip to the end without changing movement force
  if (!carry_flag) {
    lda_absx(Enemy_Y_MoveForce);
    carry_flag = false;
    adc_imm(0x10); // otherwise add to it
    ram[Enemy_Y_MoveForce + x] = a;
    lsr_acc(); // move high nybble to low again
    lsr_acc();
    lsr_acc();
    lsr_acc();
    tay();
  }
  // BPGet:
  lda_absy(FlyCCBPriority); // load bg priority data and store (this is very likely
  ram[Enemy_SprAttrib + x] = a; // broken or residual code, value is overwritten before
  // --------------------------------
  // $00 - used to hold horizontal difference
  // $01-$03 - used to hold difference adjusters
}

void MoveLakitu(void) {
  lda_zpx(Enemy_State); // check lakitu's enemy state
  and_imm(0b00100000); // for d5 set
  if (zero_flag) { goto ChkLS; } // if not set, continue with code
  MoveD_EnemyVertically(); return; // otherwise jump to move defeated lakitu downwards
  
ChkLS:
  lda_zpx(Enemy_State); // if lakitu's enemy state not set at all,
  if (zero_flag) { goto Fr12S; } // go ahead and continue with code
  lda_imm(0x0);
  ram[LakituMoveDirection + x] = a; // otherwise initialize moving direction to move to left
  ram[EnemyFrenzyBuffer] = a; // initialize frenzy buffer
  lda_imm(0x10);
  if (!zero_flag) { goto SetLSpd; } // load horizontal speed and do unconditional branch
  
Fr12S:
  lda_imm(Spiny);
  ram[EnemyFrenzyBuffer] = a; // set spiny identifier in frenzy buffer
  ldy_imm(0x2);
  
LdLDa:
  lda_absy(LakituDiffAdj); // load values
  ram[0x1 + y] = a; // store in zero page
  dey();
  if (!neg_flag) { goto LdLDa; } // do this until all values are stired
  PlayerLakituDiff(); // execute sub to set speed and create spinys
  
SetLSpd:
  ram[LakituMoveSpeed + x] = a; // set movement speed returned from sub
  ldy_imm(0x1); // set moving direction to right by default
  lda_zpx(LakituMoveDirection);
  and_imm(0x1); // get LSB of moving direction
  if (!zero_flag) { goto SetLMov; } // if set, branch to the end to use moving direction
  lda_zpx(LakituMoveSpeed);
  eor_imm(0xff); // get two's compliment of moving speed
  carry_flag = false;
  adc_imm(0x1);
  ram[LakituMoveSpeed + x] = a; // store as new moving speed
  iny(); // increment moving direction to left
  
SetLMov:
  ram[Enemy_MovingDir + x] = y; // store moving direction
  MoveEnemyHorizontally(); // move lakitu horizontally
}

void MovePiranhaPlant(void) {
  lda_zpx(Enemy_State); // check enemy state
  if (!zero_flag) { goto PutinPipe; } // if set at all, branch to leave
  lda_absx(EnemyFrameTimer); // check enemy's timer here
  if (!zero_flag) { goto PutinPipe; } // branch to end if not yet expired
  lda_zpx(PiranhaPlant_MoveFlag); // check movement flag
  if (!zero_flag) { goto SetupToMovePPlant; } // if moving, skip to part ahead
  lda_zpx(PiranhaPlant_Y_Speed); // if currently rising, branch
  if (neg_flag) { goto ReversePlantSpeed; } // to move enemy upwards out of pipe
  PlayerEnemyDiff(); // get horizontal difference between player and
  if (!neg_flag) { goto ChkPlayerNearPipe; } // piranha plant, and branch if enemy to right of player
  lda_zp(0x0); // otherwise get saved horizontal difference
  eor_imm(0xff);
  carry_flag = false; // and change to two's compliment
  adc_imm(0x1);
  ram[0x0] = a; // save as new horizontal difference
  
ChkPlayerNearPipe:
  lda_zp(0x0); // get saved horizontal difference
  cmp_imm(0x21);
  if (!carry_flag) { goto PutinPipe; } // if player within a certain distance, branch to leave
  
ReversePlantSpeed:
  lda_zpx(PiranhaPlant_Y_Speed); // get vertical speed
  eor_imm(0xff);
  carry_flag = false; // change to two's compliment
  adc_imm(0x1);
  ram[PiranhaPlant_Y_Speed + x] = a; // save as new vertical speed
  inc_zpx(PiranhaPlant_MoveFlag); // increment to set movement flag
  
SetupToMovePPlant:
  lda_absx(PiranhaPlantDownYPos); // get original vertical coordinate (lowest point)
  ldy_zpx(PiranhaPlant_Y_Speed); // get vertical speed
  if (!neg_flag) { goto RiseFallPiranhaPlant; } // branch if moving downwards
  lda_absx(PiranhaPlantUpYPos); // otherwise get other vertical coordinate (highest point)
  
RiseFallPiranhaPlant:
  ram[0x0] = a; // save vertical coordinate here
  lda_zp(FrameCounter); // get frame counter
  lsr_acc();
  if (!carry_flag) { goto PutinPipe; } // branch to leave if d0 set (execute code every other frame)
  lda_abs(TimerControl); // get master timer control
  if (!zero_flag) { goto PutinPipe; } // branch to leave if set (likely not necessary)
  lda_zpx(Enemy_Y_Position); // get current vertical coordinate
  carry_flag = false;
  adc_zpx(PiranhaPlant_Y_Speed); // add vertical speed to move up or down
  ram[Enemy_Y_Position + x] = a; // save as new vertical coordinate
  cmp_zp(0x0); // compare against low or high coordinate
  if (!zero_flag) { goto PutinPipe; } // branch to leave if not yet reached
  lda_imm(0x0);
  ram[PiranhaPlant_MoveFlag + x] = a; // otherwise clear movement flag
  lda_imm(0x40);
  ram[EnemyFrameTimer + x] = a; // set timer to delay piranha plant movement
  
PutinPipe:
  lda_imm(0b00100000); // set background priority bit in sprite
  ram[Enemy_SprAttrib + x] = a; // attributes to give illusion of being inside pipe
  // -------------------------------------------------------------------------------------
  // $07 - spinning speed
}

void LargePlatformSubroutines(void) {
  lda_zpx(Enemy_ID); // subtract $24 to get proper offset for jump table
  carry_flag = true;
  sbc_imm(0x24);
  carry_flag = (a & 0x80u) != 0;
  switch (a) {
    case 0: BalancePlatform(); return;
    case 1: YMovingPlatform(); return;
    case 2: MoveLargeLiftPlat(); return;
    case 3: MoveLargeLiftPlat(); return;
    case 4: XMovingPlatform(); return;
    case 5: DropPlatform(); return;
    case 6: RightPlatform(); return;
  }
}

void BalancePlatform(void) {
  lda_zpx(Enemy_Y_HighPos); // check high byte of vertical position
  cmp_imm(0x3);
  if (!zero_flag) { goto DoBPl; }
  EraseEnemyObject(); // if far below screen, kill the object
  return;
  
DoBPl:
  lda_zpx(Enemy_State); // get object's state (set to $ff or other platform offset)
  if (!neg_flag) { goto CheckBalPlatform; } // if doing other balance platform, branch to leave
  return;
  
CheckBalPlatform:
  tay(); // save offset from state as Y
  lda_absx(PlatformCollisionFlag); // get collision flag of platform
  ram[0x0] = a; // store here
  lda_zpx(Enemy_MovingDir); // get moving direction
  if (zero_flag) { goto ChkForFall; }
  PlatformFall(); // if set, jump here
  return;
  
ChkForFall:
  lda_imm(0x2d); // check if platform is above a certain point
  cmp_zpx(Enemy_Y_Position);
  if (!carry_flag) { goto ChkOtherForFall; } // if not, branch elsewhere
  cpy_zp(0x0); // if collision flag is set to same value as
  if (zero_flag) { goto MakePlatformFall; } // enemy state, branch to make platforms fall
  carry_flag = false;
  adc_imm(0x2); // otherwise add 2 pixels to vertical position
  ram[Enemy_Y_Position + x] = a; // of current platform and branch elsewhere
  StopPlatforms(); return; // to make platforms stop
  
MakePlatformFall:
  InitPlatformFall(); // make platforms fall
  return;
  
ChkOtherForFall:
  cmp_zpy(Enemy_Y_Position); // check if other platform is above a certain point
  if (!carry_flag) { goto ChkToMoveBalPlat; } // if not, branch elsewhere
  cpx_zp(0x0); // if collision flag is set to same value as
  if (zero_flag) { goto MakePlatformFall; } // enemy state, branch to make platforms fall
  carry_flag = false;
  adc_imm(0x2); // otherwise add 2 pixels to vertical position
  ram[Enemy_Y_Position + y] = a; // of other platform and branch elsewhere
  StopPlatforms(); return; // jump to stop movement and do not return
  
ChkToMoveBalPlat:
  lda_zpx(Enemy_Y_Position); // save vertical position to stack
  pha();
  lda_absx(PlatformCollisionFlag); // get collision flag
  if (!neg_flag) { goto ColFlg; } // branch if collision
  lda_absx(Enemy_Y_MoveForce);
  carry_flag = false; // add $05 to contents of moveforce, whatever they be
  adc_imm(0x5);
  ram[0x0] = a; // store here
  lda_zpx(Enemy_Y_Speed);
  adc_imm(0x0); // add carry to vertical speed
  if (neg_flag) { goto PlatDn; } // branch if moving downwards
  if (!zero_flag) { goto PlatUp; } // branch elsewhere if moving upwards
  lda_zp(0x0);
  cmp_imm(0xb); // check if there's still a little force left
  if (!carry_flag) { goto PlatSt; } // if not enough, branch to stop movement
  if (carry_flag) { goto PlatUp; } // otherwise keep branch to move upwards
  
ColFlg:
  cmp_zp(ObjectOffset); // if collision flag matches
  if (zero_flag) { goto PlatDn; } // current enemy object offset, branch
  
PlatUp:
  MovePlatformUp(); // do a sub to move upwards
  goto DoOtherPlatform; // jump ahead to remaining code
  
PlatSt:
  StopPlatforms(); // do a sub to stop movement
  goto DoOtherPlatform; // jump ahead to remaining code
  
PlatDn:
  MovePlatformDown(); // do a sub to move downwards
  
DoOtherPlatform:
  ldy_zpx(Enemy_State); // get offset of other platform
  pla(); // get old vertical coordinate from stack
  carry_flag = true;
  sbc_zpx(Enemy_Y_Position); // get difference of old vs. new coordinate
  carry_flag = false;
  adc_zpy(Enemy_Y_Position); // add difference to vertical coordinate of other
  ram[Enemy_Y_Position + y] = a; // platform to move it in the opposite direction
  lda_absx(PlatformCollisionFlag); // if no collision, skip this part here
  if (neg_flag) { goto DrawEraseRope; }
  tax(); // put offset which collision occurred here
  PositionPlayerOnVPlat(); // and use it to position player accordingly
  
DrawEraseRope:
  ldy_zp(ObjectOffset); // get enemy object offset
  lda_zpy(Enemy_Y_Speed); // check to see if current platform is
  ora_absy(Enemy_Y_MoveForce); // moving at all
  if (zero_flag) { goto ExitRp; } // if not, skip all of this and branch to leave
  ldx_abs(VRAM_Buffer1_Offset); // get vram buffer offset
  cpx_imm(0x20); // if offset beyond a certain point, go ahead
  if (carry_flag) { goto ExitRp; } // and skip this, branch to leave
  lda_zpy(Enemy_Y_Speed);
  pha(); // save two copies of vertical speed to stack
  pha();
  SetupPlatformRope(); // do a sub to figure out where to put new bg tiles
  lda_zp(0x1); // write name table address to vram buffer
  ram[VRAM_Buffer1 + x] = a; // first the high byte, then the low
  lda_zp(0x0);
  ram[VRAM_Buffer1 + 1 + x] = a;
  lda_imm(0x2); // set length for 2 bytes
  ram[VRAM_Buffer1 + 2 + x] = a;
  lda_zpy(Enemy_Y_Speed); // if platform moving upwards, branch
  if (neg_flag) { goto EraseR1; } // to do something else
  lda_imm(0xa2);
  ram[VRAM_Buffer1 + 3 + x] = a; // otherwise put tile numbers for left
  lda_imm(0xa3); // and right sides of rope in vram buffer
  ram[VRAM_Buffer1 + 4 + x] = a;
  goto OtherRope; // jump to skip this part
  
EraseR1:
  lda_imm(0x24); // put blank tiles in vram buffer
  ram[VRAM_Buffer1 + 3 + x] = a; // to erase rope
  ram[VRAM_Buffer1 + 4 + x] = a;
  
OtherRope:
  lda_zpy(Enemy_State); // get offset of other platform from state
  tay(); // use as Y here
  pla(); // pull second copy of vertical speed from stack
  eor_imm(0xff); // invert bits to reverse speed
  SetupPlatformRope(); // do sub again to figure out where to put bg tiles
  lda_zp(0x1); // write name table address to vram buffer
  ram[VRAM_Buffer1 + 5 + x] = a; // this time we're doing putting tiles for
  lda_zp(0x0); // the other platform
  ram[VRAM_Buffer1 + 6 + x] = a;
  lda_imm(0x2);
  ram[VRAM_Buffer1 + 7 + x] = a; // set length again for 2 bytes
  pla(); // pull first copy of vertical speed from stack
  if (!neg_flag) { goto EraseR2; } // if moving upwards (note inversion earlier), skip this
  lda_imm(0xa2);
  ram[VRAM_Buffer1 + 8 + x] = a; // otherwise put tile numbers for left
  lda_imm(0xa3); // and right sides of rope in vram
  ram[VRAM_Buffer1 + 9 + x] = a; // transfer buffer
  goto EndRp; // jump to skip this part
  
EraseR2:
  lda_imm(0x24); // put blank tiles in vram buffer
  ram[VRAM_Buffer1 + 8 + x] = a; // to erase rope
  ram[VRAM_Buffer1 + 9 + x] = a;
  
EndRp:
  lda_imm(0x0); // put null terminator at the end
  ram[VRAM_Buffer1 + 10 + x] = a;
  lda_abs(VRAM_Buffer1_Offset); // add ten bytes to the vram buffer offset
  carry_flag = false; // and store
  adc_imm(10);
  ram[VRAM_Buffer1_Offset] = a;
  
ExitRp:
  ldx_zp(ObjectOffset); // get enemy object buffer offset and leave
}

void StopPlatforms(void) {
  InitVStf(); // initialize vertical speed and low byte
  ram[Enemy_Y_Speed + y] = a; // for both platforms and leave
  ram[Enemy_Y_MoveForce + y] = a;
}

void YMovingPlatform(void) {
  lda_zpx(Enemy_Y_Speed); // if platform moving up or down, skip ahead to
  ora_absx(Enemy_Y_MoveForce); // check on other position
  if (zero_flag) {
    ram[Enemy_YMF_Dummy + x] = a; // initialize dummy variable
    lda_zpx(Enemy_Y_Position);
    cmp_absx(YPlatformTopYPos); // if current vertical position => top position, branch
    // ahead of all this
    if (!carry_flag) {
      lda_zp(FrameCounter);
      and_imm(0b00000111); // check for every eighth frame
      if (zero_flag) {
        inc_zpx(Enemy_Y_Position); // increase vertical position every eighth frame
      }
      // SkipIY:
      ChkYPCollision(); // skip ahead to last part
      return;
    }
  }
  // ChkYCenterPos:
  lda_zpx(Enemy_Y_Position); // if current vertical position < central position, branch
  cmp_zpx(YPlatformCenterYPos); // to slow ascent/move downwards
  if (carry_flag) {
    MovePlatformUp(); // otherwise start slowing descent/moving upwards
    ChkYPCollision();
    return;
  }
  // YMDown:
  MovePlatformDown(); // start slowing ascent/moving downwards
  ChkYPCollision(); // <fallthrough>
}

void ChkYPCollision(void) {
  lda_absx(PlatformCollisionFlag); // if collision flag not set here, branch
  if (!neg_flag) {
    PositionPlayerOnVPlat(); // otherwise position player appropriately
    // --------------------------------
    // $00 - used as adder to position player hotizontally
  }
}

void XMovingPlatform(void) {
  lda_imm(0xe); // load preset maximum value for secondary counter
  XMoveCntr_Platform(); // do a sub to increment counters for movement
  MoveWithXMCntrs(); // do a sub to move platform accordingly, and return value
  lda_absx(PlatformCollisionFlag); // if no collision with player,
  if (!neg_flag) {
    PositionPlayerOnHPlat(); // <fallthrough>
  }
}

void PositionPlayerOnHPlat(void) {
  lda_zp(Player_X_Position);
  carry_flag = false; // add saved value from second subroutine to
  adc_zp(0x0); // current player's position to position
  ram[Player_X_Position] = a; // player accordingly in horizontal position
  lda_zp(Player_PageLoc); // get player's page location
  ldy_zp(0x0); // check to see if saved value here is positive or negative
  // if negative, branch to subtract
  if (!neg_flag) {
    adc_imm(0x0); // otherwise add carry to page location
    goto SetPVar; // jump to skip subtraction
  }
  // PPHSubt:
  sbc_imm(0x0); // subtract borrow from page location
  
SetPVar:
  ram[Player_PageLoc] = a; // save result to player's page location
  ram[Platform_X_Scroll] = y; // put saved value from second sub here to be used later
  PositionPlayerOnVPlat(); // position player vertically and appropriately
  // --------------------------------
}

void DropPlatform(void) {
  lda_absx(PlatformCollisionFlag); // if no collision between platform and player
  if (!neg_flag) {
    MoveDropPlatform(); // otherwise do a sub to move platform down very quickly
    PositionPlayerOnVPlat(); // do a sub to position player appropriately
    // --------------------------------
    // $00 - residual value from sub
  }
}

void RightPlatform(void) {
  MoveEnemyHorizontally(); // move platform with current horizontal speed, if any
  ram[0x0] = a; // store saved value here (residual code)
  lda_absx(PlatformCollisionFlag); // check collision flag, if no collision between player
  if (!neg_flag) {
    lda_imm(0x10);
    ram[Enemy_X_Speed + x] = a; // otherwise set new speed (gets moving if motionless)
    PositionPlayerOnHPlat(); // use saved value from earlier sub to position player
    // --------------------------------
  }
}

void MoveLargeLiftPlat(void) {
  MoveLiftPlatforms(); // execute common to all large and small lift platforms
  ChkYPCollision(); // branch to position player correctly
}

void EraseEnemyObject(void) {
  lda_imm(0x0); // clear all enemy object variables
  ram[Enemy_Flag + x] = a;
  ram[Enemy_ID + x] = a;
  ram[Enemy_State + x] = a;
  ram[FloateyNum_Control + x] = a;
  ram[EnemyIntervalTimer + x] = a;
  ram[ShellChainCounter + x] = a;
  ram[Enemy_SprAttrib + x] = a;
  ram[EnemyFrameTimer + x] = a;
  // -------------------------------------------------------------------------------------
}

void XMoveCntr_GreenPTroopa(void) {
  lda_imm(0x13); // load preset maximum value for secondary counter
  XMoveCntr_Platform(); // <fallthrough>
}

void XMoveCntr_Platform(void) {
  ram[0x1] = a; // store value here
  lda_zp(FrameCounter);
  and_imm(0b00000011); // branch to leave if not on
  if (!zero_flag) { goto NoIncXM; } // every fourth frame
  ldy_zpx(XMoveSecondaryCounter); // get secondary counter
  lda_zpx(XMovePrimaryCounter); // get primary counter
  lsr_acc();
  if (carry_flag) { goto DecSeXM; } // if d0 of primary counter set, branch elsewhere
  cpy_zp(0x1); // compare secondary counter to preset maximum value
  if (zero_flag) { goto IncPXM; } // if equal, branch ahead of this part
  inc_zpx(XMoveSecondaryCounter); // increment secondary counter and leave
  
NoIncXM:
  return;
  
IncPXM:
  inc_zpx(XMovePrimaryCounter); // increment primary counter and leave
  return;
  
DecSeXM:
  tya(); // put secondary counter in A
  if (zero_flag) { goto IncPXM; } // if secondary counter at zero, branch back
  dec_zpx(XMoveSecondaryCounter); // otherwise decrement secondary counter and leave
}

void MoveWithXMCntrs(void) {
  lda_zpx(XMoveSecondaryCounter); // save secondary counter to stack
  pha();
  ldy_imm(0x1); // set value here by default
  lda_zpx(XMovePrimaryCounter);
  and_imm(0b00000010); // if d1 of primary counter is
  // set, branch ahead of this part here
  if (zero_flag) {
    lda_zpx(XMoveSecondaryCounter);
    eor_imm(0xff); // otherwise change secondary
    carry_flag = false; // counter to two's compliment
    adc_imm(0x1);
    ram[XMoveSecondaryCounter + x] = a;
    ldy_imm(0x2); // load alternate value here
  }
  // XMRight:
  ram[Enemy_MovingDir + x] = y; // store as moving direction
  MoveEnemyHorizontally();
  ram[0x0] = a; // save value obtained from sub here
  pla(); // get secondary counter from stack
  ram[XMoveSecondaryCounter + x] = a; // and return to original place
  // --------------------------------
}

void ProcSwimmingB(void) {
  lda_zpx(BlooperMoveCounter); // get enemy's movement counter
  and_imm(0b00000010); // check for d1 set
  // branch if set
  if (zero_flag) {
    lda_zp(FrameCounter);
    and_imm(0b00000111); // get 3 LSB of frame counter
    pha(); // and save it to the stack
    lda_zpx(BlooperMoveCounter); // get enemy's movement counter
    lsr_acc(); // check for d0 set
    // branch if set
    if (!carry_flag) {
      pla(); // pull 3 LSB of frame counter from the stack
      // branch to leave, execute code only every eighth frame
      if (zero_flag) {
        lda_absx(Enemy_Y_MoveForce);
        carry_flag = false; // add to movement force to speed up swim
        adc_imm(0x1);
        ram[Enemy_Y_MoveForce + x] = a; // set movement force
        ram[BlooperMoveSpeed + x] = a; // set as movement speed
        cmp_imm(0x2);
        // if certain horizontal speed, branch to leave
        if (zero_flag) {
          inc_zpx(BlooperMoveCounter); // otherwise increment movement counter
        }
      }
      // BSwimE:
      return;
    }
    // SlowSwim:
    pla(); // pull 3 LSB of frame counter from the stack
    // branch to leave, execute code only every eighth frame
    if (zero_flag) {
      lda_absx(Enemy_Y_MoveForce);
      carry_flag = true; // subtract from movement force to slow swim
      sbc_imm(0x1);
      ram[Enemy_Y_MoveForce + x] = a; // set movement force
      ram[BlooperMoveSpeed + x] = a; // set as movement speed
      // if any speed, branch to leave
      if (zero_flag) {
        inc_zpx(BlooperMoveCounter); // otherwise increment movement counter
        lda_imm(0x2);
        ram[EnemyIntervalTimer + x] = a; // set enemy's timer
      }
    }
    // NoSSw:
    return; // leave
  }
  // ChkForFloatdown:
  lda_absx(EnemyIntervalTimer); // get enemy timer
  // branch if expired
  if (!zero_flag) {
    
Floatdown:
    lda_zp(FrameCounter); // get frame counter
    lsr_acc(); // check for d0 set
    // branch to leave on every other frame
    if (!carry_flag) {
      inc_zpx(Enemy_Y_Position); // otherwise increment vertical coordinate
    }
    // NoFD:
    return; // leave
  }
  // ChkNearPlayer:
  lda_zpx(Enemy_Y_Position); // get vertical coordinate
  adc_imm(0x10); // add sixteen pixels
  cmp_zp(Player_Y_Position); // compare result with player's vertical coordinate
  if (!carry_flag) { goto Floatdown; } // if modified vertical less than player's, branch
  lda_imm(0x0);
  ram[BlooperMoveCounter + x] = a; // otherwise nullify movement counter
  // --------------------------------
}

void ProcFirebar(void) {
  GetEnemyOffscreenBits(); // get offscreen information
  lda_abs(Enemy_OffscreenBits); // check for d3 set
  and_imm(0b00001000); // if so, branch to leave
  if (!zero_flag) { return; }
  lda_abs(TimerControl); // if master timer control set, branch
  if (!zero_flag) { goto SusFbar; } // ahead of this part
  lda_absx(FirebarSpinSpeed); // load spinning speed of firebar
  FirebarSpin(); // modify current spinstate
  and_imm(0b00011111); // mask out all but 5 LSB
  ram[FirebarSpinState_High + x] = a; // and store as new high byte of spinstate
  
SusFbar:
  lda_zpx(FirebarSpinState_High); // get high byte of spinstate
  ldy_zpx(Enemy_ID); // check enemy identifier
  cpy_imm(0x1f);
  if (!carry_flag) { goto SetupGFB; } // if < $1f (long firebar), branch
  cmp_imm(0x8); // check high byte of spinstate
  if (zero_flag) { goto SkpFSte; } // if eight, branch to change
  cmp_imm(0x18);
  if (!zero_flag) { goto SetupGFB; } // if not at twenty-four branch to not change
  
SkpFSte:
  carry_flag = false;
  adc_imm(0x1); // add one to spinning thing to avoid horizontal state
  ram[FirebarSpinState_High + x] = a;
  
SetupGFB:
  ram[0xef] = a; // save high byte of spinning thing, modified or otherwise
  RelativeEnemyPosition(); // get relative coordinates to screen
  GetFirebarPosition(); // do a sub here (residual, too early to be used now)
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  lda_abs(Enemy_Rel_YPos); // get relative vertical coordinate
  ram[Sprite_Y_Position + y] = a; // store as Y in OAM data
  ram[0x7] = a; // also save here
  lda_abs(Enemy_Rel_XPos); // get relative horizontal coordinate
  ram[Sprite_X_Position + y] = a; // store as X in OAM data
  ram[0x6] = a; // also save here
  lda_imm(0x1);
  ram[0x0] = a; // set $01 value here (not necessary)
  FirebarCollision(); // draw fireball part and do collision detection
  ldy_imm(0x5); // load value for short firebars by default
  lda_zpx(Enemy_ID);
  cmp_imm(0x1f); // are we doing a long firebar?
  if (!carry_flag) { goto SetMFbar; } // no, branch then
  ldy_imm(0xb); // otherwise load value for long firebars
  
SetMFbar:
  ram[0xed] = y; // store maximum value for length of firebars
  lda_imm(0x0);
  ram[0x0] = a; // initialize counter here
  
DrawFbar:
  lda_zp(0xef); // load high byte of spinstate
  GetFirebarPosition(); // get fireball position data depending on firebar part
  DrawFirebar_Collision(); // position it properly, draw it and do collision detection
  lda_zp(0x0); // check which firebar part
  cmp_imm(0x4);
  if (!zero_flag) { goto NextFbar; }
  ldy_abs(DuplicateObj_Offset); // if we arrive at fifth firebar part,
  lda_absy(Enemy_SprDataOffset); // get offset from long firebar and load OAM data offset
  ram[0x6] = a; // using long firebar offset, then store as new one here
  
NextFbar:
  inc_zp(0x0); // move onto the next firebar part
  lda_zp(0x0);
  cmp_zp(0xed); // if we end up at the maximum part, go on and leave
  if (!carry_flag) { goto DrawFbar; } // otherwise go back and do another
}

void DrawFirebar_Collision(void) {
  lda_zp(0x3); // store mirror data elsewhere
  ram[0x5] = a;
  ldy_zp(0x6); // load OAM data offset for firebar
  lda_zp(0x1); // load horizontal adder we got from position loader
  lsr_zp(0x5); // shift LSB of mirror data
  if (carry_flag) { goto AddHA; } // if carry was set, skip this part
  eor_imm(0xff);
  adc_imm(0x1); // otherwise get two's compliment of horizontal adder
  
AddHA:
  carry_flag = false; // add horizontal coordinate relative to screen to
  adc_abs(Enemy_Rel_XPos); // horizontal adder, modified or otherwise
  ram[Sprite_X_Position + y] = a; // store as X coordinate here
  ram[0x6] = a; // store here for now, note offset is saved in Y still
  cmp_abs(Enemy_Rel_XPos); // compare X coordinate of sprite to original X of firebar
  if (carry_flag) { goto SubtR1; } // if sprite coordinate => original coordinate, branch
  lda_abs(Enemy_Rel_XPos);
  carry_flag = true; // otherwise subtract sprite X from the
  sbc_zp(0x6); // original one and skip this part
  goto ChkFOfs;
  
SubtR1:
  carry_flag = true; // subtract original X from the
  sbc_abs(Enemy_Rel_XPos); // current sprite X
  
ChkFOfs:
  cmp_imm(0x59); // if difference of coordinates within a certain range,
  if (!carry_flag) { goto VAHandl; } // continue by handling vertical adder
  lda_imm(0xf8); // otherwise, load offscreen Y coordinate
  if (!zero_flag) { goto SetVFbr; } // and unconditionally branch to move sprite offscreen
  
VAHandl:
  lda_abs(Enemy_Rel_YPos); // if vertical relative coordinate offscreen,
  cmp_imm(0xf8); // skip ahead of this part and write into sprite Y coordinate
  if (zero_flag) { goto SetVFbr; }
  lda_zp(0x2); // load vertical adder we got from position loader
  lsr_zp(0x5); // shift LSB of mirror data one more time
  if (carry_flag) { goto AddVA; } // if carry was set, skip this part
  eor_imm(0xff);
  adc_imm(0x1); // otherwise get two's compliment of second part
  
AddVA:
  carry_flag = false; // add vertical coordinate relative to screen to
  adc_abs(Enemy_Rel_YPos); // the second data, modified or otherwise
  
SetVFbr:
  ram[Sprite_Y_Position + y] = a; // store as Y coordinate here
  ram[0x7] = a; // also store here for now
  FirebarCollision(); // <fallthrough>
}

void FirebarCollision(void) {
  DrawFirebar(); // run sub here to draw current tile of firebar
  tya(); // return OAM data offset and save
  pha(); // to the stack for now
  lda_abs(StarInvincibleTimer); // if star mario invincibility timer
  ora_abs(TimerControl); // or master timer controls set
  if (!zero_flag) { goto NoColFB; } // then skip all of this
  ram[0x5] = a; // otherwise initialize counter
  ldy_zp(Player_Y_HighPos);
  dey(); // if player's vertical high byte offscreen,
  if (!zero_flag) { goto NoColFB; } // skip all of this
  ldy_zp(Player_Y_Position); // get player's vertical position
  lda_abs(PlayerSize); // get player's size
  if (!zero_flag) { goto AdjSm; } // if player small, branch to alter variables
  lda_abs(CrouchingFlag);
  if (zero_flag) { goto BigJp; } // if player big and not crouching, jump ahead
  
AdjSm:
  inc_zp(0x5); // if small or big but crouching, execute this part
  inc_zp(0x5); // first increment our counter twice (setting $02 as flag)
  tya();
  carry_flag = false; // then add 24 pixels to the player's
  adc_imm(0x18); // vertical coordinate
  tay();
  
BigJp:
  tya(); // get vertical coordinate, altered or otherwise, from Y
  
FBCLoop:
  carry_flag = true; // subtract vertical position of firebar
  sbc_zp(0x7); // from the vertical coordinate of the player
  if (!neg_flag) { goto ChkVFBD; } // if player lower on the screen than firebar,
  eor_imm(0xff); // skip two's compliment part
  carry_flag = false; // otherwise get two's compliment
  adc_imm(0x1);
  
ChkVFBD:
  cmp_imm(0x8); // if difference => 8 pixels, skip ahead of this part
  if (carry_flag) { goto Chk2Ofs; }
  lda_zp(0x6); // if firebar on far right on the screen, skip this,
  cmp_imm(0xf0); // because, really, what's the point?
  if (carry_flag) { goto Chk2Ofs; }
  lda_abs(Sprite_X_Position + 4); // get OAM X coordinate for sprite #1
  carry_flag = false;
  adc_imm(0x4); // add four pixels
  ram[0x4] = a; // store here
  carry_flag = true; // subtract horizontal coordinate of firebar
  sbc_zp(0x6); // from the X coordinate of player's sprite 1
  if (!neg_flag) { goto ChkFBCl; } // if modded X coordinate to the right of firebar
  eor_imm(0xff); // skip two's compliment part
  carry_flag = false; // otherwise get two's compliment
  adc_imm(0x1);
  
ChkFBCl:
  cmp_imm(0x8); // if difference < 8 pixels, collision, thus branch
  if (!carry_flag) { goto ChgSDir; } // to process
  
Chk2Ofs:
  lda_zp(0x5); // if value of $02 was set earlier for whatever reason,
  cmp_imm(0x2); // branch to increment OAM offset and leave, no collision
  if (zero_flag) { goto NoColFB; }
  ldy_zp(0x5); // otherwise get temp here and use as offset
  lda_zp(Player_Y_Position);
  carry_flag = false;
  adc_absy(FirebarYPos); // add value loaded with offset to player's vertical coordinate
  inc_zp(0x5); // then increment temp and jump back
  goto FBCLoop;
  
ChgSDir:
  ldx_imm(0x1); // set movement direction by default
  lda_zp(0x4); // if OAM X coordinate of player's sprite 1
  cmp_zp(0x6); // is greater than horizontal coordinate of firebar
  if (carry_flag) { goto SetSDir; } // then do not alter movement direction
  inx(); // otherwise increment it
  
SetSDir:
  ram[Enemy_MovingDir] = x; // store movement direction here
  ldx_imm(0x0);
  lda_zp(0x0); // save value written to $00 to stack
  pha();
  InjurePlayer(); // perform sub to hurt or kill player
  pla();
  ram[0x0] = a; // get value of $00 from stack
  
NoColFB:
  pla(); // get OAM data offset
  carry_flag = false; // add four to it and save
  adc_imm(0x4);
  ram[0x6] = a;
  ldx_zp(ObjectOffset); // get enemy object buffer offset and leave
}

void GetFirebarPosition(void) {
  pha(); // save high byte of spinstate to the stack
  and_imm(0b00001111); // mask out low nybble
  cmp_imm(0x9);
  // if lower than $09, branch ahead
  if (carry_flag) {
    eor_imm(0b00001111); // otherwise get two's compliment to oscillate
    carry_flag = false;
    adc_imm(0x1);
  }
  // GetHAdder:
  ram[0x1] = a; // store result, modified or not, here
  ldy_zp(0x0); // load number of firebar ball where we're at
  lda_absy(FirebarTblOffsets); // load offset to firebar position data
  carry_flag = false;
  adc_zp(0x1); // add oscillated high byte of spinstate
  tay(); // to offset here and use as new offset
  lda_absy(FirebarPosLookupTbl); // get data here and store as horizontal adder
  ram[0x1] = a;
  pla(); // pull whatever was in A from the stack
  pha(); // save it again because we still need it
  carry_flag = false;
  adc_imm(0x8); // add eight this time, to get vertical adder
  and_imm(0b00001111); // mask out high nybble
  cmp_imm(0x9); // if lower than $09, branch ahead
  if (carry_flag) {
    eor_imm(0b00001111); // otherwise get two's compliment
    carry_flag = false;
    adc_imm(0x1);
  }
  // GetVAdder:
  ram[0x2] = a; // store result here
  ldy_zp(0x0);
  lda_absy(FirebarTblOffsets); // load offset to firebar position data again
  carry_flag = false;
  adc_zp(0x2); // this time add value in $02 to offset here and use as offset
  tay();
  lda_absy(FirebarPosLookupTbl); // get data here and store as vertica adder
  ram[0x2] = a;
  pla(); // pull out whatever was in A one last time
  lsr_acc(); // divide by eight or shift three to the right
  lsr_acc();
  lsr_acc();
  tay(); // use as offset
  lda_absy(FirebarMirrorData); // load mirroring data here
  ram[0x3] = a; // store
  // --------------------------------
}

void PlayerLakituDiff(void) {
  ldy_imm(0x0); // set Y for default value
  PlayerEnemyDiff(); // get horizontal difference between enemy and player
  if (!neg_flag) { goto ChkLakDif; } // branch if enemy is to the right of the player
  iny(); // increment Y for left of player
  lda_zp(0x0);
  eor_imm(0xff); // get two's compliment of low byte of horizontal difference
  carry_flag = false;
  adc_imm(0x1); // store two's compliment as horizontal difference
  ram[0x0] = a;
  
ChkLakDif:
  lda_zp(0x0); // get low byte of horizontal difference
  cmp_imm(0x3c); // if within a certain distance of player, branch
  if (!carry_flag) { goto ChkPSpeed; }
  lda_imm(0x3c); // otherwise set maximum distance
  ram[0x0] = a;
  lda_zpx(Enemy_ID); // check if lakitu is in our current enemy slot
  cmp_imm(Lakitu);
  if (!zero_flag) { goto ChkPSpeed; } // if not, branch elsewhere
  tya(); // compare contents of Y, now in A
  cmp_zpx(LakituMoveDirection); // to what is being used as horizontal movement direction
  if (zero_flag) { goto ChkPSpeed; } // if moving toward the player, branch, do not alter
  lda_zpx(LakituMoveDirection); // if moving to the left beyond maximum distance,
  if (zero_flag) { goto SetLMovD; } // branch and alter without delay
  dec_zpx(LakituMoveSpeed); // decrement horizontal speed
  lda_zpx(LakituMoveSpeed); // if horizontal speed not yet at zero, branch to leave
  if (!zero_flag) { return; }
  
SetLMovD:
  tya(); // set horizontal direction depending on horizontal
  ram[LakituMoveDirection + x] = a; // difference between enemy and player if necessary
  
ChkPSpeed:
  lda_zp(0x0);
  and_imm(0b00111100); // mask out all but four bits in the middle
  lsr_acc(); // divide masked difference by four
  lsr_acc();
  ram[0x0] = a; // store as new value
  ldy_imm(0x0); // init offset
  lda_zp(Player_X_Speed);
  if (zero_flag) { goto SubDifAdj; } // if player not moving horizontally, branch
  lda_abs(ScrollAmount);
  if (zero_flag) { goto SubDifAdj; } // if scroll speed not set, branch to same place
  iny(); // otherwise increment offset
  lda_zp(Player_X_Speed);
  cmp_imm(0x19); // if player not running, branch
  if (!carry_flag) { goto ChkSpinyO; }
  lda_abs(ScrollAmount);
  cmp_imm(0x2); // if scroll speed below a certain amount, branch
  if (!carry_flag) { goto ChkSpinyO; } // to same place
  iny(); // otherwise increment once more
  
ChkSpinyO:
  lda_zpx(Enemy_ID); // check for spiny object
  cmp_imm(Spiny);
  if (!zero_flag) { goto ChkEmySpd; } // branch if not found
  lda_zp(Player_X_Speed); // if player not moving, skip this part
  if (!zero_flag) { goto SubDifAdj; }
  
ChkEmySpd:
  lda_zpx(Enemy_Y_Speed); // check vertical speed
  if (!zero_flag) { goto SubDifAdj; } // branch if nonzero
  ldy_imm(0x0); // otherwise reinit offset
  
SubDifAdj:
  lda_absy(0x1); // get one of three saved values from earlier
  ldy_zp(0x0); // get saved horizontal difference
  
SPixelLak:
  carry_flag = true; // subtract one for each pixel of horizontal difference
  sbc_imm(0x1); // from one of three saved values
  dey();
  if (!neg_flag) { goto SPixelLak; } // branch until all pixels are subtracted, to adjust difference
  // -------------------------------------------------------------------------------------
  // $04-$05 - used to store name table address in little endian order
}

void ProcessBowserHalf(void) {
  inc_abs(BowserGfxFlag); // increment bowser's graphics flag, then run subroutines
  RunRetainerObj(); // to get offscreen bits, relative position and draw bowser (finally!)
  lda_zpx(Enemy_State);
  if (zero_flag) {
    lda_imm(0xa);
    ram[Enemy_BoundBoxCtrl + x] = a; // set bounding box size control
    GetEnemyBoundBox(); // get bounding box coordinates
    PlayerEnemyCollision(); // do player-to-enemy collision detection
    // -------------------------------------------------------------------------------------
    // $00 - used to hold movement force and tile number
    // $01 - used to hold sprite attribute data
  }
}

void SetFlameTimer(void) {
  ldy_abs(BowserFlameTimerCtrl); // load counter as offset
  inc_abs(BowserFlameTimerCtrl); // increment
  lda_abs(BowserFlameTimerCtrl); // mask out all but 3 LSB
  and_imm(0b00000111); // to keep in range of 0-7
  ram[BowserFlameTimerCtrl] = a;
  lda_absy(FlameTimerData); // load value to be used then leave
}

void ProcBowserFlame(void) {
  lda_abs(TimerControl); // if master timer control flag set,
  if (!zero_flag) { goto SetGfxF; } // skip all of this
  lda_imm(0x40); // load default movement force
  ldy_abs(SecondaryHardMode);
  if (zero_flag) { goto SFlmX; } // if secondary hard mode flag not set, use default
  lda_imm(0x60); // otherwise load alternate movement force to go faster
  
SFlmX:
  ram[0x0] = a; // store value here
  lda_absx(Enemy_X_MoveForce);
  carry_flag = true; // subtract value from movement force
  sbc_zp(0x0);
  ram[Enemy_X_MoveForce + x] = a; // save new value
  lda_zpx(Enemy_X_Position);
  sbc_imm(0x1); // subtract one from horizontal position to move
  ram[Enemy_X_Position + x] = a; // to the left
  lda_zpx(Enemy_PageLoc);
  sbc_imm(0x0); // subtract borrow from page location
  ram[Enemy_PageLoc + x] = a;
  ldy_absx(BowserFlamePRandomOfs); // get some value here and use as offset
  lda_zpx(Enemy_Y_Position); // load vertical coordinate
  cmp_absy(FlameYPosData); // compare against coordinate data using $0417,x as offset
  if (zero_flag) { goto SetGfxF; } // if equal, branch and do not modify coordinate
  carry_flag = false;
  adc_absx(Enemy_Y_MoveForce); // otherwise add value here to coordinate and store
  ram[Enemy_Y_Position + x] = a; // as new vertical coordinate
  
SetGfxF:
  RelativeEnemyPosition(); // get new relative coordinates
  lda_zpx(Enemy_State); // if bowser's flame not in normal state,
  if (!zero_flag) { return; } // branch to leave
  lda_imm(0x51); // otherwise, continue
  ram[0x0] = a; // write first tile number
  ldy_imm(0x2); // load attributes without vertical flip by default
  lda_zp(FrameCounter);
  and_imm(0b00000010); // invert vertical flip bit every 2 frames
  if (zero_flag) { goto FlmeAt; } // if d1 not set, write default value
  ldy_imm(0x82); // otherwise write value with vertical flip bit set
  
FlmeAt:
  ram[0x1] = y; // set bowser's flame sprite attributes here
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  ldx_imm(0x0);
  
DrawFlameLoop:
  lda_abs(Enemy_Rel_YPos); // get Y relative coordinate of current enemy object
  ram[Sprite_Y_Position + y] = a; // write into Y coordinate of OAM data
  lda_zp(0x0);
  ram[Sprite_Tilenumber + y] = a; // write current tile number into OAM data
  inc_zp(0x0); // increment tile number to draw more bowser's flame
  lda_zp(0x1);
  ram[Sprite_Attributes + y] = a; // write saved attributes into OAM data
  lda_abs(Enemy_Rel_XPos);
  ram[Sprite_X_Position + y] = a; // write X relative coordinate of current enemy object
  carry_flag = false;
  adc_imm(0x8);
  ram[Enemy_Rel_XPos] = a; // then add eight to it and store
  iny();
  iny();
  iny();
  iny(); // increment Y four times to move onto the next OAM
  inx(); // move onto the next OAM, and branch if three
  cpx_imm(0x3); // have not yet been done
  if (!carry_flag) { goto DrawFlameLoop; }
  ldx_zp(ObjectOffset); // reload original enemy offset
  GetEnemyOffscreenBits(); // get offscreen information
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  lda_abs(Enemy_OffscreenBits); // get enemy object offscreen bits
  lsr_acc(); // move d0 to carry and result to stack
  pha();
  if (!carry_flag) { goto M3FOfs; } // branch if carry not set
  lda_imm(0xf8); // otherwise move sprite offscreen, this part likely
  ram[Sprite_Y_Position + 12 + y] = a; // residual since flame is only made of three sprites
  
M3FOfs:
  pla(); // get bits from stack
  lsr_acc(); // move d1 to carry and move bits back to stack
  pha();
  if (!carry_flag) { goto M2FOfs; } // branch if carry not set again
  lda_imm(0xf8); // otherwise move third sprite offscreen
  ram[Sprite_Y_Position + 8 + y] = a;
  
M2FOfs:
  pla(); // get bits from stack again
  lsr_acc(); // move d2 to carry and move bits back to stack again
  pha();
  if (!carry_flag) { goto M1FOfs; } // branch if carry not set yet again
  lda_imm(0xf8); // otherwise move second sprite offscreen
  ram[Sprite_Y_Position + 4 + y] = a;
  
M1FOfs:
  pla(); // get bits from stack one last time
  lsr_acc(); // move d3 to carry
  if (!carry_flag) { return; } // branch if carry not set one last time
  lda_imm(0xf8);
  ram[Sprite_Y_Position + y] = a; // otherwise move first sprite offscreen
  // --------------------------------
}

void FirebarSpin(void) {
  ram[0x7] = a; // save spinning speed here
  lda_zpx(FirebarSpinDirection); // check spinning direction
  // if moving counter-clockwise, branch to other part
  if (zero_flag) {
    ldy_imm(0x18); // possibly residual ldy
    lda_zpx(FirebarSpinState_Low);
    carry_flag = false; // add spinning speed to what would normally be
    adc_zp(0x7); // the horizontal speed
    ram[FirebarSpinState_Low + x] = a;
    lda_zpx(FirebarSpinState_High); // add carry to what would normally be the vertical speed
    adc_imm(0x0);
    return;
  }
  // SpinCounterClockwise:
  ldy_imm(0x8); // possibly residual ldy
  lda_zpx(FirebarSpinState_Low);
  carry_flag = true; // subtract spinning speed to what would normally be
  sbc_zp(0x7); // the horizontal speed
  ram[FirebarSpinState_Low + x] = a;
  lda_zpx(FirebarSpinState_High); // add carry to what would normally be the vertical speed
  sbc_imm(0x0);
  // -------------------------------------------------------------------------------------
  // $00 - used to hold collision flag, Y movement force + 5 or low byte of name table for rope
  // $01 - used to hold high byte of name table for rope
  // $02 - used to hold page location of rope
}

void SetupPlatformRope(void) {
  pha(); // save second/third copy to stack
  lda_zpy(Enemy_X_Position); // get horizontal coordinate
  carry_flag = false;
  adc_imm(0x8); // add eight pixels
  ldx_abs(SecondaryHardMode); // if secondary hard mode flag set,
  // use coordinate as-is
  if (zero_flag) {
    carry_flag = false;
    adc_imm(0x10); // otherwise add sixteen more pixels
  }
  // GetLRp:
  pha(); // save modified horizontal coordinate to stack
  lda_zpy(Enemy_PageLoc);
  adc_imm(0x0); // add carry to page location
  ram[0x2] = a; // and save here
  pla(); // pull modified horizontal coordinate
  and_imm(0b11110000); // from the stack, mask out low nybble
  lsr_acc(); // and shift three bits to the right
  lsr_acc();
  lsr_acc();
  ram[0x0] = a; // store result here as part of name table low byte
  ldx_zpy(Enemy_Y_Position); // get vertical coordinate
  pla(); // get second/third copy of vertical speed from stack
  // skip this part if moving downwards or not at all
  if (neg_flag) {
    txa();
    carry_flag = false;
    adc_imm(0x8); // add eight to vertical coordinate and
    tax(); // save as X
  }
  // GetHRp:
  txa(); // move vertical coordinate to A
  ldx_abs(VRAM_Buffer1_Offset); // get vram buffer offset
  asl_acc();
  rol_acc(); // rotate d7 to d0 and d6 into carry
  pha(); // save modified vertical coordinate to stack
  rol_acc(); // rotate carry to d0, thus d7 and d6 are at 2 LSB
  and_imm(0b00000011); // mask out all bits but d7 and d6, then set
  ora_imm(0b00100000); // d5 to get appropriate high byte of name table
  ram[0x1] = a; // address, then store
  lda_zp(0x2); // get saved page location from earlier
  and_imm(0x1); // mask out all but LSB
  asl_acc();
  asl_acc(); // shift twice to the left and save with the
  ora_zp(0x1); // rest of the bits of the high byte, to get
  ram[0x1] = a; // the proper name table and the right place on it
  pla(); // get modified vertical coordinate from stack
  and_imm(0b11100000); // mask out low nybble and LSB of high nybble
  carry_flag = false;
  adc_zp(0x0); // add to horizontal part saved here
  ram[0x0] = a; // save as name table low byte
  lda_zpy(Enemy_Y_Position);
  cmp_imm(0xe8); // if vertical position not below the
  if (carry_flag) {
    lda_zp(0x0);
    and_imm(0b10111111); // mask out d6 of low byte of name table address
    ram[0x0] = a;
  }
}

void InitPlatformFall(void) {
  tya(); // move offset of other platform from Y to X
  tax();
  GetEnemyOffscreenBits(); // get offscreen bits
  lda_imm(0x6);
  SetupFloateyNumber(); // award 1000 points to player
  lda_abs(Player_Rel_XPos);
  ram[FloateyNum_X_Pos + x] = a; // put floatey number coordinates where player is
  lda_zp(Player_Y_Position);
  ram[FloateyNum_Y_Pos + x] = a;
  lda_imm(0x1); // set moving direction as flag for
  ram[Enemy_MovingDir + x] = a; // falling platforms
  StopPlatforms(); // <fallthrough>
}

void PlatformFall(void) {
  tya(); // save offset for other platform to stack
  pha();
  MoveFallingPlatform(); // make current platform fall
  pla();
  tax(); // pull offset from stack and save to X
  MoveFallingPlatform(); // make other platform fall
  ldx_zp(ObjectOffset);
  lda_absx(PlatformCollisionFlag); // if player not standing on either platform,
  // skip this part
  if (!neg_flag) {
    tax(); // transfer collision flag offset as offset to X
    PositionPlayerOnVPlat(); // and position player appropriately
  }
  // ExPF:
  ldx_zp(ObjectOffset); // get enemy object buffer offset and leave
  // --------------------------------
}

void MoveSmallPlatform(void) {
  MoveLiftPlatforms(); // execute common to all large and small lift platforms
  ChkSmallPlatCollision(); // branch to position player correctly
}

void MoveLiftPlatforms(void) {
  lda_abs(TimerControl); // if master timer control set, skip all of this
  if (zero_flag) {
    lda_absx(Enemy_YMF_Dummy);
    carry_flag = false; // add contents of movement amount to whatever's here
    adc_absx(Enemy_Y_MoveForce);
    ram[Enemy_YMF_Dummy + x] = a;
    lda_zpx(Enemy_Y_Position); // add whatever vertical speed is set to current
    adc_zpx(Enemy_Y_Speed); // vertical position plus carry to move up or down
    ram[Enemy_Y_Position + x] = a; // and then leave
  }
}

void ChkSmallPlatCollision(void) {
  lda_absx(PlatformCollisionFlag); // get bounding box counter saved in collision flag
  if (!zero_flag) {
    PositionPlayerOnS_Plat(); // use to position player correctly
    // -------------------------------------------------------------------------------------
    // $00 - page location of extended left boundary
    // $01 - extended left boundary position
    // $02 - page location of extended right boundary
    // $03 - extended right boundary position
  }
}

void FireballEnemyCollision(void) {
  lda_zpx(Fireball_State); // check to see if fireball state is set at all
  if (zero_flag) { goto ExitFBallEnemy; } // branch to leave if not
  asl_acc();
  if (carry_flag) { goto ExitFBallEnemy; } // branch to leave also if d7 in state is set
  lda_zp(FrameCounter);
  lsr_acc(); // get LSB of frame counter
  if (carry_flag) { goto ExitFBallEnemy; } // branch to leave if set (do routine every other frame)
  txa();
  asl_acc(); // multiply fireball offset by four
  asl_acc();
  carry_flag = false;
  adc_imm(0x1c); // then add $1c or 28 bytes to it
  tay(); // to use fireball's bounding box coordinates
  ldx_imm(0x4);
  
FireballEnemyCDLoop:
  ram[0x1] = x; // store enemy object offset here
  tya();
  pha(); // push fireball offset to the stack
  lda_zpx(Enemy_State);
  and_imm(0b00100000); // check to see if d5 is set in enemy state
  if (!zero_flag) { goto NoFToECol; } // if so, skip to next enemy slot
  lda_zpx(Enemy_Flag); // check to see if buffer flag is set
  if (zero_flag) { goto NoFToECol; } // if not, skip to next enemy slot
  lda_zpx(Enemy_ID); // check enemy identifier
  cmp_imm(0x24);
  if (!carry_flag) { goto GoombaDie; } // if < $24, branch to check further
  cmp_imm(0x2b);
  if (!carry_flag) { goto NoFToECol; } // if in range $24-$2a, skip to next enemy slot
  
GoombaDie:
  cmp_imm(Goomba); // check for goomba identifier
  if (!zero_flag) { goto NotGoomba; } // if not found, continue with code
  lda_zpx(Enemy_State); // otherwise check for defeated state
  cmp_imm(0x2); // if stomped or otherwise defeated,
  if (carry_flag) { goto NoFToECol; } // skip to next enemy slot
  
NotGoomba:
  lda_absx(EnemyOffscrBitsMasked); // if any masked offscreen bits set,
  if (!zero_flag) { goto NoFToECol; } // skip to next enemy slot
  txa();
  asl_acc(); // otherwise multiply enemy offset by four
  asl_acc();
  carry_flag = false;
  adc_imm(0x4); // add 4 bytes to it
  tax(); // to use enemy's bounding box coordinates
  SprObjectCollisionCore(); // do fireball-to-enemy collision detection
  ldx_zp(ObjectOffset); // return fireball's original offset
  if (!carry_flag) { goto NoFToECol; } // if carry clear, no collision, thus do next enemy slot
  lda_imm(0b10000000);
  ram[Fireball_State + x] = a; // set d7 in enemy state
  ldx_zp(0x1); // get enemy offset
  HandleEnemyFBallCol(); // jump to handle fireball to enemy collision
  
NoFToECol:
  pla(); // pull fireball offset from stack
  tay(); // put it in Y
  ldx_zp(0x1); // get enemy object offset
  dex(); // decrement it
  if (!neg_flag) { goto FireballEnemyCDLoop; } // loop back until collision detection done on all enemies
  
ExitFBallEnemy:
  ldx_zp(ObjectOffset); // get original fireball offset and leave
}

void HandleEnemyFBallCol(void) {
  RelativeEnemyPosition(); // get relative coordinate of enemy
  ldx_zp(0x1); // get current enemy object offset
  lda_zpx(Enemy_Flag); // check buffer flag for d7 set
  if (!neg_flag) { goto ChkBuzzyBeetle; } // branch if not set to continue
  and_imm(0b00001111); // otherwise mask out high nybble and
  tax(); // use low nybble as enemy offset
  lda_zpx(Enemy_ID);
  cmp_imm(Bowser); // check enemy identifier for bowser
  if (zero_flag) { goto HurtBowser; } // branch if found
  ldx_zp(0x1); // otherwise retrieve current enemy offset
  
ChkBuzzyBeetle:
  lda_zpx(Enemy_ID);
  cmp_imm(BuzzyBeetle); // check for buzzy beetle
  if (zero_flag) { return; } // branch if found to leave (buzzy beetles fireproof)
  cmp_imm(Bowser); // check for bowser one more time (necessary if d7 of flag was clear)
  if (!zero_flag) { goto ChkOtherEnemies; } // if not found, branch to check other enemies
  
HurtBowser:
  dec_abs(BowserHitPoints); // decrement bowser's hit points
  if (!zero_flag) { return; } // if bowser still has hit points, branch to leave
  InitVStf(); // otherwise do sub to init vertical speed and movement force
  ram[Enemy_X_Speed + x] = a; // initialize horizontal speed
  ram[EnemyFrenzyBuffer] = a; // init enemy frenzy buffer
  lda_imm(0xfe);
  ram[Enemy_Y_Speed + x] = a; // set vertical speed to make defeated bowser jump a little
  ldy_abs(WorldNumber); // use world number as offset
  lda_absy(BowserIdentities); // get enemy identifier to replace bowser with
  ram[Enemy_ID + x] = a; // set as new enemy identifier
  lda_imm(0x20); // set A to use starting value for state
  cpy_imm(0x3); // check to see if using offset of 3 or more
  if (carry_flag) { goto SetDBSte; } // branch if so
  ora_imm(0x3); // otherwise add 3 to enemy state
  
SetDBSte:
  ram[Enemy_State + x] = a; // set defeated enemy state
  lda_imm(Sfx_BowserFall);
  ram[Square2SoundQueue] = a; // load bowser defeat sound
  ldx_zp(0x1); // get enemy offset
  lda_imm(0x9); // award 5000 points to player for defeating bowser
  EnemySmackScore(); // unconditional branch to award points
  return;
  
ChkOtherEnemies:
  cmp_imm(BulletBill_FrenzyVar);
  if (zero_flag) { return; } // branch to leave if bullet bill (frenzy variant)
  cmp_imm(Podoboo);
  if (zero_flag) { return; } // branch to leave if podoboo
  cmp_imm(0x15);
  if (carry_flag) { return; } // branch to leave if identifier => $15
  ShellOrBlockDefeat(); // <fallthrough>
}

void ShellOrBlockDefeat(void) {
  lda_zpx(Enemy_ID); // check for piranha plant
  cmp_imm(PiranhaPlant);
  // branch if not found
  if (zero_flag) {
    lda_zpx(Enemy_Y_Position);
    adc_imm(0x18); // add 24 pixels to enemy object's vertical position
    ram[Enemy_Y_Position + x] = a;
  }
  // StnE:
  ChkToStunEnemies(); // do yet another sub
  lda_zpx(Enemy_State);
  and_imm(0b00011111); // mask out 2 MSB of enemy object's state
  ora_imm(0b00100000); // set d5 to defeat enemy and save as new state
  ram[Enemy_State + x] = a;
  lda_imm(0x2); // award 200 points by default
  ldy_zpx(Enemy_ID); // check for hammer bro
  cpy_imm(HammerBro);
  // branch if not found
  if (zero_flag) {
    lda_imm(0x6); // award 1000 points for hammer bro
  }
  // GoombaPoints:
  cpy_imm(Goomba); // check for goomba
  // branch if not found
  if (!zero_flag) {
    EnemySmackScore();
    return;
  }
  lda_imm(0x1); // award 100 points for goomba
  EnemySmackScore(); // <fallthrough>
}

void EnemySmackScore(void) {
  SetupFloateyNumber(); // update necessary score variables
  lda_imm(Sfx_EnemySmack); // play smack enemy sound
  ram[Square1SoundQueue] = a;
  // -------------------------------------------------------------------------------------
}

void PlayerHammerCollision(void) {
  lda_zp(FrameCounter); // get frame counter
  lsr_acc(); // shift d0 into carry
  if (!carry_flag) { return; } // branch to leave if d0 not set to execute every other frame
  lda_abs(TimerControl); // if either master timer control
  ora_abs(Misc_OffscreenBits); // or any offscreen bits for hammer are set,
  if (!zero_flag) { return; } // branch to leave
  txa();
  asl_acc(); // multiply misc object offset by four
  asl_acc();
  carry_flag = false;
  adc_imm(0x24); // add 36 or $24 bytes to get proper offset
  tay(); // for misc object bounding box coordinates
  PlayerCollisionCore(); // do player-to-hammer collision detection
  ldx_zp(ObjectOffset); // get misc object offset
  if (!carry_flag) { goto ClHCol; } // if no collision, then branch
  lda_absx(Misc_Collision_Flag); // otherwise read collision flag
  if (!zero_flag) { return; } // if collision flag already set, branch to leave
  lda_imm(0x1);
  ram[Misc_Collision_Flag + x] = a; // otherwise set collision flag now
  lda_zpx(Misc_X_Speed);
  eor_imm(0xff); // get two's compliment of
  carry_flag = false; // hammer's horizontal speed
  adc_imm(0x1);
  ram[Misc_X_Speed + x] = a; // set to send hammer flying the opposite direction
  lda_abs(StarInvincibleTimer); // if star mario invincibility timer set,
  if (!zero_flag) { return; } // branch to leave
  InjurePlayer(); // otherwise jump to hurt player, do not return
  return;
  
ClHCol:
  lda_imm(0x0); // clear collision flag
  ram[Misc_Collision_Flag + x] = a;
  // -------------------------------------------------------------------------------------
}

void HandlePowerUpCollision(void) {
  EraseEnemyObject(); // erase the power-up object
  lda_imm(0x6);
  SetupFloateyNumber(); // award 1000 points to player by default
  lda_imm(Sfx_PowerUpGrab);
  ram[Square2SoundQueue] = a; // play the power-up sound
  lda_zp(PowerUpType); // check power-up type
  cmp_imm(0x2);
  if (!carry_flag) { goto Shroom_Flower_PUp; } // if mushroom or fire flower, branch
  cmp_imm(0x3);
  if (zero_flag) { goto SetFor1Up; } // if 1-up mushroom, branch
  lda_imm(0x23); // otherwise set star mario invincibility
  ram[StarInvincibleTimer] = a; // timer, and load the star mario music
  lda_imm(StarPowerMusic); // into the area music queue, then leave
  ram[AreaMusicQueue] = a;
  return;
  
Shroom_Flower_PUp:
  lda_abs(PlayerStatus); // if player status = small, branch
  if (zero_flag) { goto UpToSuper; }
  cmp_imm(0x1); // if player status not super, leave
  if (!zero_flag) { goto Shroom_Flower_PUpExit; }
  ldx_zp(ObjectOffset); // get enemy offset, not necessary
  lda_imm(0x2); // set player status to fiery
  ram[PlayerStatus] = a;
  GetPlayerColors(); // run sub to change colors of player
  ldx_zp(ObjectOffset); // get enemy offset again, and again not necessary
  lda_imm(0xc); // set value to be used by subroutine tree (fiery)
  UpToFiery(); // jump to set values accordingly
  
Shroom_Flower_PUpExit:
  return;
  
SetFor1Up:
  lda_imm(0xb); // change 1000 points into 1-up instead
  ram[FloateyNum_Control + x] = a; // and then leave
  return;
  
UpToSuper:
  lda_imm(0x1); // set player status to super
  ram[PlayerStatus] = a;
  lda_imm(0x9); // set value to be used by subroutine tree (super)
  UpToFiery(); // <fallthrough>
}

void UpToFiery(void) {
  ldy_imm(0x0); // set value to be used as new player state
  SetPRout(); // set values to stop certain things in motion
  // --------------------------------
}

void PlayerEnemyCollision(void) {
  lda_zp(FrameCounter); // check counter for d0 set
  lsr_acc();
  if (carry_flag) { goto NoPECol; } // if set, branch to leave
  CheckPlayerVertical(); // if player object is completely offscreen or
  if (carry_flag) { goto NoPECol; } // if down past 224th pixel row, branch to leave
  lda_absx(EnemyOffscrBitsMasked); // if current enemy is offscreen by any amount,
  if (!zero_flag) { goto NoPECol; } // go ahead and branch to leave
  lda_zp(GameEngineSubroutine);
  cmp_imm(0x8); // if not set to run player control routine
  if (!zero_flag) { goto NoPECol; } // on next frame, branch to leave
  lda_zpx(Enemy_State);
  and_imm(0b00100000); // if enemy state has d5 set, branch to leave
  if (!zero_flag) { goto NoPECol; }
  GetEnemyBoundBoxOfs(); // get bounding box offset for current enemy object
  PlayerCollisionCore(); // do collision detection on player vs. enemy
  ldx_zp(ObjectOffset); // get enemy object buffer offset
  if (carry_flag) { goto CheckForPUpCollision; } // if collision, branch past this part here
  lda_absx(Enemy_CollisionBits);
  and_imm(0b11111110); // otherwise, clear d0 of current enemy object's
  ram[Enemy_CollisionBits + x] = a; // collision bit
  
NoPECol:
  return;
  
CheckForPUpCollision:
  ldy_zpx(Enemy_ID);
  cpy_imm(PowerUpObject); // check for power-up object
  if (!zero_flag) { goto EColl; } // if not found, branch to next part
  HandlePowerUpCollision(); // otherwise, unconditional jump backwards
  return;
  
EColl:
  lda_abs(StarInvincibleTimer); // if star mario invincibility timer expired,
  if (zero_flag) { goto HandlePECollisions; } // perform task here, otherwise kill enemy like
  ShellOrBlockDefeat(); // hit with a shell, or from beneath
  return;
  
HandlePECollisions:
  lda_absx(Enemy_CollisionBits); // check enemy collision bits for d0 set
  and_imm(0b00000001); // or for being offscreen at all
  ora_absx(EnemyOffscrBitsMasked);
  if (!zero_flag) { goto ExPEC; } // branch to leave if either is true
  lda_imm(0x1);
  ora_absx(Enemy_CollisionBits); // otherwise set d0 now
  ram[Enemy_CollisionBits + x] = a;
  cpy_imm(Spiny); // branch if spiny
  if (zero_flag) { goto ChkForPlayerInjury; }
  cpy_imm(PiranhaPlant); // branch if piranha plant
  if (zero_flag) { InjurePlayer(); return; }
  cpy_imm(Podoboo); // branch if podoboo
  if (zero_flag) { InjurePlayer(); return; }
  cpy_imm(BulletBill_CannonVar); // branch if bullet bill
  if (zero_flag) { goto ChkForPlayerInjury; }
  cpy_imm(0x15); // branch if object => $15
  if (carry_flag) { InjurePlayer(); return; }
  lda_abs(AreaType); // branch if water type level
  if (zero_flag) { InjurePlayer(); return; }
  lda_zpx(Enemy_State); // branch if d7 of enemy state was set
  asl_acc();
  if (carry_flag) { goto ChkForPlayerInjury; }
  lda_zpx(Enemy_State); // mask out all but 3 LSB of enemy state
  and_imm(0b00000111);
  cmp_imm(0x2); // branch if enemy is in normal or falling state
  if (!carry_flag) { goto ChkForPlayerInjury; }
  lda_zpx(Enemy_ID); // branch to leave if goomba in defeated state
  cmp_imm(Goomba);
  if (zero_flag) { goto ExPEC; }
  lda_imm(Sfx_EnemySmack); // play smack enemy sound
  ram[Square1SoundQueue] = a;
  lda_zpx(Enemy_State); // set d7 in enemy state, thus become moving shell
  ora_imm(0b10000000);
  ram[Enemy_State + x] = a;
  EnemyFacePlayer(); // set moving direction and get offset
  lda_absy(KickedShellXSpdData); // load and set horizontal speed data with offset
  ram[Enemy_X_Speed + x] = a;
  lda_imm(0x3); // add three to whatever the stomp counter contains
  carry_flag = false; // to give points for kicking the shell
  adc_abs(StompChainCounter);
  ldy_absx(EnemyIntervalTimer); // check shell enemy's timer
  cpy_imm(0x3); // if above a certain point, branch using the points
  if (carry_flag) { goto KSPts; } // data obtained from the stomp counter + 3
  lda_absy(KickedShellPtsData); // otherwise, set points based on proximity to timer expiration
  
KSPts:
  SetupFloateyNumber(); // set values for floatey number now
  
ExPEC:
  return; // leave!!!
  
ChkForPlayerInjury:
  lda_zp(Player_Y_Speed); // check player's vertical speed
  if (neg_flag) { goto ChkInj; } // perform procedure below if player moving upwards
  if (!zero_flag) { EnemyStomped(); return; } // or not at all, and branch elsewhere if moving downwards
  
ChkInj:
  lda_zpx(Enemy_ID); // branch if enemy object < $07
  cmp_imm(Bloober);
  if (!carry_flag) { goto ChkETmrs; }
  lda_zp(Player_Y_Position); // add 12 pixels to player's vertical position
  carry_flag = false;
  adc_imm(0xc);
  cmp_zpx(Enemy_Y_Position); // compare modified player's position to enemy's position
  if (!carry_flag) { EnemyStomped(); return; } // branch if this player's position above (less than) enemy's
  
ChkETmrs:
  lda_abs(StompTimer); // check stomp timer
  if (!zero_flag) { EnemyStomped(); return; } // branch if set
  lda_abs(InjuryTimer); // check to see if injured invincibility timer still
  if (!zero_flag) { ExInjColRoutines(); return; } // counting down, and branch elsewhere to leave if so
  lda_abs(Player_Rel_XPos);
  cmp_abs(Enemy_Rel_XPos); // if player's relative position to the left of enemy's
  if (!carry_flag) { goto TInjE; } // relative position, branch here
  ChkEnemyFaceRight(); // otherwise do a jump here
  return;
  
TInjE:
  lda_zpx(Enemy_MovingDir); // if enemy moving towards the left,
  cmp_imm(0x1); // branch, otherwise do a jump here
  if (!zero_flag) { InjurePlayer(); return; } // to turn the enemy around
  LInj();
}

void InjurePlayer(void) {
  lda_abs(InjuryTimer); // check again to see if injured invincibility timer is
  // at zero, and branch to leave if so
  if (!zero_flag) {
    ExInjColRoutines();
    return;
  }
  ForceInjury(); // <fallthrough>
}

void ForceInjury(void) {
  ldx_abs(PlayerStatus); // check player's status
  // branch if small
  if (zero_flag) {
    KillPlayer();
    return;
  }
  ram[PlayerStatus] = a; // otherwise set player's status to small
  lda_imm(0x8);
  ram[InjuryTimer] = a; // set injured invincibility timer
  asl_acc();
  ram[Square1SoundQueue] = a; // play pipedown/injury sound
  GetPlayerColors(); // change player's palette if necessary
  lda_imm(0xa); // set subroutine to run on next frame
  SetKRout(); // <fallthrough>
}

void SetKRout(void) {
  ldy_imm(0x1); // set new player state
  SetPRout(); // <fallthrough>
}

void SetPRout(void) {
  ram[GameEngineSubroutine] = a; // load new value to run subroutine on next frame
  ram[Player_State] = y; // store new player state
  ldy_imm(0xff);
  ram[TimerControl] = y; // set master timer control flag to halt timers
  iny();
  ram[ScrollAmount] = y; // initialize scroll speed
  ExInjColRoutines(); // <fallthrough>
}

void ExInjColRoutines(void) {
  ldx_zp(ObjectOffset); // get enemy offset and leave
}

void KillPlayer(void) {
  ram[Player_X_Speed] = x; // halt player's horizontal movement by initializing speed
  inx();
  ram[EventMusicQueue] = x; // set event music queue to death music
  lda_imm(0xfc);
  ram[Player_Y_Speed] = a; // set new vertical speed
  lda_imm(0xb); // set subroutine to run on next frame
  // branch to set player's state and other things
  if (!zero_flag) {
    SetKRout();
    return;
  }
  EnemyStomped(); // <fallthrough>
}

void EnemyStomped(void) {
  lda_zpx(Enemy_ID); // check for spiny, branch to hurt player
  cmp_imm(Spiny); // if found
  if (zero_flag) { InjurePlayer(); return; }
  lda_imm(Sfx_EnemyStomp); // otherwise play stomp/swim sound
  ram[Square1SoundQueue] = a;
  lda_zpx(Enemy_ID);
  ldy_imm(0x0); // initialize points data offset for stomped enemies
  cmp_imm(FlyingCheepCheep); // branch for cheep-cheep
  if (zero_flag) { goto EnemyStompedPts; }
  cmp_imm(BulletBill_FrenzyVar); // branch for either bullet bill object
  if (zero_flag) { goto EnemyStompedPts; }
  cmp_imm(BulletBill_CannonVar);
  if (zero_flag) { goto EnemyStompedPts; }
  cmp_imm(Podoboo); // branch for podoboo (this branch is logically impossible
  if (zero_flag) { goto EnemyStompedPts; } // for cpu to take due to earlier checking of podoboo)
  iny(); // increment points data offset
  cmp_imm(HammerBro); // branch for hammer bro
  if (zero_flag) { goto EnemyStompedPts; }
  iny(); // increment points data offset
  cmp_imm(Lakitu); // branch for lakitu
  if (zero_flag) { goto EnemyStompedPts; }
  iny(); // increment points data offset
  cmp_imm(Bloober); // branch if NOT bloober
  if (!zero_flag) { goto ChkForDemoteKoopa; }
  
EnemyStompedPts:
  lda_absy(StompedEnemyPtsData); // load points data using offset in Y
  SetupFloateyNumber(); // run sub to set floatey number controls
  lda_zpx(Enemy_MovingDir);
  pha(); // save enemy movement direction to stack
  SetStun(); // run sub to kill enemy
  pla();
  ram[Enemy_MovingDir + x] = a; // return enemy movement direction from stack
  lda_imm(0b00100000);
  ram[Enemy_State + x] = a; // set d5 in enemy state
  InitVStf(); // nullify vertical speed, physics-related thing,
  ram[Enemy_X_Speed + x] = a; // and horizontal speed
  lda_imm(0xfd); // set player's vertical speed, to give bounce
  ram[Player_Y_Speed] = a;
  return;
  
ChkForDemoteKoopa:
  cmp_imm(0x9); // branch elsewhere if enemy object < $09
  if (!carry_flag) { goto HandleStompedShellE; }
  and_imm(0b00000001); // demote koopa paratroopas to ordinary troopas
  ram[Enemy_ID + x] = a;
  ldy_imm(0x0); // return enemy to normal state
  ram[Enemy_State + x] = y;
  lda_imm(0x3); // award 400 points to the player
  SetupFloateyNumber();
  InitVStf(); // nullify physics-related thing and vertical speed
  EnemyFacePlayer(); // turn enemy around if necessary
  lda_absy(DemotedKoopaXSpdData);
  ram[Enemy_X_Speed + x] = a; // set appropriate moving speed based on direction
  goto SBnce; // then move onto something else
  
HandleStompedShellE:
  lda_imm(0x4); // set defeated state for enemy
  ram[Enemy_State + x] = a;
  inc_abs(StompChainCounter); // increment the stomp counter
  lda_abs(StompChainCounter); // add whatever is in the stomp counter
  carry_flag = false; // to whatever is in the stomp timer
  adc_abs(StompTimer);
  SetupFloateyNumber(); // award points accordingly
  inc_abs(StompTimer); // increment stomp timer of some sort
  ldy_abs(PrimaryHardMode); // check primary hard mode flag
  lda_absy(RevivalRateData); // load timer setting according to flag
  ram[EnemyIntervalTimer + x] = a; // set as enemy timer to revive stomped enemy
  
SBnce:
  lda_imm(0xfc); // set player's vertical speed for bounce
  ram[Player_Y_Speed] = a; // and then leave!!!
}

void ChkEnemyFaceRight(void) {
  lda_zpx(Enemy_MovingDir); // check to see if enemy is moving to the right
  cmp_imm(0x1);
  // if not, branch
  if (!zero_flag) {
    LInj();
    return;
  }
  InjurePlayer(); // otherwise go back to hurt player
}

void LInj(void) {
  EnemyTurnAround(); // turn the enemy around, if necessary
  InjurePlayer(); // go back to hurt player
}

void EnemyFacePlayer(void) {
  ldy_imm(0x1); // set to move right by default
  PlayerEnemyDiff(); // get horizontal difference between player and enemy
  // if enemy is to the right of player, do not increment
  if (neg_flag) {
    iny(); // otherwise, increment to set to move to the left
  }
  // SFcRt:
  ram[Enemy_MovingDir + x] = y; // set moving direction here
  dey(); // then decrement to use as a proper offset
}

void SetupFloateyNumber(void) {
  ram[FloateyNum_Control + x] = a; // set number of points control for floatey numbers
  lda_imm(0x30);
  ram[FloateyNum_Timer + x] = a; // set timer for floatey numbers
  lda_zpx(Enemy_Y_Position);
  ram[FloateyNum_Y_Pos + x] = a; // set vertical coordinate
  lda_abs(Enemy_Rel_XPos);
  ram[FloateyNum_X_Pos + x] = a; // set horizontal coordinate and leave
  // -------------------------------------------------------------------------------------
  // $01 - used to hold enemy offset for second enemy
}

void EnemiesCollision(void) {
  lda_zp(FrameCounter); // check counter for d0 set
  lsr_acc();
  if (!carry_flag) { return; } // if d0 not set, leave
  lda_abs(AreaType);
  if (zero_flag) { return; } // if water area type, leave
  lda_zpx(Enemy_ID);
  cmp_imm(0x15); // if enemy object => $15, branch to leave
  if (carry_flag) { ExitECRoutine(); return; }
  cmp_imm(Lakitu); // if lakitu, branch to leave
  if (zero_flag) { ExitECRoutine(); return; }
  cmp_imm(PiranhaPlant); // if piranha plant, branch to leave
  if (zero_flag) { ExitECRoutine(); return; }
  lda_absx(EnemyOffscrBitsMasked); // if masked offscreen bits nonzero, branch to leave
  if (!zero_flag) { ExitECRoutine(); return; }
  GetEnemyBoundBoxOfs(); // otherwise, do sub, get appropriate bounding box offset for
  dex(); // first enemy we're going to compare, then decrement for second
  if (neg_flag) { ExitECRoutine(); return; } // branch to leave if there are no other enemies
  ECLoop(); // <fallthrough>
}

void ECLoop(void) {
  ram[0x1] = x; // save enemy object buffer offset for second enemy here
  tya(); // save first enemy's bounding box offset to stack
  pha();
  lda_zpx(Enemy_Flag); // check enemy object enable flag
  // branch if flag not set
  if (zero_flag) {
    ReadyNextEnemy();
    return;
  }
  lda_zpx(Enemy_ID);
  cmp_imm(0x15); // check for enemy object => $15
  // branch if true
  if (carry_flag) {
    ReadyNextEnemy();
    return;
  }
  cmp_imm(Lakitu);
  // branch if enemy object is lakitu
  if (zero_flag) {
    ReadyNextEnemy();
    return;
  }
  cmp_imm(PiranhaPlant);
  // branch if enemy object is piranha plant
  if (zero_flag) {
    ReadyNextEnemy();
    return;
  }
  lda_absx(EnemyOffscrBitsMasked);
  // branch if masked offscreen bits set
  if (!zero_flag) {
    ReadyNextEnemy();
    return;
  }
  txa(); // get second enemy object's bounding box offset
  asl_acc(); // multiply by four, then add four
  asl_acc();
  carry_flag = false;
  adc_imm(0x4);
  tax(); // use as new contents of X
  SprObjectCollisionCore(); // do collision detection using the two enemies here
  ldx_zp(ObjectOffset); // use first enemy offset for X
  ldy_zp(0x1); // use second enemy offset for Y
  // if carry clear, no collision, branch ahead of this
  if (carry_flag) {
    lda_zpx(Enemy_State);
    ora_zpy(Enemy_State); // check both enemy states for d7 set
    and_imm(0b10000000);
    // branch if at least one of them is set
    if (zero_flag) {
      lda_absy(Enemy_CollisionBits); // load first enemy's collision-related bits
      and_absx(SetBitsMask); // check to see if bit connected to second enemy is
      // already set, and move onto next enemy slot if set
      if (!zero_flag) {
        ReadyNextEnemy();
        return;
      }
      lda_absy(Enemy_CollisionBits);
      ora_absx(SetBitsMask); // if the bit is not set, set it now
      ram[Enemy_CollisionBits + y] = a;
    }
    // YesEC:
    ProcEnemyCollisions(); // react according to the nature of collision
    ReadyNextEnemy(); // move onto next enemy slot
    return;
  }
  // NoEnemyCollision:
  lda_absy(Enemy_CollisionBits); // load first enemy's collision-related bits
  and_absx(ClearBitsMask); // clear bit connected to second enemy
  ram[Enemy_CollisionBits + y] = a; // then move onto next enemy slot
  ReadyNextEnemy(); // <fallthrough>
}

void ReadyNextEnemy(void) {
  pla(); // get first enemy's bounding box offset from the stack
  tay(); // use as Y again
  ldx_zp(0x1); // get and decrement second enemy's object buffer offset
  dex();
  // loop until all enemy slots have been checked
  if (!neg_flag) {
    ECLoop();
    return;
  }
  ExitECRoutine(); // <fallthrough>
}

void ExitECRoutine(void) {
  ldx_zp(ObjectOffset); // get enemy object buffer offset
}

void ProcEnemyCollisions(void) {
  lda_zpy(Enemy_State); // check both enemy states for d5 set
  ora_zpx(Enemy_State);
  and_imm(0b00100000); // if d5 is set in either state, or both, branch
  if (!zero_flag) { goto ExitProcessEColl; } // to leave and do nothing else at this point
  lda_zpx(Enemy_State);
  cmp_imm(0x6); // if second enemy state < $06, branch elsewhere
  if (carry_flag) {
    lda_zpx(Enemy_ID); // check second enemy identifier for hammer bro
    cmp_imm(HammerBro); // if hammer bro found in alt state, branch to leave
    if (zero_flag) { goto ExitProcessEColl; }
    lda_zpy(Enemy_State); // check first enemy state for d7 set
    asl_acc();
    // branch if d7 is clear
    if (carry_flag) {
      lda_imm(0x6);
      SetupFloateyNumber(); // award 1000 points for killing enemy
      ShellOrBlockDefeat(); // then kill enemy, then load
      ldy_zp(0x1); // original offset of second enemy
    }
    // ShellCollisions:
    tya(); // move Y to X
    tax();
    ShellOrBlockDefeat(); // kill second enemy
    ldx_zp(ObjectOffset);
    lda_absx(ShellChainCounter); // get chain counter for shell
    carry_flag = false;
    adc_imm(0x4); // add four to get appropriate point offset
    ldx_zp(0x1);
    SetupFloateyNumber(); // award appropriate number of points for second enemy
    ldx_zp(ObjectOffset); // load original offset of first enemy
    inc_absx(ShellChainCounter); // increment chain counter for additional enemies
    
ExitProcessEColl:
    return; // leave!!!
  }
  // ProcSecondEnemyColl:
  lda_zpy(Enemy_State); // if first enemy state < $06, branch elsewhere
  cmp_imm(0x6);
  if (carry_flag) {
    lda_zpy(Enemy_ID); // check first enemy identifier for hammer bro
    cmp_imm(HammerBro); // if hammer bro found in alt state, branch to leave
    if (zero_flag) { goto ExitProcessEColl; }
    ShellOrBlockDefeat(); // otherwise, kill first enemy
    ldy_zp(0x1);
    lda_absy(ShellChainCounter); // get chain counter for shell
    carry_flag = false;
    adc_imm(0x4); // add four to get appropriate point offset
    ldx_zp(ObjectOffset);
    SetupFloateyNumber(); // award appropriate number of points for first enemy
    ldx_zp(0x1); // load original offset of second enemy
    inc_absx(ShellChainCounter); // increment chain counter for additional enemies
    return; // leave!!!
  }
  // MoveEOfs:
  tya(); // move Y ($01) to X
  tax();
  EnemyTurnAround(); // do the sub here using value from $01
  ldx_zp(ObjectOffset); // then do it again using value from $08
  EnemyTurnAround(); // <fallthrough>
}

void EnemyTurnAround(void) {
  lda_zpx(Enemy_ID); // check for specific enemies
  cmp_imm(PiranhaPlant);
  if (zero_flag) { return; } // if piranha plant, leave
  cmp_imm(Lakitu);
  if (zero_flag) { return; } // if lakitu, leave
  cmp_imm(HammerBro);
  if (zero_flag) { return; } // if hammer bro, leave
  cmp_imm(Spiny);
  if (zero_flag) { RXSpd(); return; } // if spiny, turn it around
  cmp_imm(GreenParatroopaJump);
  if (zero_flag) { RXSpd(); return; } // if green paratroopa, turn it around
  cmp_imm(0x7);
  if (carry_flag) { return; } // if any OTHER enemy object => $07, leave
  RXSpd();
}

void RXSpd(void) {
  lda_zpx(Enemy_X_Speed); // load horizontal speed
  eor_imm(0xff); // get two's compliment for horizontal speed
  tay();
  iny();
  ram[Enemy_X_Speed + x] = y; // store as new horizontal speed
  lda_zpx(Enemy_MovingDir);
  eor_imm(0b00000011); // invert moving direction and store, then leave
  ram[Enemy_MovingDir + x] = a; // thus effectively turning the enemy around
  // -------------------------------------------------------------------------------------
  // $00 - vertical position of platform
}

void LargePlatformCollision(void) {
  lda_imm(0xff); // save value here
  ram[PlatformCollisionFlag + x] = a;
  lda_abs(TimerControl); // check master timer control
  // if set, branch to leave
  if (!zero_flag) {
    ExLPC();
    return;
  }
  lda_zpx(Enemy_State); // if d7 set in object state,
  // branch to leave
  if (neg_flag) {
    ExLPC();
    return;
  }
  lda_zpx(Enemy_ID);
  cmp_imm(0x24); // check enemy object identifier for
  // balance platform, branch if not found
  if (!zero_flag) {
    ChkForPlayerC_LargeP();
    return;
  }
  lda_zpx(Enemy_State);
  tax(); // set state as enemy offset here
  ChkForPlayerC_LargeP(); // perform code with state offset, then original offset, in X
  ChkForPlayerC_LargeP(); // <fallthrough>
}

void ChkForPlayerC_LargeP(void) {
  CheckPlayerVertical(); // figure out if player is below a certain point
  // or offscreen, branch to leave if true
  if (carry_flag) {
    ExLPC();
    return;
  }
  txa();
  GetEnemyBoundBoxOfsArg(); // get bounding box offset in Y
  lda_zpx(Enemy_Y_Position); // store vertical coordinate in
  ram[0x0] = a; // temp variable for now
  txa(); // send offset we're on to the stack
  pha();
  PlayerCollisionCore(); // do player-to-platform collision detection
  pla(); // retrieve offset from the stack
  tax();
  // if no collision, branch to leave
  if (!carry_flag) {
    ExLPC();
    return;
  }
  ProcLPlatCollisions(); // otherwise collision, perform sub
  ExLPC(); // <fallthrough>
}

void ExLPC(void) {
  ldx_zp(ObjectOffset); // get enemy object buffer offset and leave
  // --------------------------------
  // $00 - counter for bounding boxes
}

void SmallPlatformCollision(void) {
  lda_abs(TimerControl); // if master timer control set,
  if (!zero_flag) { goto ExSPC; } // branch to leave
  ram[PlatformCollisionFlag + x] = a; // otherwise initialize collision flag
  CheckPlayerVertical(); // do a sub to see if player is below a certain point
  if (carry_flag) { goto ExSPC; } // or entirely offscreen, and branch to leave if true
  lda_imm(0x2);
  ram[0x0] = a; // load counter here for 2 bounding boxes
  
ChkSmallPlatLoop:
  ldx_zp(ObjectOffset); // get enemy object offset
  GetEnemyBoundBoxOfs(); // get bounding box offset in Y
  and_imm(0b00000010); // if d1 of offscreen lower nybble bits was set
  if (!zero_flag) { goto ExSPC; } // then branch to leave
  lda_absy(BoundingBox_UL_YPos); // check top of platform's bounding box for being
  cmp_imm(0x20); // above a specific point
  if (!carry_flag) { goto MoveBoundBox; } // if so, branch, don't do collision detection
  PlayerCollisionCore(); // otherwise, perform player-to-platform collision detection
  if (carry_flag) { goto ProcSPlatCollisions; } // skip ahead if collision
  
MoveBoundBox:
  lda_absy(BoundingBox_UL_YPos); // move bounding box vertical coordinates
  carry_flag = false; // 128 pixels downwards
  adc_imm(0x80);
  ram[BoundingBox_UL_YPos + y] = a;
  lda_absy(BoundingBox_DR_YPos);
  carry_flag = false;
  adc_imm(0x80);
  ram[BoundingBox_DR_YPos + y] = a;
  dec_zp(0x0); // decrement counter we set earlier
  if (!zero_flag) { goto ChkSmallPlatLoop; } // loop back until both bounding boxes are checked
  
ExSPC:
  ldx_zp(ObjectOffset); // get enemy object buffer offset, then leave
  return;
  // --------------------------------
  
ProcSPlatCollisions:
  ldx_zp(ObjectOffset); // return enemy object buffer offset to X, then continue
  ProcLPlatCollisions(); // <fallthrough>
}

void ProcLPlatCollisions(void) {
  lda_absy(BoundingBox_DR_YPos); // get difference by subtracting the top
  carry_flag = true; // of the player's bounding box from the bottom
  sbc_abs(BoundingBox_UL_YPos); // of the platform's bounding box
  cmp_imm(0x4); // if difference too large or negative,
  if (carry_flag) { goto ChkForTopCollision; } // branch, do not alter vertical speed of player
  lda_zp(Player_Y_Speed); // check to see if player's vertical speed is moving down
  if (!neg_flag) { goto ChkForTopCollision; } // if so, don't mess with it
  lda_imm(0x1); // otherwise, set vertical
  ram[Player_Y_Speed] = a; // speed of player to kill jump
  
ChkForTopCollision:
  lda_abs(BoundingBox_DR_YPos); // get difference by subtracting the top
  carry_flag = true; // of the platform's bounding box from the bottom
  sbc_absy(BoundingBox_UL_YPos); // of the player's bounding box
  cmp_imm(0x6);
  if (carry_flag) { goto PlatformSideCollisions; } // if difference not close enough, skip all of this
  lda_zp(Player_Y_Speed);
  if (neg_flag) { goto PlatformSideCollisions; } // if player's vertical speed moving upwards, skip this
  lda_zp(0x0); // get saved bounding box counter from earlier
  ldy_zpx(Enemy_ID);
  cpy_imm(0x2b); // if either of the two small platform objects are found,
  if (zero_flag) { goto SetCollisionFlag; } // regardless of which one, branch to use bounding box counter
  cpy_imm(0x2c); // as contents of collision flag
  if (zero_flag) { goto SetCollisionFlag; }
  txa(); // otherwise use enemy object buffer offset
  
SetCollisionFlag:
  ldx_zp(ObjectOffset); // get enemy object buffer offset
  ram[PlatformCollisionFlag + x] = a; // save either bounding box counter or enemy offset here
  lda_imm(0x0);
  ram[Player_State] = a; // set player state to normal then leave
  return;
  
PlatformSideCollisions:
  lda_imm(0x1); // set value here to indicate possible horizontal
  ram[0x0] = a; // collision on left side of platform
  lda_abs(BoundingBox_DR_XPos); // get difference by subtracting platform's left edge
  carry_flag = true; // from player's right edge
  sbc_absy(BoundingBox_UL_XPos);
  cmp_imm(0x8); // if difference close enough, skip all of this
  if (!carry_flag) { goto SideC; }
  inc_zp(0x0); // otherwise increment value set here for right side collision
  lda_absy(BoundingBox_DR_XPos); // get difference by subtracting player's left edge
  carry_flag = false; // from platform's right edge
  sbc_abs(BoundingBox_UL_XPos);
  cmp_imm(0x9); // if difference not close enough, skip subroutine
  if (carry_flag) { goto NoSideC; } // and instead branch to leave (no collision)
  
SideC:
  ImpedePlayerMove(); // deal with horizontal collision
  
NoSideC:
  ldx_zp(ObjectOffset); // return with enemy object buffer offset
  // -------------------------------------------------------------------------------------
}

void PositionPlayerOnS_Plat(void) {
  tay(); // use bounding box counter saved in collision flag
  lda_zpx(Enemy_Y_Position); // for offset
  carry_flag = false; // add positioning data using offset to the vertical
  adc_absy(PlayerPosSPlatData - 1); // coordinate
  PositionPlayerOnVPlatSkip(); //  .db $2c ;BIT instruction opcode
}

void PositionPlayerOnVPlat(void) {
  lda_zpx(Enemy_Y_Position); // get vertical coordinate
  PositionPlayerOnVPlatSkip(); // <fallthrough>
}

void PositionPlayerOnVPlatSkip(void) {
  ldy_zp(GameEngineSubroutine);
  cpy_imm(0xb); // if certain routine being executed on this frame,
  if (zero_flag) { return; } // skip all of this
  ldy_zpx(Enemy_Y_HighPos);
  cpy_imm(0x1); // if vertical high byte offscreen, skip this
  if (!zero_flag) { return; }
  carry_flag = true; // subtract 32 pixels from vertical coordinate
  sbc_imm(0x20); // for the player object's height
  ram[Player_Y_Position] = a; // save as player's new vertical coordinate
  tya();
  sbc_imm(0x0); // subtract borrow and store as player's
  ram[Player_Y_HighPos] = a; // new vertical high byte
  lda_imm(0x0);
  ram[Player_Y_Speed] = a; // initialize vertical speed and low byte of force
  ram[Player_Y_MoveForce] = a; // and then leave
  // -------------------------------------------------------------------------------------
}

void CheckPlayerVertical(void) {
  lda_abs(Player_OffscreenBits); // if player object is completely offscreen
  cmp_imm(0xf0); // vertically, leave this routine
  if (carry_flag) { return; }
  ldy_zp(Player_Y_HighPos); // if player high vertical byte is not
  dey(); // within the screen, leave this routine
  if (!zero_flag) { return; }
  lda_zp(Player_Y_Position); // if on the screen, check to see how far down
  cmp_imm(0xd0); // the player is vertically
  // -------------------------------------------------------------------------------------
}

void GetEnemyBoundBoxOfs(void) {
  lda_zp(ObjectOffset); // get enemy object buffer offset
  GetEnemyBoundBoxOfsArg(); // <fallthrough>
}

void GetEnemyBoundBoxOfsArg(void) {
  asl_acc(); // multiply A by four, then add four
  asl_acc(); // to skip player's bounding box
  carry_flag = false;
  adc_imm(0x4);
  tay(); // send to Y
  lda_abs(Enemy_OffscreenBits); // get offscreen bits for enemy object
  and_imm(0b00001111); // save low nybble
  cmp_imm(0b00001111); // check for all bits set
  // -------------------------------------------------------------------------------------
  // $00-$01 - used to hold many values, essentially temp variables
  // $04 - holds lower nybble of vertical coordinate from block buffer routine
  // $eb - used to hold block buffer adder
}

void PlayerBGCollision(void) {
  lda_abs(DisableCollisionDet); // if collision detection disabled flag set,
  if (!zero_flag) { goto ExPBGCol; } // branch to leave
  lda_zp(GameEngineSubroutine);
  cmp_imm(0xb); // if running routine #11 or $0b
  if (zero_flag) { goto ExPBGCol; } // branch to leave
  cmp_imm(0x4);
  if (!carry_flag) { goto ExPBGCol; } // if running routines $00-$03 branch to leave
  lda_imm(0x1); // load default player state for swimming
  ldy_abs(SwimmingFlag); // if swimming flag set,
  if (!zero_flag) { goto SetPSte; } // branch ahead to set default state
  lda_zp(Player_State); // if player in normal state,
  if (zero_flag) { goto SetFallS; } // branch to set default state for falling
  cmp_imm(0x3);
  if (!zero_flag) { goto ChkOnScr; } // if in any other state besides climbing, skip to next part
  
SetFallS:
  lda_imm(0x2); // load default player state for falling
  
SetPSte:
  ram[Player_State] = a; // set whatever player state is appropriate
  
ChkOnScr:
  lda_zp(Player_Y_HighPos);
  cmp_imm(0x1); // check player's vertical high byte for still on the screen
  if (!zero_flag) { goto ExPBGCol; } // branch to leave if not
  lda_imm(0xff);
  ram[Player_CollisionBits] = a; // initialize player's collision flag
  lda_zp(Player_Y_Position);
  cmp_imm(0xcf); // check player's vertical coordinate
  if (!carry_flag) { goto ChkCollSize; } // if not too close to the bottom of screen, continue
  
ExPBGCol:
  return; // otherwise leave
  
ChkCollSize:
  ldy_imm(0x2); // load default offset
  lda_abs(CrouchingFlag);
  if (!zero_flag) { goto GBBAdr; } // if player crouching, skip ahead
  lda_abs(PlayerSize);
  if (!zero_flag) { goto GBBAdr; } // if player small, skip ahead
  dey(); // otherwise decrement offset for big player not crouching
  lda_abs(SwimmingFlag);
  if (!zero_flag) { goto GBBAdr; } // if swimming flag set, skip ahead
  dey(); // otherwise decrement offset
  
GBBAdr:
  lda_absy(BlockBufferAdderData); // get value using offset
  ram[0xeb] = a; // store value here
  tay(); // put value into Y, as offset for block buffer routine
  ldx_abs(PlayerSize); // get player's size as offset
  lda_abs(CrouchingFlag);
  if (zero_flag) { goto HeadChk; } // if player not crouching, branch ahead
  inx(); // otherwise increment size as offset
  
HeadChk:
  lda_zp(Player_Y_Position); // get player's vertical coordinate
  cmp_absx(PlayerBGUpperExtent); // compare with upper extent value based on offset
  if (!carry_flag) { goto DoFootCheck; } // if player is too high, skip this part
  BlockBufferColli_Head(); // do player-to-bg collision detection on top of
  if (zero_flag) { goto DoFootCheck; } // player, and branch if nothing above player's head
  CheckForCoinMTiles(); // check to see if player touched coin with their head
  if (carry_flag) { goto AwardTouchedCoin; } // if so, branch to some other part of code
  ldy_zp(Player_Y_Speed); // check player's vertical speed
  if (!neg_flag) { goto DoFootCheck; } // if player not moving upwards, branch elsewhere
  ldy_zp(0x4); // check lower nybble of vertical coordinate returned
  cpy_imm(0x4); // from collision detection routine
  if (!carry_flag) { goto DoFootCheck; } // if low nybble < 4, branch
  CheckForSolidMTiles(); // check to see what player's head bumped on
  if (carry_flag) { goto SolidOrClimb; } // if player collided with solid metatile, branch
  ldy_abs(AreaType); // otherwise check area type
  if (zero_flag) { goto NYSpd; } // if water level, branch ahead
  ldy_abs(BlockBounceTimer); // if block bounce timer not expired,
  if (!zero_flag) { goto NYSpd; } // branch ahead, do not process collision
  PlayerHeadCollision(); // otherwise do a sub to process collision
  goto DoFootCheck; // jump ahead to skip these other parts here
  
SolidOrClimb:
  cmp_imm(0x26); // if climbing metatile,
  if (zero_flag) { goto NYSpd; } // branch ahead and do not play sound
  lda_imm(Sfx_Bump);
  ram[Square1SoundQueue] = a; // otherwise load bump sound
  
NYSpd:
  lda_imm(0x1); // set player's vertical speed to nullify
  ram[Player_Y_Speed] = a; // jump or swim
  
DoFootCheck:
  ldy_zp(0xeb); // get block buffer adder offset
  lda_zp(Player_Y_Position);
  cmp_imm(0xcf); // check to see how low player is
  if (carry_flag) { goto DoPlayerSideCheck; } // if player is too far down on screen, skip all of this
  BlockBufferColli_Feet(); // do player-to-bg collision detection on bottom left of player
  CheckForCoinMTiles(); // check to see if player touched coin with their left foot
  if (carry_flag) { goto AwardTouchedCoin; } // if so, branch to some other part of code
  pha(); // save bottom left metatile to stack
  BlockBufferColli_Feet(); // do player-to-bg collision detection on bottom right of player
  ram[0x0] = a; // save bottom right metatile here
  pla();
  ram[0x1] = a; // pull bottom left metatile and save here
  if (!zero_flag) { goto ChkFootMTile; } // if anything here, skip this part
  lda_zp(0x0); // otherwise check for anything in bottom right metatile
  if (zero_flag) { goto DoPlayerSideCheck; } // and skip ahead if not
  CheckForCoinMTiles(); // check to see if player touched coin with their right foot
  if (!carry_flag) { goto ChkFootMTile; } // if not, skip unconditional jump and continue code
  
AwardTouchedCoin:
  goto HandleCoinMetatile; // follow the code to erase coin and award to player 1 coin
  
ChkFootMTile:
  CheckForClimbMTiles(); // check to see if player landed on climbable metatiles
  if (carry_flag) { goto DoPlayerSideCheck; } // if so, branch
  ldy_zp(Player_Y_Speed); // check player's vertical speed
  if (neg_flag) { goto DoPlayerSideCheck; } // if player moving upwards, branch
  cmp_imm(0xc5);
  if (!zero_flag) { goto ContChk; } // if player did not touch axe, skip ahead
  goto HandleAxeMetatile; // otherwise jump to set modes of operation
  
ContChk:
  ChkInvisibleMTiles(); // do sub to check for hidden coin or 1-up blocks
  if (zero_flag) { goto DoPlayerSideCheck; } // if either found, branch
  ldy_abs(JumpspringAnimCtrl); // if jumpspring animating right now,
  if (!zero_flag) { goto InitSteP; } // branch ahead
  ldy_zp(0x4); // check lower nybble of vertical coordinate returned
  cpy_imm(0x5); // from collision detection routine
  if (!carry_flag) { goto LandPlyr; } // if lower nybble < 5, branch
  lda_zp(Player_MovingDir);
  ram[0x0] = a; // use player's moving direction as temp variable
  ImpedePlayerMove(); return; // jump to impede player's movement in that direction
  
LandPlyr:
  ChkForLandJumpSpring(); // do sub to check for jumpspring metatiles and deal with it
  lda_imm(0xf0);
  and_zp(Player_Y_Position); // mask out lower nybble of player's vertical position
  ram[Player_Y_Position] = a; // and store as new vertical position to land player properly
  HandlePipeEntry(); // do sub to process potential pipe entry
  lda_imm(0x0);
  ram[Player_Y_Speed] = a; // initialize vertical speed and fractional
  ram[Player_Y_MoveForce] = a; // movement force to stop player's vertical movement
  ram[StompChainCounter] = a; // initialize enemy stomp counter
  
InitSteP:
  lda_imm(0x0);
  ram[Player_State] = a; // set player's state to normal
  
DoPlayerSideCheck:
  ldy_zp(0xeb); // get block buffer adder offset
  iny();
  iny(); // increment offset 2 bytes to use adders for side collisions
  lda_imm(0x2); // set value here to be used as counter
  ram[0x0] = a;
  
SideCheckLoop:
  iny(); // move onto the next one
  ram[0xeb] = y; // store it
  lda_zp(Player_Y_Position);
  cmp_imm(0x20); // check player's vertical position
  if (!carry_flag) { goto BHalf; } // if player is in status bar area, branch ahead to skip this part
  cmp_imm(0xe4);
  if (carry_flag) { goto ExSCH; } // branch to leave if player is too far down
  BlockBufferColli_Side(); // do player-to-bg collision detection on one half of player
  if (zero_flag) { goto BHalf; } // branch ahead if nothing found
  cmp_imm(0x1c); // otherwise check for pipe metatiles
  if (zero_flag) { goto BHalf; } // if collided with sideways pipe (top), branch ahead
  cmp_imm(0x6b);
  if (zero_flag) { goto BHalf; } // if collided with water pipe (top), branch ahead
  CheckForClimbMTiles(); // do sub to see if player bumped into anything climbable
  if (!carry_flag) { goto CheckSideMTiles; } // if not, branch to alternate section of code
  
BHalf:
  ldy_zp(0xeb); // load block adder offset
  iny(); // increment it
  lda_zp(Player_Y_Position); // get player's vertical position
  cmp_imm(0x8);
  if (!carry_flag) { goto ExSCH; } // if too high, branch to leave
  cmp_imm(0xd0);
  if (carry_flag) { goto ExSCH; } // if too low, branch to leave
  BlockBufferColli_Side(); // do player-to-bg collision detection on other half of player
  if (!zero_flag) { goto CheckSideMTiles; } // if something found, branch
  dec_zp(0x0); // otherwise decrement counter
  if (!zero_flag) { goto SideCheckLoop; } // run code until both sides of player are checked
  
ExSCH:
  return; // leave
  
CheckSideMTiles:
  ChkInvisibleMTiles(); // check for hidden or coin 1-up blocks
  if (zero_flag) { goto ExCSM; } // branch to leave if either found
  CheckForClimbMTiles(); // check for climbable metatiles
  if (!carry_flag) { goto ContSChk; } // if not found, skip and continue with code
  HandleClimbing(); // otherwise jump to handle climbing
  return;
  
ContSChk:
  CheckForCoinMTiles(); // check to see if player touched coin
  if (carry_flag) { goto HandleCoinMetatile; } // if so, execute code to erase coin and award to player 1 coin
  ChkJumpspringMetatiles(); // check for jumpspring metatiles
  if (!carry_flag) { goto ChkPBtm; } // if not found, branch ahead to continue cude
  lda_abs(JumpspringAnimCtrl); // otherwise check jumpspring animation control
  if (!zero_flag) { goto ExCSM; } // branch to leave if set
  goto StopPlayerMove; // otherwise jump to impede player's movement
  
ChkPBtm:
  ldy_zp(Player_State); // get player's state
  cpy_imm(0x0); // check for player's state set to normal
  if (!zero_flag) { goto StopPlayerMove; } // if not, branch to impede player's movement
  ldy_zp(PlayerFacingDir); // get player's facing direction
  dey();
  if (!zero_flag) { goto StopPlayerMove; } // if facing left, branch to impede movement
  cmp_imm(0x6c); // otherwise check for pipe metatiles
  if (zero_flag) { goto PipeDwnS; } // if collided with sideways pipe (bottom), branch
  cmp_imm(0x1f); // if collided with water pipe (bottom), continue
  if (!zero_flag) { goto StopPlayerMove; } // otherwise branch to impede player's movement
  
PipeDwnS:
  lda_abs(Player_SprAttrib); // check player's attributes
  if (!zero_flag) { goto PlyrPipe; } // if already set, branch, do not play sound again
  ldy_imm(Sfx_PipeDown_Injury);
  ram[Square1SoundQueue] = y; // otherwise load pipedown/injury sound
  
PlyrPipe:
  ora_imm(0b00100000);
  ram[Player_SprAttrib] = a; // set background priority bit in player attributes
  lda_zp(Player_X_Position);
  and_imm(0b00001111); // get lower nybble of player's horizontal coordinate
  if (zero_flag) { goto ChkGERtn; } // if at zero, branch ahead to skip this part
  ldy_imm(0x0); // set default offset for timer setting data
  lda_abs(ScreenLeft_PageLoc); // load page location for left side of screen
  if (zero_flag) { goto SetCATmr; } // if at page zero, use default offset
  iny(); // otherwise increment offset
  
SetCATmr:
  lda_absy(AreaChangeTimerData); // set timer for change of area as appropriate
  ram[ChangeAreaTimer] = a;
  
ChkGERtn:
  lda_zp(GameEngineSubroutine); // get number of game engine routine running
  cmp_imm(0x7);
  if (zero_flag) { goto ExCSM; } // if running player entrance routine or
  cmp_imm(0x8); // player control routine, go ahead and branch to leave
  if (!zero_flag) { goto ExCSM; }
  lda_imm(0x2);
  ram[GameEngineSubroutine] = a; // otherwise set sideways pipe entry routine to run
  return; // and leave
  // --------------------------------
  // $02 - high nybble of vertical coordinate from block buffer
  // $04 - low nybble of horizontal coordinate from block buffer
  // $06-$07 - block buffer address
  
StopPlayerMove:
  ImpedePlayerMove(); // stop player's movement
  
ExCSM:
  return; // leave
  
HandleCoinMetatile:
  ErACM(); // do sub to erase coin metatile from block buffer
  inc_abs(CoinTallyFor1Ups); // increment coin tally used for 1-up blocks
  GiveOneCoin(); return; // update coin amount and tally on the screen
  
HandleAxeMetatile:
  lda_imm(0x0);
  ram[OperMode_Task] = a; // reset secondary mode
  lda_imm(0x2);
  ram[OperMode] = a; // set primary mode to autoctrl mode
  lda_imm(0x18);
  ram[Player_X_Speed] = a; // set horizontal speed and continue to erase axe metatile
  ErACM(); // <fallthrough>
}

void ErACM(void) {
  ldy_zp(0x2); // load vertical high nybble offset for block buffer
  lda_imm(0x0); // load blank metatile
  dynamic_ram_write(read_word(0x6) + y, a); // store to remove old contents from block buffer
  RemoveCoin_Axe(); return; // update the screen accordingly
  // --------------------------------
  // $02 - high nybble of vertical coordinate from block buffer
  // $04 - low nybble of horizontal coordinate from block buffer
  // $06-$07 - block buffer address
}

void ImpedePlayerMove(void) {
  lda_imm(0x0); // initialize value here
  ldy_zp(Player_X_Speed); // get player's horizontal speed
  ldx_zp(0x0); // check value set earlier for
  dex(); // left side collision
  if (!zero_flag) { goto RImpd; } // if right side collision, skip this part
  inx(); // return value to X
  cpy_imm(0x0); // if player moving to the left,
  if (neg_flag) { goto ExIPM; } // branch to invert bit and leave
  lda_imm(0xff); // otherwise load A with value to be used later
  goto NXSpd; // and jump to affect movement
  
RImpd:
  ldx_imm(0x2); // return $02 to X
  cpy_imm(0x1); // if player moving to the right,
  if (!neg_flag) { goto ExIPM; } // branch to invert bit and leave
  lda_imm(0x1); // otherwise load A with value to be used here
  
NXSpd:
  ldy_imm(0x10);
  ram[SideCollisionTimer] = y; // set timer of some sort
  ldy_imm(0x0);
  ram[Player_X_Speed] = y; // nullify player's horizontal speed
  cmp_imm(0x0); // if value set in A not set to $ff,
  if (!neg_flag) { goto PlatF; } // branch ahead, do not decrement Y
  dey(); // otherwise decrement Y now
  
PlatF:
  ram[0x0] = y; // store Y as high bits of horizontal adder
  carry_flag = false;
  adc_zp(Player_X_Position); // add contents of A to player's horizontal
  ram[Player_X_Position] = a; // position to move player left or right
  lda_zp(Player_PageLoc);
  adc_zp(0x0); // add high bits and carry to
  ram[Player_PageLoc] = a; // page location if necessary
  
ExIPM:
  txa(); // invert contents of X
  eor_imm(0xff);
  and_abs(Player_CollisionBits); // mask out bit that was set here
  ram[Player_CollisionBits] = a; // store to clear bit
  // --------------------------------
}

void HandleClimbing(void) {
  ldy_zp(0x4); // check low nybble of horizontal coordinate returned from
  cpy_imm(0x6); // collision detection routine against certain values, this
  if (!carry_flag) { goto ExHC; } // makes actual physical part of vine or flagpole thinner
  cpy_imm(0xa); // than 16 pixels
  if (!carry_flag) { goto ChkForFlagpole; }
  
ExHC:
  return; // leave if too far left or too far right
  
ChkForFlagpole:
  cmp_imm(0x24); // check climbing metatiles
  if (zero_flag) { goto FlagpoleCollision; } // branch if flagpole ball found
  cmp_imm(0x25);
  if (!zero_flag) { goto VineCollision; } // branch to alternate code if flagpole shaft not found
  
FlagpoleCollision:
  lda_zp(GameEngineSubroutine);
  cmp_imm(0x5); // check for end-of-level routine running
  if (zero_flag) { goto PutPlayerOnVine; } // if running, branch to end of climbing code
  lda_imm(0x1);
  ram[PlayerFacingDir] = a; // set player's facing direction to right
  inc_abs(ScrollLock); // set scroll lock flag
  lda_zp(GameEngineSubroutine);
  cmp_imm(0x4); // check for flagpole slide routine running
  if (zero_flag) { goto RunFR; } // if running, branch to end of flagpole code here
  lda_imm(BulletBill_CannonVar); // load identifier for bullet bills (cannon variant)
  KillEnemies(); // get rid of them
  lda_imm(Silence);
  ram[EventMusicQueue] = a; // silence music
  lsr_acc();
  ram[FlagpoleSoundQueue] = a; // load flagpole sound into flagpole sound queue
  ldx_imm(0x4); // start at end of vertical coordinate data
  lda_zp(Player_Y_Position);
  ram[FlagpoleCollisionYPos] = a; // store player's vertical coordinate here to be used later
  
ChkFlagpoleYPosLoop:
  cmp_absx(FlagpoleYPosData); // compare with current vertical coordinate data
  if (carry_flag) { goto MtchF; } // if player's => current, branch to use current offset
  dex(); // otherwise decrement offset to use
  if (!zero_flag) { goto ChkFlagpoleYPosLoop; } // do this until all data is checked (use last one if all checked)
  
MtchF:
  ram[FlagpoleScore] = x; // store offset here to be used later
  
RunFR:
  lda_imm(0x4);
  ram[GameEngineSubroutine] = a; // set value to run flagpole slide routine
  goto PutPlayerOnVine; // jump to end of climbing code
  
VineCollision:
  cmp_imm(0x26); // check for climbing metatile used on vines
  if (!zero_flag) { goto PutPlayerOnVine; }
  lda_zp(Player_Y_Position); // check player's vertical coordinate
  cmp_imm(0x20); // for being in status bar area
  if (carry_flag) { goto PutPlayerOnVine; } // branch if not that far up
  lda_imm(0x1);
  ram[GameEngineSubroutine] = a; // otherwise set to run autoclimb routine next frame
  
PutPlayerOnVine:
  lda_imm(0x3); // set player state to climbing
  ram[Player_State] = a;
  lda_imm(0x0); // nullify player's horizontal speed
  ram[Player_X_Speed] = a; // and fractional horizontal movement force
  ram[Player_X_MoveForce] = a;
  lda_zp(Player_X_Position); // get player's horizontal coordinate
  carry_flag = true;
  sbc_abs(ScreenLeft_X_Pos); // subtract from left side horizontal coordinate
  cmp_imm(0x10);
  if (carry_flag) { goto SetVXPl; } // if 16 or more pixels difference, do not alter facing direction
  lda_imm(0x2);
  ram[PlayerFacingDir] = a; // otherwise force player to face left
  
SetVXPl:
  ldy_zp(PlayerFacingDir); // get current facing direction, use as offset
  lda_zp(0x6); // get low byte of block buffer address
  asl_acc();
  asl_acc(); // move low nybble to high
  asl_acc();
  asl_acc();
  carry_flag = false;
  adc_absy(ClimbXPosAdder - 1); // add pixels depending on facing direction
  ram[Player_X_Position] = a; // store as player's horizontal coordinate
  lda_zp(0x6); // get low byte of block buffer address again
  if (!zero_flag) { return; } // if not zero, branch
  lda_abs(ScreenRight_PageLoc); // load page location of right side of screen
  carry_flag = false;
  adc_absy(ClimbPLocAdder - 1); // add depending on facing location
  ram[Player_PageLoc] = a; // store as player's page location
  // --------------------------------
}

void ChkInvisibleMTiles(void) {
  cmp_imm(0x5f); // check for hidden coin block
  if (!zero_flag) {
    cmp_imm(0x60); // check for hidden 1-up block
    // --------------------------------
    // $00-$01 - used to hold bottom right and bottom left metatiles (in that order)
    // $00 - used as flag by ImpedePlayerMove to restrict specific movement
  }
}

void ChkForLandJumpSpring(void) {
  ChkJumpspringMetatiles(); // do sub to check if player landed on jumpspring
  if (carry_flag) {
    lda_imm(0x70);
    ram[VerticalForce] = a; // otherwise set vertical movement force for player
    lda_imm(0xf9);
    ram[JumpspringForce] = a; // set default jumpspring force
    lda_imm(0x3);
    ram[JumpspringTimer] = a; // set jumpspring timer to be used later
    lsr_acc();
    ram[JumpspringAnimCtrl] = a; // set jumpspring animation control to start animating
  }
}

void ChkJumpspringMetatiles(void) {
  cmp_imm(0x67); // check for top jumpspring metatile
  if (zero_flag) { goto JSFnd; } // branch to set carry if found
  cmp_imm(0x68); // check for bottom jumpspring metatile
  carry_flag = false; // clear carry flag
  if (!zero_flag) { return; } // branch to use cleared carry if not found
  
JSFnd:
  carry_flag = true; // set carry if found
}

void HandlePipeEntry(void) {
  lda_zp(Up_Down_Buttons); // check saved controller bits from earlier
  and_imm(0b00000100); // for pressing down
  if (zero_flag) { return; } // if not pressing down, branch to leave
  lda_zp(0x0);
  cmp_imm(0x11); // check right foot metatile for warp pipe right metatile
  if (!zero_flag) { return; } // branch to leave if not found
  lda_zp(0x1);
  cmp_imm(0x10); // check left foot metatile for warp pipe left metatile
  if (!zero_flag) { return; } // branch to leave if not found
  lda_imm(0x30);
  ram[ChangeAreaTimer] = a; // set timer for change of area
  lda_imm(0x3);
  ram[GameEngineSubroutine] = a; // set to run vertical pipe entry routine on next frame
  lda_imm(Sfx_PipeDown_Injury);
  ram[Square1SoundQueue] = a; // load pipedown/injury sound
  lda_imm(0b00100000);
  ram[Player_SprAttrib] = a; // set background priority bit in player's attributes
  lda_abs(WarpZoneControl); // check warp zone control
  if (zero_flag) { return; } // branch to leave if none found
  and_imm(0b00000011); // mask out all but 2 LSB
  asl_acc();
  asl_acc(); // multiply by four
  tax(); // save as offset to warp zone numbers (starts at left pipe)
  lda_zp(Player_X_Position); // get player's horizontal position
  cmp_imm(0x60);
  if (!carry_flag) { goto GetWNum; } // if player at left, not near middle, use offset and skip ahead
  inx(); // otherwise increment for middle pipe
  cmp_imm(0xa0);
  if (!carry_flag) { goto GetWNum; } // if player at middle, but not too far right, use offset and skip
  inx(); // otherwise increment for last pipe
  
GetWNum:
  ldy_absx(WarpZoneNumbers); // get warp zone numbers
  dey(); // decrement for use as world number
  ram[WorldNumber] = y; // store as world number and offset
  ldx_absy(WorldAddrOffsets); // get offset to where this world's area offsets are
  lda_absx(AreaAddrOffsets); // get area offset based on world offset
  ram[AreaPointer] = a; // store area offset here to be used to change areas
  lda_imm(Silence);
  ram[EventMusicQueue] = a; // silence music
  lda_imm(0x0);
  ram[EntrancePage] = a; // initialize starting page number
  ram[AreaNumber] = a; // initialize area number used for area address offset
  ram[LevelNumber] = a; // initialize level number used for world display
  ram[AltEntranceControl] = a; // initialize mode of entry
  inc_abs(Hidden1UpFlag); // set flag for hidden 1-up blocks
  inc_abs(FetchNewGameTimerFlag); // set flag to load new game timer
}

void CheckForSolidMTiles(void) {
  GetMTileAttrib(); // find appropriate offset based on metatile's 2 MSB
  cmp_absx(SolidMTileUpperExt); // compare current metatile with solid metatiles
}

void CheckForClimbMTiles(void) {
  GetMTileAttrib(); // find appropriate offset based on metatile's 2 MSB
  cmp_absx(ClimbMTileUpperExt); // compare current metatile with climbable metatiles
}

void CheckForCoinMTiles(void) {
  cmp_imm(0xc2); // check for regular coin
  // branch if found
  if (!zero_flag) {
    cmp_imm(0xc3); // check for underwater coin
    // branch if found
    if (!zero_flag) {
      carry_flag = false; // otherwise clear carry and leave
      return;
    }
  }
  // CoinSd:
  lda_imm(Sfx_CoinGrab);
  ram[Square2SoundQueue] = a; // load coin grab sound and leave
}

void GetMTileAttrib(void) {
  tay(); // save metatile value into Y
  and_imm(0b11000000); // mask out all but 2 MSB
  asl_acc();
  rol_acc(); // shift and rotate d7-d6 to d1-d0
  rol_acc();
  tax(); // use as offset for metatile data
  tya(); // get original metatile value back
  // -------------------------------------------------------------------------------------
  // $06-$07 - address from block buffer routine
}

void EnemyToBGCollisionDet(void) {
  lda_zpx(Enemy_State); // check enemy state for d6 set
  and_imm(0b00100000);
  if (!zero_flag) { goto EnemyToBGCollisionDetExit; } // if set, branch to leave
  SubtEnemyYPos(); // otherwise, do a subroutine here
  if (!carry_flag) { goto EnemyToBGCollisionDetExit; } // if enemy vertical coord + 62 < 68, branch to leave
  ldy_zpx(Enemy_ID);
  cpy_imm(Spiny); // if enemy object is not spiny, branch elsewhere
  if (!zero_flag) { goto DoIDCheckBGColl; }
  lda_zpx(Enemy_Y_Position);
  cmp_imm(0x25); // if enemy vertical coordinate < 36 branch to leave
  if (!carry_flag) { goto EnemyToBGCollisionDetExit; }
  
DoIDCheckBGColl:
  cpy_imm(GreenParatroopaJump); // check for some other enemy object
  if (!zero_flag) { goto HBChk; } // branch if not found
  EnemyJump(); return; // otherwise jump elsewhere
  
HBChk:
  cpy_imm(HammerBro); // check for hammer bro
  if (!zero_flag) { goto CInvu; } // branch if not found
  HammerBroBGColl(); // otherwise jump elsewhere
  return;
  
CInvu:
  cpy_imm(Spiny); // if enemy object is spiny, branch
  if (zero_flag) { goto YesIn; }
  cpy_imm(PowerUpObject); // if special power-up object, branch
  if (zero_flag) { goto YesIn; }
  cpy_imm(0x7); // if enemy object =>$07, branch to leave
  if (carry_flag) { return; }
  
YesIn:
  ChkUnderEnemy(); // if enemy object < $07, or = $12 or $2e, do this sub
  if (!zero_flag) { goto HandleEToBGCollision; } // if block underneath enemy, branch
  
NoEToBGCollision:
  ChkForRedKoopa(); // otherwise skip and do something else
  
EnemyToBGCollisionDetExit:
  return;
  // --------------------------------
  // $02 - vertical coordinate from block buffer routine
  
HandleEToBGCollision:
  ChkForNonSolids(); // if something is underneath enemy, find out what
  if (zero_flag) { goto NoEToBGCollision; } // if blank $26, coins, or hidden blocks, jump, enemy falls through
  cmp_imm(0x23);
  if (!zero_flag) { LandEnemyProperly(); return; } // check for blank metatile $23 and branch if not found
  ldy_zp(0x2); // get vertical coordinate used to find block
  lda_imm(0x0); // store default blank metatile in that spot so we won't
  dynamic_ram_write(read_word(0x6) + y, a); // trigger this routine accidentally again
  lda_zpx(Enemy_ID);
  cmp_imm(0x15); // if enemy object => $15, branch ahead
  if (carry_flag) { ChkToStunEnemies(); return; }
  cmp_imm(Goomba); // if enemy object not goomba, branch ahead of this routine
  if (!zero_flag) { goto GiveOEPoints; }
  KillEnemyAboveBlock(); // if enemy object IS goomba, do this sub
  
GiveOEPoints:
  lda_imm(0x1); // award 100 points for hitting block beneath enemy
  SetupFloateyNumber();
  ChkToStunEnemies(); // <fallthrough>
}

void ChkToStunEnemies(void) {
  cmp_imm(0x9); // perform many comparisons on enemy object identifier
  if (!carry_flag) {
    SetStun();
    return;
  }
  cmp_imm(0x11); // if the enemy object identifier is equal to the values
  // $09, $0e, $0f or $10, it will be modified, and not
  if (carry_flag) {
    SetStun();
    return;
  }
  cmp_imm(0xa); // modified if not any of those values, note that piranha plant will
  // always fail this test because A will still have vertical
  if (carry_flag) {
    cmp_imm(PiranhaPlant); // coordinate from previous addition, also these comparisons
    // are only necessary if branching from $d7a1
    if (!carry_flag) {
      SetStun();
      return;
    }
  }
  // Demote:
  and_imm(0b00000001); // erase all but LSB, essentially turning enemy object
  ram[Enemy_ID + x] = a; // into green or red koopa troopa to demote them
  SetStun(); // <fallthrough>
}

void SetStun(void) {
  lda_zpx(Enemy_State); // load enemy state
  and_imm(0b11110000); // save high nybble
  ora_imm(0b00000010);
  ram[Enemy_State + x] = a; // set d1 of enemy state
  dec_zpx(Enemy_Y_Position);
  dec_zpx(Enemy_Y_Position); // subtract two pixels from enemy's vertical position
  lda_zpx(Enemy_ID);
  cmp_imm(Bloober); // check for bloober object
  if (zero_flag) { goto SetWYSpd; }
  lda_imm(0xfd); // set default vertical speed
  ldy_abs(AreaType);
  if (!zero_flag) { goto SetNotW; } // if area type not water, set as speed, otherwise
  
SetWYSpd:
  lda_imm(0xff); // change the vertical speed
  
SetNotW:
  ram[Enemy_Y_Speed + x] = a; // set vertical speed now
  ldy_imm(0x1);
  PlayerEnemyDiff(); // get horizontal difference between player and enemy object
  if (!neg_flag) { goto ChkBBill; } // branch if enemy is to the right of player
  iny(); // increment Y if not
  
ChkBBill:
  lda_zpx(Enemy_ID);
  cmp_imm(BulletBill_CannonVar); // check for bullet bill (cannon variant)
  if (zero_flag) { goto NoCDirF; }
  cmp_imm(BulletBill_FrenzyVar); // check for bullet bill (frenzy variant)
  if (zero_flag) { goto NoCDirF; } // branch if either found, direction does not change
  ram[Enemy_MovingDir + x] = y; // store as moving direction
  
NoCDirF:
  dey(); // decrement and use as offset
  lda_absy(EnemyBGCXSpdData); // get proper horizontal speed
  ram[Enemy_X_Speed + x] = a; // and store, then leave
  // --------------------------------
  // $04 - low nybble of vertical coordinate from block buffer routine
}

void LandEnemyProperly(void) {
  lda_zp(0x4); // check lower nybble of vertical coordinate saved earlier
  carry_flag = true;
  sbc_imm(0x8); // subtract eight pixels
  cmp_imm(0x5); // used to determine whether enemy landed from falling
  if (carry_flag) { ChkForRedKoopa(); return; } // branch if lower nybble in range of $0d-$0f before subtract
  lda_zpx(Enemy_State);
  and_imm(0b01000000); // branch if d6 in enemy state is set
  if (!zero_flag) { goto LandEnemyInitState; }
  lda_zpx(Enemy_State);
  asl_acc(); // branch if d7 in enemy state is not set
  if (!carry_flag) { goto ChkLandedEnemyState; }
  
SChkA:
  DoEnemySideCheck(); return; // if lower nybble < $0d, d7 set but d6 not set, jump here
  
ChkLandedEnemyState:
  lda_zpx(Enemy_State); // if enemy in normal state, branch back to jump here
  if (zero_flag) { goto SChkA; }
  cmp_imm(0x5); // if in state used by spiny's egg
  if (zero_flag) { goto ProcEnemyDirection; } // then branch elsewhere
  cmp_imm(0x3); // if already in state used by koopas and buzzy beetles
  if (carry_flag) { goto ExSteChk; } // or in higher numbered state, branch to leave
  lda_zpx(Enemy_State); // load enemy state again (why?)
  cmp_imm(0x2); // if not in $02 state (used by koopas and buzzy beetles)
  if (!zero_flag) { goto ProcEnemyDirection; } // then branch elsewhere
  lda_imm(0x10); // load default timer here
  ldy_zpx(Enemy_ID); // check enemy identifier for spiny
  cpy_imm(Spiny);
  if (!zero_flag) { goto SetForStn; } // branch if not found
  lda_imm(0x0); // set timer for $00 if spiny
  
SetForStn:
  ram[EnemyIntervalTimer + x] = a; // set timer here
  lda_imm(0x3); // set state here, apparently used to render
  ram[Enemy_State + x] = a; // upside-down koopas and buzzy beetles
  EnemyLanding(); // then land it properly
  
ExSteChk:
  return; // then leave
  
ProcEnemyDirection:
  lda_zpx(Enemy_ID); // check enemy identifier for goomba
  cmp_imm(Goomba); // branch if found
  if (zero_flag) { goto LandEnemyInitState; }
  cmp_imm(Spiny); // check for spiny
  if (!zero_flag) { goto InvtD; } // branch if not found
  lda_imm(0x1);
  ram[Enemy_MovingDir + x] = a; // send enemy moving to the right by default
  lda_imm(0x8);
  ram[Enemy_X_Speed + x] = a; // set horizontal speed accordingly
  lda_zp(FrameCounter);
  and_imm(0b00000111); // if timed appropriately, spiny will skip over
  if (zero_flag) { goto LandEnemyInitState; } // trying to face the player
  
InvtD:
  ldy_imm(0x1); // load 1 for enemy to face the left (inverted here)
  PlayerEnemyDiff(); // get horizontal difference between player and enemy
  if (!neg_flag) { goto CNwCDir; } // if enemy to the right of player, branch
  iny(); // if to the left, increment by one for enemy to face right (inverted)
  
CNwCDir:
  tya();
  cmp_zpx(Enemy_MovingDir); // compare direction in A with current direction in memory
  if (!zero_flag) { goto LandEnemyInitState; }
  ChkForBump_HammerBroJ(); // if equal, not facing in correct dir, do sub to turn around
  
LandEnemyInitState:
  EnemyLanding(); // land enemy properly
  lda_zpx(Enemy_State);
  and_imm(0b10000000); // if d7 of enemy state is set, branch
  if (!zero_flag) { goto NMovShellFallBit; }
  lda_imm(0x0); // otherwise initialize enemy state and leave
  ram[Enemy_State + x] = a; // note this will also turn spiny's egg into spiny
  return;
  
NMovShellFallBit:
  lda_zpx(Enemy_State); // nullify d6 of enemy state, save other bits
  and_imm(0b10111111); // and store, then leave
  ram[Enemy_State + x] = a;
  // --------------------------------
}

void ChkForRedKoopa(void) {
  lda_zpx(Enemy_ID); // check for red koopa troopa $03
  cmp_imm(RedKoopa);
  // branch if not found
  if (zero_flag) {
    lda_zpx(Enemy_State);
    // if enemy found and in normal state, branch
    if (zero_flag) {
      ChkForBump_HammerBroJ();
      return;
    }
  }
  // Chk2MSBSt:
  lda_zpx(Enemy_State); // save enemy state into Y
  tay();
  asl_acc(); // check for d7 set
  // branch if not set
  if (carry_flag) {
    lda_zpx(Enemy_State);
    ora_imm(0b01000000); // set d6
    goto SetD6Ste; // jump ahead of this part
  }
  // GetSteFromD:
  lda_absy(EnemyBGCStateData); // load new enemy state with old as offset
  
SetD6Ste:
  ram[Enemy_State + x] = a; // set as new state
  DoEnemySideCheck();
  // --------------------------------
  // $00 - used to store bitmask (not used but initialized here)
  // $eb - used in DoEnemySideCheck as counter and to compare moving directions
}

void DoEnemySideCheck(void) {
  lda_zpx(Enemy_Y_Position); // if enemy within status bar, branch to leave
  cmp_imm(0x20); // because there's nothing there that impedes movement
  if (carry_flag) {
    ldy_imm(0x16); // start by finding block to the left of enemy ($00,$14)
    lda_imm(0x2); // set value here in what is also used as
    ram[0xeb] = a; // OAM data offset
    
SdeCLoop:
    lda_zp(0xeb); // check value
    cmp_zpx(Enemy_MovingDir); // compare value against moving direction
    // branch if different and do not seek block there
    if (zero_flag) {
      lda_imm(0x1); // set flag in A for save horizontal coordinate
      BlockBufferChk_Enemy(); // find block to left or right of enemy object
      // if nothing found, branch
      if (!zero_flag) {
        ChkForNonSolids(); // check for non-solid blocks
        // branch if not found
        if (!zero_flag) {
          ChkForBump_HammerBroJ();
          return;
        }
      }
    }
    // NextSdeC:
    dec_zp(0xeb); // move to the next direction
    iny();
    cpy_imm(0x18); // increment Y, loop only if Y < $18, thus we check
    if (!carry_flag) { goto SdeCLoop; } // enemy ($00, $14) and ($10, $14) pixel coordinates
  }
}

void ChkForBump_HammerBroJ(void) {
  cpx_imm(0x5); // check if we're on the special use slot
  // and if so, branch ahead and do not play sound
  if (!zero_flag) {
    lda_zpx(Enemy_State); // if enemy state d7 not set, branch
    asl_acc(); // ahead and do not play sound
    if (carry_flag) {
      lda_imm(Sfx_Bump); // otherwise, play bump sound
      ram[Square1SoundQueue] = a; // sound will never be played if branching from ChkForRedKoopa
    }
  }
  // NoBump:
  lda_zpx(Enemy_ID); // check for hammer bro
  cmp_imm(0x5);
  // branch if not found
  if (zero_flag) {
    lda_imm(0x0);
    ram[0x0] = a; // initialize value here for bitmask
    ldy_imm(0xfa); // load default vertical speed for jumping
    SetHJ(); // jump to code that makes hammer bro jump
    return;
  }
  // InvEnemyDir:
  RXSpd(); // jump to turn the enemy around
  // --------------------------------
  // $00 - used to hold horizontal difference between player and enemy
}

void EnemyJump(void) {
  SubtEnemyYPos(); // do a sub here
  // if enemy vertical coord + 62 < 68, branch to leave
  if (carry_flag) {
    lda_zpx(Enemy_Y_Speed);
    carry_flag = false; // add two to vertical speed
    adc_imm(0x2);
    cmp_imm(0x3); // if green paratroopa not falling, branch ahead
    if (carry_flag) {
      ChkUnderEnemy(); // otherwise, check to see if green paratroopa is
      // standing on anything, then branch to same place if not
      if (!zero_flag) {
        ChkForNonSolids(); // check for non-solid blocks
        // branch if found
        if (!zero_flag) {
          EnemyLanding(); // change vertical coordinate and speed
          lda_imm(0xfd);
          ram[Enemy_Y_Speed + x] = a; // make the paratroopa jump again
        }
      }
    }
  }
  // DoSide:
  DoEnemySideCheck(); return; // check for horizontal blockage, then leave
  // --------------------------------
}

void PlayerEnemyDiff(void) {
  lda_zpx(Enemy_X_Position); // get distance between enemy object's
  carry_flag = true; // horizontal coordinate and the player's
  sbc_zp(Player_X_Position); // horizontal coordinate
  ram[0x0] = a; // and store here
  lda_zpx(Enemy_PageLoc);
  sbc_zp(Player_PageLoc); // subtract borrow, then leave
  // --------------------------------
}

void EnemyLanding(void) {
  InitVStf(); // do something here to vertical speed and something else
  lda_zpx(Enemy_Y_Position);
  and_imm(0b11110000); // save high nybble of vertical coordinate, and
  ora_imm(0b00001000); // set d3, then store, probably used to set enemy object
  ram[Enemy_Y_Position + x] = a; // neatly on whatever it's landing on
}

void SubtEnemyYPos(void) {
  lda_zpx(Enemy_Y_Position); // add 62 pixels to enemy object's
  carry_flag = false; // vertical coordinate
  adc_imm(0x3e);
  cmp_imm(0x44); // compare against a certain range
}

void HammerBroBGColl(void) {
  ChkUnderEnemy(); // check to see if hammer bro is standing on anything
  if (zero_flag) {
    NoUnderHammerBro();
    return;
  }
  cmp_imm(0x23); // check for blank metatile $23 and branch if not found
  if (!zero_flag) {
    UnderHammerBro();
    return;
  }
  KillEnemyAboveBlock(); // <fallthrough>
}

void KillEnemyAboveBlock(void) {
  ShellOrBlockDefeat(); // do this sub to kill enemy
  lda_imm(0xfc); // alter vertical speed of enemy and leave
  ram[Enemy_Y_Speed + x] = a;
}

void UnderHammerBro(void) {
  lda_absx(EnemyFrameTimer); // check timer used by hammer bro
  // branch if not expired
  if (!zero_flag) {
    NoUnderHammerBro();
    return;
  }
  lda_zpx(Enemy_State);
  and_imm(0b10001000); // save d7 and d3 from enemy state, nullify other bits
  ram[Enemy_State + x] = a; // and store
  EnemyLanding(); // modify vertical coordinate, speed and something else
  DoEnemySideCheck(); return; // then check for horizontal blockage and leave
}

void NoUnderHammerBro(void) {
  lda_zpx(Enemy_State); // if hammer bro is not standing on anything, set d0
  ora_imm(0x1); // in the enemy state to indicate jumping or falling, then leave
  ram[Enemy_State + x] = a;
}

void ChkUnderEnemy(void) {
  lda_imm(0x0); // set flag in A for save vertical coordinate
  ldy_imm(0x15); // set Y to check the bottom middle (8,18) of enemy object
  BlockBufferChk_Enemy(); return; // hop to it!
}

void BlockBufferChk_Enemy(void) {
  pha(); // save contents of A to stack
  txa();
  carry_flag = false; // add 1 to X to run sub with enemy offset in mind
  adc_imm(0x1);
  tax();
  pla(); // pull A from stack and jump elsewhere
  BBChk_E();
  //  ResidualMiscObjectCode:
  //        txa
  //        clc           ;supposedly used once to set offset for
  //        adc #$0d      ;miscellaneous objects
  //        tax
  //        ldy #$1b      ;supposedly used once to set offset for block buffer data
  //        jmp ResJmpM   ;probably used in early stages to do misc to bg collision detection
}

void ChkForNonSolids(void) {
  cmp_imm(0x26); // blank metatile used for vines?
  if (zero_flag) { return; }
  cmp_imm(0xc2); // regular coin?
  if (zero_flag) { return; }
  cmp_imm(0xc3); // underwater coin?
  if (zero_flag) { return; }
  cmp_imm(0x5f); // hidden coin block?
  if (zero_flag) { return; }
  cmp_imm(0x60); // hidden 1-up block?
  // -------------------------------------------------------------------------------------
}

void FireballBGCollision(void) {
  lda_zpx(Fireball_Y_Position); // check fireball's vertical coordinate
  cmp_imm(0x18);
  if (!carry_flag) { goto ClearBounceFlag; } // if within the status bar area of the screen, branch ahead
  BlockBufferChk_FBall(); // do fireball to background collision detection on bottom of it
  if (zero_flag) { goto ClearBounceFlag; } // if nothing underneath fireball, branch
  ChkForNonSolids(); // check for non-solid metatiles
  if (zero_flag) { goto ClearBounceFlag; } // branch if any found
  lda_zpx(Fireball_Y_Speed); // if fireball's vertical speed set to move upwards,
  if (neg_flag) { goto InitFireballExplode; } // branch to set exploding bit in fireball's state
  lda_zpx(FireballBouncingFlag); // if bouncing flag already set,
  if (!zero_flag) { goto InitFireballExplode; } // branch to set exploding bit in fireball's state
  lda_imm(0xfd);
  ram[Fireball_Y_Speed + x] = a; // otherwise set vertical speed to move upwards (give it bounce)
  lda_imm(0x1);
  ram[FireballBouncingFlag + x] = a; // set bouncing flag
  lda_zpx(Fireball_Y_Position);
  and_imm(0xf8); // modify vertical coordinate to land it properly
  ram[Fireball_Y_Position + x] = a; // store as new vertical coordinate
  return; // leave
  
ClearBounceFlag:
  lda_imm(0x0);
  ram[FireballBouncingFlag + x] = a; // clear bouncing flag by default
  return; // leave
  
InitFireballExplode:
  lda_imm(0x80);
  ram[Fireball_State + x] = a; // set exploding flag in fireball's state
  lda_imm(Sfx_Bump);
  ram[Square1SoundQueue] = a; // load bump sound
  // -------------------------------------------------------------------------------------
  // $00 - used to hold one of bitmasks, or offset
  // $01 - used for relative X coordinate, also used to store middle screen page location
  // $02 - used for relative Y coordinate, also used to store middle screen coordinate
  // this data added to relative coordinates of sprite objects
  // stored in order: left edge, top edge, right edge, bottom edge
}

void GetFireballBoundBox(void) {
  txa(); // add seven bytes to offset
  carry_flag = false; // to use in routines as offset for fireball
  adc_imm(0x7);
  tax();
  ldy_imm(0x2); // set offset for relative coordinates
  // unconditional branch
  if (!zero_flag) {
    FBallB();
    return;
  }
  GetMiscBoundBox(); // <fallthrough>
}

void GetMiscBoundBox(void) {
  txa(); // add nine bytes to offset
  carry_flag = false; // to use in routines as offset for misc object
  adc_imm(0x9);
  tax();
  ldy_imm(0x6); // set offset for relative coordinates
  FBallB(); // <fallthrough>
}

void FBallB(void) {
  BoundingBoxCore(); // get bounding box coordinates
  CheckRightScreenBBox(); // jump to handle any offscreen coordinates
}

void GetEnemyBoundBox(void) {
  ldy_imm(0x48); // store bitmask here for now
  ram[0x0] = y;
  ldy_imm(0x44); // store another bitmask here for now and jump
  GetMaskedOffScrBits();
}

void SmallPlatformBoundBox(void) {
  ldy_imm(0x8); // store bitmask here for now
  ram[0x0] = y;
  ldy_imm(0x4); // store another bitmask here for now
  GetMaskedOffScrBits(); // <fallthrough>
}

void GetMaskedOffScrBits(void) {
  lda_zpx(Enemy_X_Position); // get enemy object position relative
  carry_flag = true; // to the left side of the screen
  sbc_abs(ScreenLeft_X_Pos);
  ram[0x1] = a; // store here
  lda_zpx(Enemy_PageLoc); // subtract borrow from current page location
  sbc_abs(ScreenLeft_PageLoc); // of left side
  // if enemy object is beyond left edge, branch
  if (!neg_flag) {
    ora_zp(0x1);
    // if precisely at the left edge, branch
    if (!zero_flag) {
      ldy_zp(0x0); // if to the right of left edge, use value in $00 for A
    }
  }
  // CMBits:
  tya(); // otherwise use contents of Y
  and_abs(Enemy_OffscreenBits); // preserve bitwise whatever's in here
  ram[EnemyOffscrBitsMasked + x] = a; // save masked offscreen bits here
  // if anything set here, branch
  if (!zero_flag) {
    MoveBoundBoxOffscreen();
    return;
  }
  SetupEOffsetFBBox(); // otherwise, do something else
}

void MoveBoundBoxOffscreen(void) {
  txa(); // multiply offset by 4
  asl_acc();
  asl_acc();
  tay(); // use as offset here
  lda_imm(0xff);
  ram[EnemyBoundingBoxCoord + y] = a; // load value into four locations here and leave
  ram[EnemyBoundingBoxCoord + 1 + y] = a;
  ram[EnemyBoundingBoxCoord + 2 + y] = a;
  ram[EnemyBoundingBoxCoord + 3 + y] = a;
}

void LargePlatformBoundBox(void) {
  inx(); // increment X to get the proper offset
  GetXOffscreenBits(); // then jump directly to the sub for horizontal offscreen bits
  dex(); // decrement to return to original offset
  cmp_imm(0xfe); // if completely offscreen, branch to put entire bounding
  // box offscreen, otherwise start getting coordinates
  if (carry_flag) {
    MoveBoundBoxOffscreen();
    return;
  }
  SetupEOffsetFBBox(); // <fallthrough>
}

void SetupEOffsetFBBox(void) {
  txa(); // add 1 to offset to properly address
  carry_flag = false; // the enemy object memory locations
  adc_imm(0x1);
  tax();
  ldy_imm(0x1); // load 1 as offset here, same reason
  BoundingBoxCore(); // do a sub to get the coordinates of the bounding box
  CheckRightScreenBBox(); return; // jump to handle offscreen coordinates of bounding box
}

void CheckRightScreenBBox(void) {
  lda_abs(ScreenLeft_X_Pos); // add 128 pixels to left side of screen
  carry_flag = false; // and store as horizontal coordinate of middle
  adc_imm(0x80);
  ram[0x2] = a;
  lda_abs(ScreenLeft_PageLoc); // add carry to page location of left side of screen
  adc_imm(0x0); // and store as page location of middle
  ram[0x1] = a;
  lda_zpx(SprObject_X_Position); // get horizontal coordinate
  cmp_zp(0x2); // compare against middle horizontal coordinate
  lda_zpx(SprObject_PageLoc); // get page location
  sbc_zp(0x1); // subtract from middle page location
  // if object is on the left side of the screen, branch
  if (carry_flag) {
    lda_absy(BoundingBox_DR_XPos); // check right-side edge of bounding box for offscreen
    // coordinates, branch if still on the screen
    if (!neg_flag) {
      lda_imm(0xff); // load offscreen value here to use on one or both horizontal sides
      ldx_absy(BoundingBox_UL_XPos); // check left-side edge of bounding box for offscreen
      // coordinates, and branch if still on the screen
      if (!neg_flag) {
        ram[BoundingBox_UL_XPos + y] = a; // store offscreen value for left side
      }
      // SORte:
      ram[BoundingBox_DR_XPos + y] = a; // store offscreen value for right side
    }
    // NoOfs:
    ldx_zp(ObjectOffset); // get object offset and leave
    return;
  }
  // CheckLeftScreenBBox:
  lda_absy(BoundingBox_UL_XPos); // check left-side edge of bounding box for offscreen
  // coordinates, and branch if still on the screen
  if (neg_flag) {
    cmp_imm(0xa0); // check to see if left-side edge is in the middle of the
    // screen or really offscreen, and branch if still on
    if (carry_flag) {
      lda_imm(0x0);
      ldx_absy(BoundingBox_DR_XPos); // check right-side edge of bounding box for offscreen
      // coordinates, branch if still onscreen
      if (neg_flag) {
        ram[BoundingBox_DR_XPos + y] = a; // store offscreen value for right side
      }
      // SOLft:
      ram[BoundingBox_UL_XPos + y] = a; // store offscreen value for left side
    }
  }
  // NoOfs2:
  ldx_zp(ObjectOffset); // get object offset and leave
  // -------------------------------------------------------------------------------------
  // $06 - second object's offset
  // $07 - counter
}

void BoundingBoxCore(void) {
  ram[0x0] = x; // save offset here
  lda_absy(SprObject_Rel_YPos); // store object coordinates relative to screen
  ram[0x2] = a; // vertically and horizontally, respectively
  lda_absy(SprObject_Rel_XPos);
  ram[0x1] = a;
  txa(); // multiply offset by four and save to stack
  asl_acc();
  asl_acc();
  pha();
  tay(); // use as offset for Y, X is left alone
  lda_absx(SprObj_BoundBoxCtrl); // load value here to be used as offset for X
  asl_acc(); // multiply that by four and use as X
  asl_acc();
  tax();
  lda_zp(0x1); // add the first number in the bounding box data to the
  carry_flag = false; // relative horizontal coordinate using enemy object offset
  adc_absx(BoundBoxCtrlData); // and store somewhere using same offset * 4
  ram[BoundingBox_UL_Corner + y] = a; // store here
  lda_zp(0x1);
  carry_flag = false;
  adc_absx(BoundBoxCtrlData + 2); // add the third number in the bounding box data to the
  ram[BoundingBox_LR_Corner + y] = a; // relative horizontal coordinate and store
  inx(); // increment both offsets
  iny();
  lda_zp(0x2); // add the second number to the relative vertical coordinate
  carry_flag = false; // using incremented offset and store using the other
  adc_absx(BoundBoxCtrlData); // incremented offset
  ram[BoundingBox_UL_Corner + y] = a;
  lda_zp(0x2);
  carry_flag = false;
  adc_absx(BoundBoxCtrlData + 2); // add the fourth number to the relative vertical coordinate
  ram[BoundingBox_LR_Corner + y] = a; // and store
  pla(); // get original offset loaded into $00 * y from stack
  tay(); // use as Y
  ldx_zp(0x0); // get original offset and use as X again
}

void PlayerCollisionCore(void) {
  ldx_imm(0x0); // initialize X to use player's bounding box for comparison
  SprObjectCollisionCore(); // <fallthrough>
}

void SprObjectCollisionCore(void) {
  ram[0x6] = y; // save contents of Y here
  lda_imm(0x1);
  ram[0x7] = a; // save value 1 here as counter, compare horizontal coordinates first
  
CollisionCoreLoop:
  lda_absy(BoundingBox_UL_Corner); // compare left/top coordinates
  cmp_absx(BoundingBox_UL_Corner); // of first and second objects' bounding boxes
  if (carry_flag) { goto FirstBoxGreater; } // if first left/top => second, branch
  cmp_absx(BoundingBox_LR_Corner); // otherwise compare to right/bottom of second
  if (!carry_flag) { goto SecondBoxVerticalChk; } // if first left/top < second right/bottom, branch elsewhere
  if (zero_flag) { goto CollisionFound; } // if somehow equal, collision, thus branch
  lda_absy(BoundingBox_LR_Corner); // if somehow greater, check to see if bottom of
  cmp_absy(BoundingBox_UL_Corner); // first object's bounding box is greater than its top
  if (!carry_flag) { goto CollisionFound; } // if somehow less, vertical wrap collision, thus branch
  cmp_absx(BoundingBox_UL_Corner); // otherwise compare bottom of first bounding box to the top
  if (carry_flag) { goto CollisionFound; } // of second box, and if equal or greater, collision, thus branch
  ldy_zp(0x6); // otherwise return with carry clear and Y = $0006
  return; // note horizontal wrapping never occurs
  
SecondBoxVerticalChk:
  lda_absx(BoundingBox_LR_Corner); // check to see if the vertical bottom of the box
  cmp_absx(BoundingBox_UL_Corner); // is greater than the vertical top
  if (!carry_flag) { goto CollisionFound; } // if somehow less, vertical wrap collision, thus branch
  lda_absy(BoundingBox_LR_Corner); // otherwise compare horizontal right or vertical bottom
  cmp_absx(BoundingBox_UL_Corner); // of first box with horizontal left or vertical top of second box
  if (carry_flag) { goto CollisionFound; } // if equal or greater, collision, thus branch
  ldy_zp(0x6); // otherwise return with carry clear and Y = $0006
  return;
  
FirstBoxGreater:
  cmp_absx(BoundingBox_UL_Corner); // compare first and second box horizontal left/vertical top again
  if (zero_flag) { goto CollisionFound; } // if first coordinate = second, collision, thus branch
  cmp_absx(BoundingBox_LR_Corner); // if not, compare with second object right or bottom edge
  if (!carry_flag) { goto CollisionFound; } // if left/top of first less than or equal to right/bottom of second
  if (zero_flag) { goto CollisionFound; } // then collision, thus branch
  cmp_absy(BoundingBox_LR_Corner); // otherwise check to see if top of first box is greater than bottom
  if (!carry_flag) { goto NoCollisionFound; } // if less than or equal, no collision, branch to end
  if (zero_flag) { goto NoCollisionFound; }
  lda_absy(BoundingBox_LR_Corner); // otherwise compare bottom of first to top of second
  cmp_absx(BoundingBox_UL_Corner); // if bottom of first is greater than top of second, vertical wrap
  if (carry_flag) { goto CollisionFound; } // collision, and branch, otherwise, proceed onwards here
  
NoCollisionFound:
  carry_flag = false; // clear carry, then load value set earlier, then leave
  ldy_zp(0x6); // like previous ones, if horizontal coordinates do not collide, we do
  return; // not bother checking vertical ones, because what's the point?
  
CollisionFound:
  inx(); // increment offsets on both objects to check
  iny(); // the vertical coordinates
  dec_zp(0x7); // decrement counter to reflect this
  if (!neg_flag) { goto CollisionCoreLoop; } // if counter not expired, branch to loop
  carry_flag = true; // otherwise we already did both sets, therefore collision, so set carry
  ldy_zp(0x6); // load original value set here earlier, then leave
  // -------------------------------------------------------------------------------------
  // $02 - modified y coordinate
  // $03 - stores metatile involved in block buffer collisions
  // $04 - comes in with offset to block buffer adder data, goes out with low nybble x/y coordinate
  // $05 - modified x coordinate
  // $06-$07 - block buffer address
}

void BlockBufferChk_FBall(void) {
  ldy_imm(0x1a); // set offset for block buffer adder data
  txa();
  carry_flag = false;
  adc_imm(0x7); // add seven bytes to use
  tax();
  lda_imm(0x0); //  ResJmpM: lda #$00 ;set A to return vertical coordinate
  BBChk_E(); // <fallthrough>
}

void BBChk_E(void) {
  BlockBufferCollision(); // do collision detection subroutine for sprite object
  ldx_zp(ObjectOffset); // get object offset
  cmp_imm(0x0); // check to see if object bumped into anything
}

void BlockBufferColli_Feet(void) {
  iny(); // if branched here, increment to next set of adders
  BlockBufferColli_Head(); // <fallthrough>
}

void BlockBufferColli_Head(void) {
  lda_imm(0x0); // set flag to return vertical coordinate
  BlockBufferColli_SideSkip(); //  .db $2c ;BIT instruction opcode
}

void BlockBufferColli_Side(void) {
  lda_imm(0x1); // set flag to return horizontal coordinate
  BlockBufferColli_SideSkip(); // <fallthrough>
}

void BlockBufferColli_SideSkip(void) {
  ldx_imm(0x0); // set offset for player object
  BlockBufferCollision(); // <fallthrough>
}

void BlockBufferCollision(void) {
  pha(); // save contents of A to stack
  ram[0x4] = y; // save contents of Y here
  lda_absy(BlockBuffer_X_Adder); // add horizontal coordinate
  carry_flag = false; // of object to value obtained using Y as offset
  adc_zpx(SprObject_X_Position);
  ram[0x5] = a; // store here
  lda_zpx(SprObject_PageLoc);
  adc_imm(0x0); // add carry to page location
  and_imm(0x1); // get LSB, mask out all other bits
  lsr_acc(); // move to carry
  ora_zp(0x5); // get stored value
  ror_acc(); // rotate carry to MSB of A
  lsr_acc(); // and effectively move high nybble to
  lsr_acc(); // lower, LSB which became MSB will be
  lsr_acc(); // d4 at this point
  GetBlockBufferAddr(); // get address of block buffer into $06, $07
  ldy_zp(0x4); // get old contents of Y
  lda_zpx(SprObject_Y_Position); // get vertical coordinate of object
  carry_flag = false;
  adc_absy(BlockBuffer_Y_Adder); // add it to value obtained using Y as offset
  and_imm(0b11110000); // mask out low nybble
  carry_flag = true;
  sbc_imm(0x20); // subtract 32 pixels for the status bar
  ram[0x2] = a; // store result here
  tay(); // use as offset for block buffer
  lda_indy(0x6); // check current content of block buffer
  ram[0x3] = a; // and store here
  ldy_zp(0x4); // get old contents of Y again
  pla(); // pull A from stack
  // if A = 1, branch
  if (zero_flag) {
    lda_zpx(SprObject_Y_Position); // if A = 0, load vertical coordinate
    goto RetYC; // and jump
  }
  // RetXC:
  lda_zpx(SprObject_X_Position); // otherwise load horizontal coordinate
  
RetYC:
  and_imm(0b00001111); // and mask out high nybble
  ram[0x4] = a; // store masked out result here
  lda_zp(0x3); // get saved content of block buffer
  // -------------------------------------------------------------------------------------
  // unused byte
  //       .db $ff
  // -------------------------------------------------------------------------------------
  // $00 - offset to vine Y coordinate adder
  // $02 - offset to sprite data
}

void DrawVine(void) {
  ram[0x0] = y; // save offset here
  lda_abs(Enemy_Rel_YPos); // get relative vertical coordinate
  carry_flag = false;
  adc_absy(VineYPosAdder); // add value using offset in Y to get value
  ldx_absy(VineObjOffset); // get offset to vine
  ldy_absx(Enemy_SprDataOffset); // get sprite data offset
  ram[0x2] = y; // store sprite data offset here
  SixSpriteStacker(); // stack six sprites on top of each other vertically
  lda_abs(Enemy_Rel_XPos); // get relative horizontal coordinate
  ram[Sprite_X_Position + y] = a; // store in first, third and fifth sprites
  ram[Sprite_X_Position + 8 + y] = a;
  ram[Sprite_X_Position + 16 + y] = a;
  carry_flag = false;
  adc_imm(0x6); // add six pixels to second, fourth and sixth sprites
  ram[Sprite_X_Position + 4 + y] = a; // to give characteristic staggered vine shape to
  ram[Sprite_X_Position + 12 + y] = a; // our vertical stack of sprites
  ram[Sprite_X_Position + 20 + y] = a;
  lda_imm(0b00100001); // set bg priority and palette attribute bits
  ram[Sprite_Attributes + y] = a; // set in first, third and fifth sprites
  ram[Sprite_Attributes + 8 + y] = a;
  ram[Sprite_Attributes + 16 + y] = a;
  ora_imm(0b01000000); // additionally, set horizontal flip bit
  ram[Sprite_Attributes + 4 + y] = a; // for second, fourth and sixth sprites
  ram[Sprite_Attributes + 12 + y] = a;
  ram[Sprite_Attributes + 20 + y] = a;
  ldx_imm(0x5); // set tiles for six sprites
  
VineTL:
  lda_imm(0xe1); // set tile number for sprite
  ram[Sprite_Tilenumber + y] = a;
  iny(); // move offset to next sprite data
  iny();
  iny();
  iny();
  dex(); // move onto next sprite
  if (!neg_flag) { goto VineTL; } // loop until all sprites are done
  ldy_zp(0x2); // get original offset
  lda_zp(0x0); // get offset to vine adding data
  // if offset not zero, skip this part
  if (zero_flag) {
    lda_imm(0xe0);
    ram[Sprite_Tilenumber + y] = a; // set other tile number for top of vine
  }
  // SkpVTop:
  ldx_imm(0x0); // start with the first sprite again
  
ChkFTop:
  lda_abs(VineStart_Y_Position); // get original starting vertical coordinate
  carry_flag = true;
  sbc_absy(Sprite_Y_Position); // subtract top-most sprite's Y coordinate
  cmp_imm(0x64); // if two coordinates are less than 100/$64 pixels
  // apart, skip this to leave sprite alone
  if (carry_flag) {
    lda_imm(0xf8);
    ram[Sprite_Y_Position + y] = a; // otherwise move sprite offscreen
  }
  // NextVSp:
  iny(); // move offset to next OAM data
  iny();
  iny();
  iny();
  inx(); // move onto next sprite
  cpx_imm(0x6); // do this until all sprites are checked
  if (!zero_flag) { goto ChkFTop; }
  ldy_zp(0x0); // return offset set earlier
}

void SixSpriteStacker(void) {
  ldx_imm(0x6); // do six sprites
  
StkLp:
  ram[Sprite_Data + y] = a; // store X or Y coordinate into OAM data
  carry_flag = false;
  adc_imm(0x8); // add eight pixels
  iny();
  iny(); // move offset four bytes forward
  iny();
  iny();
  dex(); // do another sprite
  if (!zero_flag) { goto StkLp; } // do this until all sprites are done
  ldy_zp(0x2); // get saved OAM data offset and leave
  // -------------------------------------------------------------------------------------
}

void DrawHammer(void) {
  ldy_absx(Misc_SprDataOffset); // get misc object OAM data offset
  lda_abs(TimerControl);
  if (!zero_flag) { goto ForceHPose; } // if master timer control set, skip this part
  lda_zpx(Misc_State); // otherwise get hammer's state
  and_imm(0b01111111); // mask out d7
  cmp_imm(0x1); // check to see if set to 1 yet
  if (zero_flag) { goto GetHPose; } // if so, branch
  
ForceHPose:
  ldx_imm(0x0); // reset offset here
  if (zero_flag) { goto RenderH; } // do unconditional branch to rendering part
  
GetHPose:
  lda_zp(FrameCounter); // get frame counter
  lsr_acc(); // move d3-d2 to d1-d0
  lsr_acc();
  and_imm(0b00000011); // mask out all but d1-d0 (changes every four frames)
  tax(); // use as timing offset
  
RenderH:
  lda_abs(Misc_Rel_YPos); // get relative vertical coordinate
  carry_flag = false;
  adc_absx(FirstSprYPos); // add first sprite vertical adder based on offset
  ram[Sprite_Y_Position + y] = a; // store as sprite Y coordinate for first sprite
  carry_flag = false;
  adc_absx(SecondSprYPos); // add second sprite vertical adder based on offset
  ram[Sprite_Y_Position + 4 + y] = a; // store as sprite Y coordinate for second sprite
  lda_abs(Misc_Rel_XPos); // get relative horizontal coordinate
  carry_flag = false;
  adc_absx(FirstSprXPos); // add first sprite horizontal adder based on offset
  ram[Sprite_X_Position + y] = a; // store as sprite X coordinate for first sprite
  carry_flag = false;
  adc_absx(SecondSprXPos); // add second sprite horizontal adder based on offset
  ram[Sprite_X_Position + 4 + y] = a; // store as sprite X coordinate for second sprite
  lda_absx(FirstSprTilenum);
  ram[Sprite_Tilenumber + y] = a; // get and store tile number of first sprite
  lda_absx(SecondSprTilenum);
  ram[Sprite_Tilenumber + 4 + y] = a; // get and store tile number of second sprite
  lda_absx(HammerSprAttrib);
  ram[Sprite_Attributes + y] = a; // get and store attribute bytes for both
  ram[Sprite_Attributes + 4 + y] = a; // note in this case they use the same data
  ldx_zp(ObjectOffset); // get misc object offset
  lda_abs(Misc_OffscreenBits);
  and_imm(0b11111100); // check offscreen bits
  if (zero_flag) { return; } // if all bits clear, leave object alone
  lda_imm(0x0);
  ram[Misc_State + x] = a; // otherwise nullify misc object state
  lda_imm(0xf8);
  DumpTwoSpr(); // do sub to move hammer sprites offscreen
  // -------------------------------------------------------------------------------------
  // $00-$01 - used to hold tile numbers ($01 addressed in draw floatey number part)
  // $02 - used to hold Y coordinate for floatey number
  // $03 - residual byte used for flip (but value set here affects nothing)
  // $04 - attribute byte for floatey number
  // $05 - used as X coordinate for floatey number
}

void FlagpoleGfxHandler(void) {
  ldy_absx(Enemy_SprDataOffset); // get sprite data offset for flagpole flag
  lda_abs(Enemy_Rel_XPos); // get relative horizontal coordinate
  ram[Sprite_X_Position + y] = a; // store as X coordinate for first sprite
  carry_flag = false;
  adc_imm(0x8); // add eight pixels and store
  ram[Sprite_X_Position + 4 + y] = a; // as X coordinate for second and third sprites
  ram[Sprite_X_Position + 8 + y] = a;
  carry_flag = false;
  adc_imm(0xc); // add twelve more pixels and
  ram[0x5] = a; // store here to be used later by floatey number
  lda_zpx(Enemy_Y_Position); // get vertical coordinate
  DumpTwoSpr(); // and do sub to dump into first and second sprites
  adc_imm(0x8); // add eight pixels
  ram[Sprite_Y_Position + 8 + y] = a; // and store into third sprite
  lda_abs(FlagpoleFNum_Y_Pos); // get vertical coordinate for floatey number
  ram[0x2] = a; // store it here
  lda_imm(0x1);
  ram[0x3] = a; // set value for flip which will not be used, and
  ram[0x4] = a; // attribute byte for floatey number
  ram[Sprite_Attributes + y] = a; // set attribute bytes for all three sprites
  ram[Sprite_Attributes + 4 + y] = a;
  ram[Sprite_Attributes + 8 + y] = a;
  lda_imm(0x7e);
  ram[Sprite_Tilenumber + y] = a; // put triangle shaped tile
  ram[Sprite_Tilenumber + 8 + y] = a; // into first and third sprites
  lda_imm(0x7f);
  ram[Sprite_Tilenumber + 4 + y] = a; // put skull tile into second sprite
  lda_abs(FlagpoleCollisionYPos); // get vertical coordinate at time of collision
  // if zero, branch ahead
  if (!zero_flag) {
    tya();
    carry_flag = false; // add 12 bytes to sprite data offset
    adc_imm(0xc);
    tay(); // put back in Y
    lda_abs(FlagpoleScore); // get offset used to award points for touching flagpole
    asl_acc(); // multiply by 2 to get proper offset here
    tax();
    lda_absx(FlagpoleScoreNumTiles); // get appropriate tile data
    ram[0x0] = a;
    lda_absx(FlagpoleScoreNumTiles + 1);
    DrawOneSpriteRow(); // use it to render floatey number
  }
  // ChkFlagOffscreen:
  ldx_zp(ObjectOffset); // get object offset for flag
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  lda_abs(Enemy_OffscreenBits); // get offscreen bits
  and_imm(0b00001110); // mask out all but d3-d1
  if (!zero_flag) {
    // -------------------------------------------------------------------------------------
    MoveSixSpritesOffscreen(); // <fallthrough>
  }
}

void MoveSixSpritesOffscreen(void) {
  lda_imm(0xf8); // set offscreen coordinate if jumping here
  DumpSixSpr(); // <fallthrough>
}

void DumpSixSpr(void) {
  ram[Sprite_Data + 20 + y] = a; // dump A contents
  ram[Sprite_Data + 16 + y] = a; // into third row sprites
  DumpFourSpr(); // <fallthrough>
}

void DumpFourSpr(void) {
  ram[Sprite_Data + 12 + y] = a; // into second row sprites
  DumpThreeSpr(); // <fallthrough>
}

void DumpThreeSpr(void) {
  ram[Sprite_Data + 8 + y] = a;
  DumpTwoSpr(); // <fallthrough>
}

void DumpTwoSpr(void) {
  ram[Sprite_Data + 4 + y] = a; // and into first row sprites
  ram[Sprite_Data + y] = a;
  // -------------------------------------------------------------------------------------
}

void DrawLargePlatform(void) {
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  ram[0x2] = y; // store here
  iny(); // add 3 to it for offset
  iny(); // to X coordinate
  iny();
  lda_abs(Enemy_Rel_XPos); // get horizontal relative coordinate
  SixSpriteStacker(); // store X coordinates using A as base, stack horizontally
  ldx_zp(ObjectOffset);
  lda_zpx(Enemy_Y_Position); // get vertical coordinate
  DumpFourSpr(); // dump into first four sprites as Y coordinate
  ldy_abs(AreaType);
  cpy_imm(0x3); // check for castle-type level
  if (zero_flag) { goto ShrinkPlatform; }
  ldy_abs(SecondaryHardMode); // check for secondary hard mode flag set
  if (zero_flag) { goto SetLast2Platform; } // branch if not set elsewhere
  
ShrinkPlatform:
  lda_imm(0xf8); // load offscreen coordinate if flag set or castle-type level
  
SetLast2Platform:
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  ram[Sprite_Y_Position + 16 + y] = a; // store vertical coordinate or offscreen
  ram[Sprite_Y_Position + 20 + y] = a; // coordinate into last two sprites as Y coordinate
  lda_imm(0x5b); // load default tile for platform (girder)
  ldx_abs(CloudTypeOverride);
  if (zero_flag) { goto SetPlatformTilenum; } // if cloud level override flag not set, use
  lda_imm(0x75); // otherwise load other tile for platform (puff)
  
SetPlatformTilenum:
  ldx_zp(ObjectOffset); // get enemy object buffer offset
  iny(); // increment Y for tile offset
  DumpSixSpr(); // dump tile number into all six sprites
  lda_imm(0x2); // set palette controls
  iny(); // increment Y for sprite attributes
  DumpSixSpr(); // dump attributes into all six sprites
  inx(); // increment X for enemy objects
  GetXOffscreenBits(); // get offscreen bits again
  dex();
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  asl_acc(); // rotate d7 into carry, save remaining
  pha(); // bits to the stack
  if (!carry_flag) { goto SChk2; }
  lda_imm(0xf8); // if d7 was set, move first sprite offscreen
  ram[Sprite_Y_Position + y] = a;
  
SChk2:
  pla(); // get bits from stack
  asl_acc(); // rotate d6 into carry
  pha(); // save to stack
  if (!carry_flag) { goto SChk3; }
  lda_imm(0xf8); // if d6 was set, move second sprite offscreen
  ram[Sprite_Y_Position + 4 + y] = a;
  
SChk3:
  pla(); // get bits from stack
  asl_acc(); // rotate d5 into carry
  pha(); // save to stack
  if (!carry_flag) { goto SChk4; }
  lda_imm(0xf8); // if d5 was set, move third sprite offscreen
  ram[Sprite_Y_Position + 8 + y] = a;
  
SChk4:
  pla(); // get bits from stack
  asl_acc(); // rotate d4 into carry
  pha(); // save to stack
  if (!carry_flag) { goto SChk5; }
  lda_imm(0xf8); // if d4 was set, move fourth sprite offscreen
  ram[Sprite_Y_Position + 12 + y] = a;
  
SChk5:
  pla(); // get bits from stack
  asl_acc(); // rotate d3 into carry
  pha(); // save to stack
  if (!carry_flag) { goto SChk6; }
  lda_imm(0xf8); // if d3 was set, move fifth sprite offscreen
  ram[Sprite_Y_Position + 16 + y] = a;
  
SChk6:
  pla(); // get bits from stack
  asl_acc(); // rotate d2 into carry
  if (!carry_flag) { goto SLChk; } // save to stack
  lda_imm(0xf8);
  ram[Sprite_Y_Position + 20 + y] = a; // if d2 was set, move sixth sprite offscreen
  
SLChk:
  lda_abs(Enemy_OffscreenBits); // check d7 of offscreen bits
  asl_acc(); // and if d7 is not set, skip sub
  if (!carry_flag) { return; }
  MoveSixSpritesOffscreen(); // otherwise branch to move all sprites offscreen
  // -------------------------------------------------------------------------------------
}

void DrawFloateyNumber_Coin(void) {
  lda_zp(FrameCounter); // get frame counter
  lsr_acc(); // divide by 2
  // branch if d0 not set to raise number every other frame
  if (!carry_flag) {
    dec_zpx(Misc_Y_Position); // otherwise, decrement vertical coordinate
  }
  // NotRsNum:
  lda_zpx(Misc_Y_Position); // get vertical coordinate
  DumpTwoSpr(); // dump into both sprites
  lda_abs(Misc_Rel_XPos); // get relative horizontal coordinate
  ram[Sprite_X_Position + y] = a; // store as X coordinate for first sprite
  carry_flag = false;
  adc_imm(0x8); // add eight pixels
  ram[Sprite_X_Position + 4 + y] = a; // store as X coordinate for second sprite
  lda_imm(0x2);
  ram[Sprite_Attributes + y] = a; // store attribute byte in both sprites
  ram[Sprite_Attributes + 4 + y] = a;
  lda_imm(0xf7);
  ram[Sprite_Tilenumber + y] = a; // put tile numbers into both sprites
  lda_imm(0xfb); // that resemble "200"
  ram[Sprite_Tilenumber + 4 + y] = a;
}

void JCoinGfxHandler(void) {
  ldy_absx(Misc_SprDataOffset); // get coin/floatey number's OAM data offset
  lda_zpx(Misc_State); // get state of misc object
  cmp_imm(0x2); // if 2 or greater,
  // branch to draw floatey number
  if (carry_flag) {
    DrawFloateyNumber_Coin();
    return;
  }
  lda_zpx(Misc_Y_Position); // store vertical coordinate as
  ram[Sprite_Y_Position + y] = a; // Y coordinate for first sprite
  carry_flag = false;
  adc_imm(0x8); // add eight pixels
  ram[Sprite_Y_Position + 4 + y] = a; // store as Y coordinate for second sprite
  lda_abs(Misc_Rel_XPos); // get relative horizontal coordinate
  ram[Sprite_X_Position + y] = a;
  ram[Sprite_X_Position + 4 + y] = a; // store as X coordinate for first and second sprites
  lda_zp(FrameCounter); // get frame counter
  lsr_acc(); // divide by 2 to alter every other frame
  and_imm(0b00000011); // mask out d2-d1
  tax(); // use as graphical offset
  lda_absx(JumpingCoinTiles); // load tile number
  iny(); // increment OAM data offset to write tile numbers
  DumpTwoSpr(); // do sub to dump tile number into both sprites
  dey(); // decrement to get old offset
  lda_imm(0x2);
  ram[Sprite_Attributes + y] = a; // set attribute byte in first sprite
  lda_imm(0x82);
  ram[Sprite_Attributes + 4 + y] = a; // set attribute byte with vertical flip in second sprite
  ldx_zp(ObjectOffset); // get misc object offset
  // -------------------------------------------------------------------------------------
  // $00-$01 - used to hold tiles for drawing the power-up, $00 also used to hold power-up type
  // $02 - used to hold bottom row Y position
  // $03 - used to hold flip control (not used here)
  // $04 - used to hold sprite attributes
  // $05 - used to hold X position
  // $07 - counter
  // tiles arranged in top left, right, bottom left, right order
}

void DrawPowerUp(void) {
  ldy_abs(Enemy_SprDataOffset + 5); // get power-up's sprite data offset
  lda_abs(Enemy_Rel_YPos); // get relative vertical coordinate
  carry_flag = false;
  adc_imm(0x8); // add eight pixels
  ram[0x2] = a; // store result here
  lda_abs(Enemy_Rel_XPos); // get relative horizontal coordinate
  ram[0x5] = a; // store here
  ldx_zp(PowerUpType); // get power-up type
  lda_absx(PowerUpAttributes); // get attribute data for power-up type
  ora_abs(Enemy_SprAttrib + 5); // add background priority bit if set
  ram[0x4] = a; // store attributes here
  txa();
  pha(); // save power-up type to the stack
  asl_acc();
  asl_acc(); // multiply by four to get proper offset
  tax(); // use as X
  lda_imm(0x1);
  ram[0x7] = a; // set counter here to draw two rows of sprite object
  ram[0x3] = a; // init d1 of flip control
  
PUpDrawLoop:
  lda_absx(PowerUpGfxTable); // load left tile of power-up object
  ram[0x0] = a;
  lda_absx(PowerUpGfxTable + 1); // load right tile
  DrawOneSpriteRow(); // branch to draw one row of our power-up object
  dec_zp(0x7); // decrement counter
  if (!neg_flag) { goto PUpDrawLoop; } // branch until two rows are drawn
  ldy_abs(Enemy_SprDataOffset + 5); // get sprite data offset again
  pla(); // pull saved power-up type from the stack
  // if regular mushroom, branch, do not change colors or flip
  if (!zero_flag) {
    cmp_imm(0x3);
    // if 1-up mushroom, branch, do not change colors or flip
    if (!zero_flag) {
      ram[0x0] = a; // store power-up type here now
      lda_zp(FrameCounter); // get frame counter
      lsr_acc(); // divide by 2 to change colors every two frames
      and_imm(0b00000011); // mask out all but d1 and d0 (previously d2 and d1)
      ora_abs(Enemy_SprAttrib + 5); // add background priority bit if any set
      ram[Sprite_Attributes + y] = a; // set as new palette bits for top left and
      ram[Sprite_Attributes + 4 + y] = a; // top right sprites for fire flower and star
      ldx_zp(0x0);
      dex(); // check power-up type for fire flower
      // if found, skip this part
      if (!zero_flag) {
        ram[Sprite_Attributes + 8 + y] = a; // otherwise set new palette bits  for bottom left
        ram[Sprite_Attributes + 12 + y] = a; // and bottom right sprites as well for star only
      }
      // FlipPUpRightSide:
      lda_absy(Sprite_Attributes + 4);
      ora_imm(0b01000000); // set horizontal flip bit for top right sprite
      ram[Sprite_Attributes + 4 + y] = a;
      lda_absy(Sprite_Attributes + 12);
      ora_imm(0b01000000); // set horizontal flip bit for bottom right sprite
      ram[Sprite_Attributes + 12 + y] = a; // note these are only done for fire flower and star power-ups
    }
  }
  // PUpOfs:
  SprObjectOffscrChk(); // jump to check to see if power-up is offscreen at all, then leave
  // -------------------------------------------------------------------------------------
  // $00-$01 - used in DrawEnemyObjRow to hold sprite tile numbers
  // $02 - used to store Y position
  // $03 - used to store moving direction, used to flip enemies horizontally
  // $04 - used to store enemy's sprite attributes
  // $05 - used to store X position
  // $eb - used to hold sprite data offset
  // $ec - used to hold either altered enemy state or special value used in gfx handler as condition
  // $ed - used to hold enemy state from buffer
  // $ef - used to hold enemy code used in gfx handler (may or may not resemble Enemy_ID values)
  // tiles arranged in top left, right, middle left, right, bottom left, right order
}

void DrawEnemyObjRow(void) {
  lda_absx(EnemyGraphicsTable); // load two tiles of enemy graphics
  ram[0x0] = a;
  lda_absx(EnemyGraphicsTable + 1);
  DrawOneSpriteRow(); // <fallthrough>
}

void DrawOneSpriteRow(void) {
  ram[0x1] = a;
  DrawSpriteObject(); // draw them
}

void MoveESprRowOffscreen(void) {
  carry_flag = false; // add A to enemy object OAM data offset
  adc_absx(Enemy_SprDataOffset);
  tay(); // use as offset
  lda_imm(0xf8);
  DumpTwoSpr(); // move first row of sprites offscreen
}

void MoveESprColOffscreen(void) {
  carry_flag = false; // add A to enemy object OAM data offset
  adc_absx(Enemy_SprDataOffset);
  tay(); // use as offset
  MoveColOffscreen(); // move first and second row sprites in column offscreen
  ram[Sprite_Data + 16 + y] = a; // move third row sprite in column offscreen
  // -------------------------------------------------------------------------------------
  // $00-$01 - tile numbers
  // $02 - relative Y position
  // $03 - horizontal flip flag (not used here)
  // $04 - attributes
  // $05 - relative X position
}

void DrawBlock(void) {
  lda_abs(Block_Rel_YPos); // get relative vertical coordinate of block object
  ram[0x2] = a; // store here
  lda_abs(Block_Rel_XPos); // get relative horizontal coordinate of block object
  ram[0x5] = a; // store here
  lda_imm(0x3);
  ram[0x4] = a; // set attribute byte here
  lsr_acc();
  ram[0x3] = a; // set horizontal flip bit here (will not be used)
  ldy_absx(Block_SprDataOffset); // get sprite data offset
  ldx_imm(0x0); // reset X for use as offset to tile data
  
DBlkLoop:
  lda_absx(DefaultBlockObjTiles); // get left tile number
  ram[0x0] = a; // set here
  lda_absx(DefaultBlockObjTiles + 1); // get right tile number
  DrawOneSpriteRow(); // do sub to write tile numbers to first row of sprites
  cpx_imm(0x4); // check incremented offset
  if (!zero_flag) { goto DBlkLoop; } // and loop back until all four sprites are done
  ldx_zp(ObjectOffset); // get block object offset
  ldy_absx(Block_SprDataOffset); // get sprite data offset
  lda_abs(AreaType);
  cmp_imm(0x1); // check for ground level type area
  // if found, branch to next part
  if (!zero_flag) {
    lda_imm(0x86);
    ram[Sprite_Tilenumber + y] = a; // otherwise remove brick tiles with lines
    ram[Sprite_Tilenumber + 4 + y] = a; // and replace then with lineless brick tiles
  }
  // ChkRep:
  lda_absx(Block_Metatile); // check replacement metatile
  cmp_imm(0xc4); // if not used block metatile, then
  // branch ahead to use current graphics
  if (zero_flag) {
    lda_imm(0x87); // set A for used block tile
    iny(); // increment Y to write to tile bytes
    DumpFourSpr(); // do sub to dump into all four sprites
    dey(); // return Y to original offset
    lda_imm(0x3); // set palette bits
    ldx_abs(AreaType);
    dex(); // check for ground level type area again
    // if found, use current palette bits
    if (!zero_flag) {
      lsr_acc(); // otherwise set to $01
    }
    // SetBFlip:
    ldx_zp(ObjectOffset); // put block object offset back in X
    ram[Sprite_Attributes + y] = a; // store attribute byte as-is in first sprite
    ora_imm(0b01000000);
    ram[Sprite_Attributes + 4 + y] = a; // set horizontal flip bit for second sprite
    ora_imm(0b10000000);
    ram[Sprite_Attributes + 12 + y] = a; // set both flip bits for fourth sprite
    and_imm(0b10000011);
    ram[Sprite_Attributes + 8 + y] = a; // set vertical flip bit for third sprite
  }
  // BlkOffscr:
  lda_abs(Block_OffscreenBits); // get offscreen bits for block object
  pha(); // save to stack
  and_imm(0b00000100); // check to see if d2 in offscreen bits are set
  // if not set, branch, otherwise move sprites offscreen
  if (!zero_flag) {
    lda_imm(0xf8); // move offscreen two OAMs
    ram[Sprite_Y_Position + 4 + y] = a; // on the right side
    ram[Sprite_Y_Position + 12 + y] = a;
  }
  // PullOfsB:
  pla(); // pull offscreen bits from stack
  ChkLeftCo(); // <fallthrough>
}

void ChkLeftCo(void) {
  and_imm(0b00001000); // check to see if d3 in offscreen bits are set
  if (!zero_flag) {
    MoveColOffscreen(); // <fallthrough>
  }
}

void MoveColOffscreen(void) {
  lda_imm(0xf8); // move offscreen two OAMs
  ram[Sprite_Y_Position + y] = a; // on the left side (or two rows of enemy on either side
  ram[Sprite_Y_Position + 8 + y] = a; // if branched here from enemy graphics handler)
  // -------------------------------------------------------------------------------------
  // $00 - used to hold palette bits for attribute byte or relative X position
}

void DrawBrickChunks(void) {
  lda_imm(0x2); // set palette bits here
  ram[0x0] = a;
  lda_imm(0x75); // set tile number for ball (something residual, likely)
  ldy_zp(GameEngineSubroutine);
  cpy_imm(0x5); // if end-of-level routine running,
  if (zero_flag) { goto DChunks; } // use palette and tile number assigned
  lda_imm(0x3); // otherwise set different palette bits
  ram[0x0] = a;
  lda_imm(0x84); // and set tile number for brick chunks
  
DChunks:
  ldy_absx(Block_SprDataOffset); // get OAM data offset
  iny(); // increment to start with tile bytes in OAM
  DumpFourSpr(); // do sub to dump tile number into all four sprites
  lda_zp(FrameCounter); // get frame counter
  asl_acc();
  asl_acc();
  asl_acc(); // move low nybble to high
  asl_acc();
  and_imm(0xc0); // get what was originally d3-d2 of low nybble
  ora_zp(0x0); // add palette bits
  iny(); // increment offset for attribute bytes
  DumpFourSpr(); // do sub to dump attribute data into all four sprites
  dey();
  dey(); // decrement offset to Y coordinate
  lda_abs(Block_Rel_YPos); // get first block object's relative vertical coordinate
  DumpTwoSpr(); // do sub to dump current Y coordinate into two sprites
  lda_abs(Block_Rel_XPos); // get first block object's relative horizontal coordinate
  ram[Sprite_X_Position + y] = a; // save into X coordinate of first sprite
  lda_absx(Block_Orig_XPos); // get original horizontal coordinate
  carry_flag = true;
  sbc_abs(ScreenLeft_X_Pos); // subtract coordinate of left side from original coordinate
  ram[0x0] = a; // store result as relative horizontal coordinate of original
  carry_flag = true;
  sbc_abs(Block_Rel_XPos); // get difference of relative positions of original - current
  adc_zp(0x0); // add original relative position to result
  adc_imm(0x6); // plus 6 pixels to position second brick chunk correctly
  ram[Sprite_X_Position + 4 + y] = a; // save into X coordinate of second sprite
  lda_abs(Block_Rel_YPos + 1); // get second block object's relative vertical coordinate
  ram[Sprite_Y_Position + 8 + y] = a;
  ram[Sprite_Y_Position + 12 + y] = a; // dump into Y coordinates of third and fourth sprites
  lda_abs(Block_Rel_XPos + 1); // get second block object's relative horizontal coordinate
  ram[Sprite_X_Position + 8 + y] = a; // save into X coordinate of third sprite
  lda_zp(0x0); // use original relative horizontal position
  carry_flag = true;
  sbc_abs(Block_Rel_XPos + 1); // get difference of relative positions of original - current
  adc_zp(0x0); // add original relative position to result
  adc_imm(0x6); // plus 6 pixels to position fourth brick chunk correctly
  ram[Sprite_X_Position + 12 + y] = a; // save into X coordinate of fourth sprite
  lda_abs(Block_OffscreenBits); // get offscreen bits for block object
  ChkLeftCo(); // do sub to move left half of sprites offscreen if necessary
  lda_abs(Block_OffscreenBits); // get offscreen bits again
  asl_acc(); // shift d7 into carry
  if (!carry_flag) { goto ChnkOfs; } // if d7 not set, branch to last part
  lda_imm(0xf8);
  DumpTwoSpr(); // otherwise move top sprites offscreen
  
ChnkOfs:
  lda_zp(0x0); // if relative position on left side of screen,
  if (!neg_flag) { return; } // go ahead and leave
  lda_absy(Sprite_X_Position); // otherwise compare left-side X coordinate
  cmp_absy(Sprite_X_Position + 4); // to right-side X coordinate
  if (!carry_flag) { return; } // branch to leave if less
  lda_imm(0xf8); // otherwise move right half of sprites offscreen
  ram[Sprite_Y_Position + 4 + y] = a;
  ram[Sprite_Y_Position + 12 + y] = a;
  // -------------------------------------------------------------------------------------
}

void DrawFireball(void) {
  ldy_absx(FBall_SprDataOffset); // get fireball's sprite data offset
  lda_abs(Fireball_Rel_YPos); // get relative vertical coordinate
  ram[Sprite_Y_Position + y] = a; // store as sprite Y coordinate
  lda_abs(Fireball_Rel_XPos); // get relative horizontal coordinate
  ram[Sprite_X_Position + y] = a; // store as sprite X coordinate, then do shared code
  DrawFirebar(); // <fallthrough>
}

void DrawFirebar(void) {
  lda_zp(FrameCounter); // get frame counter
  lsr_acc(); // divide by four
  lsr_acc();
  pha(); // save result to stack
  and_imm(0x1); // mask out all but last bit
  eor_imm(0x64); // set either tile $64 or $65 as fireball tile
  ram[Sprite_Tilenumber + y] = a; // thus tile changes every four frames
  pla(); // get from stack
  lsr_acc(); // divide by four again
  lsr_acc();
  lda_imm(0x2); // load value $02 to set palette in attrib byte
  // if last bit shifted out was not set, skip this
  if (carry_flag) {
    ora_imm(0b11000000); // otherwise flip both ways every eight frames
  }
  // FireA:
  ram[Sprite_Attributes + y] = a; // store attribute byte and leave
  // -------------------------------------------------------------------------------------
}

void DrawExplosion_Fireball(void) {
  ldy_absx(Alt_SprDataOffset); // get OAM data offset of alternate sort for fireball's explosion
  lda_zpx(Fireball_State); // load fireball state
  inc_zpx(Fireball_State); // increment state for next frame
  lsr_acc(); // divide by 2
  and_imm(0b00000111); // mask out all but d3-d1
  cmp_imm(0x3); // check to see if time to kill fireball
  // branch if so, otherwise continue to draw explosion
  if (carry_flag) {
    KillFireBall();
    return;
  }
  DrawExplosion_Fireworks(); // <fallthrough>
}

void DrawExplosion_Fireworks(void) {
  tax(); // use whatever's in A for offset
  lda_absx(ExplosionTiles); // get tile number using offset
  iny(); // increment Y (contains sprite data offset)
  DumpFourSpr(); // and dump into tile number part of sprite data
  dey(); // decrement Y so we have the proper offset again
  ldx_zp(ObjectOffset); // return enemy object buffer offset to X
  lda_abs(Fireball_Rel_YPos); // get relative vertical coordinate
  carry_flag = true; // subtract four pixels vertically
  sbc_imm(0x4); // for first and third sprites
  ram[Sprite_Y_Position + y] = a;
  ram[Sprite_Y_Position + 8 + y] = a;
  carry_flag = false; // add eight pixels vertically
  adc_imm(0x8); // for second and fourth sprites
  ram[Sprite_Y_Position + 4 + y] = a;
  ram[Sprite_Y_Position + 12 + y] = a;
  lda_abs(Fireball_Rel_XPos); // get relative horizontal coordinate
  carry_flag = true; // subtract four pixels horizontally
  sbc_imm(0x4); // for first and second sprites
  ram[Sprite_X_Position + y] = a;
  ram[Sprite_X_Position + 4 + y] = a;
  carry_flag = false; // add eight pixels horizontally
  adc_imm(0x8); // for third and fourth sprites
  ram[Sprite_X_Position + 8 + y] = a;
  ram[Sprite_X_Position + 12 + y] = a;
  lda_imm(0x2); // set palette attributes for all sprites, but
  ram[Sprite_Attributes + y] = a; // set no flip at all for first sprite
  lda_imm(0x82);
  ram[Sprite_Attributes + 4 + y] = a; // set vertical flip for second sprite
  lda_imm(0x42);
  ram[Sprite_Attributes + 8 + y] = a; // set horizontal flip for third sprite
  lda_imm(0xc2);
  ram[Sprite_Attributes + 12 + y] = a; // set both flips for fourth sprite
}

void KillFireBall(void) {
  lda_imm(0x0); // clear fireball state to kill it
  ram[Fireball_State + x] = a;
  // -------------------------------------------------------------------------------------
}

void DrawSmallPlatform(void) {
  ldy_absx(Enemy_SprDataOffset); // get OAM data offset
  lda_imm(0x5b); // load tile number for small platforms
  iny(); // increment offset for tile numbers
  DumpSixSpr(); // dump tile number into all six sprites
  iny(); // increment offset for attributes
  lda_imm(0x2); // load palette controls
  DumpSixSpr(); // dump attributes into all six sprites
  dey(); // decrement for original offset
  dey();
  lda_abs(Enemy_Rel_XPos); // get relative horizontal coordinate
  ram[Sprite_X_Position + y] = a;
  ram[Sprite_X_Position + 12 + y] = a; // dump as X coordinate into first and fourth sprites
  carry_flag = false;
  adc_imm(0x8); // add eight pixels
  ram[Sprite_X_Position + 4 + y] = a; // dump into second and fifth sprites
  ram[Sprite_X_Position + 16 + y] = a;
  carry_flag = false;
  adc_imm(0x8); // add eight more pixels
  ram[Sprite_X_Position + 8 + y] = a; // dump into third and sixth sprites
  ram[Sprite_X_Position + 20 + y] = a;
  lda_zpx(Enemy_Y_Position); // get vertical coordinate
  tax();
  pha(); // save to stack
  cpx_imm(0x20); // if vertical coordinate below status bar,
  // do not mess with it
  if (!carry_flag) {
    lda_imm(0xf8); // otherwise move first three sprites offscreen
  }
  // TopSP:
  DumpThreeSpr(); // dump vertical coordinate into Y coordinates
  pla(); // pull from stack
  carry_flag = false;
  adc_imm(0x80); // add 128 pixels
  tax();
  cpx_imm(0x20); // if below status bar (taking wrap into account)
  // then do not change altered coordinate
  if (!carry_flag) {
    lda_imm(0xf8); // otherwise move last three sprites offscreen
  }
  // BotSP:
  ram[Sprite_Y_Position + 12 + y] = a; // dump vertical coordinate + 128 pixels
  ram[Sprite_Y_Position + 16 + y] = a; // into Y coordinates
  ram[Sprite_Y_Position + 20 + y] = a;
  lda_abs(Enemy_OffscreenBits); // get offscreen bits
  pha(); // save to stack
  and_imm(0b00001000); // check d3
  if (!zero_flag) {
    lda_imm(0xf8); // if d3 was set, move first and
    ram[Sprite_Y_Position + y] = a; // fourth sprites offscreen
    ram[Sprite_Y_Position + 12 + y] = a;
  }
  // SOfs:
  pla(); // move out and back into stack
  pha();
  and_imm(0b00000100); // check d2
  if (!zero_flag) {
    lda_imm(0xf8); // if d2 was set, move second and
    ram[Sprite_Y_Position + 4 + y] = a; // fifth sprites offscreen
    ram[Sprite_Y_Position + 16 + y] = a;
  }
  // SOfs2:
  pla(); // get from stack
  and_imm(0b00000010); // check d1
  if (!zero_flag) {
    lda_imm(0xf8); // if d1 was set, move third and
    ram[Sprite_Y_Position + 8 + y] = a; // sixth sprites offscreen
    ram[Sprite_Y_Position + 20 + y] = a;
  }
  // ExSPl:
  ldx_zp(ObjectOffset); // get enemy object offset and leave
  // -------------------------------------------------------------------------------------
}

void DrawBubble(void) {
  ldy_zp(Player_Y_HighPos); // if player's vertical high position
  dey(); // not within screen, skip all of this
  if (!zero_flag) { return; }
  lda_abs(Bubble_OffscreenBits); // check air bubble's offscreen bits
  and_imm(0b00001000);
  if (!zero_flag) { return; } // if bit set, branch to leave
  ldy_absx(Bubble_SprDataOffset); // get air bubble's OAM data offset
  lda_abs(Bubble_Rel_XPos); // get relative horizontal coordinate
  ram[Sprite_X_Position + y] = a; // store as X coordinate here
  lda_abs(Bubble_Rel_YPos); // get relative vertical coordinate
  ram[Sprite_Y_Position + y] = a; // store as Y coordinate here
  lda_imm(0x74);
  ram[Sprite_Tilenumber + y] = a; // put air bubble tile into OAM data
  lda_imm(0x2);
  ram[Sprite_Attributes + y] = a; // set attribute byte
  // -------------------------------------------------------------------------------------
  // $00 - used to store player's vertical offscreen bits
}

void DrawPlayer_Intermediate(void) {
  ldx_imm(0x5); // store data into zero page memory
  
PIntLoop:
  lda_absx(IntermediatePlayerData); // load data to display player as he always
  ram[0x2 + x] = a; // appears on world/lives display
  dex();
  if (!neg_flag) { goto PIntLoop; } // do this until all data is loaded
  ldx_imm(0xb8); // load offset for small standing
  ldy_imm(0x4); // load sprite data offset
  DrawPlayerLoop(); // draw player accordingly
  lda_abs(Sprite_Attributes + 36); // get empty sprite attributes
  ora_imm(0b01000000); // set horizontal flip bit for bottom-right sprite
  ram[Sprite_Attributes + 32] = a; // store and leave
  // -------------------------------------------------------------------------------------
  // $00-$01 - used to hold tile numbers, $00 also used to hold upper extent of animation frames
  // $02 - vertical position
  // $03 - facing direction, used as horizontal flip control
  // $04 - attributes
  // $05 - horizontal position
  // $07 - number of rows to draw
  // these also used in IntermediatePlayerData
}

void RenderPlayerSub(void) {
  ram[0x7] = a; // store number of rows of sprites to draw
  lda_abs(Player_Rel_XPos);
  ram[Player_Pos_ForScroll] = a; // store player's relative horizontal position
  ram[0x5] = a; // store it here also
  lda_abs(Player_Rel_YPos);
  ram[0x2] = a; // store player's vertical position
  lda_zp(PlayerFacingDir);
  ram[0x3] = a; // store player's facing direction
  lda_abs(Player_SprAttrib);
  ram[0x4] = a; // store player's sprite attributes
  ldx_abs(PlayerGfxOffset); // load graphics table offset
  ldy_abs(Player_SprDataOffset); // get player's sprite data offset
  DrawPlayerLoop(); // <fallthrough>
}

void DrawPlayerLoop(void) {
  lda_absx(PlayerGraphicsTable); // load player's left side
  ram[0x0] = a;
  lda_absx(PlayerGraphicsTable + 1); // now load right side
  DrawOneSpriteRow();
  dec_zp(0x7); // decrement rows of sprites to draw
  // do this until all rows are drawn
  if (!zero_flag) {
    DrawPlayerLoop();
    return;
  }
}

void ProcessPlayerAction(void) {
  lda_zp(Player_State); // get player's state
  cmp_imm(0x3);
  if (zero_flag) { goto ActionClimbing; } // if climbing, branch here
  cmp_imm(0x2);
  if (zero_flag) { goto ActionFalling; } // if falling, branch here
  cmp_imm(0x1);
  if (!zero_flag) { goto ProcOnGroundActs; } // if not jumping, branch here
  lda_abs(SwimmingFlag);
  if (!zero_flag) { goto ActionSwimming; } // if swimming flag set, branch elsewhere
  ldy_imm(0x6); // load offset for crouching
  lda_abs(CrouchingFlag); // get crouching flag
  if (!zero_flag) { goto NonAnimatedActs; } // if set, branch to get offset for graphics table
  ldy_imm(0x0); // otherwise load offset for jumping
  goto NonAnimatedActs; // go to get offset to graphics table
  
ProcOnGroundActs:
  ldy_imm(0x6); // load offset for crouching
  lda_abs(CrouchingFlag); // get crouching flag
  if (!zero_flag) { goto NonAnimatedActs; } // if set, branch to get offset for graphics table
  ldy_imm(0x2); // load offset for standing
  lda_zp(Player_X_Speed); // check player's horizontal speed
  ora_zp(Left_Right_Buttons); // and left/right controller bits
  if (zero_flag) { goto NonAnimatedActs; } // if no speed or buttons pressed, use standing offset
  lda_abs(Player_XSpeedAbsolute); // load walking/running speed
  cmp_imm(0x9);
  if (!carry_flag) { goto ActionWalkRun; } // if less than a certain amount, branch, too slow to skid
  lda_zp(Player_MovingDir); // otherwise check to see if moving direction
  and_zp(PlayerFacingDir); // and facing direction are the same
  if (!zero_flag) { goto ActionWalkRun; } // if moving direction = facing direction, branch, don't skid
  iny(); // otherwise increment to skid offset ($03)
  
NonAnimatedActs:
  GetGfxOffsetAdder(); // do a sub here to get offset adder for graphics table
  lda_imm(0x0);
  ram[PlayerAnimCtrl] = a; // initialize animation frame control
  lda_absy(PlayerGfxTblOffsets); // load offset to graphics table using size as offset
  return;
  
ActionFalling:
  ldy_imm(0x4); // load offset for walking/running
  GetGfxOffsetAdder(); // get offset to graphics table
  GetCurrentAnimOffset(); return; // execute instructions for falling state
  
ActionWalkRun:
  ldy_imm(0x4); // load offset for walking/running
  GetGfxOffsetAdder(); // get offset to graphics table
  FourFrameExtent(); // execute instructions for normal state
  return;
  
ActionClimbing:
  ldy_imm(0x5); // load offset for climbing
  lda_zp(Player_Y_Speed); // check player's vertical speed
  if (zero_flag) { goto NonAnimatedActs; } // if no speed, branch, use offset as-is
  GetGfxOffsetAdder(); // otherwise get offset for graphics table
  ThreeFrameExtent(); // then skip ahead to more code
  return;
  
ActionSwimming:
  ldy_imm(0x1); // load offset for swimming
  GetGfxOffsetAdder();
  lda_abs(JumpSwimTimer); // check jump/swim timer
  ora_abs(PlayerAnimCtrl); // and animation frame control
  if (!zero_flag) { FourFrameExtent(); return; } // if any one of these set, branch ahead
  lda_zp(A_B_Buttons);
  asl_acc(); // check for A button pressed
  if (carry_flag) { FourFrameExtent(); return; } // branch to same place if A button pressed
  GetCurrentAnimOffset(); // <fallthrough>
}

void GetCurrentAnimOffset(void) {
  lda_abs(PlayerAnimCtrl); // get animation frame control
  GetOffsetFromAnimCtrl(); // jump to get proper offset to graphics table
}

void FourFrameExtent(void) {
  lda_imm(0x3); // load upper extent for frame control
  AnimationControl(); // jump to get offset and animate player object
}

void ThreeFrameExtent(void) {
  lda_imm(0x2); // load upper extent for frame control for climbing
  AnimationControl(); // <fallthrough>
}

void AnimationControl(void) {
  ram[0x0] = a; // store upper extent here
  GetCurrentAnimOffset(); // get proper offset to graphics table
  pha(); // save offset to stack
  lda_abs(PlayerAnimTimer); // load animation frame timer
  // branch if not expired
  if (zero_flag) {
    lda_abs(PlayerAnimTimerSet); // get animation frame timer amount
    ram[PlayerAnimTimer] = a; // and set timer accordingly
    lda_abs(PlayerAnimCtrl);
    carry_flag = false; // add one to animation frame control
    adc_imm(0x1);
    cmp_zp(0x0); // compare to upper extent
    // if frame control + 1 < upper extent, use as next
    if (carry_flag) {
      lda_imm(0x0); // otherwise initialize frame control
    }
    // SetAnimC:
    ram[PlayerAnimCtrl] = a; // store as new animation frame control
  }
  // ExAnimC:
  pla(); // get offset to graphics table from stack and leave
}

void GetGfxOffsetAdder(void) {
  lda_abs(PlayerSize); // get player's size
  if (!zero_flag) {
    tya(); // for big player
    carry_flag = false; // otherwise add eight bytes to offset
    adc_imm(0x8); // for small player
    tay();
  }
}

void HandleChangeSize(void) {
  ldy_abs(PlayerAnimCtrl); // get animation frame control
  lda_zp(FrameCounter);
  and_imm(0b00000011); // get frame counter and execute this code every
  // fourth frame, otherwise branch ahead
  if (zero_flag) {
    iny(); // increment frame control
    cpy_imm(0xa); // check for preset upper extent
    // if not there yet, skip ahead to use
    if (carry_flag) {
      ldy_imm(0x0); // otherwise initialize both grow/shrink flag
      ram[PlayerChangeSizeFlag] = y; // and animation frame control
    }
    // CSzNext:
    ram[PlayerAnimCtrl] = y; // store proper frame control
  }
  // GorSLog:
  lda_abs(PlayerSize); // get player's size
  // if player small, skip ahead to next part
  if (!zero_flag) {
    ShrinkPlayer();
    return;
  }
  lda_absy(ChangeSizeOffsetAdder); // get offset adder based on frame control as offset
  ldy_imm(0xf); // load offset for player growing
  GetOffsetFromAnimCtrl(); // <fallthrough>
}

void GetOffsetFromAnimCtrl(void) {
  asl_acc(); // multiply animation frame control
  asl_acc(); // by eight to get proper amount
  asl_acc(); // to add to our offset
  adc_absy(PlayerGfxTblOffsets); // add to offset to graphics table
}

void ShrinkPlayer(void) {
  tya(); // add ten bytes to frame control as offset
  carry_flag = false;
  adc_imm(0xa); // this thing apparently uses two of the swimming frames
  tax(); // to draw the player shrinking
  ldy_imm(0x9); // load offset for small player swimming
  lda_absx(ChangeSizeOffsetAdder); // get what would normally be offset adder
  // and branch to use offset if nonzero
  if (zero_flag) {
    ldy_imm(0x1); // otherwise load offset for big player swimming
  }
  // ShrPlF:
  lda_absy(PlayerGfxTblOffsets); // get offset to graphics table based on offset loaded
}

void ChkForPlayerAttrib(void) {
  ldy_abs(Player_SprDataOffset); // get sprite data offset
  lda_zp(GameEngineSubroutine);
  cmp_imm(0xb); // if executing specific game engine routine,
  if (zero_flag) { goto KilledAtt; } // branch to change third and fourth row OAM attributes
  lda_abs(PlayerGfxOffset); // get graphics table offset
  cmp_imm(0x50);
  if (zero_flag) { goto C_S_IGAtt; } // if crouch offset, either standing offset,
  cmp_imm(0xb8); // or intermediate growing offset,
  if (zero_flag) { goto C_S_IGAtt; } // go ahead and execute code to change
  cmp_imm(0xc0); // fourth row OAM attributes only
  if (zero_flag) { goto C_S_IGAtt; }
  cmp_imm(0xc8);
  if (!zero_flag) { return; } // if none of these, branch to leave
  
KilledAtt:
  lda_absy(Sprite_Attributes + 16);
  and_imm(0b00111111); // mask out horizontal and vertical flip bits
  ram[Sprite_Attributes + 16 + y] = a; // for third row sprites and save
  lda_absy(Sprite_Attributes + 20);
  and_imm(0b00111111);
  ora_imm(0b01000000); // set horizontal flip bit for second
  ram[Sprite_Attributes + 20 + y] = a; // sprite in the third row
  
C_S_IGAtt:
  lda_absy(Sprite_Attributes + 24);
  and_imm(0b00111111); // mask out horizontal and vertical flip bits
  ram[Sprite_Attributes + 24 + y] = a; // for fourth row sprites and save
  lda_absy(Sprite_Attributes + 28);
  and_imm(0b00111111);
  ora_imm(0b01000000); // set horizontal flip bit for second
  ram[Sprite_Attributes + 28 + y] = a; // sprite in the fourth row
  // -------------------------------------------------------------------------------------
  // $00 - used in adding to get proper offset
}

void RelativePlayerPosition(void) {
  ldx_imm(0x0); // set offsets for relative cooordinates
  ldy_imm(0x0); // routine to correspond to player object
  RelWOfs(); // get the coordinates
}

void RelativeBubblePosition(void) {
  ldy_imm(0x1); // set for air bubble offsets
  GetProperObjOffset(); // modify X to get proper air bubble offset
  ldy_imm(0x3);
  RelWOfs(); // get the coordinates
}

void RelativeFireballPosition(void) {
  ldy_imm(0x0); // set for fireball offsets
  GetProperObjOffset(); // modify X to get proper fireball offset
  ldy_imm(0x2);
  RelWOfs(); // <fallthrough>
}

void RelWOfs(void) {
  GetObjRelativePosition(); // get the coordinates
  ldx_zp(ObjectOffset); // return original offset
}

void RelativeMiscPosition(void) {
  ldy_imm(0x2); // set for misc object offsets
  GetProperObjOffset(); // modify X to get proper misc object offset
  ldy_imm(0x6);
  RelWOfs(); // get the coordinates
}

void RelativeEnemyPosition(void) {
  lda_imm(0x1); // get coordinates of enemy object
  ldy_imm(0x1); // relative to the screen
  VariableObjOfsRelPos();
}

void RelativeBlockPosition(void) {
  lda_imm(0x9); // get coordinates of one block object
  ldy_imm(0x4); // relative to the screen
  VariableObjOfsRelPos();
  inx(); // adjust offset for other block object if any
  inx();
  lda_imm(0x9);
  iny(); // adjust other and get coordinates for other one
  VariableObjOfsRelPos(); // <fallthrough>
}

void VariableObjOfsRelPos(void) {
  ram[0x0] = x; // store value to add to A here
  carry_flag = false;
  adc_zp(0x0); // add A to value stored
  tax(); // use as enemy offset
  GetObjRelativePosition();
  ldx_zp(ObjectOffset); // reload old object offset and leave
}

void GetObjRelativePosition(void) {
  lda_zpx(SprObject_Y_Position); // load vertical coordinate low
  ram[SprObject_Rel_YPos + y] = a; // store here
  lda_zpx(SprObject_X_Position); // load horizontal coordinate
  carry_flag = true; // subtract left edge coordinate
  sbc_abs(ScreenLeft_X_Pos);
  ram[SprObject_Rel_XPos + y] = a; // store result here
  // -------------------------------------------------------------------------------------
  // $00 - used as temp variable to hold offscreen bits
}

void GetPlayerOffscreenBits(void) {
  ldx_imm(0x0); // set offsets for player-specific variables
  ldy_imm(0x0); // and get offscreen information about player
  GetOffScreenBitsSet();
}

void GetFireballOffscreenBits(void) {
  ldy_imm(0x0); // set for fireball offsets
  GetProperObjOffset(); // modify X to get proper fireball offset
  ldy_imm(0x2); // set other offset for fireball's offscreen bits
  GetOffScreenBitsSet(); // and get offscreen information about fireball
}

void GetBubbleOffscreenBits(void) {
  ldy_imm(0x1); // set for air bubble offsets
  GetProperObjOffset(); // modify X to get proper air bubble offset
  ldy_imm(0x3); // set other offset for airbubble's offscreen bits
  GetOffScreenBitsSet(); // and get offscreen information about air bubble
}

void GetMiscOffscreenBits(void) {
  ldy_imm(0x2); // set for misc object offsets
  GetProperObjOffset(); // modify X to get proper misc object offset
  ldy_imm(0x6); // set other offset for misc object's offscreen bits
  GetOffScreenBitsSet(); // and get offscreen information about misc object
}

void GetProperObjOffset(void) {
  txa(); // move offset to A
  carry_flag = false;
  adc_absy(ObjOffsetData); // add amount of bytes to offset depending on setting in Y
  tax(); // put back in X and leave
}

void GetEnemyOffscreenBits(void) {
  lda_imm(0x1); // set A to add 1 byte in order to get enemy offset
  ldy_imm(0x1); // set Y to put offscreen bits in Enemy_OffscreenBits
  SetOffscrBitsOffset();
}

void GetBlockOffscreenBits(void) {
  lda_imm(0x9); // set A to add 9 bytes in order to get block obj offset
  ldy_imm(0x4); // set Y to put offscreen bits in Block_OffscreenBits
  SetOffscrBitsOffset(); // <fallthrough>
}

void SetOffscrBitsOffset(void) {
  ram[0x0] = x;
  carry_flag = false; // add contents of X to A to get
  adc_zp(0x0); // appropriate offset, then give back to X
  tax();
  GetOffScreenBitsSet(); // <fallthrough>
}

void GetOffScreenBitsSet(void) {
  tya(); // save offscreen bits offset to stack for now
  pha();
  RunOffscrBitsSubs();
  asl_acc(); // move low nybble to high nybble
  asl_acc();
  asl_acc();
  asl_acc();
  ora_zp(0x0); // mask together with previously saved low nybble
  ram[0x0] = a; // store both here
  pla(); // get offscreen bits offset from stack
  tay();
  lda_zp(0x0); // get value here and store elsewhere
  ram[SprObject_OffscrBits + y] = a;
  ldx_zp(ObjectOffset);
}

void RunOffscrBitsSubs(void) {
  GetXOffscreenBits(); // do subroutine here
  lsr_acc(); // move high nybble to low
  lsr_acc();
  lsr_acc();
  lsr_acc();
  ram[0x0] = a; // store here
  GetYOffscreenBits();
  // --------------------------------
  // (these apply to these three subsections)
  // $04 - used to store proper offset
  // $05 - used as adder in DividePDiff
  // $06 - used to store preset value used to compare to pixel difference in $07
  // $07 - used to store difference between coordinates of object and screen edges
}

void GetXOffscreenBits(void) {
  ram[0x4] = x; // save position in buffer to here
  ldy_imm(0x1); // start with right side of screen
  
XOfsLoop:
  lda_absy(ScreenEdge_X_Pos); // get pixel coordinate of edge
  carry_flag = true; // get difference between pixel coordinate of edge
  sbc_zpx(SprObject_X_Position); // and pixel coordinate of object position
  ram[0x7] = a; // store here
  lda_absy(ScreenEdge_PageLoc); // get page location of edge
  sbc_zpx(SprObject_PageLoc); // subtract from page location of object position
  ldx_absy(DefaultXOnscreenOfs); // load offset value here
  cmp_imm(0x0);
  // if beyond right edge or in front of left edge, branch
  if (!neg_flag) {
    ldx_absy(DefaultXOnscreenOfs + 1); // if not, load alternate offset value here
    cmp_imm(0x1);
    // if one page or more to the left of either edge, branch
    if (neg_flag) {
      lda_imm(0x38); // if no branching, load value here and store
      ram[0x6] = a;
      lda_imm(0x8); // load some other value and execute subroutine
      DividePDiff();
    }
  }
  // XLdBData:
  lda_absx(XOffscreenBitsData); // get bits here
  ldx_zp(0x4); // reobtain position in buffer
  cmp_imm(0x0); // if bits not zero, branch to leave
  if (zero_flag) {
    dey(); // otherwise, do left side of screen now
    if (!neg_flag) { goto XOfsLoop; } // branch if not already done with left side
    // --------------------------------
  }
}

void GetYOffscreenBits(void) {
  ram[0x4] = x; // save position in buffer to here
  ldy_imm(0x1); // start with top of screen
  
YOfsLoop:
  lda_absy(HighPosUnitData); // load coordinate for edge of vertical unit
  carry_flag = true;
  sbc_zpx(SprObject_Y_Position); // subtract from vertical coordinate of object
  ram[0x7] = a; // store here
  lda_imm(0x1); // subtract one from vertical high byte of object
  sbc_zpx(SprObject_Y_HighPos);
  ldx_absy(DefaultYOnscreenOfs); // load offset value here
  cmp_imm(0x0);
  // if under top of the screen or beyond bottom, branch
  if (!neg_flag) {
    ldx_absy(DefaultYOnscreenOfs + 1); // if not, load alternate offset value here
    cmp_imm(0x1);
    // if one vertical unit or more above the screen, branch
    if (neg_flag) {
      lda_imm(0x20); // if no branching, load value here and store
      ram[0x6] = a;
      lda_imm(0x4); // load some other value and execute subroutine
      DividePDiff();
    }
  }
  // YLdBData:
  lda_absx(YOffscreenBitsData); // get offscreen data bits using offset
  ldx_zp(0x4); // reobtain position in buffer
  cmp_imm(0x0);
  if (zero_flag) {
    dey(); // otherwise, do bottom of the screen now
    if (!neg_flag) { goto YOfsLoop; }
    // --------------------------------
  }
}

void DividePDiff(void) {
  ram[0x5] = a; // store current value in A here
  lda_zp(0x7); // get pixel difference
  cmp_zp(0x6); // compare to preset value
  if (!carry_flag) {
    lsr_acc(); // divide by eight
    lsr_acc();
    lsr_acc();
    and_imm(0x7); // mask out all but 3 LSB
    cpy_imm(0x1); // right side of the screen or top?
    // if so, branch, use difference / 8 as offset
    if (!carry_flag) {
      adc_zp(0x5); // if not, add value to difference / 8
    }
    // SetOscrO:
    tax(); // use as offset
    // -------------------------------------------------------------------------------------
    // $00-$01 - tile numbers
    // $02 - Y coordinate
    // $03 - flip control
    // $04 - sprite attributes
    // $05 - X coordinate
  }
}

void DrawSpriteObject(void) {
  lda_zp(0x3); // get saved flip control bits
  lsr_acc();
  lsr_acc(); // move d1 into carry
  lda_zp(0x0);
  if (!carry_flag) { goto NoHFlip; } // if d1 not set, branch
  ram[Sprite_Tilenumber + 4 + y] = a; // store first tile into second sprite
  lda_zp(0x1); // and second into first sprite
  ram[Sprite_Tilenumber + y] = a;
  lda_imm(0x40); // activate horizontal flip OAM attribute
  if (!zero_flag) { goto SetHFAt; } // and unconditionally branch
  
NoHFlip:
  ram[Sprite_Tilenumber + y] = a; // store first tile into first sprite
  lda_zp(0x1); // and second into second sprite
  ram[Sprite_Tilenumber + 4 + y] = a;
  lda_imm(0x0); // clear bit for horizontal flip
  
SetHFAt:
  ora_zp(0x4); // add other OAM attributes if necessary
  ram[Sprite_Attributes + y] = a; // store sprite attributes
  ram[Sprite_Attributes + 4 + y] = a;
  lda_zp(0x2); // now the y coordinates
  ram[Sprite_Y_Position + y] = a; // note because they are
  ram[Sprite_Y_Position + 4 + y] = a; // side by side, they are the same
  lda_zp(0x5);
  ram[Sprite_X_Position + y] = a; // store x coordinate, then
  carry_flag = false; // add 8 pixels and store another to
  adc_imm(0x8); // put them side by side
  ram[Sprite_X_Position + 4 + y] = a;
  lda_zp(0x2); // add eight pixels to the next y
  carry_flag = false; // coordinate
  adc_imm(0x8);
  ram[0x2] = a;
  tya(); // add eight to the offset in Y to
  carry_flag = false; // move to the next two sprites
  adc_imm(0x8);
  tay();
  inx(); // increment offset to return it to the
  inx(); // routine that called this subroutine
  // -------------------------------------------------------------------------------------
  // unused space
  //         .db $ff, $ff, $ff, $ff, $ff, $ff
  // -------------------------------------------------------------------------------------
}

void SoundEngine(void) {
  lda_abs(OperMode); // are we in title screen mode?
  if (!zero_flag) { goto SndOn; }
  apu_write(SND_MASTERCTRL_REG, a); // if so, disable sound and leave
  return;
  
SndOn:
  lda_imm(0xff);
  write_joypad2(a); // disable irqs and set frame counter mode???
  lda_imm(0xf);
  apu_write(SND_MASTERCTRL_REG, a); // enable first four channels
  lda_abs(PauseModeFlag); // is sound already in pause mode?
  if (!zero_flag) { goto InPause; }
  lda_zp(PauseSoundQueue); // if not, check pause sfx queue
  cmp_imm(0x1);
  if (!zero_flag) { goto RunSoundSubroutines; } // if queue is empty, skip pause mode routine
  
InPause:
  lda_abs(PauseSoundBuffer); // check pause sfx buffer
  if (!zero_flag) { goto ContPau; }
  lda_zp(PauseSoundQueue); // check pause queue
  if (zero_flag) { goto SkipSoundSubroutines; }
  ram[PauseSoundBuffer] = a; // if queue full, store in buffer and activate
  ram[PauseModeFlag] = a; // pause mode to interrupt game sounds
  lda_imm(0x0); // disable sound and clear sfx buffers
  apu_write(SND_MASTERCTRL_REG, a);
  ram[Square1SoundBuffer] = a;
  ram[Square2SoundBuffer] = a;
  ram[NoiseSoundBuffer] = a;
  lda_imm(0xf);
  apu_write(SND_MASTERCTRL_REG, a); // enable sound again
  lda_imm(0x2a); // store length of sound in pause counter
  ram[Squ1_SfxLenCounter] = a;
  
PTone1F:
  lda_imm(0x44); // play first tone
  if (!zero_flag) { goto PTRegC; } // unconditional branch
  
ContPau:
  lda_abs(Squ1_SfxLenCounter); // check pause length left
  cmp_imm(0x24); // time to play second?
  if (zero_flag) { goto PTone2F; }
  cmp_imm(0x1e); // time to play first again?
  if (zero_flag) { goto PTone1F; }
  cmp_imm(0x18); // time to play second again?
  if (!zero_flag) { goto DecPauC; } // only load regs during times, otherwise skip
  
PTone2F:
  lda_imm(0x64); // store reg contents and play the pause sfx
  
PTRegC:
  ldx_imm(0x84);
  ldy_imm(0x7f);
  PlaySqu1Sfx();
  
DecPauC:
  dec_abs(Squ1_SfxLenCounter); // decrement pause sfx counter
  if (!zero_flag) { goto SkipSoundSubroutines; }
  lda_imm(0x0); // disable sound if in pause mode and
  apu_write(SND_MASTERCTRL_REG, a); // not currently playing the pause sfx
  lda_abs(PauseSoundBuffer); // if no longer playing pause sfx, check to see
  cmp_imm(0x2); // if we need to be playing sound again
  if (!zero_flag) { goto SkipPIn; }
  lda_imm(0x0); // clear pause mode to allow game sounds again
  ram[PauseModeFlag] = a;
  
SkipPIn:
  lda_imm(0x0); // clear pause sfx buffer
  ram[PauseSoundBuffer] = a;
  if (zero_flag) { goto SkipSoundSubroutines; }
  
RunSoundSubroutines:
  Square1SfxHandler(); // play sfx on square channel 1
  Square2SfxHandler(); //  ''  ''  '' square channel 2
  NoiseSfxHandler(); //  ''  ''  '' noise channel
  MusicHandler(); // play music on all channels
  lda_imm(0x0); // clear the music queues
  ram[AreaMusicQueue] = a;
  ram[EventMusicQueue] = a;
  
SkipSoundSubroutines:
  lda_imm(0x0); // clear the sound effects queues
  ram[Square1SoundQueue] = a;
  ram[Square2SoundQueue] = a;
  ram[NoiseSoundQueue] = a;
  ram[PauseSoundQueue] = a;
  ldy_abs(DAC_Counter); // load some sort of counter
  lda_zp(AreaMusicBuffer);
  and_imm(0b00000011); // check for specific music
  if (zero_flag) { goto NoIncDAC; }
  inc_abs(DAC_Counter); // increment and check counter
  cpy_imm(0x30);
  if (!carry_flag) { goto StrWave; } // if not there yet, just store it
  
NoIncDAC:
  tya();
  if (zero_flag) { goto StrWave; } // if we are at zero, do not decrement
  dec_abs(DAC_Counter); // decrement counter
  
StrWave:
  dynamic_ram_write(SND_DELTA_REG + 1, y); // store into DMC load register (??)
  // --------------------------------
}

void Dump_Squ1_Regs(void) {
  dynamic_ram_write(SND_SQUARE1_REG + 1, y); // dump the contents of X and Y into square 1's control regs
  apu_write(SND_SQUARE1_REG, x);
}

void PlaySqu1Sfx(void) {
  Dump_Squ1_Regs(); // do sub to set ctrl regs for square 1, then set frequency regs
  SetFreq_Squ1(); // <fallthrough>
}

void SetFreq_Squ1(void) {
  ldx_imm(0x0); // set frequency reg offset for square 1 sound channel
  Dump_Freq_Regs(); // <fallthrough>
}

void Dump_Freq_Regs(void) {
  tay();
  lda_absy(FreqRegLookupTbl + 1); // use previous contents of A for sound reg offset
  if (!zero_flag) {
    dynamic_ram_write(SND_REGISTER + 2 + x, a); // first byte goes into LSB of frequency divider
    lda_absy(FreqRegLookupTbl); // second byte goes into 3 MSB plus extra bit for
    ora_imm(0b00001000); // length counter
    dynamic_ram_write(SND_REGISTER + 3 + x, a);
  }
}

void Dump_Sq2_Regs(void) {
  apu_write(SND_SQUARE2_REG, x); // dump the contents of X and Y into square 2's control regs
  dynamic_ram_write(SND_SQUARE2_REG + 1, y);
}

void PlaySqu2Sfx(void) {
  Dump_Sq2_Regs(); // do sub to set ctrl regs for square 2, then set frequency regs
  SetFreq_Squ2(); // <fallthrough>
}

void SetFreq_Squ2(void) {
  ldx_imm(0x4); // set frequency reg offset for square 2 sound channel
  // unconditional branch
  if (!zero_flag) {
    Dump_Freq_Regs();
    return;
  }
  SetFreq_Tri(); // <fallthrough>
}

void SetFreq_Tri(void) {
  ldx_imm(0x8); // set frequency reg offset for triangle sound channel
  // unconditional branch
  if (!zero_flag) {
    Dump_Freq_Regs();
    return;
  }
  // --------------------------------
  PlayFlagpoleSlide(); // <fallthrough>
}

void PlayFlagpoleSlide(void) {
  lda_imm(0x40); // store length of flagpole sound
  ram[Squ1_SfxLenCounter] = a;
  lda_imm(0x62); // load part of reg contents for flagpole sound
  SetFreq_Squ1();
  ldx_imm(0x99); // now load the rest
  if (!zero_flag) {
    FPS2nd();
    return;
  }
  PlaySmallJump(); // <fallthrough>
}

void PlaySmallJump(void) {
  lda_imm(0x26); // branch here for small mario jumping sound
  if (!zero_flag) {
    JumpRegContents();
    return;
  }
  PlayBigJump(); // <fallthrough>
}

void PlayBigJump(void) {
  lda_imm(0x18); // branch here for big mario jumping sound
  JumpRegContents(); // <fallthrough>
}

void JumpRegContents(void) {
  ldx_imm(0x82); // note that small and big jump borrow each others' reg contents
  ldy_imm(0xa7); // anyway, this loads the first part of mario's jumping sound
  PlaySqu1Sfx();
  lda_imm(0x28); // store length of sfx for both jumping sounds
  ram[Squ1_SfxLenCounter] = a; // then continue on here
  ContinueSndJump(); // <fallthrough>
}

void ContinueSndJump(void) {
  lda_abs(Squ1_SfxLenCounter); // jumping sounds seem to be composed of three parts
  cmp_imm(0x25); // check for time to play second part yet
  if (zero_flag) {
    ldx_imm(0x5f); // load second part
    ldy_imm(0xf6);
    // unconditional branch
    if (!zero_flag) {
      DmpJpFPS();
      return;
    }
  }
  // N2Prt:
  cmp_imm(0x20); // check for third part
  if (!zero_flag) {
    DecJpFPS();
    return;
  }
  ldx_imm(0x48); // load third part
  FPS2nd(); // <fallthrough>
}

void FPS2nd(void) {
  ldy_imm(0xbc); // the flagpole slide sound shares part of third part
  DmpJpFPS(); // <fallthrough>
}

void DmpJpFPS(void) {
  Dump_Squ1_Regs();
  // unconditional branch outta here
  if (!zero_flag) {
    DecJpFPS();
    return;
  }
  PlayFireballThrow(); // <fallthrough>
}

void PlayFireballThrow(void) {
  lda_imm(0x5);
  ldy_imm(0x99); // load reg contents for fireball throw sound
  // unconditional branch
  if (!zero_flag) {
    Fthrow();
    return;
  }
  PlayBump(); // <fallthrough>
}

void PlayBump(void) {
  lda_imm(0xa); // load length of sfx and reg contents for bump sound
  ldy_imm(0x93);
  Fthrow(); // <fallthrough>
}

void Fthrow(void) {
  ldx_imm(0x9e); // the fireball sound shares reg contents with the bump sound
  ram[Squ1_SfxLenCounter] = a;
  lda_imm(0xc); // load offset for bump sound
  PlaySqu1Sfx();
  ContinueBumpThrow(); // <fallthrough>
}

void ContinueBumpThrow(void) {
  lda_abs(Squ1_SfxLenCounter); // check for second part of bump sound
  cmp_imm(0x6);
  if (!zero_flag) {
    DecJpFPS();
    return;
  }
  lda_imm(0xbb); // load second part directly
  dynamic_ram_write(SND_SQUARE1_REG + 1, a);
  DecJpFPS(); // <fallthrough>
}

void DecJpFPS(void) {
  // unconditional branch
  if (!zero_flag) {
    BranchToDecLength1();
    return;
  }
  Square1SfxHandler(); // <fallthrough>
}

void Square1SfxHandler(void) {
  ldy_zp(Square1SoundQueue); // check for sfx in queue
  if (zero_flag) { goto CheckSfx1Buffer; }
  ram[Square1SoundBuffer] = y; // if found, put in buffer
  if (neg_flag) { PlaySmallJump(); return; } // small jump
  lsr_zp(Square1SoundQueue);
  if (carry_flag) { PlayBigJump(); return; } // big jump
  lsr_zp(Square1SoundQueue);
  if (carry_flag) { PlayBump(); return; } // bump
  lsr_zp(Square1SoundQueue);
  if (carry_flag) { goto PlaySwimStomp; } // swim/stomp
  lsr_zp(Square1SoundQueue);
  if (carry_flag) { PlaySmackEnemy(); return; } // smack enemy
  lsr_zp(Square1SoundQueue);
  if (carry_flag) { PlayPipeDownInj(); return; } // pipedown/injury
  lsr_zp(Square1SoundQueue);
  if (carry_flag) { PlayFireballThrow(); return; } // fireball throw
  lsr_zp(Square1SoundQueue);
  if (carry_flag) { PlayFlagpoleSlide(); return; } // slide flagpole
  
CheckSfx1Buffer:
  lda_zp(Square1SoundBuffer); // check for sfx in buffer
  if (zero_flag) { goto ExS1H; } // if not found, exit sub
  if (neg_flag) { ContinueSndJump(); return; } // small mario jump
  lsr_acc();
  if (carry_flag) { ContinueSndJump(); return; } // big mario jump
  lsr_acc();
  if (carry_flag) { ContinueBumpThrow(); return; } // bump
  lsr_acc();
  if (carry_flag) { goto ContinueSwimStomp; } // swim/stomp
  lsr_acc();
  if (carry_flag) { ContinueSmackEnemy(); return; } // smack enemy
  lsr_acc();
  if (carry_flag) { ContinuePipeDownInj(); return; } // pipedown/injury
  lsr_acc();
  if (carry_flag) { ContinueBumpThrow(); return; } // fireball throw
  lsr_acc();
  if (carry_flag) { DecrementSfx1Length(); return; } // slide flagpole
  
ExS1H:
  return;
  
PlaySwimStomp:
  lda_imm(0xe); // store length of swim/stomp sound
  ram[Squ1_SfxLenCounter] = a;
  ldy_imm(0x9c); // store reg contents for swim/stomp sound
  ldx_imm(0x9e);
  lda_imm(0x26);
  PlaySqu1Sfx();
  
ContinueSwimStomp:
  ldy_abs(Squ1_SfxLenCounter); // look up reg contents in data section based on
  lda_absy(SwimStompEnvelopeData - 1); // length of sound left, used to control sound's
  apu_write(SND_SQUARE1_REG, a); // envelope
  cpy_imm(0x6);
  if (!zero_flag) { BranchToDecLength1(); return; }
  lda_imm(0x9e); // when the length counts down to a certain point, put this
  dynamic_ram_write(SND_SQUARE1_REG + 2, a); // directly into the LSB of square 1's frequency divider
  BranchToDecLength1(); // <fallthrough>
}

void BranchToDecLength1(void) {
  // unconditional branch (regardless of how we got here)
  if (!zero_flag) {
    DecrementSfx1Length();
    return;
  }
  PlaySmackEnemy(); // <fallthrough>
}

void PlaySmackEnemy(void) {
  lda_imm(0xe); // store length of smack enemy sound
  ldy_imm(0xcb);
  ldx_imm(0x9f);
  ram[Squ1_SfxLenCounter] = a;
  lda_imm(0x28); // store reg contents for smack enemy sound
  PlaySqu1Sfx();
  // unconditional branch
  if (!zero_flag) {
    DecrementSfx1Length();
    return;
  }
  ContinueSmackEnemy(); // <fallthrough>
}

void ContinueSmackEnemy(void) {
  ldy_abs(Squ1_SfxLenCounter); // check about halfway through
  cpy_imm(0x8);
  if (!zero_flag) { goto SmSpc; }
  lda_imm(0xa0); // if we're at the about-halfway point, make the second tone
  dynamic_ram_write(SND_SQUARE1_REG + 2, a); // in the smack enemy sound
  lda_imm(0x9f);
  if (!zero_flag) { goto SmTick; }
  
SmSpc:
  lda_imm(0x90); // this creates spaces in the sound, giving it its distinct noise
  
SmTick:
  apu_write(SND_SQUARE1_REG, a);
  DecrementSfx1Length(); // <fallthrough>
}

void DecrementSfx1Length(void) {
  dec_abs(Squ1_SfxLenCounter); // decrement length of sfx
  if (zero_flag) {
    StopSquare1Sfx(); // <fallthrough>
  }
}

void StopSquare1Sfx(void) {
  ldx_imm(0x0); // if end of sfx reached, clear buffer
  ram[0xf1] = x; // and stop making the sfx
  ldx_imm(0xe);
  apu_write(SND_MASTERCTRL_REG, x);
  ldx_imm(0xf);
  apu_write(SND_MASTERCTRL_REG, x);
}

void PlayPipeDownInj(void) {
  lda_imm(0x2f); // load length of pipedown sound
  ram[Squ1_SfxLenCounter] = a;
  ContinuePipeDownInj(); // <fallthrough>
}

void ContinuePipeDownInj(void) {
  lda_abs(Squ1_SfxLenCounter); // some bitwise logic, forces the regs
  lsr_acc(); // to be written to only during six specific times
  // during which d3 must be set and d1-0 must be clear
  if (!carry_flag) {
    lsr_acc();
    if (!carry_flag) {
      and_imm(0b00000010);
      if (!zero_flag) {
        ldy_imm(0x91); // and this is where it actually gets written in
        ldx_imm(0x9a);
        lda_imm(0x44);
        PlaySqu1Sfx();
      }
    }
  }
  // NoPDwnL:
  DecrementSfx1Length();
  // --------------------------------
}

void PlayCoinGrab(void) {
  lda_imm(0x35); // load length of coin grab sound
  ldx_imm(0x8d); // and part of reg contents
  if (!zero_flag) {
    CGrab_TTickRegL();
    return;
  }
  PlayTimerTick(); // <fallthrough>
}

void PlayTimerTick(void) {
  lda_imm(0x6); // load length of timer tick sound
  ldx_imm(0x98); // and part of reg contents
  CGrab_TTickRegL(); // <fallthrough>
}

void CGrab_TTickRegL(void) {
  ram[Squ2_SfxLenCounter] = a;
  ldy_imm(0x7f); // load the rest of reg contents
  lda_imm(0x42); // of coin grab and timer tick sound
  PlaySqu2Sfx();
  ContinueCGrabTTick();
}

void ContinueCGrabTTick(void) {
  lda_abs(Squ2_SfxLenCounter); // check for time to play second tone yet
  cmp_imm(0x30); // timer tick sound also executes this, not sure why
  if (zero_flag) {
    lda_imm(0x54); // if so, load the tone directly into the reg
    dynamic_ram_write(SND_SQUARE2_REG + 2, a);
  }
  // N2Tone:
  if (!zero_flag) {
    DecrementSfx2Length();
    return;
  }
  PlayBlast();
}

void DecrementSfx2Length(void) {
  dec_abs(Squ2_SfxLenCounter); // decrement length of sfx
  if (zero_flag) {
    EmptySfx2Buffer(); // <fallthrough>
  }
}

void EmptySfx2Buffer(void) {
  ldx_imm(0x0); // initialize square 2's sound effects buffer
  ram[Square2SoundBuffer] = x;
  StopSquare2Sfx(); // <fallthrough>
}

void StopSquare2Sfx(void) {
  ldx_imm(0xd); // stop playing the sfx
  apu_write(SND_MASTERCTRL_REG, x);
  ldx_imm(0xf);
  apu_write(SND_MASTERCTRL_REG, x);
}

void PlayBlast(void) {
  lda_imm(0x20); // load length of fireworks/gunfire sound
  ram[Squ2_SfxLenCounter] = a;
  ldy_imm(0x94); // load reg contents of fireworks/gunfire sound
  lda_imm(0x5e);
  if (!zero_flag) {
    SBlasJ();
    return;
  }
  ContinueBlast(); // <fallthrough>
}

void ContinueBlast(void) {
  lda_abs(Squ2_SfxLenCounter); // check for time to play second part
  cmp_imm(0x18);
  if (!zero_flag) {
    DecrementSfx2Length();
    return;
  }
  ldy_imm(0x93); // load second part reg contents then
  lda_imm(0x18);
  SBlasJ(); // <fallthrough>
}

void SBlasJ(void) {
  // unconditional branch to load rest of reg contents
  if (!zero_flag) {
    BlstSJp();
    return;
  }
  PlayPowerUpGrab(); // <fallthrough>
}

void PlayPowerUpGrab(void) {
  lda_imm(0x36); // load length of power-up grab sound
  ram[Squ2_SfxLenCounter] = a;
  ContinuePowerUpGrab(); // <fallthrough>
}

void ContinuePowerUpGrab(void) {
  lda_abs(Squ2_SfxLenCounter); // load frequency reg based on length left over
  lsr_acc(); // divide by 2
  // alter frequency every other frame
  if (carry_flag) {
    DecrementSfx2Length();
    return;
  }
  tay();
  lda_absy(PowerUpGrabFreqData - 1); // use length left over / 2 for frequency offset
  ldx_imm(0x5d); // store reg contents of power-up grab sound
  ldy_imm(0x7f);
  LoadSqu2Regs(); // <fallthrough>
}

void LoadSqu2Regs(void) {
  PlaySqu2Sfx();
  DecrementSfx2Length();
}

void JumpToDecLength2(void) {
  DecrementSfx2Length();
}

void BlstSJp(void) {
  if (!zero_flag) {
    PBFRegs();
    return;
  }
  ContinueBowserFall(); // <fallthrough>
}

void ContinueBowserFall(void) {
  lda_abs(Squ2_SfxLenCounter); // check for almost near the end
  cmp_imm(0x8);
  if (!zero_flag) {
    DecrementSfx2Length();
    return;
  }
  ldy_imm(0xa4); // if so, load the rest of reg contents for bowser defeat sound
  lda_imm(0x5a);
  PBFRegs(); // <fallthrough>
}

void PBFRegs(void) {
  ldx_imm(0x9f); // the fireworks/gunfire sound shares part of reg contents here
  EL_LRegs(); // <fallthrough>
}

void EL_LRegs(void) {
  // this is an unconditional branch outta here
  if (!zero_flag) {
    LoadSqu2Regs();
    return;
  }
  PlayExtraLife(); // <fallthrough>
}

void PlayExtraLife(void) {
  lda_imm(0x30); // load length of 1-up sound
  ram[Squ2_SfxLenCounter] = a;
  ContinueExtraLife(); // <fallthrough>
}

void ContinueExtraLife(void) {
  lda_abs(Squ2_SfxLenCounter);
  ldx_imm(0x3); // load new tones only every eight frames
  
DivLLoop:
  lsr_acc();
  // if any bits set here, branch to dec the length
  if (carry_flag) {
    JumpToDecLength2();
    return;
  }
  dex();
  if (!zero_flag) { goto DivLLoop; } // do this until all bits checked, if none set, continue
  tay();
  lda_absy(ExtraLifeFreqData - 1); // load our reg contents
  ldx_imm(0x82);
  ldy_imm(0x7f);
  // unconditional branch
  if (!zero_flag) {
    EL_LRegs();
    return;
  }
  PlayGrowPowerUp(); // <fallthrough>
}

void PlayGrowPowerUp(void) {
  lda_imm(0x10); // load length of power-up reveal sound
  if (!zero_flag) {
    GrowItemRegs();
    return;
  }
  PlayGrowVine(); // <fallthrough>
}

void PlayGrowVine(void) {
  lda_imm(0x20); // load length of vine grow sound
  GrowItemRegs(); // <fallthrough>
}

void GrowItemRegs(void) {
  ram[Squ2_SfxLenCounter] = a;
  lda_imm(0x7f); // load contents of reg for both sounds directly
  dynamic_ram_write(SND_SQUARE2_REG + 1, a);
  lda_imm(0x0); // start secondary counter for both sounds
  ram[Sfx_SecondaryCounter] = a;
  ContinueGrowItems(); // <fallthrough>
}

void ContinueGrowItems(void) {
  inc_abs(Sfx_SecondaryCounter); // increment secondary counter for both sounds
  lda_abs(Sfx_SecondaryCounter); // this sound doesn't decrement the usual counter
  lsr_acc(); // divide by 2 to get the offset
  tay();
  cpy_abs(Squ2_SfxLenCounter); // have we reached the end yet?
  // if so, branch to jump, and stop playing sounds
  if (!zero_flag) {
    lda_imm(0x9d); // load contents of other reg directly
    apu_write(SND_SQUARE2_REG, a);
    lda_absy(PUp_VGrow_FreqData); // use secondary counter / 2 as offset for frequency regs
    SetFreq_Squ2();
    return;
  }
  // StopGrowItems:
  EmptySfx2Buffer(); // branch to stop playing sounds
  // --------------------------------
}

void Square2SfxHandler(void) {
  lda_zp(Square2SoundBuffer); // special handling for the 1-up sound to keep it
  and_imm(Sfx_ExtraLife); // from being interrupted by other sounds on square 2
  if (!zero_flag) { ContinueExtraLife(); return; }
  ldy_zp(Square2SoundQueue); // check for sfx in queue
  if (zero_flag) { goto CheckSfx2Buffer; }
  ram[Square2SoundBuffer] = y; // if found, put in buffer and check for the following
  if (neg_flag) { PlayBowserFall(); return; } // bowser fall
  lsr_zp(Square2SoundQueue);
  if (carry_flag) { PlayCoinGrab(); return; } // coin grab
  lsr_zp(Square2SoundQueue);
  if (carry_flag) { PlayGrowPowerUp(); return; } // power-up reveal
  lsr_zp(Square2SoundQueue);
  if (carry_flag) { PlayGrowVine(); return; } // vine grow
  lsr_zp(Square2SoundQueue);
  if (carry_flag) { PlayBlast(); return; } // fireworks/gunfire
  lsr_zp(Square2SoundQueue);
  if (carry_flag) { PlayTimerTick(); return; } // timer tick
  lsr_zp(Square2SoundQueue);
  if (carry_flag) { PlayPowerUpGrab(); return; } // power-up grab
  lsr_zp(Square2SoundQueue);
  if (carry_flag) { PlayExtraLife(); return; } // 1-up
  
CheckSfx2Buffer:
  lda_zp(Square2SoundBuffer); // check for sfx in buffer
  if (zero_flag) { goto ExS2H; } // if not found, exit sub
  if (neg_flag) { ContinueBowserFall(); return; } // bowser fall
  lsr_acc();
  if (carry_flag) { goto Cont_CGrab_TTick; } // coin grab
  lsr_acc();
  if (carry_flag) { ContinueGrowItems(); return; } // power-up reveal
  lsr_acc();
  if (carry_flag) { ContinueGrowItems(); return; } // vine grow
  lsr_acc();
  if (carry_flag) { ContinueBlast(); return; } // fireworks/gunfire
  lsr_acc();
  if (carry_flag) { goto Cont_CGrab_TTick; } // timer tick
  lsr_acc();
  if (carry_flag) { ContinuePowerUpGrab(); return; } // power-up grab
  lsr_acc();
  if (carry_flag) { ContinueExtraLife(); return; } // 1-up
  
ExS2H:
  return;
  
Cont_CGrab_TTick:
  ContinueCGrabTTick();
}

void PlayBowserFall(void) {
  lda_imm(0x38); // load length of bowser defeat sound
  ram[Squ2_SfxLenCounter] = a;
  ldy_imm(0xc4); // load contents of reg for bowser defeat sound
  lda_imm(0x18);
  BlstSJp(); // <fallthrough>
}

void PlayBrickShatter(void) {
  lda_imm(0x20); // load length of brick shatter sound
  ram[Noise_SfxLenCounter] = a;
  ContinueBrickShatter(); // <fallthrough>
}

void ContinueBrickShatter(void) {
  lda_abs(Noise_SfxLenCounter);
  lsr_acc(); // divide by 2 and check for bit set to use offset
  if (!carry_flag) {
    DecrementSfx3Length();
    return;
  }
  tay();
  ldx_absy(BrickShatterFreqData); // load reg contents of brick shatter sound
  lda_absy(BrickShatterEnvData);
  PlayNoiseSfx(); // <fallthrough>
}

void PlayNoiseSfx(void) {
  apu_write(SND_NOISE_REG, a); // play the sfx
  dynamic_ram_write(SND_NOISE_REG + 2, x);
  lda_imm(0x18);
  dynamic_ram_write(SND_NOISE_REG + 3, a);
  DecrementSfx3Length(); // <fallthrough>
}

void DecrementSfx3Length(void) {
  dec_abs(Noise_SfxLenCounter); // decrement length of sfx
  if (zero_flag) {
    lda_imm(0xf0); // if done, stop playing the sfx
    apu_write(SND_NOISE_REG, a);
    lda_imm(0x0);
    ram[NoiseSoundBuffer] = a;
  }
}

void NoiseSfxHandler(void) {
  ldy_zp(NoiseSoundQueue); // check for sfx in queue
  if (zero_flag) { goto CheckNoiseBuffer; }
  ram[NoiseSoundBuffer] = y; // if found, put in buffer
  lsr_zp(NoiseSoundQueue);
  if (carry_flag) { PlayBrickShatter(); return; } // brick shatter
  lsr_zp(NoiseSoundQueue);
  if (carry_flag) { goto PlayBowserFlame; } // bowser flame
  
CheckNoiseBuffer:
  lda_zp(NoiseSoundBuffer); // check for sfx in buffer
  if (zero_flag) { goto ExNH; } // if not found, exit sub
  lsr_acc();
  if (carry_flag) { ContinueBrickShatter(); return; } // brick shatter
  lsr_acc();
  if (carry_flag) { goto ContinueBowserFlame; } // bowser flame
  
ExNH:
  return;
  
PlayBowserFlame:
  lda_imm(0x40); // load length of bowser flame sound
  ram[Noise_SfxLenCounter] = a;
  
ContinueBowserFlame:
  lda_abs(Noise_SfxLenCounter);
  lsr_acc();
  tay();
  ldx_imm(0xf); // load reg contents of bowser flame sound
  lda_absy(BowserFlameEnvData - 1);
  if (!zero_flag) { PlayNoiseSfx(); return; } // unconditional branch here
  // --------------------------------
  ContinueMusic(); // <fallthrough>
}

void ContinueMusic(void) {
  HandleSquare2Music(); // if we have music, start with square 2 channel
}

void MusicHandler(void) {
  lda_zp(EventMusicQueue); // check event music queue
  if (!zero_flag) {
    LoadEventMusic();
    return;
  }
  lda_zp(AreaMusicQueue); // check area music queue
  if (!zero_flag) {
    LoadAreaMusic();
    return;
  }
  lda_abs(EventMusicBuffer); // check both buffers
  ora_zp(AreaMusicBuffer);
  if (!zero_flag) {
    ContinueMusic();
    return;
  }
}

void LoadEventMusic(void) {
  ram[EventMusicBuffer] = a; // copy event music queue contents to buffer
  cmp_imm(DeathMusic); // is it death music?
  // if not, jump elsewhere
  if (zero_flag) {
    StopSquare1Sfx(); // stop sfx in square 1 and 2
    StopSquare2Sfx(); // but clear only square 1's sfx buffer
  }
  // NoStopSfx:
  ldx_zp(AreaMusicBuffer);
  ram[AreaMusicBuffer_Alt] = x; // save current area music buffer to be re-obtained later
  ldy_imm(0x0);
  ram[NoteLengthTblAdder] = y; // default value for additional length byte offset
  ram[AreaMusicBuffer] = y; // clear area music buffer
  cmp_imm(TimeRunningOutMusic); // is it time running out music?
  if (!zero_flag) {
    FindEventMusicHeader();
    return;
  }
  ldx_imm(0x8); // load offset to be added to length byte of header
  ram[NoteLengthTblAdder] = x;
  // unconditional branch
  if (!zero_flag) {
    FindEventMusicHeader();
    return;
  }
  LoadAreaMusic(); // <fallthrough>
}

void LoadAreaMusic(void) {
  cmp_imm(0x4); // is it underground music?
  // no, do not stop square 1 sfx
  if (zero_flag) {
    StopSquare1Sfx();
  }
  // NoStop1:
  ldy_imm(0x10); // start counter used only by ground level music
  GMLoopB(); // <fallthrough>
}

void GMLoopB(void) {
  ram[GroundMusicHeaderOfs] = y;
  HandleAreaMusicLoopB(); // <fallthrough>
}

void HandleAreaMusicLoopB(void) {
  ldy_imm(0x0); // clear event music buffer
  ram[EventMusicBuffer] = y;
  ram[AreaMusicBuffer] = a; // copy area music queue contents to buffer
  cmp_imm(0x1); // is it ground level music?
  if (zero_flag) {
    inc_abs(GroundMusicHeaderOfs); // increment but only if playing ground level music
    ldy_abs(GroundMusicHeaderOfs); // is it time to loopback ground level music?
    cpy_imm(0x32);
    // branch ahead with alternate offset
    if (!zero_flag) {
      LoadHeader();
      return;
    }
    ldy_imm(0x11);
    // unconditional branch
    if (!zero_flag) {
      GMLoopB();
      return;
    }
  }
  // FindAreaMusicHeader:
  ldy_imm(0x8); // load Y for offset of area music
  ram[MusicOffset_Square2] = y; // residual instruction here
  FindEventMusicHeader(); // <fallthrough>
}

void FindEventMusicHeader(void) {
  iny(); // increment Y pointer based on previously loaded queue contents
  lsr_acc(); // bit shift and increment until we find a set bit for music
  if (!carry_flag) {
    FindEventMusicHeader();
    return;
  }
  LoadHeader(); // <fallthrough>
}

void LoadHeader(void) {
  lda_absy(MusicHeaderOffsetData); // load offset for header
  tay();
  lda_absy(MusicHeaderData); // now load the header
  ram[NoteLenLookupTblOfs] = a;
  lda_absy(MusicHeaderData + 1);
  ram[MusicDataLow] = a;
  lda_absy(MusicHeaderData + 2);
  ram[MusicDataHigh] = a;
  lda_absy(MusicHeaderData + 3);
  ram[MusicOffset_Triangle] = a;
  lda_absy(MusicHeaderData + 4);
  ram[MusicOffset_Square1] = a;
  lda_absy(MusicHeaderData + 5);
  ram[MusicOffset_Noise] = a;
  ram[NoiseDataLoopbackOfs] = a;
  lda_imm(0x1); // initialize music note counters
  ram[Squ2_NoteLenCounter] = a;
  ram[Squ1_NoteLenCounter] = a;
  ram[Tri_NoteLenCounter] = a;
  ram[Noise_BeatLenCounter] = a;
  lda_imm(0x0); // initialize music data offset for square 2
  ram[MusicOffset_Square2] = a;
  ram[AltRegContentFlag] = a; // initialize alternate control reg data used by square 1
  lda_imm(0xb); // disable triangle channel and reenable it
  apu_write(SND_MASTERCTRL_REG, a);
  lda_imm(0xf);
  apu_write(SND_MASTERCTRL_REG, a);
  HandleSquare2Music(); // <fallthrough>
}

void HandleSquare2Music(void) {
  dec_abs(Squ2_NoteLenCounter); // decrement square 2 note length
  if (!zero_flag) { goto MiscSqu2MusicTasks; } // is it time for more data?  if not, branch to end tasks
  ldy_zp(MusicOffset_Square2); // increment square 2 music offset and fetch data
  inc_zp(MusicOffset_Square2);
  lda_indy(MusicData);
  if (zero_flag) { goto EndOfMusicData; } // if zero, the data is a null terminator
  if (!neg_flag) { goto Squ2NoteHandler; } // if non-negative, data is a note
  if (!zero_flag) { goto Squ2LengthHandler; } // otherwise it is length data
  
EndOfMusicData:
  lda_abs(EventMusicBuffer); // check secondary buffer for time running out music
  cmp_imm(TimeRunningOutMusic);
  if (!zero_flag) { goto NotTRO; }
  lda_abs(AreaMusicBuffer_Alt); // load previously saved contents of primary buffer
  if (!zero_flag) { goto MusicLoopBack; } // and start playing the song again if there is one
  
NotTRO:
  and_imm(VictoryMusic); // check for victory music (the only secondary that loops)
  if (!zero_flag) { goto VictoryMLoopBack; }
  lda_zp(AreaMusicBuffer); // check primary buffer for any music except pipe intro
  and_imm(0b01011111);
  if (!zero_flag) { goto MusicLoopBack; } // if any area music except pipe intro, music loops
  lda_imm(0x0); // clear primary and secondary buffers and initialize
  ram[AreaMusicBuffer] = a; // control regs of square and triangle channels
  ram[EventMusicBuffer] = a;
  apu_write(SND_TRIANGLE_REG, a);
  lda_imm(0x90);
  apu_write(SND_SQUARE1_REG, a);
  apu_write(SND_SQUARE2_REG, a);
  return;
  
MusicLoopBack:
  HandleAreaMusicLoopB();
  return;
  
VictoryMLoopBack:
  LoadEventMusic();
  return;
  
Squ2LengthHandler:
  ProcessLengthData(); // store length of note
  ram[Squ2_NoteLenBuffer] = a;
  ldy_zp(MusicOffset_Square2); // fetch another byte (MUST NOT BE LENGTH BYTE!)
  inc_zp(MusicOffset_Square2);
  lda_indy(MusicData);
  
Squ2NoteHandler:
  ldx_zp(Square2SoundBuffer); // is there a sound playing on this channel?
  if (!zero_flag) { goto SkipFqL1; }
  SetFreq_Squ2(); // no, then play the note
  if (zero_flag) { goto Rest; } // check to see if note is rest
  LoadControlRegs(); // if not, load control regs for square 2
  
Rest:
  ram[Squ2_EnvelopeDataCtrl] = a; // save contents of A
  Dump_Sq2_Regs(); // dump X and Y into square 2 control regs
  
SkipFqL1:
  lda_abs(Squ2_NoteLenBuffer); // save length in square 2 note counter
  ram[Squ2_NoteLenCounter] = a;
  
MiscSqu2MusicTasks:
  lda_zp(Square2SoundBuffer); // is there a sound playing on square 2?
  if (!zero_flag) { goto HandleSquare1Music; }
  lda_abs(EventMusicBuffer); // check for death music or d4 set on secondary buffer
  and_imm(0b10010001); // note that regs for death music or d4 are loaded by default
  if (!zero_flag) { goto HandleSquare1Music; }
  ldy_abs(Squ2_EnvelopeDataCtrl); // check for contents saved from LoadControlRegs
  if (zero_flag) { goto NoDecEnv1; }
  dec_abs(Squ2_EnvelopeDataCtrl); // decrement unless already zero
  
NoDecEnv1:
  LoadEnvelopeData(); // do a load of envelope data to replace default
  apu_write(SND_SQUARE2_REG, a); // based on offset set by first load unless playing
  ldx_imm(0x7f); // death music or d4 set on secondary buffer
  dynamic_ram_write(SND_SQUARE2_REG + 1, x);
  
HandleSquare1Music:
  ldy_zp(MusicOffset_Square1); // is there a nonzero offset here?
  if (zero_flag) { goto HandleTriangleMusic; } // if not, skip ahead to the triangle channel
  dec_abs(Squ1_NoteLenCounter); // decrement square 1 note length
  if (!zero_flag) { goto MiscSqu1MusicTasks; } // is it time for more data?
  
FetchSqu1MusicData:
  ldy_zp(MusicOffset_Square1); // increment square 1 music offset and fetch data
  inc_zp(MusicOffset_Square1);
  lda_indy(MusicData);
  if (!zero_flag) { goto Squ1NoteHandler; } // if nonzero, then skip this part
  lda_imm(0x83);
  apu_write(SND_SQUARE1_REG, a); // store some data into control regs for square 1
  lda_imm(0x94); // and fetch another byte of data, used to give
  dynamic_ram_write(SND_SQUARE1_REG + 1, a); // death music its unique sound
  ram[AltRegContentFlag] = a;
  if (!zero_flag) { goto FetchSqu1MusicData; } // unconditional branch
  
Squ1NoteHandler:
  AlternateLengthHandler();
  ram[Squ1_NoteLenCounter] = a; // save contents of A in square 1 note counter
  ldy_zp(Square1SoundBuffer); // is there a sound playing on square 1?
  if (!zero_flag) { goto HandleTriangleMusic; }
  txa();
  and_imm(0b00111110); // change saved data to appropriate note format
  SetFreq_Squ1(); // play the note
  if (zero_flag) { goto SkipCtrlL; }
  LoadControlRegs();
  
SkipCtrlL:
  ram[Squ1_EnvelopeDataCtrl] = a; // save envelope offset
  Dump_Squ1_Regs();
  
MiscSqu1MusicTasks:
  lda_zp(Square1SoundBuffer); // is there a sound playing on square 1?
  if (!zero_flag) { goto HandleTriangleMusic; }
  lda_abs(EventMusicBuffer); // check for death music or d4 set on secondary buffer
  and_imm(0b10010001);
  if (!zero_flag) { goto DeathMAltReg; }
  ldy_abs(Squ1_EnvelopeDataCtrl); // check saved envelope offset
  if (zero_flag) { goto NoDecEnv2; }
  dec_abs(Squ1_EnvelopeDataCtrl); // decrement unless already zero
  
NoDecEnv2:
  LoadEnvelopeData(); // do a load of envelope data
  apu_write(SND_SQUARE1_REG, a); // based on offset set by first load
  
DeathMAltReg:
  lda_abs(AltRegContentFlag); // check for alternate control reg data
  if (!zero_flag) { goto DoAltLoad; }
  lda_imm(0x7f); // load this value if zero, the alternate value
  
DoAltLoad:
  dynamic_ram_write(SND_SQUARE1_REG + 1, a); // if nonzero, and let's move on
  
HandleTriangleMusic:
  lda_zp(MusicOffset_Triangle);
  dec_abs(Tri_NoteLenCounter); // decrement triangle note length
  if (!zero_flag) { goto HandleNoiseMusic; } // is it time for more data?
  ldy_zp(MusicOffset_Triangle); // increment square 1 music offset and fetch data
  inc_zp(MusicOffset_Triangle);
  lda_indy(MusicData);
  if (zero_flag) { goto LoadTriCtrlReg; } // if zero, skip all this and move on to noise
  if (!neg_flag) { goto TriNoteHandler; } // if non-negative, data is note
  ProcessLengthData(); // otherwise, it is length data
  ram[Tri_NoteLenBuffer] = a; // save contents of A
  lda_imm(0x1f);
  apu_write(SND_TRIANGLE_REG, a); // load some default data for triangle control reg
  ldy_zp(MusicOffset_Triangle); // fetch another byte
  inc_zp(MusicOffset_Triangle);
  lda_indy(MusicData);
  if (zero_flag) { goto LoadTriCtrlReg; } // check once more for nonzero data
  
TriNoteHandler:
  SetFreq_Tri();
  ldx_abs(Tri_NoteLenBuffer); // save length in triangle note counter
  ram[Tri_NoteLenCounter] = x;
  lda_abs(EventMusicBuffer);
  and_imm(0b01101110); // check for death music or d4 set on secondary buffer
  if (!zero_flag) { goto NotDOrD4; } // if playing any other secondary, skip primary buffer check
  lda_zp(AreaMusicBuffer); // check primary buffer for water or castle level music
  and_imm(0b00001010);
  if (zero_flag) { goto HandleNoiseMusic; } // if playing any other primary, or death or d4, go on to noise routine
  
NotDOrD4:
  txa(); // if playing water or castle music or any secondary
  cmp_imm(0x12); // besides death music or d4 set, check length of note
  if (carry_flag) { goto LongN; }
  lda_abs(EventMusicBuffer); // check for win castle music again if not playing a long note
  and_imm(EndOfCastleMusic);
  if (zero_flag) { goto MediN; }
  lda_imm(0xf); // load value $0f if playing the win castle music and playing a short
  if (!zero_flag) { goto LoadTriCtrlReg; } // note, load value $1f if playing water or castle level music or any
  
MediN:
  lda_imm(0x1f); // secondary besides death and d4 except win castle or win castle and playing
  if (!zero_flag) { goto LoadTriCtrlReg; } // a short note, and load value $ff if playing a long note on water, castle
  
LongN:
  lda_imm(0xff); // or any secondary (including win castle) except death and d4
  
LoadTriCtrlReg:
  apu_write(SND_TRIANGLE_REG, a); // save final contents of A into control reg for triangle
  
HandleNoiseMusic:
  lda_zp(AreaMusicBuffer); // check if playing underground or castle music
  and_imm(0b11110011);
  if (zero_flag) { return; } // if so, skip the noise routine
  dec_abs(Noise_BeatLenCounter); // decrement noise beat length
  if (!zero_flag) { return; } // is it time for more data?
  
FetchNoiseBeatData:
  ldy_abs(MusicOffset_Noise); // increment noise beat offset and fetch data
  inc_abs(MusicOffset_Noise);
  lda_indy(MusicData); // get noise beat data, if nonzero, branch to handle
  if (!zero_flag) { goto NoiseBeatHandler; }
  lda_abs(NoiseDataLoopbackOfs); // if data is zero, reload original noise beat offset
  ram[MusicOffset_Noise] = a; // and loopback next time around
  if (!zero_flag) { goto FetchNoiseBeatData; } // unconditional branch
  
NoiseBeatHandler:
  AlternateLengthHandler();
  ram[Noise_BeatLenCounter] = a; // store length in noise beat counter
  txa();
  and_imm(0b00111110); // reload data and erase length bits
  if (zero_flag) { goto SilentBeat; } // if no beat data, silence
  cmp_imm(0x30); // check the beat data and play the appropriate
  if (zero_flag) { goto LongBeat; } // noise accordingly
  cmp_imm(0x20);
  if (zero_flag) { goto StrongBeat; }
  and_imm(0b00010000);
  if (zero_flag) { goto SilentBeat; }
  lda_imm(0x1c); // short beat data
  ldx_imm(0x3);
  ldy_imm(0x18);
  if (!zero_flag) { goto PlayBeat; }
  
StrongBeat:
  lda_imm(0x1c); // strong beat data
  ldx_imm(0xc);
  ldy_imm(0x18);
  if (!zero_flag) { goto PlayBeat; }
  
LongBeat:
  lda_imm(0x1c); // long beat data
  ldx_imm(0x3);
  ldy_imm(0x58);
  if (!zero_flag) { goto PlayBeat; }
  
SilentBeat:
  lda_imm(0x10); // silence
  
PlayBeat:
  apu_write(SND_NOISE_REG, a); // load beat data into noise regs
  dynamic_ram_write(SND_NOISE_REG + 2, x);
  dynamic_ram_write(SND_NOISE_REG + 3, y);
}

void AlternateLengthHandler(void) {
  tax(); // save a copy of original byte into X
  ror_acc(); // save LSB from original byte into carry
  txa(); // reload original byte and rotate three times
  rol_acc(); // turning xx00000x into 00000xxx, with the
  rol_acc(); // bit in carry as the MSB here
  rol_acc();
  ProcessLengthData(); // <fallthrough>
}

void ProcessLengthData(void) {
  and_imm(0b00000111); // clear all but the three LSBs
  carry_flag = false;
  adc_zp(0xf0); // add offset loaded from first header byte
  adc_abs(NoteLengthTblAdder); // add extra if time running out music
  tay();
  lda_absy(MusicLengthLookupTbl); // load length
}

void LoadControlRegs(void) {
  lda_abs(EventMusicBuffer); // check secondary buffer for win castle music
  and_imm(EndOfCastleMusic);
  if (zero_flag) { goto NotECstlM; }
  lda_imm(0x4); // this value is only used for win castle music
  if (!zero_flag) { goto AllMus; } // unconditional branch
  
NotECstlM:
  lda_zp(AreaMusicBuffer);
  and_imm(0b01111101); // check primary buffer for water music
  if (zero_flag) { goto WaterMus; }
  lda_imm(0x8); // this is the default value for all other music
  if (!zero_flag) { goto AllMus; }
  
WaterMus:
  lda_imm(0x28); // this value is used for water music and all other event music
  
AllMus:
  ldx_imm(0x82); // load contents of other sound regs for square 2
  ldy_imm(0x7f);
}

void LoadEnvelopeData(void) {
  lda_abs(EventMusicBuffer); // check secondary buffer for win castle music
  and_imm(EndOfCastleMusic);
  if (!zero_flag) {
    lda_absy(EndOfCastleMusicEnvData); // load data from offset for win castle music
    return;
  }
  // LoadUsualEnvData:
  lda_zp(AreaMusicBuffer); // check primary buffer for water music
  and_imm(0b01111101);
  if (!zero_flag) {
    lda_absy(AreaMusicEnvData); // load default data from offset for all other music
    return;
  }
  // LoadWaterEventMusEnvData:
  lda_absy(WaterEventMusEnvData); // load data from offset for water music and all other event music
  // --------------------------------
}

