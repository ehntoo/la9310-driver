granita driver findings:

Continuing on the "what are these developers doing" train for the granita innards:
- The "handle" that's the first arg to most functions gets saved to a global static variable, and is almost always subsequently accessed from the global static, completely ruining any reentrancy safety. Things will absolutely fall flat on their face if we call this from multiple threads. I'll have to think a little more about whether this is really an issue with the current kernel module design, though. I suspect it probably is with multiple granita daughterboards.
- If we'd like 4 MiB of kernel memory back, there's a 4MiB `DataBuf` symbol in .bss that does not seem to be referenced anywhere in the library and could be removed
- SiAPISetSynth copies the ~1KiB pll table from `.rodata` to a stack-allocated buffer for some reason rather than just indexing into the `.rodata` copy. Maybe someone didn't understand C's assign-by-value semantics for structs?
- There are 10 and 20 millisecond sleeps on several codepaths - setting RX gain, setting TX gain, a few others... There's even a 100msec sleep in the SiAPITXDC path
- There's most of a CLI implementation in here for reading/writing/tweaking registers. Not a whole lot of direct help in determining register names, though.


SPI transfers:
6 byte transactions
starts with 2 bytes big-endian register address, high bit set means transaction is a write
4 bytes of data, big-endian??? hard to tell for sure with the nonsense they're doing.

spi read/write functions in library stick high byte of address in byte 0 of buffer, low byte of address in byte index 3 of buffer.

I2C addresses seem to be 'L', 'M', 'N', 'O' for first 4 functional blocks:
0x4c, 0x4d, 0x4e, 0x4f

chan_num indexes which i2c address is used

address passed into fullpath:
((chan_num_i2c << 8) | (func_block_id << 4) & 0xFFF0) | reg_num

0x15[9] and 0x15[4] are cleared during a number of operations and then re-asserted at completion:
- SiAPIRXGAIN
- SiAPIPowerUpRx init
- SiAPIRXOBS
- SiAPIRXTXON
- SiAPIRXG

Looks like 0x23[26] and 0x23[4] may serve a similar purpose for TX?

So, SiAPIRX_BW writes the registers:
0x10[30:0]
0x11[30:0] // same contents as 0x10
0x14[5:0]
0x15[22:19]
0x15[16:13]

Si_Write_FullAddr is called in/with:
- SiAPIWriteDefaults
- SiAPISPITest
- SiAPISetSynth, reg 0x55
- SiAPICHTX, reg 0x2?
- SiAPICHRX, reg 0x1?
- SiAPICHRF, reg 0x0?

Si_BitRead is called in/with:
- SiAPIPowerDnRX    0x57[13:12], 0x57[11:10], 0x57[9:8], 0x57[5:4], 0x57[7:6], 0x57[3:2], 0x57[1:0], 0x58[31:28], 0x58[23:20], 0x58[15:12], 0x58[7:4]
- SiAPIPowerDnTX    0x57[13:12], 0x57[11:10], 0x57[9:8], 0x57[7:6], 0x57[5:4], 0x57[3:2], 0x57[1:0], 0x58[27:24], 0x58[19:16], 0x58[11:8], 0x58[3:0]
- SiAPIAFC          0x5c[30], 0x5c[11:6]
- SiAPIPowerUpRX    0x57[13:0], 0x58[31:0]
- SiAPIRXOBS        0x57[13:0], 0x58[31:0]
- SiAPIRSSI         0x1e[15:0]
- SiAPILoopback     0x57[13:0], 0x58[31:0]
- SiAPIPowerUpTX    0x57[13:0], 0x58[11:0]

