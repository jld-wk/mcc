<!---
SPDX-FileCopyrightText: (c) 2026 Julian Duwe
SPDX-License-Identifier: Apache-2.0
-->

# MCC IR-Specification

## Op-Codes
#### Introduction
* **pops the stack** means, that the latest stack value will be consumed and is further referred to as `a`<br />
* **pops the stack twice** means:
    - that the latest stack value (stack top) will be consumed and is further referred to as `b`
    - and again the latest stack value (stack top-1) will be consumed and is further referred to as `a`
---
#### Variable Instructions
* **store [IDENTIFIER]** -> pops the stack and stores that value
* **load [IDENTIFIER]** -> pushes the stored value onto the stack
---
#### Stack Instructions
* **push [NUMBER]** -> pushes the number onto the stack
* **pop** -> pops the stack
---
#### Arithmetic Instructions
* **add** -> pops the stack twice and pushes the result of `a + b` onto the stack
* **sub** -> ... pushes the result of `a - b`
* **mul** -> ... pushes the result of `a * b`
* **div** -> ... pushes the result of `a / b`
---
#### Comparision Instructions
* **eq** -> pops the stack twice and pushes the result of `a == b` onto the stack
* **ne** -> ... pushes the result of `a != b`
* **gt** -> ... pushes the result of `a > b`
* **ge** -> ... pushes the result of `a >= b`
* **lt** -> ... pushes the result of `a < b`
* **le** -> ... pushes the result of `a <= b`<br />
**note**: results of these comparision instructions are either 0 (false) or 1 (true)
---
#### Jump Instructions
* **jmp [IDENTIFIER]** -> jumps into the specified branch
* **jmp_f [IDENTIFIER]** -> pops the stack and jumps if `a == 0`
* **jmp_t [IDENTIFIER]** -> ... if `a != 0`<br />
**note**: jmp **doesn't return back** to the caller
---
#### Call Instructions
* **call [IDENTIFIER]** -> jumps to the specified branch
* **call_f [IDENTIFIER]** -> pops the stack and jumps if `a == 0`
* **call_t [IDENTIFIER]** -> ... if `a != 0`<br />
**note**: call **returns back** to the caller, once the callee finished execution
---
#### Exit Instruction
* **exit** -> pops the stack and exits the program with the value as exit code<br />
**note**: required for a successful program execution
---
#### Other Instructions
* **dbg_dump** -> prints the latest value added onto the stack
**note**: temporary

## Entry Point

The **entry** branch is the **entry point of the program**. This program will exit with code 0:

```IR
entry:
push 0
exit
```

## Examples
[countdown.ir](countdown.ir)<br />
[factorial.ir](factorial.ir)