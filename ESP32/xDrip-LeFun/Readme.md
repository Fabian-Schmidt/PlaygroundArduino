# Overview
LeFun protocol

# Message Structure
1. `0xAB`
2. Total Package length including `0xAB`
3. OpCode
4. Message
5. CRC

# OpCodes Recieved

| OpCodes | Name | Message length |
|---------|------|----------------|
| 0x00    | Ping | 1 |
| 0x02    | SetLocaleFeature | 5 |
| 0x04    | SetTime | 8 |
| 0x07    | SetScreens | 4 |
| 0x08    | SetFeatures | 4 |

## Ping
Example
```
0xAB 0x4 0x0 0xC
```
Reply Pong.

## SetTime
Example
```
0xAB 0xB 0x4 0x1 0x16 0x0 0x0 0x14 0x16 0x6 0x81
0xAB 0xB 0x4 0x1 0x16 0x0 0x0 0x14 0x14 0x21 0xB0
0xAB 0xB 0x4 0x1 0x16 0x0 0x0 0x14 0x13 0x1C 0x9D
0xAB 0xB 0x4 0x1 0x17 0x5 0x7 0x14 0x1B 0x17 0xA8
0xAB 0xB 0x4 0x1 0x16 0x5 0x8 0x14 0x2A 0x13 0x8D
```

Message:

| Data | Comment |
|------|---------|
| 0xAB |  |
| 0x0B | Length |
| 0x04 | OpCode |
| 0x01 | Write |
|      | Year last two digts (not correct) - Day and Month to match Day of week for current year |
|      | (Month) -> Glucose value or 0x00 if value over 12 |
|      | (Day) -> Glucose value decimal place or full value if over 12 |
|      | Hour - current |
|      | Minute - current |
|      | Second - current |

## SetFeatures

| Data | Comment |
|------|---------|
| 0xAB |  |
| 0x08 | Length |
| 0x08 | OpCode |
| 0x02 | Write |

### Feature / Bitmap	


# OpCodes Response

| OpCodes | Name | Message length |
|---------|------|----------------|
| 0x00    | Pong | 20(new) or 16(old) |

## Pong (new)
Message:

| Data | Comment |
|------|---------|
| 0x5A | StartByte |
| 0x14 | Length |
| 0x00 | OpCode |
| 0x00 | ff1 |
| 0x00 | othree |
| 0x00 | zero2 |
| 0x00 | zero3 |
|      | ASCII Model name |
|      | ASCII Model name |
|      | ASCII Model name |
| 0x00 | vers1 |
|      | ASCII Hardware version |
|      | ASCII Hardware version |
|      | ASCII Software version |
|      | ASCII Software version |
|      | ASCII Manufacturer |
|      | ASCII Manufacturer |
|      | ASCII Manufacturer |
|      | ASCII Manufacturer |
|      | CRC |

# Misc
- [Font generator](https://rop.nl/truetype2gfx/)