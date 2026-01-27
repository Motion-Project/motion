import { useState } from 'react';
import { FormSection, FormInput, FormSelect, FormToggle, FormSlider } from '@/components/form';
import {
  MOVIE_CONTAINERS,
  ENCODER_PRESETS,
  isHardwareCodec,
  isHighCpuCodec
} from '@/utils/parameterMappings';
import { recordingModeToMotion, motionToRecordingMode } from '@/utils/translations';
import { useDeviceInfo, isPi5, isPi4, hasHardwareEncoder, isHighTemperature } from '@/hooks/useDeviceInfo';

export interface MovieSettingsProps {
  config: Record<string, { value: string | number | boolean }>;
  onChange: (param: string, value: string | number | boolean) => void;
  getError?: (param: string) => string | undefined;
  showPassthrough?: boolean;
}

export function MovieSettings({ config, onChange, getError, showPassthrough = true }: MovieSettingsProps) {
  const { data: deviceInfo } = useDeviceInfo();

  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    return config[param]?.value ?? defaultValue;
  };

  // Determine current recording mode
  const movieOutput = getValue('movie_output', false) as boolean;
  const movieOutputMotion = getValue('movie_output_motion', false) as boolean;
  const emulateMotion = getValue('emulate_motion', false) as boolean;
  const currentMode = motionToRecordingMode(movieOutput, movieOutputMotion, emulateMotion);

  const [selectedMode, setSelectedMode] = useState(currentMode);

  const handleRecordingModeChange = (mode: string) => {
    setSelectedMode(mode);
    const motionParams = recordingModeToMotion(mode);

    // Apply the mode's parameter changes
    onChange('movie_output', motionParams.movie_output);
    if (motionParams.movie_output_motion !== undefined) {
      onChange('movie_output_motion', motionParams.movie_output_motion);
    }
    // Set emulate_motion for continuous recording
    if (motionParams.emulate_motion !== undefined) {
      onChange('emulate_motion', motionParams.emulate_motion);
    }
  };

  // Recording mode options
  const recordingModeOptions = [
    { value: 'off', label: 'Off' },
    { value: 'motion-triggered', label: 'Motion Triggered' },
    { value: 'continuous', label: 'Continuous Recording' },
  ];

  // Format code reference for help text
  const formatCodes = [
    '%Y - Year',
    '%m - Month',
    '%d - Day',
    '%H - Hour',
    '%M - Minute',
    '%S - Second',
    '%v - Event number',
    '%$ - Camera name',
  ].join(', ');

  // Current container value for conditional warnings
  const containerValue = String(getValue('movie_container', 'mp4'));

  // Filter container options based on hardware encoder availability
  const getAvailableContainers = () => {
    // If device info not loaded or hardware encoder available, show all options
    if (!deviceInfo || hasHardwareEncoder(deviceInfo)) {
      return MOVIE_CONTAINERS;
    }

    // Filter out hardware encoding options when no hardware encoder
    return MOVIE_CONTAINERS.filter(container => !isHardwareCodec(container.value));
  };

  // Encoder preset only applies to software encoding, not:
  // - Passthrough mode
  // - Hardware encoding (h264_v4l2m2m)
  // - VP8/VP9 (webm)
  const showEncoderPreset = () => {
    const passthrough = getValue('movie_passthrough', false);

    if (passthrough) return false;
    if (isHardwareCodec(containerValue)) return false;
    if (containerValue === 'webm') return false;

    return true;
  };

  return (
    <FormSection
      title="Movie Settings"
      description="Configure video recording settings"
      collapsible
      defaultOpen={false}
    >
      <div className="space-y-4">
        <FormSelect
          label="Recording Mode"
          value={selectedMode}
          onChange={handleRecordingModeChange}
          options={recordingModeOptions}
          helpText="When to record video. Motion Triggered = only during events, Continuous = always record."
        />

        <FormSlider
          label="Movie Quality"
          value={Number(getValue('movie_quality', 75))}
          onChange={(val) => onChange('movie_quality', val)}
          min={1}
          max={100}
          unit="%"
          helpText="Video encoding quality (1-100). Higher = better quality, larger files, more CPU."
          error={getError?.('movie_quality')}
        />

        <FormInput
          label="Movie Filename Pattern"
          value={String(getValue('movie_filename', '%Y%m%d%H%M%S'))}
          onChange={(val) => onChange('movie_filename', val)}
          helpText={`Format codes: ${formatCodes}`}
          error={getError?.('movie_filename')}
        />

        <FormSelect
          label="Container Format"
          value={String(getValue('movie_container', 'mp4'))}
          onChange={(val) => onChange('movie_container', val)}
          options={getAvailableContainers()}
          helpText="Video container format. Hardware encoding requires v4l2m2m support."
        />

        {/* Hardware encoding info - when hardware codec is selected */}
        {isHardwareCodec(containerValue) && hasHardwareEncoder(deviceInfo) && (
          <div className="text-xs text-green-400 bg-green-950/30 p-3 rounded mt-2">
            <strong>Hardware Encoding Active:</strong> Using h264_v4l2m2m hardware encoder
            for ~10% CPU usage instead of 40-70%.
          </div>
        )}

        {/* Hardware encoder not available - explain why hardware options aren't shown */}
        {deviceInfo && !hasHardwareEncoder(deviceInfo) && (
          <div className="text-xs text-blue-400 bg-blue-950/30 p-3 rounded mt-2">
            <strong>Hardware Encoding Not Available:</strong> This device does not have a hardware H.264 encoder.
            {isPi5(deviceInfo) && ' Pi 5 does not include a hardware encoder.'}
            {' '}Hardware encoding options (h264_v4l2m2m) are hidden. Using software encoding (~40-70% CPU).
          </div>
        )}

        {/* Hardware codec selected but device info not yet loaded */}
        {isHardwareCodec(containerValue) && !deviceInfo && (
          <div className="text-xs text-blue-400 bg-blue-950/30 p-3 rounded mt-2">
            <strong>Hardware Encoding:</strong> Uses h264_v4l2m2m hardware encoder
            for ~10% CPU usage instead of 40-70%. Only available on devices with v4l2m2m support.
          </div>
        )}

        {/* High CPU warning for HEVC */}
        {isHighCpuCodec(containerValue) && (
          <div className="text-xs text-amber-400 bg-amber-950/30 p-3 rounded mt-2">
            <strong>High CPU Warning:</strong> H.265/HEVC software encoding uses
            80-100% CPU on Raspberry Pi. Not recommended for continuous recording.
            Consider H.264 for better performance.
          </div>
        )}

        {/* Hardware encoding tip - shown when software codec is selected on device with hardware encoder */}
        {hasHardwareEncoder(deviceInfo) &&
         !isHardwareCodec(containerValue) && !getValue('movie_passthrough', false) &&
         !containerValue.includes('webm') && !isHighCpuCodec(containerValue) && (
          <div className="text-xs text-blue-400 bg-blue-950/30 p-3 rounded mt-2">
            <strong>Hardware Encoding Available:</strong> This device has a hardware
            H.264 encoder. Select "MKV - H.264 Hardware" or "MP4 - H.264 Hardware"
            to reduce CPU from ~40-70% to ~10%.
          </div>
        )}

        {/* Generic software encoding tip (shown when device info unavailable) */}
        {!deviceInfo && !isHardwareCodec(containerValue) && !getValue('movie_passthrough', false) &&
         !containerValue.includes('webm') && !isHighCpuCodec(containerValue) && (
          <div className="text-xs text-gray-400 bg-surface-elevated p-3 rounded mt-2">
            <strong>Tip:</strong> If your device has hardware encoding support (e.g., Raspberry Pi 4),
            consider selecting "MKV - H.264 Hardware" or "MP4 - H.264 Hardware" for ~10% CPU
            instead of ~40-70% with software encoding.
          </div>
        )}

        {/* WebM format info */}
        {containerValue === 'webm' && (
          <div className="text-xs text-blue-400 bg-blue-950/30 p-3 rounded mt-2">
            <strong>WebM Format:</strong> Uses VP8 codec, optimized for web streaming.
            Encoder preset setting does not apply to VP8.
          </div>
        )}

        {showEncoderPreset() && (
          <FormSelect
            label="Encoder Preset"
            value={String(getValue('movie_encoder_preset', 'medium'))}
            onChange={(val) => onChange('movie_encoder_preset', val)}
            options={ENCODER_PRESETS.map(p => ({
              value: p.value,
              label: p.label,
            }))}
            helpText="Tradeoff between CPU usage and video quality. Lower presets use less CPU but produce lower quality video. Requires restart to take effect."
          />
        )}

        <FormInput
          label="Max Duration (seconds)"
          value={String(getValue('movie_max_time', 0))}
          onChange={(val) => onChange('movie_max_time', Number(val))}
          type="number"
          min="0"
          helpText="Maximum movie length (0 = unlimited). Splits long events into multiple files."
          error={getError?.('movie_max_time')}
        />

        {/* Passthrough mode - only available for NETCAM cameras */}
        {showPassthrough && (
          <FormToggle
            label="Passthrough Mode"
            value={getValue('movie_passthrough', false) as boolean}
            onChange={(val) => onChange('movie_passthrough', val)}
            helpText="Copy codec without re-encoding (NETCAM only). Reduces CPU but may cause compatibility issues."
          />
        )}

        <div className="border-t border-surface-elevated pt-4">
          <h4 className="font-medium mb-2 text-sm">Format Code Reference</h4>
          <div className="text-xs text-gray-400 space-y-2">
            <p className="font-medium text-gray-300">Dynamic Folder Examples (Recommended):</p>
            <p><code>%Y-%m-%d/%H%M%S</code> → <code>2025-01-29/143022.mkv</code></p>
            <p><code>%Y/%m/%d/%v-%H%M%S</code> → <code>2025/01/29/42-143022.mkv</code></p>
            <p><code>%$/%Y-%m-%d/%v</code> → <code>Camera1/2025-01-29/42.mkv</code></p>
            <p className="mt-2 font-medium text-gray-300">Flat Structure:</p>
            <p><code>%Y%m%d%H%M%S</code> → <code>20250129143022.mkv</code></p>
            <p className="mt-2">Available codes: {formatCodes}</p>
            <p className="mt-2 text-yellow-200"><strong>Tip:</strong> Using date-based folders like <code>%Y-%m-%d/</code> keeps files organized and makes browsing faster.</p>
          </div>
        </div>

        {/* Pi 5 specific warning for continuous recording */}
        {selectedMode === 'continuous' && isPi5(deviceInfo) && !getValue('movie_passthrough', false) && (
          <div className="text-xs text-amber-400 bg-amber-950/30 p-3 rounded">
            <strong>Pi 5 CPU Warning:</strong> Pi 5 does not have a hardware H.264 encoder.
            Continuous recording uses software encoding (~35-60% CPU constant).
            <ul className="list-disc ml-4 mt-1">
              <li>Use encoder preset "Ultrafast" to reduce CPU by ~30%</li>
              <li>Add active cooling (fan) to prevent thermal throttling</li>
              <li>Enable passthrough if source is already H.264</li>
            </ul>
          </div>
        )}

        {/* Pi 4 specific info for continuous recording with hardware encoder */}
        {selectedMode === 'continuous' && isPi4(deviceInfo) && hasHardwareEncoder(deviceInfo) && (
          <div className="text-xs text-green-400 bg-green-950/30 p-3 rounded">
            <strong>Continuous Recording on Pi 4:</strong> Camera will record 24/7.
            {isHardwareCodec(containerValue) ? (
              <span> Using hardware encoder - expect ~10% CPU usage.</span>
            ) : getValue('movie_passthrough', false) ? (
              <span> Passthrough mode enabled - expect ~5-10% CPU usage.</span>
            ) : (
              <span> Consider using hardware encoder (MKV/MP4 H.264 Hardware) for ~10% CPU instead of ~40-70%.</span>
            )}
          </div>
        )}

        {/* Generic continuous recording warning (fallback when no device info) */}
        {selectedMode === 'continuous' && !deviceInfo && (
          <div className="text-xs text-yellow-200 bg-yellow-600/10 border border-yellow-600/30 p-3 rounded">
            <strong>Continuous Recording:</strong> Camera will record 24/7 regardless of motion.
            Expected CPU usage on Raspberry Pi:
            <ul className="list-disc ml-4 mt-1">
              <li><strong>Pi 4 with hardware encoder:</strong> ~10% CPU</li>
              <li><strong>Pi 5 or Pi 4 software encoding:</strong> ~35-60% CPU depending on preset</li>
              <li><strong>Passthrough mode:</strong> ~5-10% CPU (if source is H.264)</li>
            </ul>
          </div>
        )}

        {/* Temperature warning */}
        {isHighTemperature(deviceInfo) && (
          <div className="text-xs text-red-400 bg-red-950/30 p-3 rounded">
            <strong>High Temperature Warning:</strong> Device is running at {deviceInfo?.temperature?.celsius.toFixed(1)}°C.
            Consider reducing encoding quality or adding active cooling.
          </div>
        )}
      </div>
    </FormSection>
  );
}
