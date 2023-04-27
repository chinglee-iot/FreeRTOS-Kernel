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
        This rule requires that a compatiable declaration is made available in a header
        file when an object with external linkage is defined. pxCurrentTCB(s) is defined
        with external linkage but it is only used in port assembly files. Therefore, adding
        a declaration in header file is not useful as the assembly code will still need to
        declare it separately.

#### Rule 11.3

_Ref 11.3.1_

- MISRA C:2012 Rule 11.3: A cast shall not be performed between a pointer to object type and a pointer to a different object type.
        Unusual cast is ok as the structures are designed to have the same alignment, this is checked by an assert.

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