import type {
  DayKey,
  WeekSchedule,
  ScheduleAction,
  TimeRange,
} from './scheduleTypes';
import {
  DAY_KEYS,
  BLOCKS_PER_DAY,
  MINUTES_PER_BLOCK,
} from './scheduleTypes';

/**
 * Create an empty week schedule
 */
export function createEmptySchedule(): WeekSchedule {
  return {
    sun: new Set(),
    mon: new Set(),
    tue: new Set(),
    wed: new Set(),
    thu: new Set(),
    fri: new Set(),
    sat: new Set(),
  };
}

/**
 * Parse key=value params string into a Map
 * Handles multiple values for the same key (e.g., mon=0900-1200 mon=1400-1700)
 */
function parseParams(value: string): Map<string, string[]> {
  const params = new Map<string, string[]>();

  // Split by whitespace and process each key=value pair
  const parts = value.trim().split(/\s+/);

  for (const part of parts) {
    const eqIndex = part.indexOf('=');
    if (eqIndex === -1) continue;

    const key = part.slice(0, eqIndex).toLowerCase();
    const val = part.slice(eqIndex + 1);

    if (!params.has(key)) {
      params.set(key, []);
    }
    params.get(key)!.push(val);
  }

  return params;
}

/**
 * Convert time range string (HHMM-HHMM) to array of 15-min block indices
 */
function timeRangeToIndices(range: string): number[] {
  if (range.length !== 9 || range[4] !== '-') {
    return [];
  }

  const startHour = parseInt(range.slice(0, 2), 10);
  const startMin = parseInt(range.slice(2, 4), 10);
  const endHour = parseInt(range.slice(5, 7), 10);
  const endMin = parseInt(range.slice(7, 9), 10);

  // Validate
  if (
    isNaN(startHour) || isNaN(startMin) || isNaN(endHour) || isNaN(endMin) ||
    startHour < 0 || startHour > 23 ||
    startMin < 0 || startMin > 59 ||
    endHour < 0 || endHour > 23 ||
    endMin < 0 || endMin > 59
  ) {
    return [];
  }

  const startIndex = startHour * 4 + Math.floor(startMin / MINUTES_PER_BLOCK);
  // End index includes the block containing the end time
  const endIndex = endHour * 4 + Math.floor(endMin / MINUTES_PER_BLOCK);

  const indices: number[] = [];
  for (let i = startIndex; i <= endIndex && i < BLOCKS_PER_DAY; i++) {
    indices.push(i);
  }
  return indices;
}

/**
 * Parse backend format string to internal schedule representation
 */
export function parseBackendFormat(value: string): {
  schedule: WeekSchedule;
  defaultOn: boolean;
  action: ScheduleAction;
} {
  const schedule = createEmptySchedule();

  if (!value || !value.trim()) {
    return { schedule, defaultOn: true, action: 'pause' };
  }

  const params = parseParams(value);

  // Parse default and action
  const defaultValues = params.get('default');
  const defaultOn = !defaultValues || defaultValues[0] !== 'false';

  const actionValues = params.get('action');
  const action: ScheduleAction = actionValues?.[0] === 'stop' ? 'stop' : 'pause';

  // Process time ranges for each day
  for (const [key, ranges] of params.entries()) {
    if (key === 'default' || key === 'action') continue;

    // Determine which days this applies to
    let targetDays: DayKey[];

    if (key === 'sun-sat') {
      targetDays = [...DAY_KEYS];
    } else if (key === 'mon-fri') {
      targetDays = ['mon', 'tue', 'wed', 'thu', 'fri'];
    } else if (DAY_KEYS.includes(key as DayKey)) {
      targetDays = [key as DayKey];
    } else {
      continue; // Unknown key, skip
    }

    // Add indices for each range to each target day
    for (const day of targetDays) {
      for (const range of ranges) {
        const indices = timeRangeToIndices(range);
        for (const idx of indices) {
          schedule[day].add(idx);
        }
      }
    }
  }

  return { schedule, defaultOn, action };
}

/**
 * Convert block index to time range end
 * Block 0 = 00:00-00:14, Block 1 = 00:15-00:29, etc.
 */
function indexToEndTime(index: number): { hour: number; min: number } {
  const startMin = index * MINUTES_PER_BLOCK;
  const endMin = startMin + MINUTES_PER_BLOCK - 1;
  return {
    hour: Math.floor(endMin / 60),
    min: endMin % 60,
  };
}

/**
 * Convert block index to start time
 */
function indexToStartTime(index: number): { hour: number; min: number } {
  const totalMin = index * MINUTES_PER_BLOCK;
  return {
    hour: Math.floor(totalMin / 60),
    min: totalMin % 60,
  };
}

/**
 * Convert array of indices to array of contiguous time ranges
 */
