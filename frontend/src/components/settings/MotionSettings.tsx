import { FormSection, FormInput, FormSelect, FormToggle, FormSlider } from '@/components/form';
import { LOCATE_MOTION_MODES } from '@/utils/parameterMappings';
import { percentToPixels, pixelsToPercent } from '@/utils/translations';

export interface MotionSettingsProps {
  config: Record<string, { value: string | number | boolean }>;
  onChange: (param: string, value: string | number | boolean) => void;
  getError?: (param: string) => string | undefined;
}

export function MotionSettings({ config, onChange, getError }: MotionSettingsProps) {
  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue;
  };

  // Get current resolution for threshold percentage conversion
  const width = Number(getValue('width', 640));
  const height = Number(getValue('height', 480));
  const thresholdPixels = Number(getValue('threshold', 1500));
  const thresholdPercent = pixelsToPercent(thresholdPixels, width, height);

  const handleThresholdPercentChange = (percentValue: string) => {
    const percent = Number(percentValue);
    const pixels = percentToPixels(percent, width, height);
    onChange('threshold', pixels);
  };

  // Despeckle filter options
  const despeckleOptions = [
    { value: '', label: 'Off' },
    { value: 'EedDl', label: 'Light' },
    { value: 'EedDl', label: 'Medium (default)' },
    { value: 'EedDl', label: 'Heavy' },
  ];

  return (
    <FormSection
      title="Motion Detection"
      description="Configure motion detection sensitivity and behavior"
      collapsible
      defaultOpen={false}
    >
      <div className="space-y-4">
        <FormSlider
          label="Threshold"
          value={thresholdPercent}
          onChange={(val) => handleThresholdPercentChange(String(val))}
          min={0}
          max={20}
          step={0.1}
          unit="%"
          helpText={`Percentage of frame that must change (${thresholdPixels} pixels at ${width}x${height}). Higher = less sensitive.`}
          error={getError?.('threshold')}
        />

        <FormInput
          label="Threshold Maximum"
          value={String(getValue('threshold_maximum', 0))}
          onChange={(val) => onChange('threshold_maximum', Number(val))}
          type="number"
          helpText="Maximum threshold for auto-tuning (0 = disabled)"
          error={getError?.('threshold_maximum')}
        />

        <FormToggle
          label="Auto-tune Threshold"
          value={getValue('threshold_tune', false) as boolean}
          onChange={(val) => onChange('threshold_tune', val)}
          helpText="Automatically adjust threshold based on noise levels"
        />

        <FormToggle
          label="Auto-tune Noise Level"
          value={getValue('noise_tune', false) as boolean}
          onChange={(val) => onChange('noise_tune', val)}
          helpText="Automatically determine optimal noise level"
        />

        <FormSlider
          label="Noise Level"
          value={Number(getValue('noise_level', 32))}
          onChange={(val) => onChange('noise_level', val)}
          min={1}
          max={255}
          helpText="Noise tolerance (1-255). Lower values detect smaller motions."
          error={getError?.('noise_level')}
        />

        <FormSlider
          label="Light Switch Detection"
          value={Number(getValue('lightswitch_percent', 0))}
          onChange={(val) => onChange('lightswitch_percent', val)}
          min={0}
          max={100}
          unit="%"
          helpText="Ignore sudden brightness changes (0 = disabled). Prevents false triggers from lights turning on/off."
          error={getError?.('lightswitch_percent')}
        />

        <FormSelect
          label="Despeckle Filter"
          value={String(getValue('despeckle_filter', ''))}
          onChange={(val) => onChange('despeckle_filter', val)}
          options={despeckleOptions}
          helpText="Remove noise speckles from motion detection"
        />

        <FormSlider
          label="Smart Mask Speed"
          value={Number(getValue('smart_mask_speed', 0))}
          onChange={(val) => onChange('smart_mask_speed', val)}
          min={0}
          max={10}
          helpText="Auto-mask static areas (0 = disabled, 1-10 = speed). Higher values adapt faster to static objects."
          error={getError?.('smart_mask_speed')}
        />

        <FormSelect
          label="Locate Motion Mode"
          value={String(getValue('locate_motion_mode', 'off'))}
          onChange={(val) => onChange('locate_motion_mode', val)}
          options={LOCATE_MOTION_MODES}
          helpText="Draw box around motion area. 'Preview' = stream only, 'On' = saved images, 'Both' = both."
        />

        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium mb-3">Event Timing</h4>

          <FormInput
            label="Event Gap (seconds)"
            value={String(getValue('event_gap', 60))}
            onChange={(val) => onChange('event_gap', Number(val))}
            type="number"
            min="0"
            helpText="Seconds of no motion before ending an event. Prevents splitting continuous motion into multiple events."
            error={getError?.('event_gap')}
          />

          <FormInput
            label="Pre-Capture (frames)"
            value={String(getValue('pre_capture', 0))}
            onChange={(val) => onChange('pre_capture', Number(val))}
            type="number"
            min="0"
            helpText="Frames to capture before motion detected. Uses CPU/memory to buffer frames."
            error={getError?.('pre_capture')}
          />

          <FormInput
            label="Post-Capture (frames)"
            value={String(getValue('post_capture', 0))}
            onChange={(val) => onChange('post_capture', Number(val))}
            type="number"
            min="0"
            helpText="Frames to capture after motion stops"
            error={getError?.('post_capture')}
          />

          <FormInput
            label="Minimum Motion Frames"
            value={String(getValue('minimum_motion_frames', 1))}
            onChange={(val) => onChange('minimum_motion_frames', Number(val))}
            type="number"
            min="1"
            helpText="Consecutive frames with motion required to trigger event. Filters brief false positives."
            error={getError?.('minimum_motion_frames')}
          />
        </div>
      </div>
    </FormSection>
  );
}
