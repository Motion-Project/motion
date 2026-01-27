import { memo } from 'react';

// Generate hour labels (every 2 hours for cleaner display)
const HOUR_LABELS = [0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22];

function formatHour(hour: number): string {
  if (hour === 0) return '12a';
  if (hour === 12) return '12p';
  if (hour < 12) return `${hour}a`;
  return `${hour - 12}p`;
}

export const ScheduleTimeLabels = memo(function ScheduleTimeLabels() {
  return (
    <div className="flex flex-col pr-1 text-xs text-gray-500 select-none">
      {HOUR_LABELS.map((hour) => (
        <div
          key={hour}
          className="flex items-start justify-end"
          style={{ height: '48px' }} // 8 blocks * 6px = 48px per 2-hour segment
        >
          <span className="-mt-1.5">{formatHour(hour)}</span>
        </div>
      ))}
    </div>
  );
});