function indicesToRanges(indices: number[]): TimeRange[] {
  if (indices.length === 0) return [];

  // Sort indices
  const sorted = [...indices].sort((a, b) => a - b);

  const ranges: TimeRange[] = [];
  let rangeStart = sorted[0];
  let rangeEnd = sorted[0];

  for (let i = 1; i < sorted.length; i++) {
    if (sorted[i] === rangeEnd + 1) {
      // Contiguous, extend range
      rangeEnd = sorted[i];
    } else {
      // Gap found, save current range and start new one
      const start = indexToStartTime(rangeStart);
      const end = indexToEndTime(rangeEnd);
      ranges.push({
        startHour: start.hour,
        startMin: start.min,
        endHour: end.hour,
        endMin: end.min,
      });
      rangeStart = sorted[i];
      rangeEnd = sorted[i];
    }
  }

  // Don't forget the last range
  const start = indexToStartTime(rangeStart);
  const end = indexToEndTime(rangeEnd);
  ranges.push({
    startHour: start.hour,
    startMin: start.min,
    endHour: end.hour,
    endMin: end.min,
  });

  return ranges;
}

/**
 * Format TimeRange to HHMM-HHMM string (backend format)
 */
function formatTimeRange(range: TimeRange): string {
  const sh = String(range.startHour).padStart(2, '0');
  const sm = String(range.startMin).padStart(2, '0');
  const eh = String(range.endHour).padStart(2, '0');
  const em = String(range.endMin).padStart(2, '0');
  return `${sh}${sm}-${eh}${em}`;
}

/**
 * Convert internal schedule to backend format string
 */
export function toBackendFormat(
  schedule: WeekSchedule,
  defaultOn: boolean,
  action: ScheduleAction
): string {
  const parts: string[] = [];

  parts.push(`default=${defaultOn}`);
  parts.push(`action=${action}`);

  // Check if all days are identical - use sun-sat shortcut
  const allDaysSame = DAY_KEYS.every(
    (day) => setsEqual(schedule[day], schedule.sun)
  );

  if (allDaysSame && schedule.sun.size > 0) {
    const ranges = indicesToRanges(Array.from(schedule.sun));
    for (const range of ranges) {
      parts.push(`sun-sat=${formatTimeRange(range)}`);
    }
    return parts.join(' ');
  }

  // Check if Mon-Fri are identical - use mon-fri shortcut
  const weekdaysSame = (['mon', 'tue', 'wed', 'thu', 'fri'] as DayKey[]).every(
    (day) => setsEqual(schedule[day], schedule.mon)
  );

  if (weekdaysSame && schedule.mon.size > 0) {
    const ranges = indicesToRanges(Array.from(schedule.mon));
    for (const range of ranges) {
      parts.push(`mon-fri=${formatTimeRange(range)}`);
    }

    // Add weekend days separately if different
    for (const day of ['sat', 'sun'] as DayKey[]) {
      if (schedule[day].size > 0) {
        const dayRanges = indicesToRanges(Array.from(schedule[day]));
        for (const range of dayRanges) {
          parts.push(`${day}=${formatTimeRange(range)}`);
        }
      }
    }
    return parts.join(' ');
  }

  // Output each day individually
  for (const day of DAY_KEYS) {
    if (schedule[day].size === 0) continue;

    const ranges = indicesToRanges(Array.from(schedule[day]));
    for (const range of ranges) {
      parts.push(`${day}=${formatTimeRange(range)}`);
    }
  }

  return parts.join(' ');
}

/**
 * Check if two Sets are equal
 */
function setsEqual(a: Set<number>, b: Set<number>): boolean {
  if (a.size !== b.size) return false;
  for (const item of a) {
    if (!b.has(item)) return false;
  }
  return true;
}

/**
 * Get human-readable time from block index
 */
export function indexToTimeString(index: number): string {
  const { hour, min } = indexToStartTime(index);
  const hourStr = String(hour).padStart(2, '0');
  const minStr = String(min).padStart(2, '0');
  return `${hourStr}:${minStr}`;
}

/**
 * Format a time range for display
 */
export function formatRangeForDisplay(startIndex: number, endIndex: number): string {
  const start = indexToStartTime(startIndex);
  const end = indexToEndTime(endIndex);

  const formatTime = (h: number, m: number) => {
    const hour12 = h === 0 ? 12 : h > 12 ? h - 12 : h;
    const ampm = h < 12 ? 'am' : 'pm';
    const minStr = m === 0 ? '' : `:${String(m).padStart(2, '0')}`;
    return `${hour12}${minStr}${ampm}`;
  };

  return `${formatTime(start.hour, start.min)} - ${formatTime(end.hour, end.min)}`;
}

/**
 * Check if schedule is empty (no time ranges selected)
 */
export function isScheduleEmpty(schedule: WeekSchedule): boolean {
  return DAY_KEYS.every((day) => schedule[day].size === 0);
}

/**
 * Get summary of schedule for display
 */
export function getScheduleSummary(schedule: WeekSchedule): string {
  const activeDays = DAY_KEYS.filter((day) => schedule[day].size > 0);

  if (activeDays.length === 0) {
    return 'No time ranges selected';
  }

  if (activeDays.length === 7) {
    return 'All days configured';
  }

  const dayLabels = activeDays.map((d) => d.charAt(0).toUpperCase() + d.slice(1, 3));
  return dayLabels.join(', ');
}
