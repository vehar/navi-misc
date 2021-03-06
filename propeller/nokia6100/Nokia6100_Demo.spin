{{

Demonstration of the Nokia 6100 LCD driver.

Micah Elizabeth Scott <micah@navi.cx>

}}

OBJ
  lcd : "Nokia6100"
  num : "Simple_Numbers"

PUB main
  'dira[7]~~
  dira[8]~~      
  dira[9]~~
  waitcnt(cnt + clkfreq/2)  
  lcd.start(10, 11, 12, 13, lcd#EPSON)
  speedTests
  drawDemoScreen


PUB speedTests | i, c

  ' Concentric boxes
  c := lcd#WHITE
  repeat i from 0 to 16
    lcd.color(c)
    lcd.box(i*4, i*4, lcd#WIDTH-i*8, lcd#height-i*8)
    c ^= lcd#WHITE

  ' Random boxes
  repeat 300
    lcd.color(?i)
    lcd.box((?i) & 63, (?i) & 63, (?i) & 63, (?i) & 63) 

  ' Random sprites
  repeat 300
    lcd.image((?i) & 127, (?i) & 127, SPRITE_WIDTH, SPRITE_HEIGHT, @sprite_data)

  ' Text
  lcd.background(lcd#BLACK)
  lcd.color(lcd#WHITE)
  repeat 300
    lcd.text((?i) & 127, (?i) & 127, string("Poing"))


PUB drawDemoScreen | i, frame, frameTimer
    
  ' A big string that wraps around a few times.
  ' This draws with an 8x16 version of the built-in font,
  ' so all the funky characters work fine.
  lcd.background(lcd#BLUE)  
  lcd.color(lcd#YELLOW)
  lcd.clear
  lcd.text(0, 0, @test_string)

  ' White background for counter and sprite
  lcd.background(lcd#WHITE)
  lcd.color(lcd#WHITE)
  lcd.box(20, 95, lcd#WIDTH - 20, lcd#HEIGHT - 95)
  lcd.color(lcd#BLACK)
  
  ' A sprite that stress-tests the alignment of image()
  lcd.image(0, 100, 9, 15, @align_test)
  
  i~
  frame := @sprite_frames
  frameTimer := cnt
  
  repeat                 
    ' Count as fast as we can...
    lcd.text(25, 100, num.dec(++i))

    ' Animated sprite
    lcd.image(100, 100, SPRITE_WIDTH, SPRITE_HEIGHT, @sprite_data + WORD[frame])

    ' Advance the sprite at a fixed frame rate
    if (cnt - frameTimer) > 0
      frame += 2
      frameTimer += clkfreq / SPRITE_FRAME_RATE
      if not WORD[frame]
        frame := @sprite_frames

CON

SPRITE_WIDTH      = 25
SPRITE_HEIGHT     = 24
SPRITE_FRAME_SIZE = 900
SPRITE_FRAME_RATE = 5

DAT

sprite_frames word SPRITE_FRAME_SIZE * 0
              word SPRITE_FRAME_SIZE * 1
              word SPRITE_FRAME_SIZE * 2
              word SPRITE_FRAME_SIZE * 3
              word SPRITE_FRAME_SIZE * 2
              word SPRITE_FRAME_SIZE * 1
              word 0

              ' Four frames of uncompressed sprite data, concatenated together
sprite_data   file "kirby_sprite.bin"

align_test    file "align_test.bin"

test_string   byte "Hello World! <3 "
              byte "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
              byte "    ◀▶‣• ←→↑↓", 0
