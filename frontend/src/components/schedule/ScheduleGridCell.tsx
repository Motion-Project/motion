import { memo } from 'react';
import type { ScheduleGridCellProps } from './scheduleTypes';

export const ScheduleGridCell = memo(function ScheduleGridCell({
  isSelected,
  isInDragRange,
  isDragSelect,
  isHourBoundary,
  onPointerDown,
  onPointerEnter,
}: ScheduleGridCellProps) {
  // Determine visual state
  let bgClass: string;

  if (isInDragRange) {
    // Show preview of what will happen after drag completes
    bgClass = isDragSelect
      ? 'bg-primary/70' // Will be selected (lighter preview)
      : 'bg-surface-hover'; // Will be deselected
  } else if (isSelected) {
    bgClass = 'bg-primary';
  } else {
    bgClass = 'bg-surface';
  }

  // Hour boundary gets a subtle top border
  const borderClass = isHourBoundary ? 'border-t border-gray-700/50' : '';

  return (
    <div
      className={`h-[6px] ${bgClass} ${borderClass} cursor-pointer transition-colors duration-75`}
      onPointerDown={onPointerDown}
      onPointerEnter={onPointerEnter}
    />
  );
});