Si_BitWrite is called in/with:
- SiAPIRXOBG        0x57[17:14], 0x15[9], 0x15[4], 0x05[12:11], 0x05[10:9], 0x05[8:4], 0x05[3:0], 0x05[23:16], 0x13[21:15], 0x13[14:8], 0x13[7:0]
- SiAPIPowerDnRX    0x57[13:12], 0x57[11:10], 0x57[9:8], 0x57[5:4], 0x57[7:6], 0x57[3:2], 0x57[1:0], 0x58[31:28], 0x58[23:20], 0x58[15:12], 0x58[7:4], 0x15[16:10], 0x03[25:18]
- SiAPIPowerDnTX    0x57[13:12], 0x57[11:10], 0x57[9:8], 0x57[7:6], 0x57[5:4], 0x57[3:2], 0x57[1:0], 0x58[27:24], 0x58[19:16], 0x58[11:8], 0x58[3:0], 0x22[10:5], 0x24[28:25]
- SiAPIAFC          0x56[31], 0x52[0], 0x52[1], 0x53[5:0], 0x53[13:8], 0x52[4:3]
- SiAPISetSynth     0x25[15:8], 0x25[3:0], 0x24[24:19], 0x0{2,3}?[3:1], 0x25[{13,14,15}], 0x57[31:28], 0x57[27:19], 0x51[21:19], 0x51[18:16], 0x56[27:16], 0x56[30], 0x51[{21:0,15:0}]
                    0x50[15:8], 0x50[7:0], 0x53[20:19], 0x50[31:28], 0x50[27:24], 0x53[18], 0x56[15], 0x56[14], 0x51[26:23]
- SiAPIRXGAIN       0x57[17:14], 0x15[9], 0x15[4], 0x05[12:11], 0x05[10:9], 0x05[8:4], 0x05[3:0], 0x05[23:16], 0x13[21:15], 0x13[14:8], 0x13[7:0]
- SiAPITXGAIN       0x57[17:14], 0x23[26], 0x23[4], 0x25[7:4], 0x20[6:0], 0x21[6:0]
- SiAPIRX_BW        0x10[29:24], 0x11[29:24], 0x10[23:18], 0x11[23:18], 0x10[11:6], 0x11[11:6], 0x10[17:12], 0x11[17:12], 0x10[5:0], 0x11[5:0], 0x14[5:0], 0x15[22:19], 0x15[16:13]
- SiAPIPowerUpRX    0x57[17:14], 0x15[9], 0x15[4], 0x53[24], 0x57[13:0], 0x58[31:0], 0x58[{27:24},{19:16},{11:8},{3:0}], 0x0{1,3,5,7}[17], 0x03[{7:6,5:4,3:2,1:0}],
                    0x10[26], 0x14[23], 0x14[15], 0x14[12], 0x14[{30,28,26,24}], 0x15[16:0], 0x22[10:5], 0x24[28:25]
- SiAPIRXOBS        0x57[17:14], 0x15[9], 0x15[4], 0x05[12:11], 0x05[10:9], 0x05[8:4], 0x05[3:0], 0x05[23:16], 0x13[21:15], 0x13[14:8], 0x13[7:0], 0x53[24], 0x57[13:0], 0x58[31:0],
                    0x58[27:24], 0x58[19:16], 0x58[11:8], 0x58[3:0], 0x0{1,2,3,4}[17], 0x03[??], 0x10[26], 0x14[23], 0x14[15], 0x14[12], 0x14[{30,28,26,24}], 0x15[16:0], 0x22[10:5],
                    0x24[28:25], 0x15[9], 0x15[4], 0x57[{17,16,15,14}]
- SiAPITX_BW        0x20[23:18], 0x21[23:18], 0x20[14:11], 0x21[14:11], 0x20[10:7], 0x21[10:7]
- SiAPIRXTXON       0x57[17:14], 0x23[26], 0x23[4], 0x15[9], 0x15[4], 0x53[24], 0x57[13:0], 0x58[31:0], 0x22[10:5], 0x22[0], 0x24[28:25], 0x24[0], 0x25[20], 0x0{1,2,3,4}[17],
                    0x03[??], 0x10[26], 0x14[23], 0x14[15], 0x14[12], 0x14[{30,28,26,24}], 0x15[16:0], 0x23[26], 0x23[4], 0x15[9], 0x15[4], 0x57[17:14]
