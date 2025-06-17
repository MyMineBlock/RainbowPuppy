# RainbowPuppy

## Prerequisites
ViGEm driver (controller emulation)
https://vigembusdriver.com/download/

Intercept driver
https://github.com/oblitum/Interception

## Who this is for 

This is for people who want to play their favorite controller only games on mouse and keyboard. This is not meant to be abused and its use must comply with the game's terms of service!

## FAQ

**Q**: Why is interception being used when low level hooks do the same thing?
---
**A**: Low level hooks do not block games that use raw input from acquiring hooked inputs

## JSON example

```json
{
  "bindings": [
    {
      "input": 17,
      "stick": {
        "lx": 128,
        "ly": 0
      }
    },
    {
      "input": 30,
      "stick": {
        "lx": 0,
        "ly": 128
      }
    },
    {
      "input": 32,
      "stick": {
        "lx": 255,
        "ly": 128
      }
    },
    {
      "input": 31,
      "stick": {
        "lx": 128,
        "ly": 255
      }
    },
    {
      "input": 56,
      "button": 64
    },
    {
      "input": 164,
      "button": 16
    },
    {
      "input": 2,
      "button": 128
    },
    {
      "input": 57,
      "button": 32
    },
    {
      "input": 15,
      "button": 2
    },
    {
      "input": 1,
      "button": 8192
    },
    {
      "input": 514,
      "button": 256
    },
    {
      "input": 513,
      "button": 512
    },
    {
      "input":  42,
      "button": 2048
    },
    {
      "input": 47,
      "dpad": 1
    }
    {
      "input": 16,
      "trigger": {
        "lt": 255
      }
    },
    {
      "input": 33,
      "trigger": {
        "rt": 255
      }
    }
  ]
}
```
