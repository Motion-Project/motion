import { useState, useCallback, useEffect, useRef } from 'react';
import { ScheduleGridDay } from './ScheduleGridDay';
import { ScheduleTimeLabels } from './ScheduleTimeLabels';
import type { ScheduleGridProps, DragState, DayKey, WeekSchedule } from './scheduleTypes';
import { DAY_KEYS, DAY_LABELS, BLOCKS_PER_DAY } from './scheduleTypes';
import { formatRangeForDisplay } from './scheduleUtils';

const INITIAL_DRAG_STATE: DragState = {
  isDragging: false,
  startDay: null,
  startIndex: null,
  currentIndex: null,
  selectMode: true,
};

export function ScheduleGrid({
  schedule,
  onScheduleChange,
  disabled = false,
}: ScheduleGridProps) {
  const [dragState, setDragState] = useState<DragState>(INITIAL_DRAG_STATE);
  const gridRef = useRef<HTMLDivElement>(null);

  // Handle pointer down (works for both mouse and touch)
  const handlePointerDown = useCallback(
    (day: DayKey, index: number) => {
      if (disabled) return;

      const isCurrentlySelected = schedule[day].has(index);

      setDragState({
        isDragging: true,
        startDay: day,
        startIndex: index,
        currentIndex: index,
        selectMode: !isCurrentlySelected, // Toggle mode based on cell state
      });
    },
    [schedule, disabled]
  );

  // Handle pointer move
  const handlePointerMove = useCallback(
    (day: DayKey, index: number) => {
      if (!dragState.isDragging || day !== dragState.startDay) return;

      setDragState((prev) => ({ ...prev, currentIndex: index }));
    },
    [dragState.isDragging, dragState.startDay]
  );

  // Handle pointer up - commit the selection
  const handlePointerUp = useCallback(() => {
    if (
      !dragState.isDragging ||
      dragState.startDay === null ||
      dragState.startIndex === null ||
      dragState.currentIndex === null
    ) {
      setDragState(INITIAL_DRAG_STATE);
      return;
    }

    const newSchedule: WeekSchedule = {
      sun: new Set(schedule.sun),
      mon: new Set(schedule.mon),
      tue: new Set(schedule.tue),
      wed: new Set(schedule.wed),
      thu: new Set(schedule.thu),
      fri: new Set(schedule.fri),
      sat: new Set(schedule.sat),
    };

    const daySet = new Set(schedule[dragState.startDay]);

    const [from, to] =
      dragState.startIndex <= dragState.currentIndex
        ? [dragState.startIndex, dragState.currentIndex]
        : [dragState.currentIndex, dragState.startIndex];

    for (let i = from; i <= to; i++) {
      if (dragState.selectMode) {
        daySet.add(i);
      } else {
        daySet.delete(i);
      }
    }

    newSchedule[dragState.startDay] = daySet;
    onScheduleChange(newSchedule);

    setDragState(INITIAL_DRAG_STATE);
  }, [dragState, schedule, onScheduleChange]);

  // Cancel drag on escape
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape' && dragState.isDragging) {
        setDragState(INITIAL_DRAG_STATE);
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [dragState.isDragging]);

  // Prevent scrolling and text selection while dragging
  useEffect(() => {
    if (dragState.isDragging) {
      document.body.style.touchAction = 'none';
      document.body.style.userSelect = 'none';
    } else {
      document.body.style.touchAction = '';
      document.body.style.userSelect = '';
    }

    return () => {
      document.body.style.touchAction = '';
      document.body.style.userSelect = '';
    };
  }, [dragState.isDragging]);

  // Toggle entire day on header click
  const handleDayHeaderClick = useCallback(
    (day: DayKey) => {
      if (disabled) return;

      const newSchedule: WeekSchedule = {
        sun: new Set(schedule.sun),
        mon: new Set(schedule.mon),
        tue: new Set(schedule.tue),
        wed: new Set(schedule.wed),
        thu: new Set(schedule.thu),
        fri: new Set(schedule.fri),
        sat: new Set(schedule.sat),
      };

      if (newSchedule[day].size === BLOCKS_PER_DAY) {
        // All selected, clear
        newSchedule[day] = new Set();
      } else {
        // Select all
        newSchedule[day] = new Set(
          Array.from({ length: BLOCKS_PER_DAY }, (_, i) => i)
        );
      }

      onScheduleChange(newSchedule);
    },
    [schedule, onScheduleChange, disabled]
  );

  // Get drag preview text
  const getDragPreviewText = () => {
    if (
      !dragState.isDragging ||
      dragState.startIndex === null ||
      dragState.currentIndex === null
    ) {
      return null;
    }

    const from = Math.min(dragState.startIndex, dragState.currentIndex);
    const to = Math.max(dragState.startIndex, dragState.currentIndex);

    return formatRangeForDisplay(from, to);
  };

  const dragPreview = getDragPreviewText();

  return (
    <div className="select-none">
      {/* Day headers */}
      <div className="flex mb-1">
        {/* Spacer for time labels */}
        <div className="w-8 shrink-0" />

        <div className="grid grid-cols-7 gap-px flex-1">
          {DAY_KEYS.map((day) => {
            const isFullySelected = schedule[day].size === BLOCKS_PER_DAY;
            const hasSelection = schedule[day].size > 0;

            return (
              <button
                key={day}
                type="button"
                onClick={() => handleDayHeaderClick(day)}
                disabled={disabled}
                className={`text-xs font-medium py-1 rounded-t transition-colors ${
                  isFullySelected
                    ? 'bg-primary text-white'
                    : hasSelection
                    ? 'bg-primary/30 text-primary'
                    : 'bg-surface-elevated text-gray-400 hover:bg-surface-hover'
                } ${disabled ? 'cursor-not-allowed opacity-50' : 'cursor-pointer'}`}
                title={`Click to ${isFullySelected ? 'clear' : 'select all'} ${DAY_LABELS[day].full}`}
              >
                <span className="hidden sm:inline">{DAY_LABELS[day].short}</span>
                <span className="sm:hidden">{day.charAt(0).toUpperCase()}</span>
              </button>
            );
          })}
        </div>
      </div>

      {/* Grid with time labels */}
      <div className="flex">
        {/* Time labels */}
        <div className="w-8 shrink-0">
          <ScheduleTimeLabels />
        </div>

        {/* Grid */}
        <div
          ref={gridRef}
          className={`grid grid-cols-7 gap-px bg-surface-elevated flex-1 rounded ${
            disabled ? 'opacity-50' : ''
          }`}
          style={{ touchAction: 'none' }}
          onPointerUp={handlePointerUp}
          onPointerLeave={handlePointerUp}
          onPointerCancel={handlePointerUp}
        >
          {DAY_KEYS.map((day) => (
            <ScheduleGridDay
              key={day}
              day={day}
              schedule={schedule[day]}
              dragState={dragState}
              onPointerDown={(index) => handlePointerDown(day, index)}
              onPointerMove={(index) => handlePointerMove(day, index)}
            />
          ))}
        </div>
      </div>

      {/* Drag preview tooltip */}
      {dragPreview && (
        <div className="mt-2 text-center">
          <span className="text-xs bg-surface-elevated px-2 py-1 rounded text-gray-300">
            {dragState.selectMode ? 'Selecting' : 'Deselecting'}:{' '}
            <span className="text-white font-medium">{dragPreview}</span>
          </span>
        </div>
      )}

      {/* Instructions */}
      <div className="mt-2 text-xs text-gray-500 text-center">
        Click and drag to select time ranges. Click day header to toggle entire day.
      </div>
    </div>
  );
}
