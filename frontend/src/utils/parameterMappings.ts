// Direct parameter mappings between MotionEye UI names and Motion backend params
// These require no translation, just pass through the value
//
// SYNC REQUIREMENT: When changing default values or "(Default)" labels, also update:
//   1. src/conf.cpp - Compiled-in defaults (edit_generic_* functions)
//   2. data/motion-dist.conf.in - Config template (installed as starting config)

export const DIRECT_MAPPINGS = {
  // Device
  device_name: 'device_name',
  framerate: 'framerate',
  rotate: 'rotate',
  width: 'width',
  height: 'height',

  // libcamera controls
  brightness: 'libcam_brightness',
  contrast: 'libcam_contrast',
  gain: 'libcam_gain',
  awb_enable: 'libcam_awb_enable',
  awb_mode: 'libcam_awb_mode',
  awb_locked: 'libcam_awb_locked',
  colour_temp: 'libcam_colour_temp',
  colour_gain_r: 'libcam_colour_gain_r',
  colour_gain_b: 'libcam_colour_gain_b',
  autofocus_mode: 'libcam_af_mode',
  autofocus_range: 'libcam_af_range',
  autofocus_speed: 'libcam_af_speed',
  lens_position: 'libcam_lens_position',
  libcam_buffer_count: 'libcam_buffer_count',

  // Text overlay
  text_left: 'text_left',
  text_right: 'text_right',
  text_scale: 'text_scale',

  // Streaming
  stream_quality: 'stream_quality',
  stream_maxrate: 'stream_maxrate',
  webcontrol_auth_method: 'webcontrol_auth_method',
  stream_motion: 'stream_motion',
  stream_preview_scale: 'stream_preview_scale',

  // Motion detection
  threshold: 'threshold',
  threshold_maximum: 'threshold_maximum',
  threshold_tune: 'threshold_tune',
  noise_tune: 'noise_tune',
  noise_level: 'noise_level',
  lightswitch_percent: 'lightswitch_percent',
  despeckle_filter: 'despeckle_filter',
  smart_mask_speed: 'smart_mask_speed',
  locate_motion_mode: 'locate_motion_mode',

  // Event timing
  event_gap: 'event_gap',
  pre_capture: 'pre_capture',
  post_capture: 'post_capture',
  minimum_motion_frames: 'minimum_motion_frames',

  // Pictures
  picture_output: 'picture_output',
  picture_quality: 'picture_quality',
  picture_filename: 'picture_filename',
  snapshot_interval: 'snapshot_interval',
  picture_max_per_event: 'picture_max_per_event',
  picture_min_interval: 'picture_min_interval',

  // Movies
  movie_output: 'movie_output',
  movie_output_motion: 'movie_output_motion',
  movie_quality: 'movie_quality',
  movie_filename: 'movie_filename',
  movie_max_time: 'movie_max_time',
  movie_passthrough: 'movie_passthrough',
  movie_container: 'movie_container',
  movie_encoder_preset: 'movie_encoder_preset',
  emulate_motion: 'emulate_motion',

  // Scripts/hooks
  on_event_start: 'on_event_start',
  on_event_end: 'on_event_end',
  on_picture_save: 'on_picture_save',
  on_movie_start: 'on_movie_start',
  on_movie_end: 'on_movie_end',

  // Masks
  mask_file: 'mask_file',
  mask_privacy: 'mask_privacy',

  // Storage
  target_dir: 'target_dir',

  // Schedule
  schedule_params: 'schedule_params',
} as const;

// Standard resolution presets for dropdown
export const RESOLUTION_PRESETS = [
  { width: 320, height: 240, label: '320x240 (QVGA)' },
  { width: 640, height: 480, label: '640x480 (VGA)' },
  { width: 800, height: 600, label: '800x600 (SVGA)' },
  { width: 1024, height: 768, label: '1024x768 (XGA)' },
  { width: 1280, height: 720, label: '1280x720 (HD)' },
  { width: 1920, height: 1080, label: '1920x1080 (Full HD)' },
  { width: 2560, height: 1440, label: '2560x1440 (QHD)' },
  { width: 3840, height: 2160, label: '3840x2160 (4K)' },
];

// Rotation presets
export const ROTATION_OPTIONS = [
  { value: 0, label: '0°' },
  { value: 90, label: '90°' },
  { value: 180, label: '180°' },
  { value: 270, label: '270°' },
];

