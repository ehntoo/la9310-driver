## What functions do we call in the SiTune/Arctic Semiconductor Granita API interface?
- SiAPIOpen (during init)
- SiAPISPITest (during init)
- SiAPILoopback (for TDD operation)
- SiAPIRXGAIN
- SiAPITXGAIN
- SiAPIPowerUpTX
- SiAPIPowerUpRX (for all rx-only operations)

Only `SiAPIPowerUpRX` and `SiAPIRXGAIN` are used when setting RX channels.

## SiAPIOpen
- Write default register values from STCfgGeneral
    - First 0x10 bytes are i2c/spi "fixed address" options
    - Register table starts at +0x10 bytes, and is (0x7a0 - 0x10) = 0x790 bytes long, each entry 0x10 bytes, 0x79 entries
    - Byte 0 is "full" register address
    - Byte 1 is path/channel number
    - Byte 8-11 are 32-bit little-endian data
    - As confirmed by IceWings datasheet, byte 0's msbit indicates whether a SPI transaction is a read (0) or a write (1)
    - I believe Byte 1 can be used to index into the first 0x10 bytes of STCfgGeneral

## SiAPISPITest
- Seems to re-write the default register values, but with a read-back to validate they took afterwards

## SiAPIRXGAIN
- Gain constants are in a lookup table, `RX_Gain_LUT`
    - Entries are 0x8 bytes large, 0x4f entries.
        - reg_0x05_b_12_11
        - reg_0x05_b_10_09
        - reg_0x05_b_08_04
        - reg_0x05_b_03_00
        - reg_0x05_b_23_16
        - reg_0x13_b_21_15
        - reg_0x13_b_14_08
        - reg_0x13_b_7_0
    - Modifies bits 23:16, 12:0 of register 0x05
    - Modifies bits 21:0 of register 0x13
- For each path/channel you want to modify:
    - Disable the channel by clearing bit (0x11-chan_idx) in channel 0's register 0x57
    - Disable global rx (?) by clearing bits 9, then 4 in the desired channel's register 0x15
    - Wait 10msec!? Surely there's some status bit we can discover to poll instead
    - Apply modifications from gain index LUT to registers 0x05 then 0x13
    - Original code modifies:
        - reg 0x5: 0xc:0xb, 0xa:0x9, 0x8:0x4, 0x3:0x0, 0x17:0x10
        - reg 0x13: 0x15:0xF, 0xE:0x8, 0x7:0x0
    - Wait 20msec!?
    - Re-enable global rx (?) by setting bit 9, then bit 4 in the desired channel's register 0x15
    - Re-enable the channel by setting bit (0x11-chan_idx) in channel 0's register 0x57

## SiAPIPowerUpRX
- Find the entry of RangeLUT.2 that has a range_upper_bound >= your target and range_lower_bound <= your target
- For each path/channel you want to modify:
    - Disable the channel by clearing bit (0x11-chan_idx) in channel 0's register 0x57
    - Disable global rx (?) by clearing bits 9, then 4 in the desired channel's register 0x15
    - Wait 10msec!? Surely there's some status bit we can discover to poll instead
- Call synthesizer setup function
- If synth setup was not successful (returned freq was 0), return error
- Clear bit 0x18 of reg 0x53 chan 0
- 

## SiAPISetSynth

## SiAPIRX_BW
