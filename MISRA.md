# MISRA Compliance

FreeRTOS-Kernel conforms to [MISRA C:2012](https://www.misra.org.uk/misra-c)
guidelines, with the deviations listed below. Compliance is checked with
Coverity static analysis. Since the FreeRTOS kernel is designed for
small-embedded devices, it needs to have a very small memory footprint and
has to be efficient. To achieve that and to increase the performance, it
deviates from some MISRA rules. The specific deviations, suppressed inline,
are listed below.

Additionally, [MISRA configuration](#misra-configuration) contains project
wide deviations.

### Suppressed with Coverity Comments
To find the violation references in the source files run grep on the source code
with ( Assuming rule 8.4 violation; with justification in point 1 ):
```
grep 'MISRA Ref 8.4.1' . -rI
```

#### Rule 8.4

_Ref 8.4.1_

- MISRA C:2012 Rule 8.4: A compatible declaration shall be visible when an
        object or function with external linkage is defined.
        This rule requires that a compatible declaration is made available
        in a header file when an object with external linkage is defined.
        pxCurrentTCB(s) is defined with external linkage but it is only
        referenced from the assembly code in the port files. Therefore, adding
        a declaration in header file is not useful as the assembly code will
        still need to declare it separately.

_Ref 8.4.2_

- MISRA C:2012 Rule 8.4: A compatible declaration shall be visible when an
        object or function with external linkage is defined.
        This rule requires that a compatible declaration is made available
        in a header file when an object with external linkage is defined.
        xQueueRegistry is being accessed by the Kernel Unit Test files and
        hence cannot be declared as a static variable.

#### Rule 8.6

_Ref 8.6.1_

- MISRA C:2012 Rule 8.6: An identifier with external linkage shall have exactly
        one external definition.
        This rule requires an identifier should not have multiple definitions or
        no definition. Otherwise, the behavior is undefined. FreeRTOS hook functions
        are defined in user application if corresponding config is enabled in
        FreeRTOSConfig.h, so it is a false positive.


#### Rule 11.1

_Ref 11.1.1_

- MISRA C:2012 Rule 11.1: Conversions shall not be performed between a pointer to
        function and any other type.
        This rule requires that a pointer to a function shall not be converted into
        or from a pointer to a function with a compatible type.
        `vEventGroupClearBitsCallback` and `vEventGroupSetBitsCallback` use const
        qualifier for the second parameter `ulBitsToClear` to specify that this parameter
        is not modified in the callback function.

#### Rule 11.2

- MISRA C:2012 Rule 11.2: Conversions shall not be performed between a pointer to
        function and any other type.
        This rule requires that a pointer to a function shall not be converted into
        or from a pointer to a function with a compatible type.
        `pxSendCompletedCallback` and `pxReceiveCompletedCallback` are parameters to
        `prvInitialiseNewStreamBuffer`. These two callback functions are not used when
        `configUSE_SB_COMPLETED_CALLBACK` is set to 0.

#### Rule 11.3

_Ref 11.3.1_

- MISRA C:2012 Rule 11.3: A cast shall not be performed between a pointer to object
        type and a pointer to a different object type.
        The rule requires not to cast a pointer to object into a pointer to a
        different object to prevent undefined behavior due to incorrectly aligned.
        To support static memory allocation, FreeRTOS creates static type kernel
        objects which are aliases for kernel object type with prefix "Static" for
        data hiding purpose. A static kernel object type is guaranteed to have the
        same size and alignment with kernel object, which is checked by configASSERT.
        Static kernel object types include StaticEventGroup_t, StaticQueue_t,
        StaticStreamBuffer_t, StaticTimer_t and StaticTask_t.

#### Rule 11.5

_Ref 11.5.1_

- MISRA C:2012 Rule 11.5: A conversion should not be performed from pointer to
        void into pointer to object.
        The rule requires a pointer to void should not be converted into a pointer
        to object cause this may result in a pointer that is not correctly aligned,
        resulting in undefined behavior. The memory blocks allocated by pvPortMalloc()
        must be guaranteed to meet the alignment requirements specified by portBYTE_ALIGNMENT_MASK.
        Therefore, casting the void pointer which points to the returned memory to
        a pointer to object is ensured to be aligned.

_Ref 11.5.2_

- MISRA C:2012 Rule 11.5: EventGroupHandle_t is a pointer to an EventGroup_t, but
        EventGroupHandle_t is kept opaque outside of this file for data hiding
        purposes.

_Ref 11.5.3_

- MISRA C:2012 Rule 11.5: ` void * ` is used in list macros for list item owner as these
        macros are used with tasks, timers and co-routines. Alignment is known to be
        fine as the type of the pointer stored and retrieved is the same.

_Ref 11.5.4_

- MISRA C:2012 Rule 11.5: ` void * ` is used in a generic callback function prototype since
        this callback is for general use case. Casting this pointer back to original
        type is safe.

_Ref 11.5.5_

- MISRA C:2012 Rule 11.5: ` void * `  is converted into a pointer to uint8_t for ease of
        sizing, alignment and access.

#### Rule 21.6

_Ref 21.6.1_

- MISRA C-2012 Rule 21.6: The Standard Library input/output functions shall not
        be used.
        This rule warns about the use of standard library input/output functions
        as they might have implementation defined or undefined behavior. The function
        'snprintf' is used for debugging only ( when configUSE_TRACE_FACILITY is
        set to 1 and configUSE_STATS_FORMATTING_FUNCTIONS is set to greater than 0 )
        and is not part of the 'core' kernel code.

### MISRA configuration

Copy below content to `misra.conf` to run Coverity on FreeRTOS-Kernel.

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
            reason: "HeapRegion_t and HeapStats_t are used only in heap files but declared in portable.h which is included in multiple source files. As a result, these definitions appear in multiple source files where they are not used."
        },
        {
            deviation: "Directive 4.9",
            reason: "FreeRTOS-Kernel is optimised to work on small micro-controllers. To achieve that, function-like macros are used."
        },
        {
            deviation: "Rule 1.2",
            reason: "The __attribute__ tags are used via macros which are defined in port files."
        },
        {
            deviation: "Rule 3.1",
            reason: "We post HTTP links in code comments which contain // inside comments blocks."
        },
        {
            deviation: "Rule 8.7",
            reason: "API functions are not used by the library outside of the files they are defined; however, they must be externally visible in order to be used by an application."
        },
        {
            deviation: "Rule 11.5",
            reason: "Allow casts from `void *`. List owner, pvOwner, is stored as `void *` and are cast to various types for use in functions."
        }
        {
            deviation: "Rule 21.6",
            reason: "snprintf is used in APIs vTaskListTasks and vTaskGetRunTimeStatistics to print the buffer."
        }
    ]
}
```
