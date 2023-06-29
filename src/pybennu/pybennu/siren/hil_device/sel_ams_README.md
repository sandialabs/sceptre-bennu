## Config
* In the siren configuration file for the AMS:
    * The `to_endpoint` block should only contain `active` tags.  No other digital tags are supported.
    * The value of the `to_endpoint` dictionary entries need to be formatted `digital<whatever you choose>`.  Siren needs the `digital` keyword in there to know it is a digital or boolean value
    * The `to_harware` block
    * The value of the `to_hardware` dictionary entries need to be the name of the tag, rather than a generic name.  This is so that the SEL AMS specific code knows whether or not the data to write to the AMS is a `bus` or `branch`

## Notes

### SEL Adaptive Multichannel Source (AMS).

There are 12 ouput channels on the AMS that send data to the SEL
device over a ribbon cable to the top board. By plugging into the top board
of the device, you can bypass the relay input transfomers of the bottom board
so that you don't need large amplifiers.

Analog channels for SEL-351S (only uses 8 of the 12 channels):
* CH1 IA
* CH2 IB
* CH3 IC
* CH4 IN
* CH5 VA
* CH6 VB
* CH7 VC
* CH8 VS

NOTE: If `PHANTV` is enabled on the relay, you only need to send voltage to VA
      of the relay. The relay will generate VB and VC from VA.

The following is an example serial message (in ASCII) to the AMS: AS117C7BE3109603C

Broken up into its parts: AS / 1 / 17C7BE31/ 0 / 960 / 3C

* AS --> Preamble (AI means you're ramping the magnitude) 
* 1 --> Channel number 
* 17C7BE310 --> Magnitude
* 0960 --> Angle
* 3C --> Number of samples, "04" ("EOT" character) for disabled channel 
    
The time for the test is specified in the "TT" string as hex. The number is given in 1/10,000 of a second:

TT / FFFFFFFF
* TT --> Test time
* FFFFFFFF --> Time in hex, maximum time is 119.304647083333 hours (119 hrs, 18 min, 16 secs)


|SEL|Channel||Magnitude||
|---|-------|---|-------|---|
|351|VA|PT=1672|400KV = 0x26A3D70A|603.4KV = 0x40000000|
|351|VB||550KV = 0x40000000||
|351|VC||594KV = 0x40000000||
|351|VS||604KV = 0x40000000||
||||||
|451|VAY|PT=2495|400KV = 0x26A3D70A|662.9KV = 0x40000000|
|451|VBY||||
|451|VCY||||



NOTE:Before sending voltage or current magnitudes to the AMS, be aware that the AMS down samples data. For example, the current inputs are down sampled in this way:
        
The smallest current value you can send to the AMS is 0.0000001; however,
for every two bytes of data sent to the AMS (or 16 values), you can only
represent 0.000002 worth of data. The data sent to the AMS for values 
0.0000001 through 0.000004 looks like this. Notice how some values sent
are actually identical in hex.

|HEX|DEC|
|---|---|
|00000001|0.000001|
|00000002|0.000002|
|00000002|0.000003|
|00000003|0.000004|
|00000004|0.000005|
|00000005|0.000006|
|00000006|0.000007|
|00000006|0.000008|
|00000007|0.000009|
|00000008|0.000010|
|00000009|0.000011|
|0000000A|0.000012|
|0000000B|0.000013|
|0000000B|0.000014|
|0000000C|0.000015|
|0000000D|0.000016|
|0000000E|0.000017|
|0000000F|0.000018|
|0000000F|0.000019|
|00000010|0.000020|
|00000011|0.000021|
|00000012|0.000022|
|00000013|0.000023|
|00000013|0.000024|
|00000014|0.000025|
|00000015|0.000026|
|00000016|0.000027|
|00000017|0.000028|
|00000017|0.000029|
|00000018|0.000030|
|00000019|0.000031|
|0000001A|0.000032|
|0000001B|0.000033|
|0000001C|0.000034|
|0000001C|0.000035|
|0000001D|0.000036|
|0000001E|0.000037|
|0000001F|0.000038|
|00000020|0.000039|
|00000020|0.000040|

Given this down sampling, to calculate the amplitude values correctly, you need
to multiply all scaled current values by `8098421` before converting to hex.

For voltage, the data is down sampled even more, where you multiply all
scaled voltage values by `4049210`. Essentially, the maximum allowed voltage
value is double the max allowed current value. 

The voltage and current angles are different. For those, you need to multiply all
phase angles by `10`.
