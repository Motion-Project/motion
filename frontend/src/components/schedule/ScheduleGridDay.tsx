import { memo, useMemo } from 'react';
import { ScheduleGridCell } from './ScheduleGridCell';
import type { ScheduleGridDayProps } from './scheduleTypes';
import { BLOCKS_PER_DAY } from './scheduleTypes';

export const ScheduleGridDay = memo(function ScheduleGridDay({
  day,
  schedule,
  dragState,
  onPointerDown,
  onPointerMove,
}: ScheduleGridDayProps) {
  // Calculate drag range for this day
  const dragRange = useMemo(() => {
    if (!dragState.isDragging || dragState.startDay !== day) {
      return null;
    }
    const start = dragState.startIndex!;
    const end = dragState.currentIndex!;
    return { from: Math.min(start, end), to: Math.max(start, end) };
  }, [
    dragState.isDragging,
    dragState.startDay,
    dragState.startIndex,
    dragState.currentIndex,
    day,
  ]);

  // Pre-compute which cells are in drag range for efficient rendering
  const cellStates = useMemo(() => {
    return Array.from({ length: BLOCKS_PER_DAY }, (_, i) => ({
      isSelected: schedule.has(i),
      isInDragRange:
        dragRange !== null && i >= dragRange.from && i <= dragRange.to,
      isHourBoundary: i % 4 === 0,
    }));
  }, [schedule, dragRange]);

  return (
    <div className="flex flex-col">
      {cellStates.map((state, index) => (
        <ScheduleGridCell
          key={index}
          index={index}
          isSelected={state.isSelected}
          isInDragRange={state.isInDragRange}
          isDragSelect={dragState.selectMode}
          isHourBoundary={state.isHourBoundary}
          onPointerDown={() => onPointerDown(index)}
          onPointerEnter={() => onPointerMove(index)}
        />
      ))}
    </div>
  );
});
