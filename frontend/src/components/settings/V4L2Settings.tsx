import { FormSection, FormSlider, FormToggle, FormSelect } from '@/components/form';
import type { V4L2Control } from '@/api/types';

export interface V4L2SettingsProps {
  config: Record<string, { value: string | number | boolean }>;
  onChange: (param: string, value: string | number | boolean) => void;
  controls?: V4L2Control[];
  getError?: (param: string) => string | undefined;
}

/**
 * V4L2 Settings Component
 *
 * Renders runtime controls for USB cameras (V4L2 devices).
 * Controls are discovered dynamically from the camera hardware.
 *
 * Common controls: brightness, contrast, saturation, gain, exposure,
 * white balance, focus, sharpness, etc. (device-dependent)
 *
 * @param config - Camera configuration
 * @param onChange - Configuration change handler
 * @param controls - Array of V4L2 controls from device
 * @param getError - Error message getter
 */
export function V4L2Settings({ config, onChange, controls, getError }: V4L2SettingsProps) {
  if (!controls || controls.length === 0) {
    return (
      <FormSection
        title="Camera Controls"
        description="USB camera settings"
        collapsible
        defaultOpen={false}
      >
        <p className="text-gray-400 text-sm">
          No controls available for this camera.
        </p>
      </FormSection>
    );
  }

  // Group controls by category (heuristic-based grouping)
  const groupedControls = groupV4L2Controls(controls);

  return (
    <FormSection
      title="Camera Controls"
      description="USB camera settings (V4L2)"
      collapsible
      defaultOpen={false}
    >
      {Object.entries(groupedControls).map(([group, groupControls]) => (
        <div key={group} className="space-y-4">
          {group !== 'Other' && (
            <h4 className="text-sm font-medium text-gray-300 mt-4 first:mt-0">{group}</h4>
          )}
          {groupControls.map((control) => (
            <V4L2ControlInput
              key={control.id}
              control={control}
              value={getControlValue(config, control)}
              onChange={(val) => onChange(`v4l2_${control.id}`, val)}
              error={getError?.(`v4l2_${control.id}`)}
            />
          ))}
        </div>
      ))}
    </FormSection>
  );
}

/**
 * V4L2 Control Input Component
 *
 * Renders appropriate form control based on V4L2 control type
 */
function V4L2ControlInput({
  control,
  value,
  onChange,
  error,
}: {
  control: V4L2Control;
  value: number | boolean;
  onChange: (value: number | boolean) => void;
  error?: string;
}) {
  switch (control.type) {
    case 'boolean':
      return (
        <FormToggle
          label={control.name}
          value={Boolean(value)}
          onChange={onChange}
          helpText={`Range: ${control.min}-${control.max}, Default: ${control.default}`}
        />
      );

    case 'menu':
      return (
        <FormSelect
          label={control.name}
          value={String(value)}
          onChange={(v) => onChange(Number(v))}
          options={
            control.menuItems?.map((item) => ({
              value: String(item.value),
              label: item.label,
            })) ?? []
          }
          helpText={`Default: ${control.default}`}
          error={error}
        />
      );

    case 'integer':
    default:
      return (
        <FormSlider
          label={control.name}
          value={Number(value)}
          onChange={onChange}
          min={control.min}
          max={control.max}
          step={control.step ?? 1}
          helpText={`Range: ${control.min}-${control.max}, Default: ${control.default}`}
          error={error}
        />
      );
  }
}

/**
 * Get control value from config with fallback to current value
 */
function getControlValue(
  config: Record<string, { value: string | number | boolean }>,
  control: V4L2Control
): number | boolean {
  const paramKey = `v4l2_${control.id}`;
  const configValue = config[paramKey]?.value;

  if (configValue !== undefined) {
    return control.type === 'boolean' ? Boolean(configValue) : Number(configValue);
  }

  // Fallback to current value from device
  return control.type === 'boolean' ? Boolean(control.current) : control.current;
}

/**
 * Group V4L2 controls by category (heuristic-based)
 *
 * Groups controls into logical categories for better UX.
 * Falls back to "Other" for unrecognized controls.
 */
function groupV4L2Controls(controls: V4L2Control[]): Record<string, V4L2Control[]> {
  const groups: Record<string, V4L2Control[]> = {
    'Image Quality': [],
    'Exposure & Gain': [],
    'White Balance': [],
    'Focus': [],
    'Other': [],
  };

  const categoryKeywords: Record<string, string[]> = {
    'Image Quality': ['brightness', 'contrast', 'saturation', 'hue', 'sharpness', 'gamma'],
    'Exposure & Gain': ['exposure', 'gain', 'iso', 'backlight'],
    'White Balance': ['white', 'balance', 'color', 'colour', 'temperature'],
    'Focus': ['focus', 'zoom', 'pan', 'tilt', 'lens'],
  };

  for (const control of controls) {
    const name = control.name.toLowerCase();
    let categorized = false;

    for (const [category, keywords] of Object.entries(categoryKeywords)) {
      if (keywords.some((keyword) => name.includes(keyword))) {
        groups[category].push(control);
        categorized = true;
        break;
      }
    }

    if (!categorized) {
      groups['Other'].push(control);
    }
  }

  // Remove empty groups
  return Object.fromEntries(
    Object.entries(groups).filter(([_, groupControls]) => groupControls.length > 0)
  );
}
