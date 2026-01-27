// Internal representation: Set of selected 15-minute block indices per day
// Index 0 = 00:00-00:15, Index 95 = 23:45-24:00
export type DaySchedule = Set<number>;

export interface WeekSchedule {
  sun: DaySchedule;
  mon: DaySchedule;
  tue: DaySchedule;
  wed: DaySchedule;
  thu: DaySchedule;
  fri: DaySchedule;
  sat: DaySchedule;
}

export type DayKey = keyof WeekSchedule;

export const DAY_KEYS: readonly DayKey[] = ['sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat'] as const;

export const DAY_LABELS: Record<DayKey, { full: string; short: string }> = {
  sun: { full: 'Sunday', short: 'Sun' },
  mon: { full: 'Monday', short: 'Mon' },
  tue: { full: 'Tuesday', short: 'Tue' },
  wed: { full: 'Wednesday', short: 'Wed' },
  thu: { full: 'Thursday', short: 'Thu' },
  fri: { full: 'Friday', short: 'Fri' },
  sat: { full: 'Saturday', short: 'Sat' },
};

export type ScheduleAction = 'pause' | 'stop';

export interface SchedulePickerProps {
  value: string;
  onChange: (value: string) => void;
  helpText?: string;
  error?: string;
  disabled?: boolean;
}

export interface ScheduleGridProps {
  schedule: WeekSchedule;
  onScheduleChange: (schedule: WeekSchedule) => void;
  disabled?: boolean;
}

export interface ScheduleGridDayProps {
  day: DayKey;
  schedule: DaySchedule;
  dragState: DragState;
  onPointerDown: (index: number) => void;
  onPointerMove: (index: number) => void;
}

export interface ScheduleGridCellProps {
  index: number;
  isSelected: boolean;
  isInDragRange: boolean;
  isDragSelect: boolean;
  isHourBoundary: boolean;
  onPointerDown: () => void;
  onPointerEnter: () => void;
}

export interface DragState {
  isDragging: boolean;
  startDay: DayKey | null;
  startIndex: number | null;
  currentIndex: number | null;
  selectMode: boolean; // true = selecting, false = deselecting
}

export interface TimeRange {
  startHour: number;
  startMin: number;
  endHour: number;
  endMin: number;
}

export interface SchedulePreset {
  label: string;
  value: string;
  description: string;
}

export const BLOCKS_PER_DAY = 96; // 24 hours * 4 blocks per hour
export const MINUTES_PER_BLOCK = 15;
