import { FormSection, FormInput, FormSelect, FormSlider } from '@/components/form';
import { RESOLUTION_PRESETS, ROTATION_OPTIONS } from '@/utils/parameterMappings';
import { formatResolution, parseResolution } from '@/utils/translations';

export interface DeviceSettingsProps {
  config: Record<string, { value: string | number | boolean }>;
  onChange: (param: string, value: string | number | boolean) => void;
  getError?: (param: string) => string | undefined;
}

export function DeviceSettings({ config, onChange, getError }: DeviceSettingsProps) {
  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue;
  };

  // Get current resolution
  const width = Number(getValue('width', 640));
  const height = Number(getValue('height', 480));
  const currentResolution = formatResolution(width, height);

  // Check if current resolution matches a preset
  const matchesPreset = RESOLUTION_PRESETS.some(
    (preset) => preset.width === width && preset.height === height
  );

  const handleResolutionChange = (resolution: string) => {
    if (resolution === 'custom') {
      // Keep current values when switching to custom
      return;
    }

    const { width: newWidth, height: newHeight } = parseResolution(resolution);
    onChange('width', newWidth);
    onChange('height', newHeight);
  };

  const handleCustomWidth = (value: string) => {
    onChange('width', Number(value));
  };

  const handleCustomHeight = (value: string) => {
    onChange('height', Number(value));
  };

  return (
    <FormSection
      title="Device Settings"
      description="Basic camera configuration and identification"
      collapsible
      defaultOpen={false}
    >
      <FormInput
        label="Camera Name"
        value={String(getValue('device_name', ''))}
        onChange={(val) => onChange('device_name', val)}
        placeholder="My Camera"
        helpText="Friendly name for this camera"
        error={getError?.('device_name')}
      />

      <FormSelect
        label="Resolution"
        value={matchesPreset ? currentResolution : 'custom'}
        onChange={handleResolutionChange}
        options={[
          ...RESOLUTION_PRESETS.map((preset) => ({
            value: formatResolution(preset.width, preset.height),
            label: preset.label,
          })),
          { value: 'custom', label: 'Custom' },
        ]}
        helpText="Video resolution (width x height)"
      />

      {!matchesPreset && (
        <div className="grid grid-cols-2 gap-4">
          <FormInput
            label="Width"
            value={String(width)}
            onChange={handleCustomWidth}
            type="number"
            helpText="Custom width in pixels"
            error={getError?.('width')}
          />
          <FormInput
            label="Height"
            value={String(height)}
            onChange={handleCustomHeight}
            type="number"
            helpText="Custom height in pixels"
            error={getError?.('height')}
          />
        </div>
      )}

      <FormSlider
        label="Framerate"
        value={Number(getValue('framerate', 15))}
        onChange={(val) => onChange('framerate', val)}
        min={2}
        max={30}
        unit=" fps"
        helpText="Frames per second (higher uses more CPU)"
        error={getError?.('framerate')}
      />

      <FormSelect
        label="Rotation"
        value={String(getValue('rotate', 0))}
        onChange={(val) => onChange('rotate', Number(val))}
        options={ROTATION_OPTIONS.map((opt) => ({
          value: String(opt.value),
          label: opt.label,
        }))}
        helpText="Rotate camera image"
      />
    </FormSection>
  );
}
