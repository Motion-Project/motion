import { useState } from 'react';
import { FormSection, FormInput, FormSelect, FormSlider } from '@/components/form';
import { captureModeToMotion, motionToCaptureMode } from '@/utils/translations';

export interface PictureSettingsProps {
  config: Record<string, { value: string | number | boolean }>;
  onChange: (param: string, value: string | number | boolean) => void;
  getError?: (param: string) => string | undefined;
}

export function PictureSettings({ config, onChange, getError }: PictureSettingsProps) {
  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue;
  };

  // Determine current capture mode
  const pictureOutput = String(getValue('picture_output', 'off'));
  const snapshotInterval = Number(getValue('snapshot_interval', 0));
  const currentMode = motionToCaptureMode(pictureOutput, snapshotInterval);

  const [selectedMode, setSelectedMode] = useState(currentMode);

  const handleCaptureModeChange = (mode: string) => {
    setSelectedMode(mode);
    const motionParams = captureModeToMotion(mode);

    // Apply the mode's parameter changes
    onChange('picture_output', motionParams.picture_output);
    if (motionParams.snapshot_interval !== undefined) {
      onChange('snapshot_interval', motionParams.snapshot_interval);
    }
  };

  // Capture mode options
  const captureModeOptions = [
    { value: 'off', label: 'Off' },
    { value: 'motion-triggered', label: 'Motion Triggered (all frames)' },
    { value: 'motion-triggered-one', label: 'Motion Triggered (first frame only)' },
    { value: 'best', label: 'Best Quality Frame' },
    { value: 'center', label: 'Center Frame' },
    { value: 'interval-snapshots', label: 'Interval Snapshots' },
    { value: 'manual', label: 'Manual Only' },
  ];

  // Format code reference for help text
  const formatCodes = [
    '%Y - Year (4 digits)',
    '%m - Month (01-12)',
    '%d - Day (01-31)',
    '%H - Hour (00-23)',
    '%M - Minute (00-59)',
    '%S - Second (00-59)',
    '%q - Frame number',
    '%v - Event number',
    '%$ - Camera name',
  ].join(', ');

  return (
    <FormSection
      title="Picture Settings"
      description="Configure picture capture and snapshots"
      collapsible
      defaultOpen={false}
    >
      <div className="space-y-4">
        <FormSelect
          label="Capture Mode"
          value={selectedMode}
          onChange={handleCaptureModeChange}
          options={captureModeOptions}
          helpText="When to capture still images during motion events"
        />

        {/* Show conversion explanation */}
        <div className="text-xs text-gray-400 bg-surface-elevated p-3 rounded">
          <strong>Current settings:</strong> picture_output={pictureOutput}
          {snapshotInterval > 0 && `, snapshot_interval=${snapshotInterval}s`}
        </div>

        {/* Warning and limits for all-frames mode */}
        {selectedMode === 'motion-triggered' && (
          <>
            <div className="text-xs text-yellow-300 bg-yellow-900/30 border border-yellow-700 p-3 rounded">
              <strong>Warning:</strong> This mode captures every frame during motion. At 15fps,
              continuous motion can generate 900+ pictures per minute. Configure limits below
              to prevent runaway capture.
            </div>

            <FormInput
              label="Max Pictures Per Event"
              value={String(getValue('picture_max_per_event', 0))}
              onChange={(val) => onChange('picture_max_per_event', Number(val))}
              type="number"
              min="0"
              max="100000"
              helpText="Maximum pictures per motion event (0 = unlimited)"
              error={getError?.('picture_max_per_event')}
            />

            <FormInput
              label="Min Interval Between Pictures (ms)"
              value={String(getValue('picture_min_interval', 0))}
              onChange={(val) => onChange('picture_min_interval', Number(val))}
              type="number"
              min="0"
              max="60000"
              helpText="Minimum milliseconds between captures (0 = no limit). 1000ms = 1 picture/second."
              error={getError?.('picture_min_interval')}
            />
          </>
        )}

        {selectedMode === 'interval-snapshots' && (
          <FormInput
            label="Snapshot Interval (seconds)"
            value={String(getValue('snapshot_interval', 60))}
            onChange={(val) => onChange('snapshot_interval', Number(val))}
            type="number"
            min="1"
            helpText="Seconds between snapshots (independent of motion)"
            error={getError?.('snapshot_interval')}
          />
        )}

        <FormSlider
          label="Picture Quality"
          value={Number(getValue('picture_quality', 75))}
          onChange={(val) => onChange('picture_quality', val)}
          min={1}
          max={100}
          unit="%"
          helpText="JPEG quality (1-100). Higher = better quality, larger files."
          error={getError?.('picture_quality')}
        />

        <FormInput
          label="Picture Filename Pattern"
          value={String(getValue('picture_filename', '%Y%m%d%H%M%S-%q'))}
          onChange={(val) => onChange('picture_filename', val)}
          helpText={`Format codes: ${formatCodes}`}
          error={getError?.('picture_filename')}
        />

        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium mb-2 text-sm">Format Code Reference</h4>
          <div className="text-xs text-gray-400 space-y-2">
            <p className="font-medium text-gray-300">Dynamic Folder Examples (Recommended):</p>
            <p><code>%Y-%m-%d/%H%M%S-%q</code> → <code>2025-01-29/143022-05.jpg</code></p>
            <p><code>%Y/%m/%d/%H%M%S</code> → <code>2025/01/29/143022.jpg</code></p>
            <p><code>%$/%Y-%m-%d/%H%M%S</code> → <code>Camera1/2025-01-29/143022.jpg</code></p>
            <p className="mt-2 font-medium text-gray-300">Flat Structure:</p>
            <p><code>%Y%m%d%H%M%S-%q</code> → <code>20250129143022-05.jpg</code></p>
            <p className="mt-2">Available codes: {formatCodes}</p>
            <p className="mt-2 text-yellow-200"><strong>Tip:</strong> Using date-based folders like <code>%Y-%m-%d/</code> keeps files organized and makes browsing faster.</p>
          </div>
        </div>
      </div>
    </FormSection>
  );
}