- SiAPIRSSI         0x57[{17,16,15,14}], 0x12[25:22], 0x12[21:20], 0x12[19:17], 0x12[16], 0x12[15:0], 0x12[25], 0x12[23], 0x12[16], 0x57[{17,16,15,14}]
- SiAPIRXTXON4FDD   (skipping for now, since I don't care about this I think?)
- SiAPILoopback     0x57[17:14], 0x23[26], 0x23[4], 0x15[9], 0x15[4], 0x51[22], 0x53[24], 0x57[29:28], 0x53[24], 0x57[13:0], 0x58[31:0], 0x0??[17], 0x03[??], 0x22[10:5], 0x22[0],
                    0x24[{28,27,26,25}], 0x24[0], 0x25[20], 0x25[{15,14,13,12}], 0x57[13:0], 0x58[31:0], 0x0??[17], 0x03[??], 0x10[26], 0x14[23], 0x14[15], 0x14[12], 0x14[{30,28,26,24}],
                    0x15[16:0], 0x23[26], 0x23[4], 0x15[9], 0x15[4], 0x57[17:14], 0x57[17:14]
- SiAPITXDC         0x57[{17,16,15,14}], 0x23[26], 0x23[4], 0x22[19:12], 0x22[28:21], 0x23[26], 0x23[4], 0x57[{17,16,15,14}]
- SiAPIRXG          0x57[{17,16,15,14}], 0x15[9], 0x15[4], 0x05[12:11], 0x05[10:9], 0x05[8:4], 0x05[3:0], 0x05[23:16], 0x13[21:15], 0x13[14:8], 0x13[7:4], 0x13[3:0], 0x15[9], 0x15[4], 0x57[{17,16,15,14}]
- SiAPIPowerDn      0x51[22], 0x51[31], 0x53[24] (for both channels 0 and 1)
- SiAPITXC          0x25[3:0], 0x24[24:19]
- SiAPIPowerUpTX    0x57[17], 0x23[26], 0x23[4], 0x53[24], 0x57[13:0], 0x58[31:0], 0x24[28:25], 0x24[0], 0x25[20], 0x22[4:1], 0x15[16:10], 0x03[25:18], 0x23[26], 0x23[4], 0x57[17:14]

Initial guesses for functional block naming:
0. ???
1. Rx
2. Tx
5. Enables? Tuner?

Known registers:
0x02 - 3:1 synthesizer related?
0x03 - power control related
0x05 - RX gain related
0x0{1,2,3,4} may have bit 17 accessed/modified in SiAPIRXOBS, SiAPIRXTXON, SiAPICHRF, SiAPILoopback

0x10 - RX IQ LPF, bit 26 is an enable of some sort
0x11 - RX IQ LPF
0x12 - RSSI-related
0x13 - RX Gain related
0x14 - RX IQ LPF, other enables?
0x15 - enables? 22:19 written in rx_bw, 16:0 written by loopback/poweruprx, others
0x1e - 15:0 read by SiAPIRSSI

0x20, 0x21 - 23:18, 14:11, 10:7 TX_BW, 6:0 TX_GAIN
0x22 - 10:5 rx power, 4:1 tx power, 0 loopback enable?
0x23 - 26, 24 tx enables?
0x24 - 28:25 power controls, 24:19 synth-related, 0 tx related
0x25 - 15:8, 3:0 synth related, 7:4 tx gain related, 20 tx power control?

0x50 - 31:24, 15:0 synthesizer related
0x51 - 22 loopback+power related, 31 power related, 26:23 and 21:0 synthesizer settings
0x52 - 4:0 AFC related
0x53 - 13:8 and 5:0 afc related, 24 power control
0x55 - written by SiAPISetSynth
0x56 - 31 afc related, 30, 27:14 synth related
0x57 - 17:0 are used by many things
0x58 - 31:28, 23:20, 15:12, 7:4 rx power related, 27:24, 19:16, 11:8, 3:0 tx power related
0x5c - read in AFC


Looking through default register values:
- channel 0 reg 0x04 has the MS bit set, whereas channel 1,2,3 reg 0x04 does not.
- channel 0 reg 0x16 is 0x88e7ad6b, whereas channel 1,2,3 reg 0x16 is 0x8863ad6b.
- channel 0,1,2 reg 0x10 is 0x1f7bdef, whereas channel 3 reg 0x10 is 0x1f7adef
- channel 0,1 reg 0x22 is 0x60b2006, whereas channel 2,3 reg 0x22 is 0x6086006
- channel 0 reg 0x26 is 0x17ff5ef7, whereas channel 1 reg 0x26 is 0x10e35ef7 and channel 2,3 reg 0x26 is 0x17ff5ef7
- channel 0 reg 0x50 is 0xf0f5150e, whereas channel 1 reg 0x50 is 0xfff5150e and channel 2 reg 0x50 is 0x2aab0000
- channel 0 reg 0x51 is 0x2d17ffff, whereas channel 1 reg 0x51 is 0x2d57ffff and channel 2 reg 0x51 is 0x8020000a
- channel 0,1 reg 0x52 is 0x346c001f, whereas channel 2 reg 0x52 is 0x55555555
- channel 0 reg 0x53 is 0x70573f7f, whereas channel 1 reg 0x53 is 0x70473f7f and channel 2 reg 0x53 is 0xc00df6ff
- channel 0,1 reg 0x54 is 0x54653025, whereas channel 2 reg 0x54 is 0x78008000
- channel 0,1 reg 0x55 is 0x78008000, whereas channel 2 reg 0x55 is 0x0
- channel 0,1 reg 0x56 is 0xc01d3401, whereas channel 2 reg 0x56 is 0x0
- channel 0 reg 0x57 is 0xf603fdf, whereas channel 1 reg 0x57 is 0xdb983fff and channel 2 reg 0x57 is 0
- channel 0,1 reg 0x58 is 0xffffffff, whereas channel 2 reg 0x58 is 0x0


default regs that are written:
0x00->0x05, ch0-3
0x10->0x17, ch0-3
0x20->0x26, ch0-3
0x30->0x34, ch0
0x70->0x72, ch0
0x63->0x64, ch0
0x50->0x58, ch0-2


Full default register dump:
ch0 reg 0x00 = 0x42200840
ch0 reg 0x01 = 0x80116a00
ch0 reg 0x02 = 0x16a00
ch0 reg 0x03 = 0xc3fd6a00
ch0 reg 0x04 = 0x8ffd6a00
ch0 reg 0x05 = 0xe00000

ch1 reg 0x00 = 0x42200840
ch1 reg 0x01 = 0x80116a00
ch1 reg 0x02 = 0x16a00
ch1 reg 0x03 = 0xc3fd6a00
ch1 reg 0x04 = 0xffd6a00
ch1 reg 0x05 = 0xe00000

ch2 reg 0x00 = 0x42200840
ch2 reg 0x01 = 0x80116a00
ch2 reg 0x02 = 0x16a00
ch2 reg 0x03 = 0xc3fd6a00
ch2 reg 0x04 = 0xffd6a00
ch2 reg 0x05 = 0xe00000

ch3 reg 0x00 = 0x42200840
ch3 reg 0x01 = 0x80116a00
ch3 reg 0x02 = 0x16a00
ch3 reg 0x03 = 0xc3fd6a00
ch3 reg 0x04 = 0xffd6a00
ch3 reg 0x05 = 0xe00000


ch0 reg 0x10 = 0x1f7bdef
ch0 reg 0x11 = 0x55f7bdef
ch0 reg 0x12 = 0x15000000
ch0 reg 0x13 = 0x38707
ch0 reg 0x14 = 0x282003
ch0 reg 0x15 = 0x500001ad
ch0 reg 0x16 = 0x88e7ad6b
ch0 reg 0x17 = 0x3863c000

ch1 reg 0x10 = 0x1f7bdef
ch1 reg 0x11 = 0x55f7bdef
ch1 reg 0x12 = 0x15000000
ch1 reg 0x13 = 0x38707
ch1 reg 0x14 = 0x282003
ch1 reg 0x15 = 0x500001ad
ch1 reg 0x16 = 0x8863ad6b
ch1 reg 0x17 = 0x3863c000

ch2 reg 0x10 = 0x1f7bdef
ch2 reg 0x11 = 0x55f7bdef
ch2 reg 0x12 = 0x15000000
ch2 reg 0x13 = 0x38707
ch2 reg 0x14 = 0x282003
ch2 reg 0x15 = 0x500001ad
ch2 reg 0x16 = 0x8863ad6b
ch2 reg 0x17 = 0x3863c000

ch3 reg 0x10 = 0x1f7adef
ch3 reg 0x11 = 0x55f7bdef
ch3 reg 0x12 = 0x15000000
ch3 reg 0x13 = 0x38707
ch3 reg 0x14 = 0x282003
ch3 reg 0x15 = 0x500001ad
ch3 reg 0x16 = 0x8863ad6b
ch3 reg 0x17 = 0x3863c000


ch0 reg 0x20 = 0xe99b83
ch0 reg 0x21 = 0xe99b83
ch0 reg 0x22 = 0x60b2006
ch0 reg 0x23 = 0x1f801ad
ch0 reg 0x24 = 0x1f02aafe
ch0 reg 0x25 = 0xa5000f
ch0 reg 0x26 = 0x17ff5ef7

ch1 reg 0x20 = 0xe99b83
ch1 reg 0x21 = 0xe99b83
ch1 reg 0x22 = 0x60b2006
ch1 reg 0x23 = 0x1f801ad
ch1 reg 0x24 = 0x1f02aafe
ch1 reg 0x25 = 0xa5000f
ch1 reg 0x26 = 0x10e35ef7

ch2 reg 0x20 = 0xe99b83
ch2 reg 0x21 = 0xe99b83
ch2 reg 0x22 = 0x6086006
ch2 reg 0x23 = 0x1f801ad
ch2 reg 0x24 = 0x1f02aafe
ch2 reg 0x25 = 0xa5000f
ch2 reg 0x26 = 0x17ff5ef7

ch3 reg 0x20 = 0xe99b83
ch3 reg 0x21 = 0xe99b83
ch3 reg 0x22 = 0x6086006
ch3 reg 0x23 = 0x1f801ad
ch3 reg 0x24 = 0x1f02aafe
ch3 reg 0x25 = 0xa5000f
ch3 reg 0x26 = 0x17ff5ef7


ch0 reg 0x30 = 0x14003b
ch0 reg 0x31 = 0xdcba4321
ch0 reg 0x32 = 0x2ca92a
ch0 reg 0x33 = 0x27
ch0 reg 0x34 = 0xa92a


ch0 reg 0x70 = 0x14003b
ch0 reg 0x71 = 0xdcba4321
ch0 reg 0x72 = 0x0


ch0 reg 0x63 = 0x27
ch0 reg 0x64 = 0x0


ch0 reg 0x50 = 0xf0f5150e
ch0 reg 0x51 = 0x2d17ffff
ch0 reg 0x52 = 0x346c001f
ch0 reg 0x53 = 0x70573f7f
ch0 reg 0x54 = 0x54653025
ch0 reg 0x55 = 0x26000000
ch0 reg 0x56 = 0xc01d3401
ch0 reg 0x57 = 0xf603fdf
ch0 reg 0x58 = 0xffffffff

ch1 reg 0x50 = 0xfff5150e
ch1 reg 0x51 = 0x2d57ffff
ch1 reg 0x52 = 0x346c001f
ch1 reg 0x53 = 0x70473f7f
ch1 reg 0x54 = 0x54653025
ch1 reg 0x55 = 0x26000000
ch1 reg 0x56 = 0xc01d3401
ch1 reg 0x57 = 0xdb983fff
ch1 reg 0x58 = 0xffffffff

ch2 reg 0x50 = 0x2aab0000
ch2 reg 0x51 = 0x8020000a
ch2 reg 0x52 = 0x55555555
ch2 reg 0x53 = 0xc00df6ff
ch2 reg 0x54 = 0x78008000
ch2 reg 0x55 = 0x0
ch2 reg 0x56 = 0x0
ch2 reg 0x57 = 0x0
ch2 reg 0x58 = 0x0


RX LPF table dump:
lpf_config_high_cutoff_mhz = 10
reg_val_1d_18 = 0x3c
reg_val_17_12 = 0x17
reg_val_0b_06 = 0x17
reg_val_11_0c = 0x14
reg_val_05_00 = 0x14
reg_4_val_05_00 = 0x2a
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 20
reg_val_1d_18 = 0x39
reg_val_17_12 = 0x17
reg_val_0b_06 = 0x17
reg_val_11_0c = 0x14
reg_val_05_00 = 0x14
reg_4_val_05_00 = 0x23
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 40
reg_val_1d_18 = 0x37
reg_val_17_12 = 0x1b
reg_val_0b_06 = 0x1b
reg_val_11_0c = 0x14
reg_val_05_00 = 0x14
reg_4_val_05_00 = 0x1f
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 60
reg_val_1d_18 = 0x33
reg_val_17_12 = 0x1b
reg_val_0b_06 = 0x1b
reg_val_11_0c = 0x14
reg_val_05_00 = 0x14
reg_4_val_05_00 = 0xf
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 80
reg_val_1d_18 = 0x28
reg_val_17_12 = 0x1c
reg_val_0b_06 = 0x1c
reg_val_11_0c = 0x14
reg_val_05_00 = 0x14
reg_4_val_05_00 = 0x3
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 100
reg_val_1d_18 = 0x23
reg_val_17_12 = 0x1c
reg_val_0b_06 = 0x1c
reg_val_11_0c = 0x14
reg_val_05_00 = 0x14
reg_4_val_05_00 = 0x2
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 200
reg_val_1d_18 = 0x10
reg_val_17_12 = 0x27
reg_val_0b_06 = 0x27
reg_val_11_0c = 0x1c
reg_val_05_00 = 0x1c
reg_4_val_05_00 = 0x0
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 160
reg_val_1d_18 = 0x1a
reg_val_17_12 = 0x26
reg_val_0b_06 = 0x26
reg_val_11_0c = 0x1c
reg_val_05_00 = 0x1c
reg_4_val_05_00 = 0x7
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 300
reg_val_1d_18 = 0x6
reg_val_17_12 = 0x2f
reg_val_0b_06 = 0x2f
reg_val_11_0c = 0x1f
reg_val_05_00 = 0x1f
reg_4_val_05_00 = 0x0
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 320
reg_val_1d_18 = 0x10
reg_val_17_12 = 0x2f
reg_val_0b_06 = 0x2f
reg_val_11_0c = 0x1f
reg_val_05_00 = 0x1f
reg_4_val_05_00 = 0x7
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0

lpf_config_high_cutoff_mhz = 400
reg_val_1d_18 = 0x4
reg_val_17_12 = 0x2f
reg_val_0b_06 = 0x2f
reg_val_11_0c = 0x1f
reg_val_05_00 = 0x1f
reg_4_val_05_00 = 0x0
reg_5_val_16_13 = 0x8
reg_5_val_10_0d = 0x0


## Actual spi dev tests:
```
root@imx8mp-rfnm:~# modprobe spidev
root@imx8mp-rfnm:~# echo spidev > /sys/bus/spi/devices/spi2.0/driver_override
root@imx8mp-rfnm:~# rmmod rfnm_granita
root@imx8mp-rfnm:~# cat /sys/bus/spi/devices/spi2.0/driver_override
spidev
root@imx8mp-rfnm:~# echo spi2.0 > /sys/bus/spi/drivers/spidev/bind
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x00\x00\x00\x00\x00" -v
spi mode: 0x4
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 00 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF A1 10 04 20 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |..... |
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x00\x00\x00\x00\x00" -v
spi mode: 0x4
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 00 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF A1 10 04 20 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |..... |
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "N\x00\x00\x00\x00\x00" -v
spi mode: 0x4
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4E 00 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |N.....|
RX | FF FF A1 10 04 20 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |..... |
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "O\x00\x00\x00\x00\x00" -v
spi mode: 0x4
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4F 00 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |O.....|
RX | FF FF A1 10 04 20 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |..... |
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "P\x00\x00\x00\x00\x00" -v
spi mode: 0x4
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 50 00 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |P.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x04\x00\x00\x00\x00" -v
spi mode: 0x4
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 04 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF C7 FE B5 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x04\x00\x00\x00\x00" -v
spi mode: 0x4
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 04 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF 87 FE B5 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
```

... those don't seem quite right. Tinkering with SPI phase:

```
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x00\x00\x00\x00\x00" -H 1 -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 00 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 42 20 08 40 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |..B .@|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x04\x00\x00\x00\x00" -H 1 -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 04 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 8F FD 6A 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |....j.|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x04\x00\x00\x00\x00" -H 1 -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 04 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF 0F FD 6A 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |....j.|
```

_Much_ better.

What additional registers exist, then?

0x0c->0x0e
```
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x05\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 05 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 00 E0 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x06\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 06 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x07\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 07 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x08\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 08 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x09\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 09 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x0a\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 0A 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x0b\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 0B 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x0c\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 0C 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF ED CD FF BA __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x0d\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 0D 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 0F 58 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |...X..|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x0e\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 0E 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 3A E0 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |..:...|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x0f\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 0F 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
```

ch0,ch1 - 0x18-0x1e
```
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x17\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 17 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 38 63 C0 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |..8c..|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x18\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 18 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 0B BE 04 0F __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x19\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 19 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF F3 70 44 D1 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |...pD.|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x1a\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 1A 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 17 DE 00 F0 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x1b\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 1B 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 17 DE 00 F0 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x1c\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 1C 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF FD 70 2C 0B __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |...p,.|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x1d\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 1D 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 13 83 87 07 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x1e\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 1E 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF 74 FF 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |..t...|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x1f\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 1F 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|


root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x18\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 18 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF 0B BE 04 0F __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x19\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 19 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF F3 70 44 D1 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |...pD.|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x1a\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 1A 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF 17 DE 00 F0 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x1b\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 1B 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF 17 DE 00 F0 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x1c\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 1C 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF BE B8 82 20 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |..... |
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x1d\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 1D 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF C9 C3 87 07 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x1e\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 1E 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF 8F ED 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "M\x1f\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4D 1F 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |M.....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
```

0x2c->0x2E
```
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x26\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 26 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L&....|
RX | FF FF 17 FF 5E F7 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |....^.|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x27\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 27 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L'....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x28\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 28 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L(....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x29\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 29 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L)....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x2a\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 2A 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L*....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x2b\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 2B 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L+....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x2c\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 2C 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L,....|
RX | FF FF 2E BF 83 E0 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x2d\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 2D 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L-....|
RX | FF FF 00 00 00 46 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |.....F|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x2e\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 2E 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....|
RX | FF FF D6 FB 9F FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x2f\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 2F 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L/....|
RX | FF FF FF FF FF FF __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |......|
```

You can read multiple registers in a single transfer by appending the next address you want.

```
root@imx8mp-rfnm:~# spidev_test -D /dev/spidev2.0 -p "L\x04\x00\x00\x00\x00M\x04\x00\x00\x00\x00" -H -v
spi mode: 0x5
bits per word: 8
max speed: 500000 Hz (500 kHz)
TX | 4C 04 00 00 00 00 4D 04 00 00 00 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |L.....M.....|
RX | FF FF 8F FD 6A 00 00 00 0F FD 6A 00 __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __  |....j.....j.|
```

I used this to dump the full 256-register space for all 4 channels.

Registers that seem mapped (return something other than all 0xFF):
ch0->ch3: 0x00->0x05, 0x0C->0x0E
ch0->ch3: 0x10->0x1E
ch0->ch3: 0x20->0x26, 0x2C->0x0E
ch0: 0x30->0x34, 0x3A->0x3D
ch0->ch1: 0x50->0x57, 0x5C->0x5E
ch0: 0x60->0x62, 0x64->0x6D
ch0: 0x70->76, 0x7A

## Command to get the necessary cal tables out of the kernel module:
~/toolchains/arm-gnu-toolchain-13.2.Rel1-darwin-arm64-aarch64-none-elf/bin/aarch64-none-elf-objcopy -S -K STCfgGeneral -K TXGainLUT -K bw_data.1 -K bw_data.0 -K RX_Gain_LUT --globalize-symbol RX_Gain_LUT -K OBS_Gain_LUT ../../../newkernel4/rfnm_granita.ko rfnm_granita_old_trimmed.o
