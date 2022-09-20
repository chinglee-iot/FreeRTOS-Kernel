#!/bin/sh

BRANCH_LIST="
  origin/smp-integration
  origin/smp-vTaskYieldWithinAPI
  origin/smp-xTaskRunState
  origin/smp-pxCurrentTCBs
  origin/smp-dev-critical-section
  origin/smp-dev-yield
  origin/smp-dev-idleTask
  origin/smp-dev-suspend-resume-scheduler
  origin/smp-dev-increment-tick
  origin/smp-dev-prvYieldForTask-usage
  origin/smp-dev-prvAddNewTaskToReadyList
  origin/smp-dev-vTaskSwitchContext
  origin/smp-dev-xTaskDelete
  origin/smp-dev-vTaskSuspend
  origin/smp-dev-prvSelectHighestPriorityTask
  origin/smp-dev-prvCheckTasksWaitingTermination
  origin/smp-dev-yieldWithinAPI-criticalSection
  origin/smp-dev-vTaskPrioritySet
  origin/smp-dev-xTimerGenericCommand
  origin/smp-dev-RP2040
  origin/smp-dev-eTaskGetState
  origin/smp-dev-multiple-priorities
  origin/smp-dev-task-preemption-disable
  origin/smp-dev-core-affinity
  origin/smp-dev-complete
"

for BRANCH in $BRANCH_LIST
do
    LOCAL_BRANCH=$(echo $BRANCH | awk -F/ '{ print $2 }' )
    echo $LOCAL_BRANCH
    git checkout $BRANCH
    git checkout -b $LOCAL_BRANCH
    for MERGE_BRANCH in $BRANCH_LIST
    do
        if [ "$BRANCH" == "$MERGE_BRANCH" ]; then
            break
        fi
        LOCAL_MERGE_BRANCH=$(echo $MERGE_BRANCH | awk -F/ '{ print $2 }' )
        echo "--> "$LOCAL_MERGE_BRANCH
        git merge --no-edit $LOCAL_MERGE_BRANCH
    done
done
