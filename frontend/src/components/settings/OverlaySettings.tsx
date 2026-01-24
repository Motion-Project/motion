import { useState } from 'react';
import { FormSection, FormInput, FormSelect, FormSlider } from '@/components/form';
import { presetToMotionText, motionTextToPreset } from '@/utils/translations';

export interface OverlaySettingsProps {
  config: Record<string, { value: string | number | boolean }>;
  onChange: (param: string, value: string | number | boolean) => void;
  getError?: (param: string) => string | undefined;
}

export function OverlaySettings({ config, onChange, getError }: OverlaySettingsProps) {
  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue;
  };

  const leftText = String(getValue('text_left', ''));
  const rightText = String(getValue('text_right', ''));

  const leftPreset = motionTextToPreset(leftText);
  const rightPreset = motionTextToPreset(rightText);

  const [customLeftText, setCustomLeftText] = useState(
    leftPreset === 'custom' ? leftText : ''
  );
  const [customRightText, setCustomRightText] = useState(
    rightPreset === 'custom' ? rightText : ''
  );

  const handleLeftPresetChange = (preset: string) => {
    if (preset === 'custom') {
      onChange('text_left', customLeftText);
    } else {
      onChange('text_left', presetToMotionText(preset));
    }
  };

  const handleRightPresetChange = (preset: string) => {
    if (preset === 'custom') {
      onChange('text_right', customRightText);
    } else {
      onChange('text_right', presetToMotionText(preset));
    }
  };

  const handleCustomLeftChange = (value: string) => {
    setCustomLeftText(value);
    onChange('text_left', value);
  };

  const handleCustomRightChange = (value: string) => {
    setCustomRightText(value);
    onChange('text_right', value);
  };

  const presetOptions = [
    { value: 'disabled', label: 'Disabled' },
    { value: 'camera-name', label: 'Camera Name' },
    { value: 'timestamp', label: 'Timestamp' },
    { value: 'custom', label: 'Custom Text' },
  ];

  return (
    <FormSection
      title="Text Overlay"
      description="Add text overlays to video frames"
      collapsible
      defaultOpen={false}
    >
      <FormSelect
        label="Left Text"
        value={leftPreset}
        onChange={handleLeftPresetChange}
        options={presetOptions}
        helpText="Text displayed in top-left corner"
      />

      {leftPreset === 'custom' && (
        <FormInput
          label="Custom Left Text"
          value={customLeftText}
          onChange={handleCustomLeftChange}
          placeholder="Enter custom text"
          helpText="Use Motion format codes: %Y (year), %m (month), %d (day), %H (hour), %M (minute), %S (second), %$ (camera name)"
          error={getError?.('text_left')}
        />
      )}

      <FormSelect
        label="Right Text"
        value={rightPreset}
        onChange={handleRightPresetChange}
        options={presetOptions}
        helpText="Text displayed in top-right corner"
      />

      {rightPreset === 'custom' && (
        <FormInput
          label="Custom Right Text"
          value={customRightText}
          onChange={handleCustomRightChange}
          placeholder="Enter custom text"
          helpText="Use Motion format codes: %Y (year), %m (month), %d (day), %H (hour), %M (minute), %S (second), %$ (camera name)"
          error={getError?.('text_right')}
        />
      )}

      <FormSlider
        label="Text Scale"
        value={Number(getValue('text_scale', 1))}
        onChange={(val) => onChange('text_scale', val)}
        min={1}
        max={10}
        unit="x"
        helpText="Text size multiplier (1-10)"
        error={getError?.('text_scale')}
      />
    </FormSection>
  );
}
