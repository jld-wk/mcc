<!---
SPDX-FileCopyrightText: (c) 2026 Julian Duwe
SPDX-License-Identifier: Apache-2.0
-->

# MCC IR-Specification

Too lazy to finish...

## Op-Codes
#### Variable Instructions
* **store [X: CONSTANT|VARIABLE], [Y]** -> stores the constant or the value inside X into Y
---
#### Arithmetic Instructions
* **add [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x + y` into Z
* **sub [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x - y` into Z
* **mul [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x * y` into Z
* **div [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x / y` into Z
---
#### Comparision Instructions
* **eq [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x == y` into Z
* **ne [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x != y` into Z
* **lt [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x < y` into Z
* **le [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x <= y` into Z
* **gt [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x > y` into Z
* **ge [X: CONSTANT|VARIABLE], [Y: CONSTANT|VARIABLE], [Z]** -> stores the result of `x >= y` into Z<br />
**note**: results of these comparision instructions are either 0 (false) or 1 (true)
---
#### Jump Instructions
* **jmp [IDENTIFIER]** -> jumps into the specified branch
* **jmp_if [IDENTIFIER]** -> jumps into the specified branch
**note**: jmp **doesn't return back** to the caller
---
#### Call Instructions
* **call [IDENTIFIER]** -> jumps to the specified branch
* **call_if [IDENTIFIER]** -> jumps to the specified branch
**note**: call **returns back** to the caller, once the callee finished execution
---
#### Exit Instruction
* **exit** -> pops the stack and exits the program with the value as exit code<br />
**note**: required for a successful program execution
---
#### Other Instructions
* **dump_d** -> prints the latest value added onto the stack
**note**: temporary

## Entry Point

The **entry** branch is the **entry point of the program**. This program will exit with code 0:

```IR
entry:
-
exit 0
-
```

## Examples
[strings.ir](strings.ir)<br />
[countdown.ir](countdown.ir)<br />
[factorial.ir](factorial.ir)