// libcamera AWB modes - values must match libcamera::controls::AwbModeEnum
// Source: https://libcamera.org/api-html/namespacelibcamera_1_1controls.html
export const AWB_MODES = [
  { value: 0, label: 'Auto' },
  { value: 1, label: 'Incandescent' },
  { value: 2, label: 'Tungsten' },
  { value: 3, label: 'Fluorescent' },
  { value: 4, label: 'Indoor' },
  { value: 5, label: 'Daylight' },
  { value: 6, label: 'Cloudy' },
  { value: 7, label: 'Custom' },
];

// libcamera autofocus modes
export const AUTOFOCUS_MODES = [
  { value: 0, label: 'Manual' },
  { value: 1, label: 'Auto' },
  { value: 2, label: 'Continuous' },
];

// libcamera autofocus ranges
export const AUTOFOCUS_RANGES = [
  { value: 0, label: 'Normal' },
  { value: 1, label: 'Macro' },
  { value: 2, label: 'Full' },
];

// libcamera autofocus speeds
export const AUTOFOCUS_SPEEDS = [
  { value: 0, label: 'Normal' },
  { value: 1, label: 'Fast' },
];

// Picture output modes
export const PICTURE_OUTPUT_MODES = [
  { value: 'off', label: 'Off' },
  { value: 'on', label: 'On (all motion)' },
  { value: 'first', label: 'First frame only' },
  { value: 'best', label: 'Best quality frame' },
  { value: 'center', label: 'Center frame' },
];

// Movie container formats with optional codec specification
// Format: container or container:codec
// Backend parses colon-separated format in movie.cpp
export const MOVIE_CONTAINERS = [
  // === Basic Containers (auto-select codec) ===
  { value: 'mkv', label: 'MKV (Matroska)', group: 'basic' },
  { value: 'mp4', label: 'MP4', group: 'basic' },
  { value: 'mov', label: 'MOV (QuickTime)', group: 'basic' },
  { value: '3gp', label: '3GP (Mobile)', group: 'basic' },

  // === Hardware Encoding (requires v4l2m2m support, ~10% CPU) ===
  { value: 'mkv:h264_v4l2m2m', label: 'MKV - H.264 Hardware', group: 'hardware' },
  { value: 'mp4:h264_v4l2m2m', label: 'MP4 - H.264 Hardware', group: 'hardware' },

  // === Software H.264 (explicit) ===
  { value: 'mkv:libx264', label: 'MKV - H.264 Software', group: 'software' },
  { value: 'mp4:libx264', label: 'MP4 - H.264 Software', group: 'software' },

  // === H.265/HEVC (high CPU warning) ===
  { value: 'mkv:libx265', label: 'MKV - H.265 Software (high CPU)', group: 'hevc' },
  { value: 'hevc', label: 'HEVC/H.265 in MP4 (high CPU)', group: 'hevc' },

  // === WebM/VP8 ===
  { value: 'webm', label: 'WebM (VP8)', group: 'webm' },
];

// Check if container uses hardware encoding
export function isHardwareCodec(container: string): boolean {
  return container.includes('v4l2m2m');
}

// Check if container uses high-CPU codec
export function isHighCpuCodec(container: string): boolean {
  return container.includes('libx265') || container === 'hevc';
}

// Get container base (before colon)
export function getContainerBase(container: string): string {
  const colonPos = container.indexOf(':');
  return colonPos === -1 ? container : container.substring(0, colonPos);
}

// Encoder presets for software encoding (libx264/libx265)
// Affects CPU usage vs quality tradeoff
// Note: Does NOT apply to hardware encoding or passthrough
export const ENCODER_PRESETS = [
  { value: 'ultrafast', label: 'Ultrafast', description: 'Lowest CPU (~35-40%), lowest quality' },
  { value: 'superfast', label: 'Superfast', description: 'Very low CPU' },
  { value: 'veryfast', label: 'Very Fast', description: 'Low CPU' },
  { value: 'faster', label: 'Faster', description: 'Below average CPU' },
  { value: 'fast', label: 'Fast', description: 'Slightly below average CPU' },
  { value: 'medium', label: 'Medium', description: 'Balanced CPU/quality (~50-60%)' },
  { value: 'slow', label: 'Slow', description: 'Above average quality' },
  { value: 'slower', label: 'Slower', description: 'High quality' },
  { value: 'veryslow', label: 'Very Slow', description: 'Highest quality (~70-80%), highest CPU' },
];

// Auth method options
export const AUTH_METHODS = [
  { value: 0, label: 'None' },
  { value: 1, label: 'Basic' },
  { value: 2, label: 'Digest' },
];

// Locate motion mode options
export const LOCATE_MOTION_MODES = [
  { value: 'off', label: 'Off' },
  { value: 'on', label: 'On' },
  { value: 'preview', label: 'Preview' },
  { value: 'both', label: 'Both' },
];
