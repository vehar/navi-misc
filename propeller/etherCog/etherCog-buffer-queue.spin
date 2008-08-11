{{                                              

 etherCog-buffer-queue
──────────────────────────────────────────────────────────────────

This is a utility object which manages a FIFO buffer of allocatable
buffer descriptors. Buffer queues can be initialized from a fixed
chunk of hub memory, or from an initial linked list of buffer
descriptors.

┌───────────────────────────────────┐
│ Copyright (c) 2008 Micah Dowty    │               
│ See end of file for terms of use. │
└───────────────────────────────────┘

}}

CON
  BD_SIZE = 2             ' Buffer descriptor size, in longs
  BUF_BYTESWAP = 1        ' Pointer to byte-swapped buffer
  
VAR
  word  head, tail

PUB empty
  '' Empty the queue.
  head~

PUB initFromList(l)
  '' Initialize the buffer queue from an existing linked list
  '' of buffer descriptors. The provided linked list must not
  '' be circular.

  empty
  putN(l)
  
PUB initFromMem(numBuffers, bufferSize, bufferMem, bdMem) : p
  '' Initialize the buffer queue to use a static chunk of hub
  '' memory, preallocated in your object's VAR or DAT section.
  ''
  '' bufferSize, bufferMem, and bdMem must all be multiples
  '' of 4 bytes. bufferMem must point to (numBuffers * bufferSize)
  '' bytes, and bdMem points to (numBuffers * BD_SIZE) longs.
  ''
  '' The resulting buffer list will start at the beginning of
  '' bufferMem and increase linearly to the end of the buffer.
  ''
  '' The low bits of packetBuf may contain optional flags, like
  '' BUF_BYTESWAP. This flag enables automatic endian conversion
  '' in the driver cog, and increases transfer speeds slightly.

  empty
  repeat numBuffers
    LONG[bdMem]~
    LONG[bdMem + 4] := (bufferSize << 16) | bufferMem
    put(bdMem)

    bufferMem += bufferSize
    bdMem += constant(BD_SIZE * 4)

PUB put(bd)
  '' Append a single buffer descriptor. The descriptor's "next"
  '' pointer is ignored, and always overwritten.

  WORD[bd]~
  if head
    WORD[tail] := bd
    tail := bd
  else
    head := tail := bd

PUB putN(l) | nextPtr
  '' Append any number of buffer descriptors. 'l' points to
  '' a linked list of buffer descriptors. The list must not be
  '' circular.

  if head
    WORD[tail] := l
  else
    head := tail := l

  repeat while nextPtr := WORD[tail]
    tail := nextPtr

PUB get : bd
  '' Remove a single buffer descriptor from the head of the
  '' queue, and return it. If no buffers are available, returns
  '' zero. The returned buffer will have its 'next' pointer set
  '' to zero.

  if head
    head := WORD[bd := head]
    WORD[bd] := 0
  else
    bd~

PUB getN(n) : l | t
  '' Remove any number of buffer descriptors from the head of
  '' the queue, and return them as a linked list. If we run
  '' out of buffers, returns a short list.

  if n > 0 and l := head
    repeat n
      head := WORD[t := head]
    WORD[t] := 0
  else
    l~  
    

DAT

{{
┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                                   TERMS OF USE: MIT License                                                  │                                                            
├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation    │ 
│files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,    │
│modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software│
│is furnished to do so, subject to the following conditions:                                                                   │
│                                                                                                                              │
│The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.│
│                                                                                                                              │
│THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE          │
│WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR         │
│COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,   │
│ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                         │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
}}   