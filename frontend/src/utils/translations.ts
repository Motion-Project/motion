// Translation utilities for converting between MotionEye UI formats and Motion backend formats

/**
 * Parse resolution string "WxH" into width and height numbers
 */
export function parseResolution(res: string): { width: number; height: number } {
  const [w, h] = res.split('x').map(Number);
  return { width: w || 0, height: h || 0 };
}

/**
 * Format width and height into resolution string "WxH"
 */
export function formatResolution(width: number, height: number): string {
  return `${width}x${height}`;
}

/**
 * Convert motion detection threshold from percentage to pixel count
 * MotionEye uses percentage (e.g., 1.5% of frame)
 * Motion uses pixel count (e.g., 4608 pixels for 640x480 at 1.5%)
 */
export function percentToPixels(percent: number, width: number, height: number): number {
  return Math.round((percent / 100) * width * height);
}

/**
 * Convert motion detection threshold from pixel count to percentage
 */
export function pixelsToPercent(pixels: number, width: number, height: number): number {
  const totalPixels = width * height;
  if (totalPixels === 0) return 0;
  return Number(((pixels / totalPixels) * 100).toFixed(2));
}

/**
 * Text overlay presets
 * Maps MotionEye preset names to Motion format strings
 */
export const TEXT_PRESETS = {
  disabled: '',
  'camera-name': '%$', // Device name
  timestamp: '%Y-%m-%d\\n%T', // Date and time
  // 'custom' is handled separately - user provides the value
} as const;

/**
 * Reverse lookup: Motion format string to preset name
 */
export function motionTextToPreset(motionValue: string): string {
  for (const [preset, value] of Object.entries(TEXT_PRESETS)) {
    if (value === motionValue) return preset;
  }
  return 'custom';
}

/**
 * Convert MotionEye preset name to Motion format string
 */
export function presetToMotionText(preset: string, customValue?: string): string {
  if (preset === 'custom' && customValue !== undefined) {
    return customValue;
  }
  return TEXT_PRESETS[preset as keyof typeof TEXT_PRESETS] || '';
}

/**
 * Capture mode mappings
 * Maps MotionEye capture mode names to Motion parameter values
 */
export interface CaptureMode {
  picture_output: string;
  snapshot_interval?: number;
}

export const CAPTURE_MODE_MAP: Record<string, CaptureMode> = {
  'motion-triggered': { picture_output: 'on' },
  'motion-triggered-one': { picture_output: 'first' },
  best: { picture_output: 'best' },
  center: { picture_output: 'center' },
  'interval-snapshots': { picture_output: 'off', snapshot_interval: 60 },
  manual: { picture_output: 'off', snapshot_interval: 0 },
  off: { picture_output: 'off', snapshot_interval: 0 },
};

/**
 * Determine capture mode from Motion parameters
 */
export function motionToCaptureMode(
  pictureOutput: string,
  snapshotInterval: number
): string {
  // If snapshot interval is set, it's interval mode
  if (snapshotInterval > 0) {
    return 'interval-snapshots';
  }

  // Otherwise, based on picture_output
  switch (pictureOutput) {
    case 'on':
      return 'motion-triggered';
    case 'first':
      return 'motion-triggered-one';
    case 'best':
      return 'best';
    case 'center':
      return 'center';
    case 'off':
      return 'manual';
    default:
      return 'manual';
  }
}

/**
 * Convert capture mode preset to Motion parameter changes
 */
export function captureModeToMotion(mode: string): CaptureMode {
  return (
    CAPTURE_MODE_MAP[mode] || {
      picture_output: 'off',
      snapshot_interval: 0,
    }
  );
}

/**
 * Recording mode mappings
 * emulate_motion=true makes Motion act as if motion is always detected,
 * enabling continuous 24/7 recording regardless of actual motion
 */
export interface RecordingMode {
  movie_output: boolean;
  movie_output_motion?: boolean;
  emulate_motion?: boolean;
}

export const RECORDING_MODE_MAP: Record<string, RecordingMode> = {
  'motion-triggered': { movie_output: true, movie_output_motion: true, emulate_motion: false },
  continuous: { movie_output: true, movie_output_motion: false, emulate_motion: true },
  off: { movie_output: false, emulate_motion: false },
};

/**
 * Determine recording mode from Motion parameters
 * emulate_motion=true indicates continuous recording mode
 */
export function motionToRecordingMode(
  movieOutput: boolean,
  movieOutputMotion: boolean,
  emulateMotion: boolean = false
): string {
  if (!movieOutput) return 'off';
  if (emulateMotion) return 'continuous';
  return movieOutputMotion ? 'motion-triggered' : 'motion-triggered';
}

/**
 * Convert recording mode to Motion parameters
 */
export function recordingModeToMotion(mode: string): RecordingMode {
  return (
    RECORDING_MODE_MAP[mode] || { movie_output: false, emulate_motion: false }
  );
}

/**
 * Streaming resolution presets
 * Motion uses stream_preview_scale (percentage), MotionEye uses descriptive names
 */
export const STREAM_RESOLUTION_MAP = {
  full: 100, // 100% of source
  '75%': 75,
  half: 50,
  '25%': 25,
  custom: 0, // User specifies exact percentage
} as const;

/**
 * Convert stream_preview_scale percentage to preset name
 */
export function motionToStreamResolution(scale: number): string {
  for (const [preset, value] of Object.entries(STREAM_RESOLUTION_MAP)) {
    if (value === scale) return preset;
  }
  return 'custom';
}

/**
 * Convert preset name to stream_preview_scale percentage
 */
export function streamResolutionToMotion(preset: string, customValue?: number): number {
  if (preset === 'custom' && customValue !== undefined) {
    return customValue;
  }
  return STREAM_RESOLUTION_MAP[preset as keyof typeof STREAM_RESOLUTION_MAP] || 100;
}
