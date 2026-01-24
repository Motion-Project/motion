import { useMemo, useCallback } from 'react';
import type {
  DayKey,
  WeekSchedule,
  ScheduleAction,
} from './scheduleTypes';
import { BLOCKS_PER_DAY } from './scheduleTypes';
import {
  parseBackendFormat,
  toBackendFormat,
  createEmptySchedule,
} from './scheduleUtils';

interface UseScheduleStateOptions {
  value: string;
  onChange: (value: string) => void;
}

export function useScheduleState({ value, onChange }: UseScheduleStateOptions) {
  // Parse backend format to internal state
  const { schedule, defaultOn, action } = useMemo(
    () => parseBackendFormat(value),
    [value]
  );

  // Update schedule and convert back to backend format
  const updateSchedule = useCallback(
    (newSchedule: WeekSchedule) => {
      const backendValue = toBackendFormat(newSchedule, defaultOn, action);
      onChange(backendValue);
    },
    [onChange, defaultOn, action]
  );

  // Toggle all blocks for a day
  const toggleDay = useCallback(
    (day: DayKey) => {
      const newSchedule = cloneSchedule(schedule);

      if (newSchedule[day].size === BLOCKS_PER_DAY) {
        // All selected, clear the day
        newSchedule[day] = new Set();
      } else {
        // Not all selected, select all
        newSchedule[day] = new Set(
          Array.from({ length: BLOCKS_PER_DAY }, (_, i) => i)
        );
      }

      updateSchedule(newSchedule);
    },
    [schedule, updateSchedule]
  );

  // Set a range of blocks for a day
  const setRange = useCallback(
    (
      day: DayKey,
      startIndex: number,
      endIndex: number,
      selected: boolean
    ) => {
      const newSchedule = cloneSchedule(schedule);
      const newDaySet = new Set(schedule[day]);

      const [from, to] =
        startIndex <= endIndex
          ? [startIndex, endIndex]
          : [endIndex, startIndex];

      for (let i = from; i <= to; i++) {
        if (selected) {
          newDaySet.add(i);
        } else {
          newDaySet.delete(i);
        }
      }

      newSchedule[day] = newDaySet;
      updateSchedule(newSchedule);
    },
    [schedule, updateSchedule]
  );

  // Clear a specific day
  const clearDay = useCallback(
    (day: DayKey) => {
      const newSchedule = cloneSchedule(schedule);
      newSchedule[day] = new Set();
      updateSchedule(newSchedule);
    },
    [schedule, updateSchedule]
  );

  // Clear all days
  const clearAll = useCallback(() => {
    updateSchedule(createEmptySchedule());
  }, [updateSchedule]);

  // Set default mode (true = detection on by default, schedule defines off times)
  const setDefaultOn = useCallback(
    (on: boolean) => {
      const backendValue = toBackendFormat(schedule, on, action);
      onChange(backendValue);
    },
    [schedule, action, onChange]
  );

  // Set action (pause or stop)
  const setAction = useCallback(
    (newAction: ScheduleAction) => {
      const backendValue = toBackendFormat(schedule, defaultOn, newAction);
      onChange(backendValue);
    },
    [schedule, defaultOn, onChange]
  );

  // Apply a preset value directly
  const applyPreset = useCallback(
    (presetValue: string) => {
      onChange(presetValue);
    },
    [onChange]
  );

  return {
    schedule,
    defaultOn,
    action,
    updateSchedule,
    toggleDay,
    setRange,
    clearDay,
    clearAll,
    setDefaultOn,
    setAction,
    applyPreset,
  };
}

/**
 * Deep clone a WeekSchedule (Sets are reference types)
 */
function cloneSchedule(schedule: WeekSchedule): WeekSchedule {
  return {
    sun: new Set(schedule.sun),
    mon: new Set(schedule.mon),
    tue: new Set(schedule.tue),
    wed: new Set(schedule.wed),
    thu: new Set(schedule.thu),
    fri: new Set(schedule.fri),
    sat: new Set(schedule.sat),
  };
}
