import { memo } from 'react';
import type { ScheduleAction, SchedulePreset } from './scheduleTypes';

interface ScheduleControlsProps {
  defaultOn: boolean;
  action: ScheduleAction;
  onDefaultOnChange: (value: boolean) => void;
  onActionChange: (value: ScheduleAction) => void;
  onApplyPreset: (value: string) => void;
  onClearAll: () => void;
  disabled?: boolean;
}

const PRESETS: SchedulePreset[] = [
  {
    label: 'Business Hours',
    value: 'default=true action=pause mon-fri=0900-1700',
    description: 'Pause Mon-Fri 9am-5pm',
  },
  {
    label: 'Night Watch',
    value: 'default=false action=pause sun-sat=1800-2359 sun-sat=0000-0600',
    description: 'Active 6pm-6am only',
  },
  {
    label: 'Weekends Only',
    value: 'default=false action=pause sat=0000-2359 sun=0000-2359',
    description: 'Active Sat & Sun only',
  },
  {
    label: 'Always On',
    value: 'default=true action=pause',
    description: 'No schedule restrictions',
  },
];

export const ScheduleControls = memo(function ScheduleControls({
  defaultOn,
  action,
  onDefaultOnChange,
  onActionChange,
  onApplyPreset,
  onClearAll,
  disabled = false,
}: ScheduleControlsProps) {
  return (
    <div className="space-y-4">
      {/* Mode Controls */}
      <div className="grid grid-cols-2 gap-4">
        {/* Default Mode */}
        <div>
          <label className="block text-xs text-gray-400 mb-1">
            Detection Default
          </label>
          <select
            value={defaultOn ? 'on' : 'off'}
            onChange={(e) => onDefaultOnChange(e.target.value === 'on')}
            disabled={disabled}
            className="w-full bg-surface-elevated border border-gray-700 rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-primary disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <option value="on">On by default</option>
            <option value="off">Off by default</option>
          </select>
          <p className="text-xs text-gray-500 mt-1">
            {defaultOn
              ? 'Schedule defines when detection is paused'
              : 'Schedule defines when detection is active'}
          </p>
        </div>

        {/* Action Type */}
        <div>
          <label className="block text-xs text-gray-400 mb-1">
            Schedule Action
          </label>
          <select
            value={action}
            onChange={(e) => onActionChange(e.target.value as ScheduleAction)}
            disabled={disabled}
            className="w-full bg-surface-elevated border border-gray-700 rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-primary disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <option value="pause">Pause detection</option>
            <option value="stop">Stop camera</option>
          </select>
          <p className="text-xs text-gray-500 mt-1">
            {action === 'pause'
              ? 'Camera runs but ignores motion'
              : 'Camera completely stops during schedule'}
          </p>
        </div>
      </div>

      {/* Quick Presets */}
      <div>
        <label className="block text-xs text-gray-400 mb-2">Quick Presets</label>
        <div className="flex flex-wrap gap-2">
          {PRESETS.map((preset) => (
            <button
              key={preset.label}
              type="button"
              onClick={() => onApplyPreset(preset.value)}
              disabled={disabled}
              className="px-3 py-1.5 text-xs bg-surface-elevated hover:bg-surface-hover border border-gray-700 rounded transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
              title={preset.description}
            >
              {preset.label}
            </button>
          ))}
          <button
            type="button"
            onClick={onClearAll}
            disabled={disabled}
            className="px-3 py-1.5 text-xs bg-danger/20 hover:bg-danger/30 text-danger border border-danger/30 rounded transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
            title="Clear all time ranges"
          >
            Clear All
          </button>
        </div>
      </div>
    </div>
  );
});
