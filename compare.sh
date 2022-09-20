#!/bin/sh

MERGE_LIST="
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

PREV_BRANCH=""
for BRANCH in $MERGE_LIST
do
    LOCAL_BRANCH=$(echo $BRANCH | awk -F/ '{ print $2 }' )
    echo $LOCAL_BRANCH
    git checkout $LOCAL_BRANCH
    git pull

    if [ "$PREV_BRANCH" != "" ]; then
        # git diff -w $PREV_BRANCH..$LOCAL_BRANCH
        git merge --no-edit $PREV_BRANCH
    fi
    
    PREV_BRANCH=$LOCAL_BRANCH

    git push -u push_origin $LOCAL_BRANCH
done
