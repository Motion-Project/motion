import { useScheduleState } from './useScheduleState';
import { ScheduleGrid } from './ScheduleGrid';
import { ScheduleControls } from './ScheduleControls';
import { getScheduleSummary, isScheduleEmpty } from './scheduleUtils';
import type { SchedulePickerProps } from './scheduleTypes';

export function SchedulePicker({
  value,
  onChange,
  helpText,
  error,
  disabled = false,
}: SchedulePickerProps) {
  const {
    schedule,
    defaultOn,
    action,
    updateSchedule,
    setDefaultOn,
    setAction,
    applyPreset,
    clearAll,
  } = useScheduleState({ value, onChange });

  const isEmpty = isScheduleEmpty(schedule);
  const summary = getScheduleSummary(schedule);

  return (
    <div className="space-y-4">
      {/* Controls */}
      <ScheduleControls
        defaultOn={defaultOn}
        action={action}
        onDefaultOnChange={setDefaultOn}
        onActionChange={setAction}
        onApplyPreset={applyPreset}
        onClearAll={clearAll}
        disabled={disabled}
      />

      {/* Visual Grid */}
      <div className="border border-gray-700 rounded-lg p-4 bg-surface">
        <ScheduleGrid
          schedule={schedule}
          onScheduleChange={updateSchedule}
          disabled={disabled}
        />
      </div>

      {/* Summary */}
      <div className="flex items-center justify-between text-sm">
        <span className="text-gray-400">
          {isEmpty ? (
            <span className="text-yellow-400">No time ranges configured</span>
          ) : (
            <>
              Schedule: <span className="text-white">{summary}</span>
            </>
          )}
        </span>
        {defaultOn ? (
          <span className="text-xs text-gray-500">
            Detection paused during selected times
          </span>
        ) : (
          <span className="text-xs text-gray-500">
            Detection active during selected times
          </span>
        )}
      </div>

      {/* Help text */}
      {helpText && !error && (
        <p className="text-sm text-gray-400">{helpText}</p>
      )}

      {/* Error message */}
      {error && (
        <p className="text-sm text-red-400" role="alert">
          {error}
        </p>
      )}

      {/* Raw value preview (collapsible for debugging) */}
      <details className="text-xs">
        <summary className="cursor-pointer text-gray-500 hover:text-gray-400">
          Show raw schedule format
        </summary>
        <code className="block mt-2 p-2 bg-surface-elevated rounded text-gray-400 break-all">
          {value || '(empty)'}
        </code>
      </details>
    </div>
  );
}
