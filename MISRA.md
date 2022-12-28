# MISRA Compliance (for configNUMBER_OF_CORES > 1)

For now, FreeRTOS-Kernel only conforms [MISRA C:2012](https://www.misra.org.uk/misra-c) guidelines in SMP part (configNUMBER_OF_CORES > 1), 
with the deviations listed below. Compliance is checked with Coverity static analysis. Refer to [configuration](#misra-configuration) to build an application for the tool to analyze.

### Suppressed with Coverity Comments
To find the violation references in the source files run grep on the source code
with ( Assuming rule 4.6 violation; with justification in point 1 ):
```
grep 'MISRA Ref 8.4.1' . -rI
```
        
#### Rule 8.4

_Ref 8.4.1_

- MISRA C:2012 Rule 8.4: A compatible declaration shall be visible when an object or function with external linkage is defined.
        vTaskEnterCriticalFromISR()/vTaskExitCriticalFromISR are used at some ports.
        
#### Rule 8.5

_Ref 8.5.1_

- MISRA C:2012 Rule 8.5: An external object or function shall be declared once in one and only one file.
        Function is declared in header file correctly. But the function name in task.h/tasks.c is different if portUSING_MPU_WRAPPERS enabled.
        
#### Rule 8.6

_Ref 8.6.1_

- MISRA C:2012 Rule 8.6: An identifier with external linkage shall have exactly one external definition.
        Function is declared in header file correctly. But the function name in task.h/tasks.c is different if portUSING_MPU_WRAPPERS enabled.
        
#### Rule 8.9

_Ref 8.9.1_

- MISRA C:2012 Rule 8.9: An object should be defined at block scope if its identifier only appears in a single function.
        False alarm. This variable is actually used by several different functions.
        
#### Rule 11.3

_Ref 11.3.1_

- MISRA C:2012 Rule 11.3: A cast shall not be performed between a pointer to object type and a pointer to a different object type.
        Unusual cast is ok as the structures are designed to have the same alignment, this is checked by an assert.

_Ref 11.3.2_

- MISRA C:2012 Rule 11.3: A cast shall not be performed between a pointer to object type and a pointer to a different object type.
        The mini list structure is used as the list end to save RAM.  This is checked and valid.
  
#### Rule 11.5

_Ref 11.5.1_

- MISRA C:2012 Rule 11.5: A conversion should not be performed from pointer to void into pointer to object.
        void * is used as this macro is used with timers and co-routines too.  Alignment is known to be fine as the type of the pointer stored and retrieved is the same.
        
#### Rule 18.1

_Ref 18.1.1_

- MISRA C:2012 Rule 18.1: A pointer resulting from arithmetic on a pointer operand shall address an element of the same array as that pointer operand.
        Loop breaks before index overrun.

### MISRA configuration

Copy below content as misra.conf to run coverity on FreeRTOS-Kernel.

```
// MISRA C-2012 Rules
{
    version : "2.0",
    standard : "c2012",
    title: "Coverity MISRA Configuration",
    deviations : [
        // Disable the following rules.
        {
            deviation: "Directive 4.8",
            reason: "We include lots of header files from other sources such as the kernel which defines structures that violate that Dir"
        },
        {
            deviation: "Directive 4.9",
            reason: "It is important the FreeRTOS-Kernel is optimised to work on small micro-controllers. To achieve that, macros are being used."
        },
        {
            deviation: "Rule 1.2",
            reason: "Allow FreeRTOS-Kernel to use attributes."
        },
        {
            deviation: "Rule 2.3",
            reason: "The way we declare structures are conformant with the FreeRTOS libraries, which leaves somes types unused."
        },
        {
            deviation: "Rule 2.4",
            reason: "Structures are always declared with both a struct tag and typedef alias. Some of these structs are always referred to by their typedef alias and thus the corresponding tags are unused."
        },
        {
            deviation: "Rule 2.5",
            reason: "We use unused macros for backward compatibility in addition to macros comming from FreeRTOS"
        },
        {
            deviation: "Rule 3.1",
            reason: "We post links which contain // inside comments blocks"
        },
        {
            deviation: "Rule 21.2",
            reason: "Allow use of all names."
        }
    ]
}
```