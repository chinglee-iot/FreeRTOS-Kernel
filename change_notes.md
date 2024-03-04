# Granular Locks V2

Granular locks V2. These changes are based on the feedback of the `FreeRTOS SMP Granular Locks.pdf` document.

## Algorithm

Granular locks V2 attempts to take all the required spinlocks (of different data groups) in a single call. This is so that in case the current running task needs to yield upon enter a critical section, all of the taken locks can be released (in revserse order) from the `prvCheckForRunStateChange()` call.

## API Additions

### Porting API

- The following port config macro must be defined to 1

```c
#define portUSING_GRANULAR_LOCKS    1
```

- The following types must be defined

```c
#define portSPINLOCK_TYPE       ...
```

- The following port macros must be defined

```c
#define portGET_SPINLOCK( pxLock )                  /* void vPortSpinlockGet( portSPINLOCK_TYPE *pxLock ) */
#define portRELEASE_SPINLOCK( pxLock )              /* void vPortSpinlockRelease( portSPINLOCK_TYPE *pxLock ) */
#define portINIT_EVENT_GROUP_SPINLOCK( pxLock )     /* void vPortSpinlockInitGroup( portSPINLOCK_TYPE *pxLock ) */
#define portINIT_QUEUE_SPINLOCK( pxLock )           /* void vPortSpinlockInitQueue( portSPINLOCK_TYPE *pxLock ) */
#define portINIT_STREAM_BUFFER_SPINLOCK( pxLock )   /* void vPortSpinlockInitSB( portSPINLOCK_TYPE *pxLock ) */
#define portINIT_KERNEL_TASK_SPINLOCK_STATIC        xPortSpinlockStaticInitTask
#define portINIT_KERNEL_ISR_SPINLOCK_STATIC         xPortSpinlockStaticInitISR
#define portINIT_TIMERS_SPINLOCK_STATIC             xPortSpinlockStaticInitTimers
```

### Kernel API

- The following macros have been added to `task.h` to entering/exiting of critical sections by taking multiple spinlocks. `N` in the API indicates the number of locks taken/released
- On critical section entry, the locks are taken in order of arguments supplied (left to right)
- On critical section exit, the locks are released in reverse order of the arguments supplied (right to left).

```c
taskENTER_CRITICAL_GRANULAR_N()
taskENTER_CRITICAL_FROM_ISR_GRANULAR_N()
taskEXIT_CRITICAL_GRANULAR_N()
taskEXIT_CRITICAL_FROM_ISR_GRANULAR_N()
```

## Source Code Changes

- `event_groups.c` critical sections updated to take the event groups own `xEventGroupSpinlock` lock.
- `queue.c` critical sections updated to take the kernel locks and the queues own `xQueueSpinlock`. The kernel locks are required because `xTaskRemoveFromEventList()` accesses kernel data structures
- `stream_buffer.c` critical sections updated to take the stream buffer's own `xStreamBufferSpinlock`
- `tasks.c` critical sections updated to take the kernel spinlocks. Updated `prvCheckForRunStateChangeGranular()` so that a yield only occurs if non-nested critical section calls.
- `timers.c`  critical sections updated to take the shared `xTimerSpinlock` lock.
